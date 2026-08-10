#include "Traps/MedusaTest.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "TimerManager.h"

AMedusaTest::AMedusaTest()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	RootCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("RootCollision"));
	RootCollision->SetBoxExtent(FVector(50.f, 200.f, 200.f));
	RootCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetRootComponent(RootCollision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootCollision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);

	GazeOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("GazeOrigin"));
	GazeOrigin->SetupAttachment(RootCollision);

	TargetBoneNames.Add(TEXT("head"));
}

void AMedusaTest::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority() || ScanInterval <= 0.f)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		ScanTimerHandle,
		this,
		&AMedusaTest::ScanForEnemies,
		ScanInterval,
		true,
		ScanInterval
	);
}

void AMedusaTest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ScanTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AMedusaTest::ScanForEnemies()
{
	if (!HasAuthority() || TargetBoneNames.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !GazeOrigin)
	{
		return;
	}

	if (bDrawDebug)
	{
		const float LifeTime = DebugDuration > 0.f ? DebugDuration : ScanInterval;
		MulticastDrawDetectionDebug(
			GazeOrigin->GetComponentLocation(),
			GazeOrigin->GetForwardVector().GetSafeNormal(),
			GetOverlapRadius(),
			PetrifyRange,
			ConeAngle,
			LifeTime
		);
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(EnemyObjectChannel);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MedusaOverlap), false, this);
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		GazeOrigin->GetComponentLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(GetOverlapRadius()),
		QueryParams
	);

	TSet<AEnemyBase*> UniqueEnemies;
	for (const FOverlapResult& Overlap : OverlapResults)
	{
		if (AEnemyBase* Enemy = Cast<AEnemyBase>(Overlap.GetActor()))
		{
			UniqueEnemies.Add(Enemy);
		}
	}

	const FVector Origin = GazeOrigin->GetComponentLocation();
	const FVector Forward = GazeOrigin->GetForwardVector();

	for (AEnemyBase* Enemy : UniqueEnemies)
	{
		if (!IsValid(Enemy)
			|| Enemy->EnemyMode != EEnemyMode::Combat
			|| Enemy->EnemyState == EEnemyState::Stone
			|| Enemy->EnemyState == EEnemyState::StoneDie
			|| Enemy->EnemyState == EEnemyState::Die)
		{
			continue;
		}

		USkeletalMeshComponent* EnemyMesh = Enemy->GetMesh();
		if (!EnemyMesh)
		{
			continue;
		}

		for (const FName BoneName : TargetBoneNames)
		{
			if (BoneName.IsNone() || EnemyMesh->GetBoneIndex(BoneName) == INDEX_NONE)
			{
				continue;
			}

			const FVector BoneLocation = EnemyMesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);
			if (!IsPointInsideHorizontalCone(Origin, Forward, BoneLocation))
			{
				continue;
			}

			if (!HasClearSightToPoint(Enemy, BoneLocation))
			{
				continue;
			}

			Enemy->TryEnterStone();
			break;
		}
	}
}

bool AMedusaTest::IsPointInsideHorizontalCone(const FVector& Origin, const FVector& Forward, const FVector& Point) const
{
	const FVector ToPoint2D = FVector(Point.X - Origin.X, Point.Y - Origin.Y, 0.f);
	const float DistanceSquared2D = ToPoint2D.SizeSquared();
	if (DistanceSquared2D > FMath::Square(PetrifyRange))
	{
		return false;
	}

	if (DistanceSquared2D <= UE_KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const FVector Forward2D = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal();
	if (Forward2D.IsNearlyZero())
	{
		return false;
	}

	const float HalfAngleRadians = FMath::DegreesToRadians(FMath::Clamp(ConeAngle, 0.f, 360.f) * 0.5f);
	const float MinimumDot = FMath::Cos(HalfAngleRadians);
	return FVector::DotProduct(Forward2D, ToPoint2D.GetSafeNormal()) >= MinimumDot;
}

bool AMedusaTest::HasClearSightToPoint(AActor* TargetActor, const FVector& TargetPoint) const
{
	const UWorld* World = GetWorld();
	if (!World || !GazeOrigin)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MedusaSight), true, this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(TargetActor);
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}

	FHitResult Hit;
	return !World->LineTraceSingleByChannel(
		Hit,
		GazeOrigin->GetComponentLocation(),
		TargetPoint,
		MedusaSightChannel,
		QueryParams
	);
}

float AMedusaTest::GetOverlapRadius() const
{
	return FMath::Sqrt(FMath::Square(FMath::Max(PetrifyRange, 0.f)) + FMath::Square(FMath::Max(MaxHeightDifference, 0.f)));
}

void AMedusaTest::DrawDetectionDebug(
	const FVector& Origin,
	const FVector& Forward,
	const float SphereRadius,
	const float InPetrifyRange,
	const float InConeAngle,
	const float LifeTime
) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Forward2D = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal();

	if (Forward2D.IsNearlyZero())
	{
		return;
	}

	DrawDebugSphere(World, Origin, SphereRadius, 32, FColor::Cyan, false, LifeTime, 0, 1.f);

	const float ClampedAngle = FMath::Clamp(InConeAngle, 0.f, 360.f);
	const float HalfAngle = ClampedAngle * 0.5f;
	const int32 SegmentCount = FMath::Max(2, FMath::CeilToInt(ClampedAngle / 5.f));
	const FVector LeftDirection = Forward2D.RotateAngleAxis(-HalfAngle, FVector::UpVector);
	const FVector RightDirection = Forward2D.RotateAngleAxis(HalfAngle, FVector::UpVector);

	DrawDebugLine(World, Origin, Origin + LeftDirection * InPetrifyRange, FColor::Yellow, false, LifeTime, 0, 2.f);
	DrawDebugLine(World, Origin, Origin + RightDirection * InPetrifyRange, FColor::Yellow, false, LifeTime, 0, 2.f);

	FVector PreviousPoint = Origin + LeftDirection * InPetrifyRange;
	for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
	{
		const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const float CurrentAngle = FMath::Lerp(-HalfAngle, HalfAngle, Alpha);
		const FVector CurrentPoint = Origin + Forward2D.RotateAngleAxis(CurrentAngle, FVector::UpVector) * InPetrifyRange;
		DrawDebugLine(World, PreviousPoint, CurrentPoint, FColor::Yellow, false, LifeTime, 0, 2.f);
		PreviousPoint = CurrentPoint;
	}
}

void AMedusaTest::MulticastDrawDetectionDebug_Implementation(
	FVector_NetQuantize Origin,
	FVector_NetQuantizeNormal Forward,
	const float SphereRadius,
	const float InPetrifyRange,
	const float InConeAngle,
	const float LifeTime
)
{
	if (!bDrawDebug)
	{
		return;
	}

	DrawDetectionDebug(Origin, Forward, SphereRadius, InPetrifyRange, InConeAngle, LifeTime);
}
