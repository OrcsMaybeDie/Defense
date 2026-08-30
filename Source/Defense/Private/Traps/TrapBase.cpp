#include "Traps/TrapBase.h"

#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Collision/DefenseCollisionChannels.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Traps/TrapData.h"
#include "Traps/Grid/GridManager.h"

namespace
{
	constexpr float WallTraceRange = 1400.f;
	constexpr float WallTraceStartOffset = 10.f;
	constexpr float WallTraceDebugTime = 0.35f;
	const FVector WallTraceBoxExtent(120.f, 140.f, 20.f);
	constexpr float WallTraceLaneOffset = 120.f;
	constexpr float WallEffectLaneOffset = 50.f;

	float GetBoxHalfExtentAlongDirection(const UBoxComponent* BoxComponent, const FVector& WorldDirection)
	{
		if (!BoxComponent) return 0.f;

		const FVector Direction = WorldDirection.GetSafeNormal();
		const FVector Extent = BoxComponent->GetScaledBoxExtent();

		return FMath::Abs(FVector::DotProduct(BoxComponent->GetForwardVector(), Direction)) * Extent.X
			+ FMath::Abs(FVector::DotProduct(BoxComponent->GetRightVector(), Direction)) * Extent.Y
			+ FMath::Abs(FVector::DotProduct(BoxComponent->GetUpVector(), Direction)) * Extent.Z;
	}

	bool IsCombatEnemy(const AActor* Actor)
	{
		const AEnemyBase* Enemy = Cast<AEnemyBase>(Actor);
		return Enemy && Enemy->EnemyMode == EEnemyMode::Combat;
	}
}

const FName ATrapBase::ManualDamageAreaTag(TEXT("ManualDamageArea"));

ATrapBase::ATrapBase()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true; // Actor 서버 -> 클라로 복제
	// SetReplicateMovement(true); // Actor의 위치/회전/속도 같은 movement 정보 복제

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);

	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(SceneRoot);

	DamageArea = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageArea"));
	DamageArea->SetupAttachment(SceneRoot);
	DamageArea->SetBoxExtent(FVector(50.f, 50.f, 50.f)); // test

	ApplyTrapCollision();
}

void ATrapBase::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATrapBase, Damage);
	DOREPLIFETIME(ATrapBase, DamageInterval);
	
	DOREPLIFETIME(ATrapBase, RuntimeState);
	DOREPLIFETIME(ATrapBase, OwnerPS);
	DOREPLIFETIME(ATrapBase, OccupiedCells);
}

void ATrapBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RefreshTrapMeshComponents();
	CenterTrapMeshOnRoot();
	SyncDamageAreaToMesh();
}

void ATrapBase::BeginPlay()
{
	Super::BeginPlay();

	// Grid Actor 자체의 Scale은 항상 1이며, 함정별 크기는 Mesh Relative Scale로 관리한다.
	// BP Root/Construction Script Scale이 프리뷰와 실제 스폰 크기를 다르게 만드는 것도 방지한다.
	SetActorScale3D(FVector::OneVector);
	RefreshTrapMeshComponents();
	CenterTrapMeshOnRoot();
	ResetAttackAnimation();
	ApplyTrapCollision();

	if (DamageArea)
	{
		DamageArea->OnComponentBeginOverlap.AddUniqueDynamic(this, &ATrapBase::OnDamageAreaBeginOverlap);
		DamageArea->OnComponentEndOverlap.AddUniqueDynamic(this, &ATrapBase::OnDamageAreaEndOverlap);
	}

	SyncDamageAreaToMesh();

	if (!HasAuthority() && !OccupiedCells.IsEmpty())
	{
		OnRep_OccupiedCells();
	}
}

void ATrapBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDamageTimer();

	if (!HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AGridManager> It(World); It; ++It)
			{
				It->ReleaseClientTrap(this);
				break;
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ATrapBase::InitializePreviewTrap(UTrapData* TrapData)
{
	if (bInitialized) return;

	bInitialized = true;
	RuntimeState = ETrapRuntimeState::Preview;
	OwnerPS = nullptr;
	OverlappingEnemies.Empty();

	ConfigureFromTrapData(TrapData);
	StopDamageTimer();
	ResetAttackAnimation();
	ApplyTrapCollision();

	ApplyPreviewVisual();
}

void ATrapBase::InitializePlacedTrap(UTrapData* TrapData, ADefensePlayerState* InInstalledByPlayerState)
{
	InitializePlacedTrap(TrapData, InInstalledByPlayerState, {});
}

void ATrapBase::DestroyByEnemy()
{
	if (!HasAuthority()) return;

	Multicast_PlayEnemyDestroyedVFX();
	Destroy();
}

void ATrapBase::InitializePlacedTrap(
	UTrapData* TrapData,
	ADefensePlayerState* InInstalledByPlayerState,
	const TArray<FTrapCellKey>& InOccupiedCells
)
{
	if (bInitialized) return;

	bInitialized = true;
	RuntimeState = ETrapRuntimeState::Placed;
	OwnerPS = InInstalledByPlayerState;
	OccupiedCells = InOccupiedCells;

	ConfigureFromTrapData(TrapData);
	ResetAttackAnimation();
	ApplyTrapCollision();

	if (DamageArea)
	{
		DamageArea->UpdateOverlaps();
		CacheCurrentOverlaps();
	}
	
	StartDamageTimer();
}

void ATrapBase::ConfigureFromTrapData(UTrapData* TrapData)
{
	if (!TrapData) return;

	SourceTrapData = TrapData;
	Damage = TrapData->Damage;
	DamageInterval = TrapData->DamageInterval;
}

UMeshComponent* ATrapBase::GetActiveTrapMeshComponent() const
{
	// 두 에셋이 모두 지정된 경우 Skeletal Mesh를 우선 사용
	if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset())
	{
		return SkeletalMesh;
	}

	return Mesh && Mesh->GetStaticMesh() ? Mesh.Get() : nullptr;
}

bool ATrapBase::GetTrapMeshLocalBounds(FVector& OutBoundsCenter, FVector& OutBoundsExtent) const
{
	if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset())
	{
		const FBoxSphereBounds MeshBounds = SkeletalMesh->GetSkeletalMeshAsset()->GetBounds();
		OutBoundsCenter = MeshBounds.Origin;
		OutBoundsExtent = MeshBounds.BoxExtent;
		return true;
	}

	if (Mesh && Mesh->GetStaticMesh())
	{
		FVector BoundsMin;
		FVector BoundsMax;
		Mesh->GetLocalBounds(BoundsMin, BoundsMax);
		OutBoundsCenter = (BoundsMin + BoundsMax) * 0.5f;
		OutBoundsExtent = (BoundsMax - BoundsMin) * 0.5f;
		return true;
	}

	return false;
}

void ATrapBase::RefreshTrapMeshComponents()
{
	const bool bUseSkeletalMesh = SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset();
	const bool bUseStaticMesh = !bUseSkeletalMesh && Mesh && Mesh->GetStaticMesh();

	if (Mesh)
	{
		Mesh->SetCastShadow(false);
		Mesh->SetVisibility(bUseStaticMesh, true);
		Mesh->SetHiddenInGame(!bUseStaticMesh, true);
	}

	if (SkeletalMesh)
	{
		SkeletalMesh->SetCastShadow(false);
		SkeletalMesh->SetVisibility(bUseSkeletalMesh, true);
		SkeletalMesh->SetHiddenInGame(!bUseSkeletalMesh, true);
		SkeletalMesh->SetComponentTickEnabled(bUseSkeletalMesh);
	}
}

void ATrapBase::CenterTrapMeshOnRoot()
{
	UMeshComponent* ActiveMesh = GetActiveTrapMeshComponent();
	FVector BoundsCenter;
	FVector BoundsExtent;
	if (!ActiveMesh || !GetTrapMeshLocalBounds(BoundsCenter, BoundsExtent)) return;

	// Actor 원점 = Grid가 계산한 Footprint 중심
	// 메시의 BP 회전/스케일을 적용한 뒤, 설치 평면의 XY Bounds 중심을 Actor 원점에 맞춤
	const FVector MeshScale = ActiveMesh->GetRelativeScale3D();
	const FVector ScaledBoundsCenter = BoundsCenter * MeshScale;
	const FVector TransformedBoundsCenter = ActiveMesh->GetRelativeRotation().RotateVector(ScaledBoundsCenter);
	FVector MeshLocation = ActiveMesh->GetRelativeLocation();
	MeshLocation.X = -TransformedBoundsCenter.X;
	MeshLocation.Y = -TransformedBoundsCenter.Y;
	ActiveMesh->SetRelativeLocation(MeshLocation);
}

void ATrapBase::OnRep_RuntimeState()
{
	ResetAttackAnimation();
	ApplyTrapCollision();
}

void ATrapBase::ApplyTrapCollision()
{
	SetActorEnableCollision(IsPlaced());
	UMeshComponent* ActiveMesh = GetActiveTrapMeshComponent();

	// Trap 유형 2개
	if (Mesh)
	{
		Mesh->SetCanEverAffectNavigation(false); // Mesh Nav에서 제외 (판매용)
		Mesh->SetCollisionObjectType(ECC_WorldDynamic);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCollisionEnabled(IsPlaced() && ActiveMesh == Mesh.Get()
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision
		);
	}
	if (SkeletalMesh)
	{
		SkeletalMesh->SetCanEverAffectNavigation(false); // Mesh Nav에서 제외 (판매용)
		SkeletalMesh->SetCollisionObjectType(ECC_WorldDynamic);
		SkeletalMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		SkeletalMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		SkeletalMesh->SetGenerateOverlapEvents(false);
		SkeletalMesh->SetCollisionEnabled(IsPlaced() && ActiveMesh == SkeletalMesh.Get()
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision
		);
	}

	if (DamageArea)
	{
		DamageArea->SetCollisionObjectType(ECC_WorldDynamic);
		DamageArea->SetCollisionResponseToAllChannels(ECR_Ignore);
		DamageArea->SetCollisionResponseToChannel(DefenseCollisionChannels::Enemy, ECR_Overlap);
		DamageArea->SetCollisionResponseToChannel(ECC_Pawn, ShouldBlockPawn() ? ECR_Block : ECR_Ignore);
		DamageArea->SetGenerateOverlapEvents(IsPlaced());
		DamageArea->SetCollisionEnabled(IsPlaced() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void ATrapBase::OnRep_OccupiedCells()
{
	if (HasAuthority() || OccupiedCells.IsEmpty())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AGridManager> It(World); It; ++It)
		{
			It->RegisterClientOccupiedCells(this, OccupiedCells);
			break;
		}
	}
}

void ATrapBase::Multicast_PlayDamageVFX_Implementation(FVector_NetQuantize EffectLocation)
{
	if (GetNetMode() == NM_DedicatedServer) return;

	PlayDamageVFX(EffectLocation);
}

void ATrapBase::Multicast_PlayEnemyDestroyedVFX_Implementation()
{
	if (GetNetMode() == NM_DedicatedServer) return;

	if (!EnemyDestroyedVFX) return;

	const FTransform EffectWorldTransform = EnemyDestroyedVFXTransform * GetActorTransform();
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		EnemyDestroyedVFX,
		EffectWorldTransform.GetLocation(),
		EffectWorldTransform.Rotator(),
		EffectWorldTransform.GetScale3D()
	);
}

void ATrapBase::Multicast_PlayWallShotVFX_Implementation(FVector_NetQuantize StartLocation, FVector_NetQuantize EndLocation)
{
	if (GetNetMode() == NM_DedicatedServer) return;

	PlayWallShotVFX(StartLocation, EndLocation);
}

void ATrapBase::Multicast_PlayAttackAnimation_Implementation()
{
	if (GetNetMode() == NM_DedicatedServer
		|| !SkeletalMesh
		|| !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	// 같은 공격 주기 안에 이전 재생이 끝나지 않았어도 항상 첫 프레임부터 1회 재생
	SkeletalMesh->Stop();
	SkeletalMesh->SetPosition(0.f, false);
	SkeletalMesh->Play(false);
}

void ATrapBase::ApplyPreviewVisual()
{
	if (UMeshComponent* ActiveMesh = GetActiveTrapMeshComponent())
	{
		for (int32 MaterialIndex = 0; MaterialIndex < ActiveMesh->GetNumMaterials(); ++MaterialIndex)
		{
			if (UMaterialInstanceDynamic* PreviewMaterial = ActiveMesh->CreateDynamicMaterialInstance(MaterialIndex))
			{
				PreviewMaterial->SetVectorParameterValue(TEXT("PreviewColor"), FLinearColor(0.f, 1.f, 0.2f));
			}
		}
	}
}

// Mesh 크기에 맞춰 DamageArea 자동 조정
void ATrapBase::SyncDamageAreaToMesh()
{
	if (!DamageArea || DamageArea->ComponentHasTag(ManualDamageAreaTag)) return;

	UMeshComponent* ActiveMesh = GetActiveTrapMeshComponent();
	FVector BoundsCenter;
	FVector BoundsExtent;
	if (!ActiveMesh || !GetTrapMeshLocalBounds(BoundsCenter, BoundsExtent)) return;
	
	// Mesh가 Actor 안에서 상대 위치/회전/스케일을 가질 수 있어서 반영함
	const FTransform MeshRelativeTransform = ActiveMesh->GetRelativeTransform();
	const FVector MeshScale = MeshRelativeTransform.GetScale3D();
	const FVector AbsMeshScale(FMath::Abs(MeshScale.X), FMath::Abs(MeshScale.Y), FMath::Abs(MeshScale.Z));

	// DamageArea를 Mesh 중심/회전에 맞춰 배치
	DamageArea->SetRelativeLocation(MeshRelativeTransform.TransformPosition(BoundsCenter));
	DamageArea->SetRelativeRotation(MeshRelativeTransform.GetRotation().Rotator());
	DamageArea->SetRelativeScale3D(FVector::OneVector);
	DamageArea->SetBoxExtent(FVector(
		FMath::Max(BoundsExtent.X * AbsMeshScale.X, 1.f),
		FMath::Max(BoundsExtent.Y * AbsMeshScale.Y, 1.f),
		FMath::Max(BoundsExtent.Z * AbsMeshScale.Z, 1.f)
	));
}

void ATrapBase::ResetAttackAnimation()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	SkeletalMesh->Stop();
	SkeletalMesh->SetPosition(0.f, false);
}

void ATrapBase::StartDamageTimer()
{
	if (!HasAuthority() || !IsPlaced() || Damage <= 0.f || DamageInterval <= 0.f) return;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(DamageTimerHandle, this, &ATrapBase::ApplyPeriodicDamage, DamageInterval, true, DamageInterval);
}

void ATrapBase::StopDamageTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTimerHandle);
	}
}

void ATrapBase::ApplyPeriodicDamage()
{
	if (!HasAuthority() || !IsPlaced()) return;

	if (SourceTrapData && SourceTrapData->GridSurface == ETrapGridSurface::Wall)
	{
		const bool bDidAttack = ApplyWallBoxTraceDamage();
		if (bDidAttack && SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset())
		{
			Multicast_PlayAttackAnimation();
		}
		return;
	}

	bool bDidAttack = false;
	bool bDidDamage = false;
	for (auto It = OverlappingEnemies.CreateIterator(); It; ++It)
	{
		AActor* OverlappingActor = It->Get();
		if (!IsValid(OverlappingActor))
		{
			It.RemoveCurrent();
			continue;
		}

		if (!IsCombatEnemy(OverlappingActor))
		{
			continue;
		}

		bDidAttack = true;
		const float ActualDamage = UGameplayStatics::ApplyDamage(OverlappingActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
		
		if (ActualDamage > 0.f)
		{
			bDidDamage = true;
		}
	}

	if (bDidDamage)
	{
		const FVector EffectLocation = DamageArea ? DamageArea->GetComponentLocation() : GetActorLocation();
		Multicast_PlayDamageVFX(EffectLocation);
	}

	if (bDidAttack && SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset())
	{
		Multicast_PlayAttackAnimation();
	}
}

bool ATrapBase::ApplyWallBoxTraceDamage()
{
	UWorld* World = GetWorld();
	if (!World) return false;

	// BuildGridSurface convention: local +Z is the trap's outward direction.
	const FVector TraceDirection = GetActorUpVector().GetSafeNormal();
	if (TraceDirection.IsNearlyZero()) return false;

	const FVector TraceCenter = DamageArea ? DamageArea->GetComponentLocation() : GetActorLocation();
	const float TraceHalfDepth = GetBoxHalfExtentAlongDirection(DamageArea, TraceDirection);
	const FVector TraceLateralDirection = GetActorForwardVector().GetSafeNormal();
	const FQuat TraceRotation = GetActorQuat();
	const FVector WallVFXStart = TraceCenter + TraceDirection * TraceHalfDepth;
	FVector WallVFXEnd = WallVFXStart + TraceDirection * WallTraceRange;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(DefenseCollisionChannels::Enemy);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WallTrapTrace), false, this);
	QueryParams.AddIgnoredActor(this);
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}

	const FCollisionShape TraceShape = FCollisionShape::MakeBox(WallTraceBoxExtent);
	const float LaneOffsets[] = { -WallTraceLaneOffset, 0.f, WallTraceLaneOffset };
	TSet<AActor*> DamagedActors;
	bool bDidAttack = false;

	for (const float LaneOffset : LaneOffsets)
	{
		const FVector LaneCenter = TraceCenter + TraceLateralDirection * LaneOffset;
		const FVector TraceStart = LaneCenter + TraceDirection * (TraceHalfDepth + WallTraceStartOffset);
		const FVector TraceEnd = TraceStart + TraceDirection * WallTraceRange;

		TArray<FHitResult> Hits;
		const bool bHit = World->SweepMultiByObjectType(
			Hits,
			TraceStart,
			TraceEnd,
			TraceRotation,
			ObjectQueryParams,
			TraceShape,
			QueryParams
		);

		if (!bHit)
		{
			continue;
		}

		for (const FHitResult& Hit : Hits)
		{
			AEnemyBase* Enemy = Cast<AEnemyBase>(Hit.GetActor());
			if (!IsValid(Enemy)
				|| DamagedActors.Contains(Enemy)
				|| Enemy->EnemyMode != EEnemyMode::Combat)
			{
				continue;
			}

			const float EffectLaneOffset = FMath::Clamp(LaneOffset, -WallEffectLaneOffset, WallEffectLaneOffset);
			const FVector EffectLaneCenter = TraceCenter + TraceLateralDirection * EffectLaneOffset;
			const FVector LaneEffectStart = EffectLaneCenter + TraceDirection * TraceHalfDepth;
			// Multicast_DrawWallTraceDebug(LaneEffectStart, Hit.ImpactPoint, true);

			if (!ApplyWallHitEffect(Enemy, LaneEffectStart, Hit.ImpactPoint))
			{
				continue;
			}

			if (!bDidAttack)
			{
				WallVFXEnd = Hit.ImpactPoint;
			}

			DamagedActors.Add(Enemy);
			bDidAttack = true;
			break;
		}
	}

	if (bDidAttack)
	{
		Multicast_PlayWallShotVFX(WallVFXStart, WallVFXEnd);
	}

	return bDidAttack;
}

bool ATrapBase::ApplyWallHitEffect(AEnemyBase* Enemy, const FVector& EffectStart, const FVector& EffectEnd)
{
	if (!IsValid(Enemy))
	{
		return false;
	}

	const float ActualDamage = UGameplayStatics::ApplyDamage(
		Enemy,
		Damage,
		GetInstigatorController(),
		this,
		UDamageType::StaticClass()
	);

	return ActualDamage > 0.f;
}

void ATrapBase::Multicast_DrawWallTraceDebug_Implementation(FVector TraceStart, FVector TraceEnd, bool bHit)
{
	// if (UWorld* World = GetWorld())
	// {
	// 	DrawDebugLine(
	// 		World,
	// 		TraceStart,
	// 		TraceEnd,
	// 		bHit ? FColor::Red : FColor::Green,
	// 		false,
	// 		WallTraceDebugTime,
	// 		0,
	// 		2.f
	// 	);
	// }
}

void ATrapBase::CacheCurrentOverlaps()
{
	OverlappingEnemies.Empty();
	if (!DamageArea) return;

	TArray<AActor*> CurrentOverlaps;
	DamageArea->GetOverlappingActors(CurrentOverlaps);

	for (AActor* OverlappingActor : CurrentOverlaps)
	{
		if (IsValid(OverlappingActor) && OverlappingActor != this)
		{
			OverlappingEnemies.Add(TWeakObjectPtr<AActor>(OverlappingActor));
		}
	}
}

void ATrapBase::OnDamageAreaBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!HasAuthority() || !IsPlaced() || !IsValid(OtherActor) || OtherActor == this) return;
	if (OtherComp && OtherComp->GetCollisionObjectType() != DefenseCollisionChannels::Enemy) return;

	OverlappingEnemies.Add(TWeakObjectPtr<AActor>(OtherActor));
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(OtherActor))
	{
		HandleEnemyEnteredDamageArea(Enemy);
	}
}

void ATrapBase::OnDamageAreaEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
)
{
	if (!HasAuthority() || !IsValid(OtherActor)) return;

	OverlappingEnemies.Remove(TWeakObjectPtr<AActor>(OtherActor));
}
