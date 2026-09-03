#include "Traps/FractureDoor.h"

#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Traps/FractureDoorDebris.h"

AFractureDoor::AFractureDoor()
{
	// Fracture doors are authored directly in the map and never enter the trap
	// preview/placement pipeline.
	RuntimeState = ETrapRuntimeState::Placed;

	DoorVisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorVisualRoot"));
	DoorVisualRoot->SetupAttachment(SceneRoot);

	// Only the rendered door moves during a hit reaction. Gameplay collision and
	// the enemy sensor stay attached to SceneRoot.
	Mesh->SetupAttachment(DoorVisualRoot);
	SkeletalMesh->SetupAttachment(DoorVisualRoot);
}

void AFractureDoor::BeginPlay()
{
	Super::BeginPlay();

	if (DoorVisualRoot)
	{
		DoorVisualRestTransform = DoorVisualRoot->GetRelativeTransform();
	}

	if (HasAuthority())
	{
		ScheduleSensorActivation();
	}
}

void AFractureDoor::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateHitShake(DeltaTime);
}
void AFractureDoor::HandleDamageApplied(const float AppliedDamage, AActor* DamageCauser)
{
	Super::HandleDamageApplied(AppliedDamage, DamageCauser);

	// A lethal hit immediately transitions to the fracture visual instead.
	if (HP > 0.0f)
	{
		Multicast_PlayHitShake();
	}
}

void AFractureDoor::HandleHPDepleted(AActor* DamageCauser)
{
	RestoreDoorVisual();
	Multicast_PlayDestructionSound();

	if (UWorld* World = GetWorld(); HasAuthority() && FractureDebrisClass && World)
	{
		const UMeshComponent* PrimaryDoorMesh = GetActiveTrapMeshComponent();
		const FTransform DoorTransform = PrimaryDoorMesh
			? PrimaryDoorMesh->GetComponentTransform()
			: GetActorTransform();
		const FTransform SpawnTransform = DoorVisualRoot
			? DoorVisualRoot->GetComponentTransform()
			: GetActorTransform();

		AFractureDoorDebris* DebrisActor = World->SpawnActorDeferred<AFractureDoorDebris>(
			FractureDebrisClass,
			SpawnTransform,
			GetOwner(),
			GetInstigator(),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		);
		if (DebrisActor)
		{
			DebrisActor->InitializeDoorTransform(DoorTransform);
			UGameplayStatics::FinishSpawningActor(DebrisActor, SpawnTransform);
		}
	}

	Super::HandleHPDepleted(DamageCauser);
}

void AFractureDoor::Multicast_PlayHitShake_Implementation()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, GetActorLocation());
	}

	if (!DoorVisualRoot)
	{
		return;
	}

	HitShakeElapsed = 0.0f;
	bHitShakeActive = true;
}

void AFractureDoor::Multicast_PlayDestructionSound_Implementation()
{
	if (IsRunningDedicatedServer() || !DestructionSound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation());
}

void AFractureDoor::UpdateHitShake(const float DeltaTime)
{
	if (!bHitShakeActive || !DoorVisualRoot)
	{
		return;
	}

	HitShakeElapsed += FMath::Max(DeltaTime, 0.0f);
	const float Duration = FMath::Max(HitShakeDuration, UE_KINDA_SMALL_NUMBER);
	const float NormalizedTime = FMath::Clamp(HitShakeElapsed / Duration, 0.0f, 1.0f);

	if (NormalizedTime >= 1.0f)
	{
		RestoreDoorVisual();
		return;
	}

	const float Envelope = FMath::Pow(1.0f - NormalizedTime, FMath::Max(HitShakeDampingExponent, 0.1f));
	const float Wave = FMath::Sin(HitShakeElapsed * HitShakeFrequency * 2.0f * PI) * Envelope;
	const FVector ShakenLocation = DoorVisualRestTransform.GetLocation() + HitShakeLocationAmplitude * Wave;
	const FRotator ShakenRotation = DoorVisualRestTransform.Rotator() + HitShakeRotationAmplitude * Wave;

	DoorVisualRoot->SetRelativeLocationAndRotation(ShakenLocation, ShakenRotation);
}

void AFractureDoor::RestoreDoorVisual()
{
	bHitShakeActive = false;
	HitShakeElapsed = 0.0f;

	if (DoorVisualRoot)
	{
		DoorVisualRoot->SetRelativeTransform(DoorVisualRestTransform);
	}
}
