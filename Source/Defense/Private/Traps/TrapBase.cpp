#include "Traps/TrapBase.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Characters/Enemy/EnemyBase.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Traps/TrapData.h"
#include "Traps/Grid/GridManager.h"

namespace
{
	constexpr ECollisionChannel EnemyCollisionChannel = ECC_GameTraceChannel1;
	constexpr float TrapPlacedHeightScale = 1.f / 3.f;
	constexpr float WallTraceRange = 1400.f;
	constexpr float WallTraceStartOffset = 10.f;
	constexpr float WallTraceDebugTime = 0.35f;
	const FVector WallTraceBoxExtent(120.f, 140.f, 20.f);
	constexpr float WallTraceLaneOffset = 120.f;
	constexpr float WallDebugLaneOffset = 50.f;

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

ATrapBase::ATrapBase()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true; // Actor 서버 -> 클라로 복제
	// SetReplicateMovement(true); // Actor의 위치/회전/속도 같은 movement 정보 복제

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);

	DamageArea = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageArea"));
	DamageArea->SetupAttachment(SceneRoot);
	DamageArea->SetBoxExtent(FVector(50.f, 50.f, 50.f)); // test
	DamageArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamageArea->SetCollisionObjectType(ECC_WorldDynamic);
	DamageArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageArea->SetCollisionResponseToChannel(EnemyCollisionChannel, ECR_Overlap);
	DamageArea->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	DamageArea->SetGenerateOverlapEvents(true);
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

	SyncDamageAreaToMesh();
}

void ATrapBase::BeginPlay()
{
	Super::BeginPlay();

	ApplyTrapMeshScale();
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
	ApplyTrapMeshScale();
	StopDamageTimer();
	ApplyTrapCollision();

	ApplyPreviewVisual();
}

void ATrapBase::InitializePlacedTrap(UTrapData* TrapData, ADefensePlayerState* InInstalledByPlayerState)
{
	InitializePlacedTrap(TrapData, InInstalledByPlayerState, {});
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
	ApplyTrapMeshScale();
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

void ATrapBase::ApplyTrapMeshScale()
{
	if (!Mesh || !Mesh->GetStaticMesh()) return;

	AGridManager* GridManager = nullptr;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AGridManager> It(World); It; ++It)
		{
			GridManager = *It;
			break;
		}
	}
	if (!GridManager) return;

	FVector BoundsMin;
	FVector BoundsMax;
	Mesh->GetLocalBounds(BoundsMin, BoundsMax);

	const FVector BoundsSize = BoundsMax - BoundsMin;
	if (BoundsSize.X <= 0.f || BoundsSize.Y <= 0.f || BoundsSize.Z <= 0.f) return;

	const float FootprintSizeCm = GridManager->GetTrapFootprintSizeCm();
	FVector TargetScale(
		FootprintSizeCm / BoundsSize.X,
		FootprintSizeCm / BoundsSize.Y,
		FMath::Min(FootprintSizeCm / BoundsSize.X, FootprintSizeCm / BoundsSize.Y)
	);
	if (RuntimeState == ETrapRuntimeState::Placed)
	{
		TargetScale.Z *= TrapPlacedHeightScale;
	}

	Mesh->SetRelativeScale3D(TargetScale);

	// Actor 위치는 Grid가 정한 Trap 중심이다. Mesh pivot 위치와 무관하게
	// Bounds 중심을 Root에 맞춰 시각적 중심과 논리 중심을 일치시킨다.
	const FVector BoundsCenter = (BoundsMin + BoundsMax) * 0.5f;
	FVector MeshLocation = Mesh->GetRelativeLocation();
	MeshLocation.X = -BoundsCenter.X * TargetScale.X;
	MeshLocation.Y = -BoundsCenter.Y * TargetScale.Y;
	Mesh->SetRelativeLocation(MeshLocation);

	SyncDamageAreaToMesh();
}

void ATrapBase::OnRep_RuntimeState()
{
	ApplyTrapMeshScale();
	ApplyTrapCollision();
}

void ATrapBase::ApplyTrapCollision()
{
	SetActorEnableCollision(IsPlaced());

	if (DamageArea)
	{
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

void ATrapBase::ApplyPreviewVisual()
{
	if (Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);

		for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
		{
			if (UMaterialInstanceDynamic* PreviewMaterial = Mesh->CreateDynamicMaterialInstance(MaterialIndex))
			{
				PreviewMaterial->SetVectorParameterValue(TEXT("PreviewColor"), FLinearColor(0.f, 1.f, 0.2f));
			}
		}
	}
}

// test : Mesh 크기에 맞춰 Collision을 자동 조정
void ATrapBase::SyncDamageAreaToMesh()
{
	if (!Mesh || !DamageArea || !Mesh->GetStaticMesh()) return;
	
	// StaticMesh의 로컬 공간 기준 최소/최대 범위
	FVector BoundsMin;
	FVector BoundsMax;
	Mesh->GetLocalBounds(BoundsMin, BoundsMax);

	const FVector BoundsCenter = (BoundsMin + BoundsMax) * 0.5f;
	const FVector BoundsExtent = (BoundsMax - BoundsMin) * 0.5f;
	
	// Mesh가 Actor 안에서 상대 위치/회전/스케일을 가질 수 있어서 반영함
	const FTransform MeshRelativeTransform = Mesh->GetRelativeTransform();
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
		ApplyWallBoxTraceDamage();
		return;
	}

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

		UGameplayStatics::ApplyDamage(OverlappingActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
	}
}

void ATrapBase::ApplyWallBoxTraceDamage()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// BuildGridSurface convention: local +Z is the trap's outward direction.
	const FVector TraceDirection = GetActorUpVector().GetSafeNormal();
	if (TraceDirection.IsNearlyZero()) return;

	const FVector TraceCenter = DamageArea ? DamageArea->GetComponentLocation() : GetActorLocation();
	const float TraceHalfDepth = GetBoxHalfExtentAlongDirection(DamageArea, TraceDirection);
	const FVector TraceLateralDirection = GetActorForwardVector().GetSafeNormal();
	const FQuat TraceRotation = GetActorQuat();

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(EnemyCollisionChannel);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WallTrapTrace), false, this);
	QueryParams.AddIgnoredActor(this);
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}

	const FCollisionShape TraceShape = FCollisionShape::MakeBox(WallTraceBoxExtent);
	const float LaneOffsets[] = { -WallTraceLaneOffset, 0.f, WallTraceLaneOffset };
	TSet<AActor*> DamagedActors;

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
			AActor* HitActor = Hit.GetActor();
			if (!IsValid(HitActor) || HitActor == this || DamagedActors.Contains(HitActor) || !IsCombatEnemy(HitActor))
			{
				continue;
			}

			DamagedActors.Add(HitActor);

			const float DebugLaneOffset = FMath::Clamp(LaneOffset, -WallDebugLaneOffset, WallDebugLaneOffset);
			const FVector DebugLaneCenter = TraceCenter + TraceLateralDirection * DebugLaneOffset;
			const FVector DebugStart = DebugLaneCenter + TraceDirection * TraceHalfDepth;
			Multicast_DrawWallTraceDebug(DebugStart, Hit.ImpactPoint, true);

			UGameplayStatics::ApplyDamage(HitActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
			break;
		}
	}
}

void ATrapBase::Multicast_DrawWallTraceDebug_Implementation(FVector TraceStart, FVector TraceEnd, bool bHit)
{
	if (UWorld* World = GetWorld())
	{
		DrawDebugLine(
			World,
			TraceStart,
			TraceEnd,
			bHit ? FColor::Red : FColor::Green,
			false,
			WallTraceDebugTime,
			0,
			2.f
		);
	}
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
	if (OtherComp && OtherComp->GetCollisionObjectType() != EnemyCollisionChannel) return;

	OverlappingEnemies.Add(TWeakObjectPtr<AActor>(OtherActor));
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
