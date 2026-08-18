#include "Traps/FractureDoorDebris.h"

#include "Components/SceneComponent.h"
#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Net/UnrealNetwork.h"

AFractureDoorDebris::AFractureDoorDebris()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FracturedDoor = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("FracturedDoor"));
	FracturedDoor->SetupAttachment(SceneRoot);
	ConfigureFractureComponent(FracturedDoor);
}

void AFractureDoorDebris::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFractureDoorDebris, DoorWorldTransform);
	DOREPLIFETIME(AFractureDoorDebris, bHasDoorTransform);
}

void AFractureDoorDebris::InitializeDoorTransform(const FTransform& DoorTransform)
{
	if (!HasAuthority())
	{
		return;
	}

	DoorWorldTransform = DoorTransform;
	bHasDoorTransform = true;
}

void AFractureDoorDebris::BeginPlay()
{
	Super::BeginPlay();
	ApplyDoorTransform();

	if (HasAuthority())
	{
		SetLifeSpan(FMath::Max(DebrisLifetime, 0.1f));
	}

	if (IsRunningDedicatedServer() || !FracturedDoor)
	{
		SetActorTickEnabled(false);
		return;
	}

	ActivateFractureComponent(FracturedDoor);

	FractureElapsed = 0.0f;
	bPendingFracture = true;
}

void AFractureDoorDebris::ApplyDoorTransform()
{
	if (!bHasDoorTransform)
	{
		return;
	}

	if (FracturedDoor)
	{
		FracturedDoor->SetWorldTransform(
			DoorWorldTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
	}
}

void AFractureDoorDebris::ConfigureFractureComponent(UGeometryCollectionComponent* Component)
{
	if (!Component)
	{
		return;
	}

	Component->SetMobility(EComponentMobility::Movable);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCollisionObjectType(ECC_WorldDynamic);
	Component->SetCollisionResponseToAllChannels(ECR_Ignore);
	Component->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Component->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Component->SetEnableDamageFromCollision(false);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetSimulatePhysics(false);
	Component->SetVisibility(false, true);
}

void AFractureDoorDebris::ActivateFractureComponent(UGeometryCollectionComponent* Component)
{
	if (!Component)
	{
		return;
	}

	Component->SetSimulatePhysics(false);
	Component->ResetState();
	Component->SetVisibility(true, true);
	Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Component->SetSimulatePhysics(true);
}

void AFractureDoorDebris::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bPendingFracture)
	{
		SetActorTickEnabled(false);
		return;
	}

	FractureElapsed += FMath::Max(DeltaTime, 0.0f);
	if (FractureElapsed >= FMath::Max(FractureDelay, 0.0f))
	{
		ApplyFracture();
		SetActorTickEnabled(false);
	}
}

void AFractureDoorDebris::ApplyFracture()
{
	if (!bPendingFracture || !FracturedDoor)
	{
		return;
	}

	bPendingFracture = false;
	const FVector FractureOrigin = FracturedDoor->GetComponentTransform().TransformPosition(FractureOriginOffset);

	ActiveStrainField = NewObject<URadialFalloff>(this);
	if (ActiveStrainField && ExternalStrain > 0.0f && FractureRadius > 0.0f)
	{
		ActiveStrainField->SetRadialFalloff(
			ExternalStrain,
			0.0f,
			1.0f,
			0.0f,
			FractureRadius,
			FractureOrigin,
			EFieldFalloffType::Field_FallOff_None
		);

		FracturedDoor->ApplyPhysicsField(
			true,
			EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain,
			nullptr,
			ActiveStrainField
		);
	}

	if (RadialImpulseStrength > 0.0f && FractureRadius > 0.0f)
	{
		FracturedDoor->AddRadialImpulse(
			FractureOrigin,
			FractureRadius,
			RadialImpulseStrength,
			ERadialImpulseFalloff::RIF_Linear,
			true
		);
	}

	OnFractureActivated(FractureOrigin);
}
