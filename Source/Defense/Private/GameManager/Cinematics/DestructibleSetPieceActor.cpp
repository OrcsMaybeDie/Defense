// Fill out your copyright notice in the Description page of Project Settings.

#include "GameManager/Cinematics/DestructibleSetPieceActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADestructibleSetPieceActor::ADestructibleSetPieceActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(30.0f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	IntactMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IntactMesh"));
	IntactMesh->SetupAttachment(SceneRoot);

	GeometryCollection = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
	GeometryCollection->SetupAttachment(SceneRoot);
	GeometryCollection->SetMobility(EComponentMobility::Movable);
	GeometryCollection->SetGenerateOverlapEvents(false);
	GeometryCollection->SetCanEverAffectNavigation(false);
	GeometryCollection->SetCollisionObjectType(ECC_WorldDynamic);
	GeometryCollection->SetCollisionResponseToAllChannels(ECR_Ignore);
	GeometryCollection->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	GeometryCollection->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	GeometryCollection->SetEnableDamageFromCollision(false);
	GeometryCollection->SetHiddenInGame(true);
	GeometryCollection->SetVisibility(false, true);
	GeometryCollection->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GeometryCollection->SetSimulatePhysics(false);
	GeometryCollection->SetIsReplicated(false);
	GeometryCollection->SetEnableReplication(false);
}

void ADestructibleSetPieceActor::BeginPlay()
{
	Super::BeginPlay();

	if (bDestroyed)
	{
		ApplyDestroyedState();
	}
	else
	{
		ApplyIntactState();
	}
}

void ADestructibleSetPieceActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADestructibleSetPieceActor, bDestroyed);
	DOREPLIFETIME(ADestructibleSetPieceActor, bDestructionDebrisCleared);
}

bool ADestructibleSetPieceActor::TriggerDestruction()
{
	if (!HasAuthority() || bDestroyed)
	{
		return false;
	}

	bDestroyed = true;
	ApplyDestroyedState();
	ForceNetUpdate();
	return true;
}

bool ADestructibleSetPieceActor::ClearDestructionDebris()
{
	if (!HasAuthority() || !bDestroyed || bDestructionDebrisCleared)
	{
		return false;
	}

	bDestructionDebrisCleared = true;
	ApplyDestructionDebrisClearedState();
	ForceNetUpdate();
	return true;
}

void ADestructibleSetPieceActor::OnRep_Destroyed()
{
	if (bDestroyed)
	{
		ApplyDestroyedState();
	}
	else
	{
		ApplyIntactState();
	}
}

void ADestructibleSetPieceActor::OnRep_DestructionDebrisCleared()
{
	if (bDestructionDebrisCleared)
	{
		ApplyDestructionDebrisClearedState();
	}
}

void ADestructibleSetPieceActor::ApplyIntactState()
{
	GetWorldTimerManager().ClearTimer(LocalFractureTimerHandle);
	ActiveStrainField = nullptr;
	bDestroyedStateApplied = false;

	IntactMesh->SetHiddenInGame(false, true);
	IntactMesh->SetVisibility(true, true);
	IntactMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	GeometryCollection->SetSimulatePhysics(false);
	GeometryCollection->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GeometryCollection->SetHiddenInGame(true, true);
	GeometryCollection->SetVisibility(false, true);
	GeometryCollection->ResetState();
}

void ADestructibleSetPieceActor::ApplyDestroyedState()
{
	if (bDestroyedStateApplied)
	{
		if (bDestructionDebrisCleared)
		{
			ApplyDestructionDebrisClearedState();
		}
		return;
	}
	bDestroyedStateApplied = true;

	IntactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IntactMesh->SetHiddenInGame(true, true);
	IntactMesh->SetVisibility(false, true);

	if (bDestructionDebrisCleared)
	{
		ApplyDestructionDebrisClearedState();
	}
	else if (!IsRunningDedicatedServer())
	{
		ActivateLocalGeometryCollection();
	}

	OnDestructionStarted();
}

void ADestructibleSetPieceActor::ApplyDestructionDebrisClearedState()
{
	GetWorldTimerManager().ClearTimer(LocalFractureTimerHandle);
	ActiveStrainField = nullptr;

	GeometryCollection->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GeometryCollection->SetSimulatePhysics(false);
	GeometryCollection->ResetState();
	GeometryCollection->SetHiddenInGame(true, true);
	GeometryCollection->SetVisibility(false, true);
}

void ADestructibleSetPieceActor::ActivateLocalGeometryCollection()
{
	if (!GeometryCollection)
	{
		return;
	}

	GeometryCollection->SetSimulatePhysics(false);
	GeometryCollection->ResetState();
	GeometryCollection->SetHiddenInGame(false, true);
	GeometryCollection->SetVisibility(true, true);
	GeometryCollection->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GeometryCollection->SetSimulatePhysics(true);

	LocalFractureTimerHandle = GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ADestructibleSetPieceActor::ApplyLocalFracture)
	);
}

void ADestructibleSetPieceActor::ApplyLocalFracture()
{
	if (!bDestroyed || bDestructionDebrisCleared || !GeometryCollection || IsRunningDedicatedServer())
	{
		return;
	}

	const FVector FractureOrigin = GeometryCollection->GetComponentTransform().TransformPosition(DestructionOriginOffset);
	ActiveStrainField = NewObject<URadialFalloff>(this);
	if (ActiveStrainField && ExternalStrain > 0.0f && RadialImpulseRadius > 0.0f)
	{
		ActiveStrainField->SetRadialFalloff(
			ExternalStrain,
			0.0f,
			1.0f,
			0.0f,
			RadialImpulseRadius,
			FractureOrigin,
			EFieldFalloffType::Field_FallOff_None
		);

		GeometryCollection->ApplyPhysicsField(
			true,
			EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain,
			nullptr,
			ActiveStrainField
		);
	}

	if (RadialImpulseRadius > 0.0f && !FMath::IsNearlyZero(RadialImpulseStrength))
	{
		GeometryCollection->AddRadialImpulse(
			FractureOrigin,
			RadialImpulseRadius,
			RadialImpulseStrength,
			ERadialImpulseFalloff::RIF_Linear,
			true
		);
	}
}
