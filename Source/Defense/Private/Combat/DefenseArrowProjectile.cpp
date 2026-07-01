#include "Combat/DefenseArrowProjectile.h"

#include "Defense.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ADefenseArrowProjectile::ADefenseArrowProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetReplicateMovement(false);
	InitialLifeSpan = 5.f;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(8.f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	CollisionComponent->OnComponentHit.AddDynamic(this, &ADefenseArrowProjectile::OnProjectileHit);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ADefenseArrowProjectile::OnProjectileBeginOverlap);
	RootComponent = CollisionComponent;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetIsReplicated(false);
	MeshComponent->SetHiddenInGame(false);
	MeshComponent->SetVisibility(true, true);
	if (!MeshComponent->GetStaticMesh())
	{
		if (UStaticMesh* DefaultArrowMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/_Defense/Combat/Weapons/Arrow/SM_Arrow.SM_Arrow")))
		{
			MeshComponent->SetStaticMesh(DefaultArrowMesh);
		}
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->ProjectileGravityScale = 0.15f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
}

void ADefenseArrowProjectile::BeginPlay()
{
	Super::BeginPlay();
	LastDebugLocation = GetActorLocation();
	bDrawDebugTrail = bDrawDebugTrailByDefault;
	if (MeshComponent && !MeshComponent->GetStaticMesh())
	{
		if (UStaticMesh* DefaultArrowMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/_Defense/Combat/Weapons/Arrow/SM_Arrow.SM_Arrow")))
		{
			MeshComponent->SetStaticMesh(DefaultArrowMesh);
		}
	}
}

void ADefenseArrowProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector CurrentLocation = GetActorLocation();
	if (bDrawDebugTrail)
	{
		if (UWorld* World = GetWorld())
		{
			DrawDebugLine(World, LastDebugLocation, CurrentLocation, FColor::Yellow, false, DebugTrailLifeTime, 0, DebugTrailThickness);
		}
	}

	if (HasAuthority() && !bCosmeticOnly && bDamageTraceEnabled && !bHasHit)
	{
		TraceDamageAlongMovement(LastDebugLocation, CurrentLocation);
	}

	LastDebugLocation = CurrentLocation;
}

void ADefenseArrowProjectile::Launch(const FVector& Velocity, float InDamage)
{
	Damage = InDamage;
	bDamageTraceEnabled = !bCosmeticOnly;

	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = Velocity;
		ProjectileMovement->InitialSpeed = Velocity.Size();
		ProjectileMovement->MaxSpeed = FMath::Max(ProjectileMovement->MaxSpeed, Velocity.Size());
	}

	if (!bCosmeticOnly)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				CollisionEnableTimer,
				this,
				&ADefenseArrowProjectile::EnableProjectileCollision,
				0.05f,
				false
			);
		}
	}

	SetActorTickEnabled(!bCosmeticOnly || bDrawDebugTrail);
}

void ADefenseArrowProjectile::IgnoreActor(AActor* ActorToIgnore)
{
	if (!ActorToIgnore || ActorToIgnore == this)
	{
		return;
	}

	IgnoredActors.AddUnique(ActorToIgnore);
	if (CollisionComponent)
	{
		CollisionComponent->IgnoreActorWhenMoving(ActorToIgnore, true);
	}
}

void ADefenseArrowProjectile::SetCosmeticOnly(bool bNewCosmeticOnly)
{
	bCosmeticOnly = bNewCosmeticOnly;

	if (bCosmeticOnly)
	{
		bDamageTraceEnabled = false;
		SetActorEnableCollision(false);
		if (CollisionComponent)
		{
			CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		SetActorTickEnabled(bDrawDebugTrail);
	}
}

void ADefenseArrowProjectile::SetDebugTrailEnabled(bool bEnabled)
{
	bDrawDebugTrail = bEnabled;
	SetActorTickEnabled(!bCosmeticOnly || bDrawDebugTrail);
}

void ADefenseArrowProjectile::OnProjectileHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	DamageAndDestroy(OtherActor);
}

void ADefenseArrowProjectile::OnProjectileBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	DamageAndDestroy(OtherActor);
}

void ADefenseArrowProjectile::DamageAndDestroy(AActor* OtherActor)
{
	if (bCosmeticOnly)
	{
		if (!bHasHit)
		{
			bHasHit = true;

			if (ProjectileMovement)
			{
				ProjectileMovement->StopMovementImmediately();
				ProjectileMovement->Deactivate();
			}

			SetActorEnableCollision(false);
			SetLifeSpan(5.f);
		}

		return;
	}

	if (bHasHit)
	{
		return;
	}

	const bool bIgnoredActor = IgnoredActors.ContainsByPredicate(
		[OtherActor](const TObjectPtr<AActor>& IgnoredActor)
		{
			return IgnoredActor.Get() == OtherActor;
		});

	if (!HasAuthority()
		|| !OtherActor
		|| OtherActor == GetOwner()
		|| OtherActor == GetInstigator()
		|| bIgnoredActor)
	{
		return;
	}

	bHasHit = true;

	UGameplayStatics::ApplyDamage(
		OtherActor,
		Damage,
		GetInstigatorController(),
		this,
		UDamageType::StaticClass()
	);

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	SetActorEnableCollision(false);
	SetLifeSpan(5.f);
	UE_LOG(LogDefense, Verbose, TEXT("Arrow hit and stayed: %s Other=%s Location=%s"),
		*GetNameSafe(this),
		*GetNameSafe(OtherActor),
		*GetActorLocation().ToString());
}

void ADefenseArrowProjectile::EnableProjectileCollision()
{
	SetActorEnableCollision(true);
	if (CollisionComponent)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void ADefenseArrowProjectile::TraceDamageAlongMovement(const FVector& Start, const FVector& End)
{
	if (!GetWorld() || Start.Equals(End, KINDA_SMALL_NUMBER))
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DefenseArrowProjectileTrace), false, this);
	Params.AddIgnoredActor(this);
	if (AActor* OwnerActor = GetOwner())
	{
		Params.AddIgnoredActor(OwnerActor);
	}
	if (APawn* InstigatorPawn = GetInstigator())
	{
		Params.AddIgnoredActor(InstigatorPawn);
	}
	for (const TObjectPtr<AActor>& IgnoredActor : IgnoredActors)
	{
		if (IgnoredActor)
		{
			Params.AddIgnoredActor(IgnoredActor.Get());
		}
	}

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(8.f),
		Params
	);

	if (bHit && Hit.GetActor())
	{
		SetActorLocation(Hit.ImpactPoint);
		DamageAndDestroy(Hit.GetActor());
	}
}
