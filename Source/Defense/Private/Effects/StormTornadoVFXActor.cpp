#include "Effects/StormTornadoVFXActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace StormTornado
{
	// Unity 원본은 투명 메시 3겹과 많은 소프트 파티클을 겹친다.
	// TPS의 자동 노출/블룸에서 흰 덩어리가 되지 않도록 레이어별 최대 알파를 제한한다.
	constexpr float CoreOpacity = 0.42f;
	constexpr float OuterWindOpacity = 0.07f;
	constexpr float OuterLightningOpacity = 0.09f;
	constexpr float LightningOpacity = 0.18f;
	constexpr float SmokeOpacity = 0.028f;
	constexpr float ParticleOpacity = 0.12f;
	constexpr int32 LowerCloudCount = 250;
	constexpr int32 UpperCloudCount = 250;
	constexpr int32 GroundSparkCount = 50;
}

AStormTornadoVFXActor::AStormTornadoVFXActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	SetReplicateMovement(false);
	NetCullDistanceSquared = FMath::Square(12000.f);
	InitialLifeSpan = 8.25f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StormRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TornadoAsset(
		TEXT("/Game/_Defense/VFX/Weapons/Pistol/Stage3_StormTornado/Source/Tornado.Tornado")
	);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LongSlideAsset(
		TEXT("/Game/_Defense/VFX/Weapons/Pistol/Stage3_StormTornado/Source/LongSlide.LongSlide")
	);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneAsset(
		TEXT("/Engine/BasicShapes/Plane.Plane")
	);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CoreMaterialAsset(
		TEXT("/Game/_Defense/VFX/Weapons/Pistol/Stage3_StormTornado/Materials/M_Storm_Core_Original.M_Storm_Core_Original")
	);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OuterWindMaterialAsset(
		TEXT("/Game/_Defense/VFX/Weapons/Pistol/Stage3_StormTornado/Materials/M_Storm_OuterWind_Original.M_Storm_OuterWind_Original")
	);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OuterLightningMaterialAsset(
		TEXT("/Game/_Defense/VFX/Weapons/Pistol/Stage3_StormTornado/Materials/M_Storm_OuterLightning_Original.M_Storm_OuterLightning_Original")
	);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LightningMaterialAsset(
		TEXT("/Game/_Defense/VFX/Weapons/Pistol/Stage3_StormTornado/Materials/M_Storm_Lightning_Original.M_Storm_Lightning_Original")
	);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SmokeMaterialAsset(
		TEXT("/Game/_Defense/VFX/Weapons/Pistol/Stage3_StormTornado/Materials/M_Storm_Smoke_OriginalISM.M_Storm_Smoke_OriginalISM")
	);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ParticleMaterialAsset(
		TEXT("/Game/_Defense/VFX/Weapons/Pistol/Stage3_StormTornado/Materials/M_Storm_Particle_Original.M_Storm_Particle_Original")
	);

	TornadoMesh = TornadoAsset.Object;
	LongSlideMesh = LongSlideAsset.Object;
	PlaneMesh = PlaneAsset.Object;
	CoreMaterial = CoreMaterialAsset.Object;
	OuterWindMaterial = OuterWindMaterialAsset.Object;
	OuterLightningMaterial = OuterLightningMaterialAsset.Object;
	LightningMaterial = LightningMaterialAsset.Object;
	SmokeMaterial = SmokeMaterialAsset.Object;
	ParticleMaterial = ParticleMaterialAsset.Object;
}

void AStormTornadoVFXActor::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}

	CreateTornadoMeshes();
	CreateCloudLayers();
	CreateLightningLayers();
	CreateGroundSparks();
}

void AStormTornadoVFXActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedTime += DeltaSeconds;
	const float FadeIn = FMath::Clamp(ElapsedTime / 0.25f, 0.f, 1.f);
	const float FadeOut = FMath::Clamp((VisualDuration - ElapsedTime) / 0.9f, 0.f, 1.f);
	const float Fade = FMath::Min(FadeIn, FadeOut);

	UpdateMaterialFade(Fade);
	UpdateTornadoMeshes(DeltaSeconds);
	UpdateOrbitingSprites(GetCameraLocation());
	UpdateLightning(Fade);
	if (UInstancedStaticMeshComponent* Sparks = GroundSparkInstances.Get())
	{
		Sparks->SetVisibility(ElapsedTime <= 0.35f, false);
	}
}

UStaticMeshComponent* AStormTornadoVFXActor::CreateLayerComponent(
	FName ComponentName,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	float BaseOpacity,
	int32 SortPriority,
	UMaterialInstanceDynamic*& OutDynamicMaterial
)
{
	OutDynamicMaterial = nullptr;
	if (!SceneRoot || !Mesh || !Material)
	{
		return nullptr;
	}

	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, ComponentName, RF_Transient);
	if (!Component)
	{
		return nullptr;
	}

	AddInstanceComponent(Component);
	Component->SetupAttachment(SceneRoot);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCastShadow(false);
	Component->SetReceivesDecals(false);
	// 원본 Tornado/LongSlide FBX에는 Nanite가 켜져 있지만, Nanite는 이 효과의
	// Translucent/Additive 머티리얼을 지원하지 않으므로 fallback mesh를 사용한다.
	Component->bDisallowNanite = true;
	Component->SetStaticMesh(Mesh);
	Component->SetTranslucentSortPriority(SortPriority);
	Component->SetBoundsScale(2.f);

	OutDynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
	if (OutDynamicMaterial)
	{
		OutDynamicMaterial->SetScalarParameterValue(TEXT("OpacityMultiplier"), 0.f);
		Component->SetMaterial(0, OutDynamicMaterial);
		DynamicMaterials.Add(OutDynamicMaterial);
		MaterialStates.Add({OutDynamicMaterial, BaseOpacity});
	}

	Component->RegisterComponent();
	SpawnedComponents.Add(Component);
	return Component;
}

UInstancedStaticMeshComponent* AStormTornadoVFXActor::CreateInstancedLayerComponent(
	FName ComponentName,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	float BaseOpacity,
	int32 SortPriority,
	UMaterialInstanceDynamic*& OutDynamicMaterial
)
{
	OutDynamicMaterial = nullptr;
	if (!SceneRoot || !Mesh || !Material)
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(
		this, ComponentName, RF_Transient
	);
	if (!Component)
	{
		return nullptr;
	}

	AddInstanceComponent(Component);
	Component->SetupAttachment(SceneRoot);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCastShadow(false);
	Component->SetReceivesDecals(false);
	Component->bDisallowNanite = true;
	Component->SetStaticMesh(Mesh);
	Component->SetTranslucentSortPriority(SortPriority);
	Component->SetBoundsScale(3.f);

	OutDynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
	if (OutDynamicMaterial)
	{
		OutDynamicMaterial->SetScalarParameterValue(TEXT("OpacityMultiplier"), 0.f);
		Component->SetMaterial(0, OutDynamicMaterial);
		DynamicMaterials.Add(OutDynamicMaterial);
		MaterialStates.Add({OutDynamicMaterial, BaseOpacity});
	}

	Component->RegisterComponent();
	SpawnedInstanceComponents.Add(Component);
	return Component;
}

void AStormTornadoVFXActor::CreateTornadoMeshes()
{
	UMaterialInstanceDynamic* CoreMID = nullptr;
	UStaticMeshComponent* Core = CreateLayerComponent(
		TEXT("TornadoCore"), TornadoMesh, CoreMaterial, StormTornado::CoreOpacity, 2, CoreMID
	);
	if (Core)
	{
		Core->SetRelativeLocation(FVector(0.f, 0.f, 8.f));
		Core->SetRelativeScale3D(FVector(0.60f));
		CoreComponent = Core;
	}

	UMaterialInstanceDynamic* WindMID = nullptr;
	UStaticMeshComponent* Wind = CreateLayerComponent(
		TEXT("TornadoOuterWind"), TornadoMesh, OuterWindMaterial, StormTornado::OuterWindOpacity, 1, WindMID
	);
	if (Wind)
	{
		Wind->SetRelativeLocation(FVector(0.f, 0.f, 8.f));
		Wind->SetRelativeScale3D(FVector(0.60f, 0.60f, 0.40f));
		Wind->SetRelativeRotation(FRotator(0.f, 37.f, 0.f));
		OuterWindComponent = Wind;
	}

	UMaterialInstanceDynamic* OuterLightningMID = nullptr;
	UStaticMeshComponent* OuterLightning = CreateLayerComponent(
		TEXT("TornadoOuterLightning"),
		TornadoMesh,
		OuterLightningMaterial,
		StormTornado::OuterLightningOpacity,
		3,
		OuterLightningMID
	);
	if (OuterLightning)
	{
		OuterLightning->SetRelativeLocation(FVector(0.f, 0.f, 8.f));
		OuterLightning->SetRelativeScale3D(FVector(0.80f, 0.80f, 0.40f));
		OuterLightning->SetRelativeRotation(FRotator(0.f, 81.f, 0.f));
		OuterLightningComponent = OuterLightning;
	}
}

void AStormTornadoVFXActor::CreateCloudLayers()
{
	FRandomStream Random(31071991);
	UMaterialInstanceDynamic* LowerCloudMID = nullptr;
	UInstancedStaticMeshComponent* LowerClouds = CreateInstancedLayerComponent(
		TEXT("LowerClouds"), PlaneMesh, SmokeMaterial, StormTornado::SmokeOpacity, 4, LowerCloudMID
	);
	UMaterialInstanceDynamic* UpperCloudMID = nullptr;
	UInstancedStaticMeshComponent* UpperClouds = CreateInstancedLayerComponent(
		TEXT("UpperClouds"), PlaneMesh, SmokeMaterial, StormTornado::SmokeOpacity, 5, UpperCloudMID
	);

	for (int32 Index = 0; Index < StormTornado::LowerCloudCount + StormTornado::UpperCloudCount; ++Index)
	{
		const bool bUpperCloud = Index >= StormTornado::LowerCloudCount;
		UInstancedStaticMeshComponent* CloudLayer = bUpperCloud ? UpperClouds : LowerClouds;
		if (!CloudLayer)
		{
			continue;
		}

		FOrbitingSprite Layer;
		Layer.Component = CloudLayer;
		Layer.InstanceIndex = CloudLayer->AddInstance(FTransform::Identity, false);
		Layer.StartAngle = Random.FRandRange(0.f, UE_TWO_PI);
		// Unity 원본: Upper/Lower 모두 250/s, Lifetime 1s, Size 7.5/2.5.
		// 게임 월드에서는 원본 1m를 40cm로 환산해 TPS 화면을 덮지 않도록 한다.
		Layer.MinHeight = bUpperCloud ? 260.f : 10.f;
		Layer.HeightRange = bUpperCloud ? 320.f : 360.f;
		Layer.HeightPhase = Random.FRandRange(0.f, Layer.HeightRange);
		Layer.RiseSpeed = Random.FRandRange(bUpperCloud ? 20.f : 80.f, bUpperCloud ? 200.f : 160.f);
		Layer.AngularSpeed = Random.FRandRange(0.75f, 1.8f) * (Index % 2 == 0 ? 1.f : -1.f);
		Layer.Radius = Random.FRandRange(
			bUpperCloud ? 70.f : 4.f,
			bUpperCloud ? 300.f : 24.f
		);
		Layer.BaseScale = Random.FRandRange(
			bUpperCloud ? 2.4f : 0.78f,
			bUpperCloud ? 3.6f : 1.18f
		);
		Layer.PulsePhase = Random.FRandRange(0.f, UE_TWO_PI);
		OrbitingSprites.Add(Layer);
	}
}

void AStormTornadoVFXActor::CreateLightningLayers()
{
	FRandomStream Random(7701);
	constexpr int32 LightningCount = 8;
	for (int32 Index = 0; Index < LightningCount; ++Index)
	{
		UMaterialInstanceDynamic* LightningMID = nullptr;
		UStaticMeshComponent* Lightning = CreateLayerComponent(
			FName(*FString::Printf(TEXT("LightningStorm_%02d"), Index)),
			LongSlideMesh,
			LightningMaterial,
			StormTornado::LightningOpacity,
			7,
			LightningMID
		);
		if (!Lightning || !LightningMID)
		{
			continue;
		}

		const float Angle = Random.FRandRange(0.f, 360.f);
		const float Radius = Random.FRandRange(35.f, 185.f);
		Lightning->SetRelativeLocation(FVector(
			FMath::Cos(FMath::DegreesToRadians(Angle)) * Radius,
			FMath::Sin(FMath::DegreesToRadians(Angle)) * Radius,
			Random.FRandRange(110.f, 600.f)
		));
		Lightning->SetRelativeRotation(FRotator(
			Random.FRandRange(-18.f, 18.f),
			Angle,
			Random.FRandRange(-35.f, 35.f)
		));
		Lightning->SetRelativeScale3D(FVector(
			Random.FRandRange(0.22f, 0.42f),
			Random.FRandRange(0.22f, 0.42f),
			Random.FRandRange(0.55f, 1.20f)
		));

		FPulsingLightning Pulse;
		Pulse.Component = Lightning;
		Pulse.Material = LightningMID;
		Pulse.Phase = Random.FRandRange(0.f, 1.f);
		Pulse.Period = Random.FRandRange(0.55f, 1.15f);
		Pulse.VisibleDuration = Random.FRandRange(0.08f, 0.18f);
		Pulse.BaseOpacity = StormTornado::LightningOpacity;
		PulsingLightning.Add(Pulse);
	}
}

void AStormTornadoVFXActor::CreateGroundSparks()
{
	FRandomStream Random(9917);
	UMaterialInstanceDynamic* SparkMID = nullptr;
	UInstancedStaticMeshComponent* Sparks = CreateInstancedLayerComponent(
		TEXT("GroundSparks"), PlaneMesh, ParticleMaterial, StormTornado::ParticleOpacity, 6, SparkMID
	);
	GroundSparkInstances = Sparks;
	if (!Sparks)
	{
		return;
	}

	for (int32 Index = 0; Index < StormTornado::GroundSparkCount; ++Index)
	{
		FOrbitingSprite Layer;
		Layer.Component = Sparks;
		Layer.InstanceIndex = Sparks->AddInstance(FTransform::Identity, false);
		Layer.StartAngle = Random.FRandRange(0.f, UE_TWO_PI);
		Layer.Radius = Random.FRandRange(80.f, 420.f);
		Layer.MinHeight = Random.FRandRange(8.f, 25.f);
		Layer.HeightRange = Random.FRandRange(35.f, 90.f);
		Layer.HeightPhase = Random.FRandRange(0.f, Layer.HeightRange);
		Layer.RiseSpeed = Random.FRandRange(70.f, 150.f);
		Layer.AngularSpeed = Random.FRandRange(0.15f, 0.55f) * (Index % 2 == 0 ? 1.f : -1.f);
		Layer.BaseScale = Random.FRandRange(0.08f, 0.18f);
		Layer.PulsePhase = Random.FRandRange(0.f, UE_TWO_PI);
		OrbitingSprites.Add(Layer);
	}
}

void AStormTornadoVFXActor::UpdateMaterialFade(float Fade)
{
	for (const FMaterialState& State : MaterialStates)
	{
		if (UMaterialInstanceDynamic* Material = State.Material.Get())
		{
			Material->SetScalarParameterValue(
				TEXT("OpacityMultiplier"),
				State.BaseOpacity * Fade
			);
		}
	}
}

void AStormTornadoVFXActor::UpdateTornadoMeshes(float DeltaSeconds)
{
	if (UStaticMeshComponent* Core = CoreComponent.Get())
	{
		Core->AddRelativeRotation(FRotator(0.f, 72.f * DeltaSeconds, 0.f));
		const float Pulse = 0.60f + FMath::Sin(ElapsedTime * 3.4f) * 0.022f;
		Core->SetRelativeScale3D(FVector(Pulse));
	}
	if (UStaticMeshComponent* Wind = OuterWindComponent.Get())
	{
		Wind->AddRelativeRotation(FRotator(0.f, -43.f * DeltaSeconds, 0.f));
	}
	if (UStaticMeshComponent* OuterLightning = OuterLightningComponent.Get())
	{
		OuterLightning->AddRelativeRotation(FRotator(0.f, 31.f * DeltaSeconds, 0.f));
	}
}

void AStormTornadoVFXActor::UpdateOrbitingSprites(const FVector& CameraLocation)
{
	TSet<UInstancedStaticMeshComponent*> DirtyComponents;
	for (const FOrbitingSprite& Layer : OrbitingSprites)
	{
		UInstancedStaticMeshComponent* Component = Layer.Component.Get();
		if (!Component || Layer.InstanceIndex == INDEX_NONE)
		{
			continue;
		}

		const float HeightOffset = FMath::Fmod(
			Layer.HeightPhase + ElapsedTime * Layer.RiseSpeed,
			FMath::Max(1.f, Layer.HeightRange)
		);
		const float Height = Layer.MinHeight + HeightOffset;
		const float HeightAlpha = HeightOffset / FMath::Max(1.f, Layer.HeightRange);
		const float Radius = Layer.Radius * FMath::Lerp(0.65f, 1.1f, HeightAlpha);
		const float Angle = Layer.StartAngle + ElapsedTime * Layer.AngularSpeed;
		const FVector RelativeLocation(
			FMath::Cos(Angle) * Radius,
			FMath::Sin(Angle) * Radius,
			Height
		);
		const FVector WorldLocation = GetActorTransform().TransformPosition(RelativeLocation);
		const FVector ToCamera = (CameraLocation - WorldLocation).GetSafeNormal();
		FQuat LocalRotation = FQuat::Identity;
		if (!ToCamera.IsNearlyZero())
		{
			const FQuat WorldRotation = FQuat::FindBetweenNormals(FVector::UpVector, ToCamera);
			LocalRotation = GetActorQuat().Inverse() * WorldRotation;
		}

		const float ScalePulse = 1.f + 0.18f * FMath::Sin(ElapsedTime * 2.8f + Layer.PulsePhase);
		const FTransform InstanceTransform(
			LocalRotation,
			RelativeLocation,
			FVector(Layer.BaseScale * ScalePulse)
		);
		Component->UpdateInstanceTransform(
			Layer.InstanceIndex, InstanceTransform, false, false, true
		);
		DirtyComponents.Add(Component);
	}

	for (UInstancedStaticMeshComponent* Component : DirtyComponents)
	{
		Component->MarkRenderStateDirty();
	}
}

void AStormTornadoVFXActor::UpdateLightning(float Fade)
{
	for (const FPulsingLightning& Pulse : PulsingLightning)
	{
		UStaticMeshComponent* Component = Pulse.Component.Get();
		UMaterialInstanceDynamic* Material = Pulse.Material.Get();
		if (!Component || !Material)
		{
			continue;
		}

		const float PulseTime = FMath::Fmod(ElapsedTime + Pulse.Phase, Pulse.Period);
		const bool bVisible = PulseTime <= Pulse.VisibleDuration;
		Component->SetVisibility(bVisible, false);
		if (bVisible)
		{
			const float PulseAlpha = 1.f - PulseTime / FMath::Max(KINDA_SMALL_NUMBER, Pulse.VisibleDuration);
			Material->SetScalarParameterValue(
				TEXT("OpacityMultiplier"),
				Pulse.BaseOpacity * Fade * PulseAlpha
			);
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
