#include "Traps/TrapBase.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Traps/TrapData.h"

namespace
{
	constexpr ECollisionChannel EnemyCollisionChannel = ECC_GameTraceChannel1;
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
	DamageArea->SetGenerateOverlapEvents(true);
}

void ATrapBase::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATrapBase, Damage);
	DOREPLIFETIME(ATrapBase, DamageInterval);
	
	DOREPLIFETIME(ATrapBase, RuntimeState);
	DOREPLIFETIME(ATrapBase, OwnerPS);
}

void ATrapBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SyncDamageAreaToMesh();
}

void ATrapBase::BeginPlay()
{
	Super::BeginPlay();

	if (DamageArea)
	{
		DamageArea->OnComponentBeginOverlap.AddUniqueDynamic(this, &ATrapBase::OnDamageAreaBeginOverlap);
		DamageArea->OnComponentEndOverlap.AddUniqueDynamic(this, &ATrapBase::OnDamageAreaEndOverlap);
	}

	SyncDamageAreaToMesh();
}

void ATrapBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDamageTimer();

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
	SetActorEnableCollision(false);

	if (DamageArea)
	{
		DamageArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	ApplyPreviewVisual();
}

void ATrapBase::InitializePlacedTrap(UTrapData* TrapData, ADefensePlayerState* InInstalledByPlayerState)
{
	if (bInitialized) return;

	bInitialized = true;
	RuntimeState = ETrapRuntimeState::Placed;
	OwnerPS = InInstalledByPlayerState;

	ConfigureFromTrapData(TrapData);
	SetActorEnableCollision(true);

	if (DamageArea)
	{
		DamageArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
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

	for (auto It = OverlappingEnemies.CreateIterator(); It; ++It)
	{
		AActor* OverlappingActor = It->Get();
		if (!IsValid(OverlappingActor))
		{
			It.RemoveCurrent();
			continue;
		}

		UGameplayStatics::ApplyDamage(OverlappingActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
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
