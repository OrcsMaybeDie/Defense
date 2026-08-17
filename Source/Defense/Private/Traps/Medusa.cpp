#include "Traps/Medusa.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Collision/DefenseCollisionChannels.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 MaxPetrifiedEnemyCount = 5;

	struct FPetrifyCandidate
	{
		AEnemyBase* Enemy = nullptr;
		double DistanceSquared = 0.0;
		FVector TargetPoint = FVector::ZeroVector;
	};
}

AMedusa::AMedusa()
{
	PrimaryActorTick.bCanEverTick = false;

	GazeOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("GazeOrigin"));
	GazeOrigin->SetupAttachment(SceneRoot);

	LeftEyeSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NativeLeftEyeSphere"));
	LeftEyeSphere->SetupAttachment(SceneRoot);
	LeftEyeSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftEyeSphere->SetGenerateOverlapEvents(false);
	LeftEyeSphere->SetCastShadow(false);
	LeftEyeSphere->SetCanEverAffectNavigation(false);

	RightEyeSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NativeRightEyeSphere"));
	RightEyeSphere->SetupAttachment(SceneRoot);
	RightEyeSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightEyeSphere->SetGenerateOverlapEvents(false);
	RightEyeSphere->SetCastShadow(false);
	RightEyeSphere->SetCanEverAffectNavigation(false);

	LeftBeamOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("NativeLeftBeamOrigin"));
	LeftBeamOrigin->SetupAttachment(SceneRoot);

	RightBeamOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("NativeRightBeamOrigin"));
	RightBeamOrigin->SetupAttachment(SceneRoot);

	TargetBoneNames.Add(TEXT("head"));
}

void AMedusa::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyEyeMaterial();
}

void AMedusa::BeginPlay()
{
	Super::BeginPlay();
	InitializeEyeVisuals();
	StartScanTimer();
}

void AMedusa::InitializePlacedTrap(
	UTrapData* TrapData,
	ADefensePlayerState* InInstalledByPlayerState,
	const TArray<FTrapCellKey>& InOccupiedCells
)
{
	Super::InitializePlacedTrap(TrapData, InInstalledByPlayerState, InOccupiedCells);
	StartScanTimer();
}

void AMedusa::StartScanTimer()
{
	if (!HasAuthority() || !IsPlaced() || ScanInterval <= 0.f || ScanTimerHandle.IsValid())
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		ScanTimerHandle,
		this,
		&AMedusa::ScanForEnemies,
		ScanInterval,
		true,
		ScanInterval
	);

	MulticastStartEyeVisual(ScanInterval);
}

void AMedusa::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ScanTimerHandle);
	GetWorldTimerManager().ClearTimer(EyeVisualTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AMedusa::ScanForEnemies()
{
	if (!HasAuthority() || !IsPlaced() || TargetBoneNames.IsEmpty())
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
	ObjectQueryParams.AddObjectTypesToQuery(DefenseCollisionChannels::Enemy);

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
	TArray<FPetrifyCandidate> Candidates;
	Candidates.Reserve(UniqueEnemies.Num());

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

			Candidates.Add({Enemy, FVector::DistSquared(Origin, Enemy->GetActorLocation()), BoneLocation});
			break;
		}
	}

	Candidates.Sort([](const FPetrifyCandidate& Left, const FPetrifyCandidate& Right)
	{
		return Left.DistanceSquared < Right.DistanceSquared;
	});

	int32 PetrifiedEnemyCount = 0;
	for (const FPetrifyCandidate& Candidate : Candidates)
	{
		if (!IsValid(Candidate.Enemy) || !Candidate.Enemy->TryEnterStone())
		{
			continue;
		}

		MulticastPlayPetrifyBeam(Candidate.TargetPoint);
		if (++PetrifiedEnemyCount >= MaxPetrifiedEnemyCount)
		{
			break;
		}
	}

	MulticastStartEyeVisual(ScanInterval);
}

void AMedusa::ApplyEyeMaterial()
{
	if (!EyeMaterial)
	{
		return;
	}

	if (LeftEyeSphere)
	{
		LeftEyeSphere->SetMaterial(0, EyeMaterial);
	}
	if (RightEyeSphere)
	{
		RightEyeSphere->SetMaterial(0, EyeMaterial);
	}
}

void AMedusa::InitializeEyeVisuals()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	ApplyEyeMaterial();
	if (!LeftEyeMID && LeftEyeSphere && LeftEyeSphere->GetNumMaterials() > 0)
	{
		LeftEyeMID = LeftEyeSphere->CreateDynamicMaterialInstance(0);
	}
	if (!RightEyeMID && RightEyeSphere && RightEyeSphere->GetNumMaterials() > 0)
	{
		RightEyeMID = RightEyeSphere->CreateDynamicMaterialInstance(0);
	}

	SetEyeOpenAmount(0.f);
}

void AMedusa::StartEyeVisual(const float Duration)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	InitializeEyeVisuals();
	GetWorldTimerManager().ClearTimer(EyeVisualTimerHandle);
	EyeVisualDuration = FMath::Max(Duration, UE_KINDA_SMALL_NUMBER);
	EyeVisualStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	SetEyeOpenAmount(0.f);

	GetWorldTimerManager().SetTimer(
		EyeVisualTimerHandle,
		this,
		&AMedusa::UpdateEyeVisual,
		FMath::Max(EyeVisualUpdateInterval, 0.001f),
		true
	);
}

void AMedusa::UpdateEyeVisual()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float ElapsedTime = static_cast<float>(World->GetTimeSeconds() - EyeVisualStartTime);
	const float Alpha = FMath::Clamp(ElapsedTime / FMath::Max(EyeVisualDuration, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);
	SetEyeOpenAmount(Alpha);

	if (Alpha >= 1.f)
	{
		GetWorldTimerManager().ClearTimer(EyeVisualTimerHandle);
	}
}

void AMedusa::SetEyeOpenAmount(const float Amount)
{
	const float ClampedAmount = FMath::Clamp(Amount, 0.f, 1.f);
	if (LeftEyeMID)
	{
		LeftEyeMID->SetScalarParameterValue(EyeOpenParameterName, ClampedAmount);
	}
	if (RightEyeMID)
	{
		RightEyeMID->SetScalarParameterValue(EyeOpenParameterName, ClampedAmount);
	}
}

void AMedusa::SpawnBeamFromOrigin(const USceneComponent* BeamOrigin, const FVector& TargetPoint) const
{
	if (!BeamOrigin || !BeamVFXSystem)
	{
		return;
	}

	const FVector StartPoint = BeamOrigin->GetComponentLocation();
	UNiagaraComponent* BeamComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		BeamVFXSystem,
		StartPoint,
		FRotator::ZeroRotator,
		FVector::OneVector,
		true,
		false,
		ENCPoolMethod::None,
		true
	);
	if (!BeamComponent)
	{
		return;
	}

	BeamComponent->SetVariablePosition(BeamStartParameterName, StartPoint);
	BeamComponent->SetVariablePosition(BeamEndParameterName, TargetPoint);
	BeamComponent->SetVariableFloat(BeamTravelTimeParameterName, FMath::Max(BeamTravelTime, UE_KINDA_SMALL_NUMBER));
	BeamComponent->Activate(true);
}

bool AMedusa::IsPointInsideHorizontalCone(const FVector& Origin, const FVector& Forward, const FVector& Point) const
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

bool AMedusa::HasClearSightToPoint(AActor* TargetActor, const FVector& TargetPoint) const
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
		DefenseCollisionChannels::MedusaSight,
		QueryParams
	);
}

float AMedusa::GetOverlapRadius() const
{
	return FMath::Sqrt(FMath::Square(FMath::Max(PetrifyRange, 0.f)) + FMath::Square(FMath::Max(MaxHeightDifference, 0.f)));
}

void AMedusa::DrawDetectionDebug(
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

void AMedusa::MulticastDrawDetectionDebug_Implementation(
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

void AMedusa::MulticastStartEyeVisual_Implementation(const float Duration)
{
	StartEyeVisual(Duration);
}

void AMedusa::MulticastPlayPetrifyBeam_Implementation(FVector_NetQuantize TargetPoint)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	SpawnBeamFromOrigin(LeftBeamOrigin, TargetPoint);
	SpawnBeamFromOrigin(RightBeamOrigin, TargetPoint);
}
