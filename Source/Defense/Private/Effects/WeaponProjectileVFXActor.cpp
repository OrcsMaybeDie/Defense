#include "Effects/WeaponProjectileVFXActor.h"

#include "Components/SceneComponent.h"

AWeaponProjectileVFXActor::AWeaponProjectileVFXActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ProjectileRoot"));
	SetRootComponent(SceneRoot);

}

void AWeaponProjectileVFXActor::Configure(
	const FVector& InTargetLocation,
	TSubclassOf<AActor> InProjectileVFXActorClass,
	TSubclassOf<AActor> InImpactVFXActorClass,
	float InSpeed,
	float InScale
)
{
	TargetLocation = InTargetLocation;
	ProjectileVFXActorClass = InProjectileVFXActorClass;
	ImpactVFXActorClass = InImpactVFXActorClass;
	TravelSpeed = FMath::Max(1.f, InSpeed);
	EffectScale = FMath::Max(0.01f, InScale);
	bConfigured = true;
}

void AWeaponProjectileVFXActor::BeginPlay()
{
	Super::BeginPlay();

	if (!bConfigured || !ProjectileVFXActorClass)
	{
		Destroy();
		return;
	}

	const FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal();
	if (!Direction.IsNearlyZero())
	{
		SetActorRotation(Direction.Rotation());
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.Instigator = GetInstigator();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ProjectileEffectActor = GetWorld()->SpawnActor<AActor>(
		ProjectileVFXActorClass,
		GetActorLocation(),
		GetActorRotation(),
		SpawnParameters
	);
	if (!ProjectileEffectActor)
	{
		Destroy();
		return;
	}
	ProjectileEffectActor->SetActorScale3D(FVector(EffectScale));
	ProjectileEffectActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

	const float TravelTime = FVector::Distance(GetActorLocation(), TargetLocation) / TravelSpeed;
	SetLifeSpan(FMath::Max(1.f, TravelTime + 1.f));
}

void AWeaponProjectileVFXActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ProjectileEffectActor))
	{
		ProjectileEffectActor->Destroy();
	}
	Super::EndPlay(EndPlayReason);
}

void AWeaponProjectileVFXActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector CurrentLocation = GetActorLocation();
	const float Step = TravelSpeed * DeltaSeconds;
	if (FVector::DistSquared(CurrentLocation, TargetLocation) <= FMath::Square(FMath::Max(5.f, Step)))
	{
		SetActorLocation(TargetLocation);
		FinishProjectile();
		return;
	}

	SetActorLocation(FMath::VInterpConstantTo(CurrentLocation, TargetLocation, DeltaSeconds, TravelSpeed));
}

void AWeaponProjectileVFXActor::FinishProjectile()
{
	if (ImpactVFXActorClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = GetOwner();
		SpawnParameters.Instigator = GetInstigator();
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* ImpactActor = GetWorld()->SpawnActor<AActor>(
			ImpactVFXActorClass,
			TargetLocation,
			GetActorRotation(),
			SpawnParameters
		);
		if (ImpactActor)
		{
			ImpactActor->SetActorScale3D(FVector(EffectScale));
			if (ImpactActor->GetLifeSpan() <= 0.f)
			{
				ImpactActor->SetLifeSpan(4.f);
			}
		}
	}

	Destroy();
}
