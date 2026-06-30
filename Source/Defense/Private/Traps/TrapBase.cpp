#include "Traps/TrapBase.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
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

	bReplicates = true;
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);

	DamageArea = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageArea"));
	DamageArea->SetupAttachment(SceneRoot);
	DamageArea->SetBoxExtent(FVector(50.f, 50.f, 50.f));
	DamageArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageArea->SetCollisionObjectType(ECC_WorldDynamic);
	DamageArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageArea->SetCollisionResponseToChannel(EnemyCollisionChannel, ECR_Overlap);
}

void ATrapBase::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATrapBase, Damage);
	DOREPLIFETIME(ATrapBase, DamageInterval);
}

void ATrapBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SyncDamageAreaToMesh();
}

void ATrapBase::BeginPlay()
{
	Super::BeginPlay();

	SyncDamageAreaToMesh();
	StartDamageTimer();
}

void ATrapBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDamageTimer();

	Super::EndPlay(EndPlayReason);
}

void ATrapBase::InitializeTrap(const UTrapData* TrapData)
{
	if (!TrapData) return;

	Damage = TrapData->Damage;
	DamageInterval = TrapData->DamageInterval;
	StartDamageTimer();
}

void ATrapBase::SetPreviewMode(bool bPreview)
{
	bPreviewMode = bPreview;
	SetActorEnableCollision(!bPreview);
	if (bPreview)
	{
		StopDamageTimer();
	}
	else
	{
		StartDamageTimer();
	}

	if (Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		if (bPreview)
		{
			for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
			{
				if (UMaterialInstanceDynamic* PreviewMaterial = Mesh->CreateDynamicMaterialInstance(MaterialIndex))
				{
					PreviewMaterial->SetVectorParameterValue(TEXT("PreviewColor"), FLinearColor(0.f, 1.f, 0.2f));
				}
			}
		}
	}

	if (DamageArea)
	{
		DamageArea->SetCollisionEnabled(bPreview ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	}
}

void ATrapBase::SyncDamageAreaToMesh()
{
	if (!Mesh || !DamageArea || !Mesh->GetStaticMesh())
	{
		return;
	}

	FVector BoundsMin;
	FVector BoundsMax;
	Mesh->GetLocalBounds(BoundsMin, BoundsMax);

	const FVector BoundsCenter = (BoundsMin + BoundsMax) * 0.5f;
	const FVector BoundsExtent = (BoundsMax - BoundsMin) * 0.5f;
	const FTransform MeshRelativeTransform = Mesh->GetRelativeTransform();
	const FVector MeshScale = MeshRelativeTransform.GetScale3D();
	const FVector AbsMeshScale(FMath::Abs(MeshScale.X), FMath::Abs(MeshScale.Y), FMath::Abs(MeshScale.Z));

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
	if (!HasAuthority() || bPreviewMode || Damage <= 0.f || DamageInterval <= 0.f)
	{
		return;
	}

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
	if (!HasAuthority() || bPreviewMode || Damage <= 0.f)
	{
		return;
	}

	if (!DamageArea)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(EnemyCollisionChannel);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TrapDamage), false, this);
	QueryParams.AddIgnoredActor(this);

	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		DamageArea->GetComponentLocation(),
		DamageArea->GetComponentQuat(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(DamageArea->GetScaledBoxExtent()),
		QueryParams
	);

	if (!bHasOverlap)
	{
		return;
	}

	TSet<TWeakObjectPtr<AActor>> DamagedActors;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* OverlappingActor = OverlapResult.GetActor();
		if (!IsValid(OverlappingActor) || OverlappingActor == this || DamagedActors.Contains(OverlappingActor))
		{
			continue;
		}

		DamagedActors.Add(TWeakObjectPtr<AActor>(OverlappingActor));
		UGameplayStatics::ApplyDamage(OverlappingActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
	}
}
