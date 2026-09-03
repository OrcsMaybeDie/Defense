#include "Effects/StormTornadoVFXActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "WorldCollision.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace StormTornado
{
	constexpr float CloudOpacity = 0.009f;
	constexpr int32 CloudCount = 140;
	constexpr float RainStartTime = 0.45f;
	constexpr float RainEndTime = 3.65f;
	constexpr float RainIntervalMin = 0.65f;
	constexpr float RainIntervalMax = 0.95f;
	constexpr float RainDropLifeSpan = 0.75f;
	constexpr float DamagePerSecond = 30.f;
	constexpr float DamageRadius = 350.f;
	constexpr int32 MaxDamagedEnemies = 5;
}

AStormTornadoVFXActor::AStormTornadoVFXActor()
	: RandomStream(31071991)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	SetReplicateMovement(false);
	SetActorEnableCollision(false);
	VisualDuration = 4.f;
	InitialLifeSpan = VisualDuration;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StormRoot"));
	SetRootComponent(SceneRoot);
	TornadoParticleComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("TornadoParticle"));
	TornadoParticleComponent->SetupAttachment(SceneRoot);
	TornadoParticleComponent->bAutoActivate = false;
	TornadoParticleComponent->bAutoDestroy = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneAsset(
		TEXT("/Engine/BasicShapes/Plane.Plane")
	);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SmokeMaterialAsset(
		TEXT("/Game/_Defense/VFX/Weapons/Pistol/Stage3_StormTornado/Materials/M_Storm_Smoke_OriginalISM.M_Storm_Smoke_OriginalISM")
	);
	static ConstructorHelpers::FObjectFinder<UParticleSystem> TornadoParticleAsset(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_aquaStorm.P_ky_aquaStorm")
	);
	static ConstructorHelpers::FObjectFinder<UParticleSystem> RainParticleAsset(
		TEXT("/Game/FXVarietyPack/Particles/P_ky_laser01.P_ky_laser01")
	);
	PlaneMesh = PlaneAsset.Object;
	SmokeMaterial = SmokeMaterialAsset.Object;
	TornadoParticleComponent->SetTemplate(TornadoParticleAsset.Object);
	RainParticleSystem = RainParticleAsset.Object;
}

void AStormTornadoVFXActor::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (TornadoParticleComponent && TornadoParticleComponent->Template)
	{
		TornadoParticleComponent->ActivateSystem(true);
	}

	CreateCloudLayer();
	NextRainTime = StormTornado::RainStartTime;
}

void AStormTornadoVFXActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FActiveRainDrop& RainDrop : ActiveRainDrops)
	{
		if (UParticleSystemComponent* Component = RainDrop.Component.Get())
		{
			Component->DeactivateSystem();
			Component->DestroyComponent();
		}
	}
	ActiveRainDrops.Reset();
	Super::EndPlay(EndPlayReason);
}

void AStormTornadoVFXActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ElapsedTime += DeltaSeconds;
	if (HasAuthority() && ElapsedTime >= NextDamageTime)
	{
		ApplyAreaDamage();
		NextDamageTime += 1.f;
	}
	if (ElapsedTime >= VisualDuration)
	{
		if (HasAuthority())
		{
			Destroy();
		}
		return;
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const float FadeIn = FMath::Clamp(ElapsedTime / 0.25f, 0.f, 1.f);
	UpdateCloudLayer(FadeIn);
	UpdateRainDrops();

	if (ElapsedTime <= StormTornado::RainEndTime && ElapsedTime >= NextRainTime)
	{
		SpawnRainDrop();
		NextRainTime = ElapsedTime + RandomStream.FRandRange(
			StormTornado::RainIntervalMin,
			StormTornado::RainIntervalMax
		);
	}
}

void AStormTornadoVFXActor::ApplyAreaDamage()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return;
	}

	TArray<AEnemyBase*> Enemies;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* Enemy = *It;
		if (Enemy
			&& Enemy->EnemyMode == EEnemyMode::Combat
			&& Enemy->EnemyState != EEnemyState::Die
			&& Enemy->EnemyState != EEnemyState::StoneDie
			&& FVector::DistSquared(Enemy->GetActorLocation(), GetActorLocation())
				<= FMath::Square(StormTornado::DamageRadius))
		{
			Enemies.Add(Enemy);
		}
	}
	Enemies.Sort([this](const AEnemyBase& Left, const AEnemyBase& Right)
	{
		return FVector::DistSquared(Left.GetActorLocation(), GetActorLocation())
			< FVector::DistSquared(Right.GetActorLocation(), GetActorLocation());
	});

	const int32 DamageCount = FMath::Min(Enemies.Num(), StormTornado::MaxDamagedEnemies);
	for (int32 Index = 0; Index < DamageCount; ++Index)
	{
		UGameplayStatics::ApplyDamage(
			Enemies[Index],
			StormTornado::DamagePerSecond,
			GetInstigatorController(),
			GetOwner() ? GetOwner() : this,
			UDamageType::StaticClass()
		);
	}
}

void AStormTornadoVFXActor::CreateCloudLayer()
{
	if (!PlaneMesh || !SmokeMaterial)
	{
		return;
	}

	CloudInstances = NewObject<UInstancedStaticMeshComponent>(this, TEXT("UpperStormClouds"), RF_Transient);
	if (!CloudInstances)
	{
		return;
	}

	AddInstanceComponent(CloudInstances);
	CloudInstances->SetupAttachment(SceneRoot);
	CloudInstances->SetMobility(EComponentMobility::Movable);
	CloudInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CloudInstances->SetGenerateOverlapEvents(false);
	CloudInstances->SetCastShadow(false);
	CloudInstances->SetReceivesDecals(false);
	CloudInstances->bDisallowNanite = true;
	CloudInstances->SetStaticMesh(PlaneMesh);
	CloudInstances->SetTranslucentSortPriority(4);
	CloudInstances->SetBoundsScale(3.f);

	CloudDynamicMaterial = UMaterialInstanceDynamic::Create(SmokeMaterial, this);
	if (CloudDynamicMaterial)
	{
		CloudDynamicMaterial->SetScalarParameterValue(TEXT("OpacityMultiplier"), 0.f);
		CloudInstances->SetMaterial(0, CloudDynamicMaterial);
	}

	CloudInstances->RegisterComponent();
	CloudSprites.Reserve(StormTornado::CloudCount);
	for (int32 Index = 0; Index < StormTornado::CloudCount; ++Index)
	{
		FCloudSprite Sprite;
		Sprite.InstanceIndex = CloudInstances->AddInstance(FTransform::Identity, false);
		Sprite.StartAngle = RandomStream.FRandRange(0.f, UE_TWO_PI);
		Sprite.Radius = RandomStream.FRandRange(70.f, 300.f);
		Sprite.HeightRange = 320.f;
		Sprite.HeightPhase = RandomStream.FRandRange(0.f, Sprite.HeightRange);
		Sprite.RiseSpeed = RandomStream.FRandRange(20.f, 200.f);
		Sprite.AngularSpeed = RandomStream.FRandRange(0.75f, 1.8f)
			* (Index % 2 == 0 ? 1.f : -1.f);
		Sprite.BaseScale = RandomStream.FRandRange(2.4f, 3.6f);
		Sprite.PulsePhase = RandomStream.FRandRange(0.f, UE_TWO_PI);
		CloudSprites.Add(Sprite);
	}
}

void AStormTornadoVFXActor::UpdateCloudLayer(float Fade)
{
	if (!CloudInstances)
	{
		return;
	}

	if (CloudDynamicMaterial)
	{
		CloudDynamicMaterial->SetScalarParameterValue(
			TEXT("OpacityMultiplier"),
			StormTornado::CloudOpacity * Fade
		);
	}

	const FVector CameraLocation = GetCameraLocation();
	for (const FCloudSprite& Sprite : CloudSprites)
	{
		const float HeightOffset = FMath::Fmod(
			Sprite.HeightPhase + ElapsedTime * Sprite.RiseSpeed,
			Sprite.HeightRange
		);
		const float HeightAlpha = HeightOffset / Sprite.HeightRange;
		const float Radius = Sprite.Radius * FMath::Lerp(0.65f, 1.1f, HeightAlpha);
		const float Angle = Sprite.StartAngle + ElapsedTime * Sprite.AngularSpeed;
		const FVector RelativeLocation(
			FMath::Cos(Angle) * Radius,
			FMath::Sin(Angle) * Radius,
			260.f + HeightOffset
		);

		const FVector WorldLocation = GetActorTransform().TransformPosition(RelativeLocation);
		const FVector ToCamera = (CameraLocation - WorldLocation).GetSafeNormal();
		FQuat LocalRotation = FQuat::Identity;
		if (!ToCamera.IsNearlyZero())
		{
			const FQuat WorldRotation = FQuat::FindBetweenNormals(FVector::UpVector, ToCamera);
			LocalRotation = GetActorQuat().Inverse() * WorldRotation;
		}

		const float ScalePulse = 1.f + 0.18f * FMath::Sin(ElapsedTime * 2.8f + Sprite.PulsePhase);
		CloudInstances->UpdateInstanceTransform(
			Sprite.InstanceIndex,
			FTransform(LocalRotation, RelativeLocation, FVector(Sprite.BaseScale * ScalePulse)),
			false,
			false,
			true
		);
	}
	CloudInstances->MarkRenderStateDirty();
}

void AStormTornadoVFXActor::SpawnRainDrop()
{
	if (!RainParticleSystem || !GetWorld())
	{
		return;
	}

	const float Angle = RandomStream.FRandRange(0.f, UE_TWO_PI);
	const float Radius = FMath::Sqrt(RandomStream.FRand()) * 285.f;
	const FVector Location = GetActorLocation() + FVector(
		FMath::Cos(Angle) * Radius,
		FMath::Sin(Angle) * Radius,
		2.f
	);
	const float Scale = RandomStream.FRandRange(0.32f, 0.44f);
	UParticleSystemComponent* RainDrop = UGameplayStatics::SpawnEmitterAtLocation(
		GetWorld(),
		RainParticleSystem,
		FTransform(FRotator::ZeroRotator, Location, FVector(Scale)),
		true,
		EPSCPoolMethod::None,
		true
	);
	if (RainDrop)
	{
		RainDrop->SetWorldRotation(FRotator::ZeroRotator);
		ActiveRainDrops.Add({RainDrop, ElapsedTime});
	}
}

void AStormTornadoVFXActor::UpdateRainDrops()
{
	for (int32 Index = ActiveRainDrops.Num() - 1; Index >= 0; --Index)
	{
		UParticleSystemComponent* Component = ActiveRainDrops[Index].Component.Get();
		if (!Component || ElapsedTime - ActiveRainDrops[Index].SpawnTime >= StormTornado::RainDropLifeSpan)
		{
			if (Component)
			{
				Component->DeactivateSystem();
				Component->DestroyComponent();
			}
			ActiveRainDrops.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
}

FVector AStormTornadoVFXActor::GetCameraLocation() const
{
	if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		return CameraManager->GetCameraLocation();
	}

	return GetActorLocation() + FVector(1000.f, 0.f, 300.f);
}
