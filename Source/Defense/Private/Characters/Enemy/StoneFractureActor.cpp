#include "Characters/Enemy/StoneFractureActor.h"

#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"

AStoneFractureActor::AStoneFractureActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	AddDefaultPart(TEXT("head"));
	AddDefaultPart(TEXT("spine_03"));
	AddDefaultPart(TEXT("pelvis"));
	AddDefaultPart(TEXT("upperarm_l"));
	AddDefaultPart(TEXT("lowerarm_l"));
	AddDefaultPart(TEXT("hand_l"));
	AddDefaultPart(TEXT("upperarm_r"));
	AddDefaultPart(TEXT("lowerarm_r"));
	AddDefaultPart(TEXT("hand_r"));
	AddDefaultPart(TEXT("thigh_l"));
	AddDefaultPart(TEXT("calf_l"));
	AddDefaultPart(TEXT("foot_l"));
	AddDefaultPart(TEXT("thigh_r"));
	AddDefaultPart(TEXT("calf_r"));
	AddDefaultPart(TEXT("foot_r"));
}

void AStoneFractureActor::BeginPlay()
{
	Super::BeginPlay();
	EnsurePartComponents();
	DeactivateToPool();
}

void AStoneFractureActor::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bFractureActive)
	{
		return;
	}

	ActiveElapsedTime += DeltaTime;
	if (bPendingFracture)
	{
		FractureElapsedTime += DeltaTime;
		if (FractureElapsedTime >= FractureDelay)
		{
			ApplyFracture();
		}
	}

	if (ActiveElapsedTime >= FMath::Max(ActiveLifetime, 0.1f))
	{
		ReturnSelfToPool();
	}
}

bool AStoneFractureActor::ActivateFromSkeletalMesh(USkeletalMeshComponent* SourceMesh)
{
	if (!IsValid(SourceMesh))
	{
		return false;
	}

	EnsurePartComponents();
	SetActorTransform(FTransform::Identity);
	SetActorHiddenInGame(false);

	int32 ActivatedPartCount = 0;
	FVector AccumulatedOrigin = FVector::ZeroVector;

	for (int32 PartIndex = 0; PartIndex < FractureParts.Num(); ++PartIndex)
	{
		UGeometryCollectionComponent* PartComponent = PartComponents.IsValidIndex(PartIndex)
			? PartComponents[PartIndex]
			: nullptr;
		const FStoneFracturePart& Part = FractureParts[PartIndex];

		if (!IsValid(PartComponent) || Part.BoneName.IsNone() || !SourceMesh->DoesSocketExist(Part.BoneName))
		{
			continue;
		}

		PartComponent->SetSimulatePhysics(false);
		PartComponent->ResetState();

		const FTransform BoneWorldTransform = SourceMesh->GetSocketTransform(Part.BoneName, RTS_World);
		const FTransform PartWorldTransform = Part.BoneOffset * BoneWorldTransform;
		PartComponent->SetWorldTransform(PartWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
		PartComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PartComponent->SetVisibility(true, true);
		
		// 물리 켜기
		PartComponent->SetSimulatePhysics(true);

		AccumulatedOrigin += PartWorldTransform.GetLocation();
		++ActivatedPartCount;
	}

	if (ActivatedPartCount <= 0)
	{
		DeactivateToPool();
		return false;
	}

	FractureOrigin = AccumulatedOrigin / static_cast<float>(ActivatedPartCount);
	ActiveElapsedTime = 0.f;
	FractureElapsedTime = 0.f;
	bFractureActive = true;
	bPendingFracture = true;
	SetActorTickEnabled(true);
	OnFractureActivated(FractureOrigin);
	return true;
}

void AStoneFractureActor::DeactivateToPool()
{
	const bool bWasActive = bFractureActive;
	bFractureActive = false;
	bPendingFracture = false;
	ActiveElapsedTime = 0.f;
	FractureElapsedTime = 0.f;
	ActiveStrainField = nullptr;
	SetActorTickEnabled(false);

	for (UGeometryCollectionComponent* PartComponent : PartComponents)
	{
		if (!IsValid(PartComponent))
		{
			continue;
		}

		PartComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PartComponent->SetSimulatePhysics(false);
		PartComponent->ResetState();
		PartComponent->SetVisibility(false, true);
	}

	SetActorHiddenInGame(true);
	SetActorLocation(FVector(0.f, 0.f, -100000.f), false, nullptr, ETeleportType::TeleportPhysics);

	if (bWasActive)
	{
		OnFractureReturnedToPool();
	}
}

void AStoneFractureActor::AddDefaultPart(const FName BoneName)
{
	FStoneFracturePart& NewPart = FractureParts.AddDefaulted_GetRef();
	NewPart.BoneName = BoneName;
}

void AStoneFractureActor::EnsurePartComponents()
{
	if (PartComponents.Num() == FractureParts.Num())
	{
		return;
	}

	PartComponents.SetNum(FractureParts.Num());
	for (int32 PartIndex = 0; PartIndex < FractureParts.Num(); ++PartIndex)
	{
		if (IsValid(PartComponents[PartIndex]) || !IsValid(FractureParts[PartIndex].GeometryCollection))
		{
			continue;
		}

		const FName ComponentName(*FString::Printf(TEXT("StoneFracturePart_%02d"), PartIndex));
		UGeometryCollectionComponent* PartComponent = NewObject<UGeometryCollectionComponent>(this, ComponentName);
		if (!PartComponent)
		{
			continue;
		}

		AddInstanceComponent(PartComponent);
		PartComponent->SetupAttachment(SceneRoot);
		PartComponent->SetMobility(EComponentMobility::Movable);
		PartComponent->SetCastShadow(false);
		PartComponent->SetRestCollection(FractureParts[PartIndex].GeometryCollection);
		PartComponent->SetGenerateOverlapEvents(false);
		PartComponent->SetCanEverAffectNavigation(false);
		PartComponent->SetCollisionObjectType(ECC_WorldDynamic);
		PartComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		PartComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		PartComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		PartComponent->SetEnableDamageFromCollision(false);
		PartComponent->SetVisibility(false, true);
		PartComponent->RegisterComponent();
		PartComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PartComponent->SetSimulatePhysics(false);
		PartComponents[PartIndex] = PartComponent;
	}
}

void AStoneFractureActor::ApplyFracture()
{
	if (!bPendingFracture)
	{
		return;
	}

	bPendingFracture = false;
	ActiveStrainField = NewObject<URadialFalloff>(this);
	if (ActiveStrainField)
	{
		ActiveStrainField->SetRadialFalloff(
			ExternalStrain,
			0.f,
			1.f,
			0.f,
			FMath::Max(FractureRadius, 0.f),
			FractureOrigin,
			EFieldFalloffType::Field_FallOff_None);
	}

	for (UGeometryCollectionComponent* PartComponent : PartComponents)
	{
		if (!IsValid(PartComponent) || !PartComponent->IsVisible())
		{
			continue;
		}

		if (ActiveStrainField && ExternalStrain > 0.f)
		{
			PartComponent->ApplyPhysicsField(
				true,
				EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain,
				nullptr,
				ActiveStrainField);
		}

		if (RadialImpulseStrength > 0.f && FractureRadius > 0.f)
		{
			PartComponent->AddRadialImpulse(
				FractureOrigin,
				FractureRadius,
				RadialImpulseStrength,
				ERadialImpulseFalloff::RIF_Linear,
				true);
		}
	}
}

void AStoneFractureActor::ReturnSelfToPool()
{
	SetActorTickEnabled(false);
	if (UWorld* World = GetWorld())
	{
		if (UEnemyPoolSubsystem* EnemyPool = World->GetSubsystem<UEnemyPoolSubsystem>())
		{
			EnemyPool->ReturnStoneFractureToPool(this);
			return;
		}
	}

}