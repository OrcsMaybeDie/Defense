#include "Traps/Grid/GridManager.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Traps/TrapBase.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Math/RotationMatrix.h"
#include "Traps/TrapData.h"


namespace
{
	constexpr float PlaneCoordinateUnitCm = 0.1f;
	constexpr float AxisAlignedThreshold = 0.999f;
	const FName TrapFloorTag(TEXT("TrapSurface.Floor"));
	const FName TrapWallTag(TEXT("TrapSurface.Wall"));
	const FName TrapCeilingTag(TEXT("TrapSurface.Ceiling"));

	int32 ToPlaneCoordinateKey(const float CoordinateCm)
	{
		return FMath::RoundToInt(CoordinateCm / PlaneCoordinateUnitCm);
	}

	float FromPlaneCoordinateKey(const int32 CoordinateKey)
	{
		return static_cast<float>(CoordinateKey) * PlaneCoordinateUnitCm;
	}

	// Axis에 따라 평면 위치를 선택
	float GetPlaneCoordinate(const FVector& RelativeLocation, ETrapPlaneAxis PlaneAxis)
	{
		switch (PlaneAxis)
		{
		case ETrapPlaneAxis::X:
			return RelativeLocation.X;

		case ETrapPlaneAxis::Y:
			return RelativeLocation.Y;

		case ETrapPlaneAxis::Z:
		default:
			return RelativeLocation.Z;
		}
	}

	bool ResolveWorldPlane(const FVector& WorldNormal, ETrapPlaneAxis& OutPlaneAxis, ETrapPlaneNormal& OutPlaneNormal)
	{
		const FVector SafeNormal = WorldNormal.GetSafeNormal();
		const FVector AbsNormal = SafeNormal.GetAbs();

		if (AbsNormal.X >= AxisAlignedThreshold)
		{
			OutPlaneAxis = ETrapPlaneAxis::X;
			OutPlaneNormal = SafeNormal.X >= 0.f ? ETrapPlaneNormal::Positive : ETrapPlaneNormal::Negative;
			return true;
		}
		if (AbsNormal.Y >= AxisAlignedThreshold)
		{
			OutPlaneAxis = ETrapPlaneAxis::Y;
			OutPlaneNormal = SafeNormal.Y >= 0.f ? ETrapPlaneNormal::Positive : ETrapPlaneNormal::Negative;
			return true;
		}
		if (AbsNormal.Z >= AxisAlignedThreshold)
		{
			OutPlaneAxis = ETrapPlaneAxis::Z;
			OutPlaneNormal = SafeNormal.Z >= 0.f ? ETrapPlaneNormal::Positive : ETrapPlaneNormal::Negative;
			return true;
		}

		return false;
	}

	FVector GetPlaneNormal(ETrapPlaneAxis PlaneAxis, ETrapPlaneNormal PlaneNormal)
	{
		const float Sign = PlaneNormal == ETrapPlaneNormal::Positive ? 1.f : -1.f;

		switch (PlaneAxis)
		{
		case ETrapPlaneAxis::X:
			return FVector(Sign, 0.f, 0.f);

		case ETrapPlaneAxis::Y:
			return FVector(0.f, Sign, 0.f);

		case ETrapPlaneAxis::Z:
		default:
			return FVector(0.f, 0.f, Sign);
		}
	}

	FVector AddSurfaceOffset(
		const FVector& Location,
		ETrapPlaneAxis PlaneAxis,
		float OffsetU,
		float OffsetV
	)
	{
		switch (PlaneAxis)
		{
		case ETrapPlaneAxis::X:
			return Location + FVector(0.f, OffsetU, OffsetV);

		case ETrapPlaneAxis::Y:
			return Location + FVector(OffsetU, 0.f, OffsetV);

		case ETrapPlaneAxis::Z:
		default:
			return Location + FVector(OffsetU, OffsetV, 0.f);
		}
	}

	ETrapGridSurface GetSurfaceType(ETrapPlaneAxis PlaneAxis, ETrapPlaneNormal PlaneNormal)
	{
		if (PlaneAxis != ETrapPlaneAxis::Z)
		{
			return ETrapGridSurface::Wall;
		}

		return PlaneNormal == ETrapPlaneNormal::Positive
			? ETrapGridSurface::Floor
			: ETrapGridSurface::Ceiling;
	}

	FName GetSurfaceTag(ETrapGridSurface SurfaceType)
	{
		switch (SurfaceType)
		{
		case ETrapGridSurface::Wall:
			return TrapWallTag;

		case ETrapGridSurface::Ceiling:
			return TrapCeilingTag;

		case ETrapGridSurface::Floor:
		default:
			return TrapFloorTag;
		}
	}
}

AGridManager::AGridManager()
{
	/*
		* 서버 Manager → 권위 있는 점유 상태
		* 클라이언트 Manager → 프리뷰 계산용 상태
	*/
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

FTrapCellKey AGridManager::WorldToCellKey(
	const FVector& WorldLocation,
	ETrapPlaneAxis PlaneAxis,
	ETrapPlaneNormal PlaneNormal
) const
{
	FTrapCellKey Result;
	Result.PlaneAxis = PlaneAxis;
	Result.PlaneNormal = PlaneNormal;
	
	const float SafeCellSize = FMath::Max(CellSize, 1.f);
	const FVector RelativeLocation = WorldLocation - GetActorLocation();
	
	Result.PlaneCoordinate = ToPlaneCoordinateKey(GetPlaneCoordinate(RelativeLocation, PlaneAxis));
	
	switch (PlaneAxis)
	{
	case ETrapPlaneAxis::X:
		Result.Cell = FIntPoint(
			FMath::FloorToInt(RelativeLocation.Y / SafeCellSize),
			FMath::FloorToInt(RelativeLocation.Z / SafeCellSize)
		);
		break;
		
	case ETrapPlaneAxis::Y:
		Result.Cell = FIntPoint(
			FMath::FloorToInt(RelativeLocation.X / SafeCellSize),
			FMath::FloorToInt(RelativeLocation.Z / SafeCellSize)
		);
		break;
		
	case ETrapPlaneAxis::Z:
		Result.Cell = FIntPoint(
			FMath::FloorToInt(RelativeLocation.X / SafeCellSize),
			FMath::FloorToInt(RelativeLocation.Y / SafeCellSize)
		);
		break;
	}
	
	return Result;
}

FVector AGridManager::CellKeyToWorldCenter(const FTrapCellKey& CellKey) const
{
	const float SafeCellSize = FMath::Max(CellSize, 1.f);
	const float PlaneCoordinateCm = FromPlaneCoordinateKey(CellKey.PlaneCoordinate);
	
	const float CellCenterA = (CellKey.Cell.X + 0.5f) * SafeCellSize;
	const float CellCenterB = (CellKey.Cell.Y + 0.5f) * SafeCellSize;
	
	FVector RelativeCenter = FVector::ZeroVector;
	
	switch (CellKey.PlaneAxis)
	{
	case ETrapPlaneAxis::X:
		RelativeCenter = FVector(
			PlaneCoordinateCm,
			CellCenterA,
			CellCenterB
		);
		break;
	
	case ETrapPlaneAxis::Y:
		RelativeCenter = FVector(
			CellCenterA,
			PlaneCoordinateCm,
			CellCenterB
		);
		break;
		
	case ETrapPlaneAxis::Z:
		RelativeCenter = FVector(
			CellCenterA,
			CellCenterB,
			PlaneCoordinateCm
		);
		break;
	}
	
	return GetActorLocation() + RelativeCenter;
}

bool AGridManager::IsPlaceableHit(const FHitResult& Hit, ETrapGridSurface SurfaceType) const
{
	const UPrimitiveComponent* HitComponent = Hit.GetComponent();
	const AActor* HitActor = Hit.GetActor();
	if (!Hit.bBlockingHit || !HitComponent)
	{
		return false;
	}

	const FName RequiredTag = GetSurfaceTag(SurfaceType);
	return HitComponent->ComponentHasTag(RequiredTag)
		|| (HitActor && HitActor->ActorHasTag(RequiredTag));
}

bool AGridManager::IsTrapSurfaceCompatible(
	const UTrapData* TrapData,
	const FTrapCellKey& CellKey
) const
{
	return TrapData && TrapData->GridSurface == GetSurfaceType(CellKey.PlaneAxis, CellKey.PlaneNormal);
}

// Hit 위치를 Trap Footprint의 시작 Cell(Anchor)로 변환
// 홀수/짝수 크기 모두 Footprint 중심이 조준 위치에 가장 가깝게 Snap
bool AGridManager::FindClosestValidAnchor(
	const UTrapData* TrapData,
	const FVector& HitLocation,
	ETrapPlaneAxis PlaneAxis,
	ETrapPlaneNormal PlaneNormal,
	FTrapCellKey& OutCellKey
)
{
	if (!TrapData)
	{
		return false;
	}

	const FIntPoint Footprint = TrapData->FootprintCells;
	if (Footprint.X <= 0 || Footprint.Y <= 0)
	{
		return false;
	}

	const FTrapCellKey RequestedAnchor = WorldToTrapAnchorCellKey(
		HitLocation,
		PlaneAxis,
		PlaneNormal,
		Footprint
	);

	struct FAnchorCandidate
	{
		FTrapCellKey CellKey;
		float DistanceSquared = 0.f;
	};

	TArray<FAnchorCandidate> Candidates;
	const int32 SearchRadiusU = FMath::Max(1, (Footprint.X + 1) / 2);
	const int32 SearchRadiusV = FMath::Max(1, (Footprint.Y + 1) / 2);
	Candidates.Reserve((SearchRadiusU * 2 + 1) * (SearchRadiusV * 2 + 1));

	for (int32 UOffset = -SearchRadiusU; UOffset <= SearchRadiusU; ++UOffset)
	{
		for (int32 VOffset = -SearchRadiusV; VOffset <= SearchRadiusV; ++VOffset)
		{
			FAnchorCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.CellKey = RequestedAnchor;
			Candidate.CellKey.Cell += FIntPoint(UOffset, VOffset);
			Candidate.DistanceSquared = FVector::DistSquared(
				GetTrapFootprintCenter(TrapData, Candidate.CellKey),
				HitLocation
			);
		}
	}

	Candidates.Sort([](const FAnchorCandidate& A, const FAnchorCandidate& B)
	{
		return A.DistanceSquared < B.DistanceSquared;
	});

	for (const FAnchorCandidate& Candidate : Candidates)
	{
		TArray<FTrapCellKey> CandidateFootprint;
		GetTrapFootprintCells(TrapData, Candidate.CellKey, CandidateFootprint);
		if (AreCellsValid(CandidateFootprint))
		{
			OutCellKey = Candidate.CellKey;
			return true;
		}
	}

	return false;
}

bool AGridManager::TryGetCellKeyForHit(
	const UTrapData* TrapData,
	const FHitResult& Hit,
	FTrapCellKey& OutCellKey
)
{
	if (!TrapData || !Hit.GetComponent())
	{
		return false;
	}

	ETrapPlaneAxis PlaneAxis;
	ETrapPlaneNormal PlaneNormal;
	if (!ResolveWorldPlane(Hit.ImpactNormal, PlaneAxis, PlaneNormal))
	{
		return false;
	}

	const FTrapCellKey HitCell = WorldToCellKey(Hit.ImpactPoint, PlaneAxis, PlaneNormal);
	if (!IsTrapSurfaceCompatible(TrapData, HitCell)
		|| !IsPlaceableHit(Hit, TrapData->GridSurface))
	{
		return false;
	}

	return FindClosestValidAnchor(
		TrapData,
		Hit.ImpactPoint,
		PlaneAxis,
		PlaneNormal,
		OutCellKey
	);
}

bool AGridManager::EvaluateCellStaticValidity(const FTrapCellKey& CellKey) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float SafeCellSize = FMath::Max(CellSize, 1.f);
	const float ProbeHalfSpan = FMath::Max(SafeCellSize * 0.5f - CellProbeInset, 0.f);
	const FVector CellCenter = CellKeyToWorldCenter(CellKey);
	const FVector SurfaceNormal = GetPlaneNormal(CellKey.PlaneAxis, CellKey.PlaneNormal);
	const ETrapGridSurface SurfaceType = GetSurfaceType(CellKey.PlaneAxis, CellKey.PlaneNormal);

	TArray<FVector2D, TInlineAllocator<5>> ProbeOffsets;
	ProbeOffsets.Add(FVector2D::ZeroVector);
	if (ProbeHalfSpan > KINDA_SMALL_NUMBER)
	{
		ProbeOffsets.Add(FVector2D(-ProbeHalfSpan, -ProbeHalfSpan));
		ProbeOffsets.Add(FVector2D(-ProbeHalfSpan, ProbeHalfSpan));
		ProbeOffsets.Add(FVector2D(ProbeHalfSpan, -ProbeHalfSpan));
		ProbeOffsets.Add(FVector2D(ProbeHalfSpan, ProbeHalfSpan));
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TrapGridCellProbe), false, this);
	const FVector ManagerLocation = GetActorLocation();
	const float ExpectedPlaneCoordinate = FromPlaneCoordinateKey(CellKey.PlaneCoordinate);

	for (const FVector2D& ProbeOffset : ProbeOffsets)
	{
		const FVector ProbeLocation = AddSurfaceOffset(
			CellCenter,
			CellKey.PlaneAxis,
			ProbeOffset.X,
			ProbeOffset.Y
		);
		const FVector TraceStart = ProbeLocation + SurfaceNormal * SurfaceProbeDistance;
		const FVector TraceEnd = ProbeLocation - SurfaceNormal * SurfaceProbeDistance;

		FHitResult SurfaceHit;
		if (!World->LineTraceSingleByObjectType(
			SurfaceHit,
			TraceStart,
			TraceEnd,
			ObjectQueryParams,
			QueryParams
		))
		{
			return false;
		}

		ETrapPlaneAxis HitPlaneAxis;
		ETrapPlaneNormal HitPlaneNormal;
		if (!ResolveWorldPlane(SurfaceHit.ImpactNormal, HitPlaneAxis, HitPlaneNormal)
			|| HitPlaneAxis != CellKey.PlaneAxis
			|| HitPlaneNormal != CellKey.PlaneNormal
			|| !IsPlaceableHit(SurfaceHit, SurfaceType))
		{
			return false;
		}

		const float HitPlaneCoordinate = GetPlaneCoordinate(
			SurfaceHit.ImpactPoint - ManagerLocation,
			CellKey.PlaneAxis
		);
		if (FMath::Abs(HitPlaneCoordinate - ExpectedPlaneCoordinate) > SurfacePlaneTolerance)
		{
			return false;
		}
	}

	return true;
}

bool AGridManager::IsCellValid(const FTrapCellKey& CellKey)
{
	if (ValidCells.Contains(CellKey))
	{
		return true;
	}
	if (InvalidCells.Contains(CellKey))
	{
		return false;
	}

	if (EvaluateCellStaticValidity(CellKey))
	{
		ValidCells.Add(CellKey);
		return true;
	}

	InvalidCells.Add(CellKey);
	return false;
}

bool AGridManager::IsCellOccupied(const FTrapCellKey& CellKey) const
{
	// Collision 검사에서 같은 평면으로 허용한 높이 차이는 점유에서도 같은 Cell로 처리
	const int32 PlaneToleranceKey = FMath::FloorToInt(
		FMath::Max(SurfacePlaneTolerance, 0.f) / PlaneCoordinateUnitCm + KINDA_SMALL_NUMBER
	);

	for (int32 PlaneOffset = -PlaneToleranceKey; PlaneOffset <= PlaneToleranceKey; ++PlaneOffset)
	{
		FTrapCellKey OccupancyKey = CellKey;
		OccupancyKey.PlaneCoordinate += PlaneOffset;

		const TWeakObjectPtr<ATrapBase>* FoundTrap = CellToTrap.Find(OccupancyKey);
		if (FoundTrap && FoundTrap->IsValid())
		{
			return true;
		}
	}

	return false;
}

bool AGridManager::AreCellsValid(const TArray<FTrapCellKey>& CellKeys)
{
	if (CellKeys.IsEmpty())
	{
		return false;
	}

	for (const FTrapCellKey& CellKey : CellKeys)
	{
		if (!IsCellValid(CellKey))
		{
			return false;
		}
	}

	return true;
}

bool AGridManager::AreCellsAvailable(const TArray<FTrapCellKey>& CellKeys)
{
	if (!AreCellsValid(CellKeys))
	{
		return false;
	}

	for (const FTrapCellKey& CellKey : CellKeys)
	{
		if (IsCellOccupied(CellKey))
		{
			return false;
		}
	}

	return true;
}

FTrapCellKey AGridManager::WorldToTrapAnchorCellKey(
	const FVector& WorldLocation,
	ETrapPlaneAxis PlaneAxis,
	ETrapPlaneNormal PlaneNormal,
	const FIntPoint& FootprintCells
) const
{
	FTrapCellKey AnchorCell = WorldToCellKey(WorldLocation, PlaneAxis, PlaneNormal);
	const float SafeCellSize = FMath::Max(GetCellSize(), 1.f);
	const FVector RelativeLocation = WorldLocation - GetActorLocation();

	float CoordinateU = 0.f;
	float CoordinateV = 0.f;

	switch (PlaneAxis)
	{
	case ETrapPlaneAxis::X:
		// X 평면 벽: World Y/Z가 Surface U/V
		CoordinateU = RelativeLocation.Y;
		CoordinateV = RelativeLocation.Z;
		break;

	case ETrapPlaneAxis::Y:
		// Y 평면 벽: World X/Z가 Surface U/V
		CoordinateU = RelativeLocation.X;
		CoordinateV = RelativeLocation.Z;
		break;

	case ETrapPlaneAxis::Z:
	default:
		// 바닥/천장: World X/Y가 Surface U/V
		CoordinateU = RelativeLocation.X;
		CoordinateV = RelativeLocation.Y;
		break;
	}

	// 홀수/짝수 Footprint 모두 중심이 조준 위치에 가장 가까운 Anchor를 계산
	AnchorCell.Cell = FIntPoint(
		FMath::RoundToInt(CoordinateU / SafeCellSize - static_cast<float>(FootprintCells.X) * 0.5f),
		FMath::RoundToInt(CoordinateV / SafeCellSize - static_cast<float>(FootprintCells.Y) * 0.5f)
	);

	return AnchorCell;
}

void AGridManager::GetTrapFootprintCells(
	const UTrapData* TrapData,
	const FTrapCellKey& AnchorCell,
	TArray<FTrapCellKey>& OutCellKeys
) const
{
	OutCellKeys.Reset();

	if (!TrapData)
	{
		return;
	}

	const FIntPoint Footprint = TrapData->FootprintCells;
	if (Footprint.X <= 0 || Footprint.Y <= 0)
	{
		return;
	}

	OutCellKeys.Reserve(Footprint.X * Footprint.Y);

	for (int32 UIndex = 0; UIndex < Footprint.X; ++UIndex)
	{
		for (int32 VIndex = 0; VIndex < Footprint.Y; ++VIndex)
		{
			FTrapCellKey CellKey = AnchorCell;
			CellKey.Cell += FIntPoint(UIndex, VIndex);
			OutCellKeys.Add(CellKey);
		}
	}
}

FVector AGridManager::GetTrapFootprintCenter(
	const UTrapData* TrapData,
	const FTrapCellKey& AnchorCell
) const
{
	FVector Center = CellKeyToWorldCenter(AnchorCell);
	if (!TrapData)
	{
		return Center;
	}

	const FIntPoint Footprint = TrapData->FootprintCells;
	const float OffsetU = (Footprint.X - 1) * GetCellSize() * 0.5f;
	const float OffsetV = (Footprint.Y - 1) * GetCellSize() * 0.5f;

	switch (AnchorCell.PlaneAxis)
	{
	case ETrapPlaneAxis::X:
		Center += FVector(0.f, OffsetU, OffsetV);
		break;
	case ETrapPlaneAxis::Y:
		Center += FVector(OffsetU, 0.f, OffsetV);
		break;
	case ETrapPlaneAxis::Z:
	default:
		Center += FVector(OffsetU, OffsetV, 0.f);
		break;
	}

	return Center;
}

FTransform AGridManager::GetTrapFootprintTransform(
	const UTrapData* TrapData,
	const FTrapCellKey& AnchorCell
) const
{
	FVector TrapLocalUp = FVector::UpVector;

	switch (AnchorCell.PlaneAxis)
	{
	case ETrapPlaneAxis::X:
		TrapLocalUp = AnchorCell.PlaneNormal == ETrapPlaneNormal::Positive
			? FVector::ForwardVector
			: -FVector::ForwardVector;
		break;

	case ETrapPlaneAxis::Y:
		TrapLocalUp = AnchorCell.PlaneNormal == ETrapPlaneNormal::Positive
			? FVector::RightVector
			: -FVector::RightVector;
		break;

	case ETrapPlaneAxis::Z:
	default:
		TrapLocalUp = AnchorCell.PlaneNormal == ETrapPlaneNormal::Positive
			? FVector::UpVector
			: -FVector::UpVector;
		break;
	}

	// 벽 양쪽 모두 Trap local +Y가 World +Z를 향하도록 local +X를 결정
	FVector TrapLocalForward = FVector::ForwardVector;
	if (AnchorCell.PlaneAxis == ETrapPlaneAxis::X)
	{
		TrapLocalForward = AnchorCell.PlaneNormal == ETrapPlaneNormal::Positive
			? FVector::RightVector
			: -FVector::RightVector;
	}
	else if (AnchorCell.PlaneAxis == ETrapPlaneAxis::Y)
	{
		TrapLocalForward = AnchorCell.PlaneNormal == ETrapPlaneNormal::Positive
			? -FVector::ForwardVector
			: FVector::ForwardVector;
	}

	const FQuat Rotation = FRotationMatrix::MakeFromXZ(
		TrapLocalForward,
		TrapLocalUp
	).ToQuat();

	return FTransform(
		Rotation,
		GetTrapFootprintCenter(TrapData, AnchorCell),
		FVector::OneVector
	);
}

bool AGridManager::TryOccupyCells(const TArray<FTrapCellKey>& CellKeys, ATrapBase* Trap)
{
	// Server
	
	if (!HasAuthority() || !IsValid(Trap)) { return false; }

	const FObjectKey TrapKey(Trap);

	if (TrapToCells.Contains(TrapKey)) { return false; } // 이미 등록된 Trap인가?
	if (!AreCellsAvailable(CellKeys)) { return false; }

	
	// 중복 제거 → Footprint 계산 실수로 중복 Cell이 전달돼도 내부 점유 기록에는 한 번만 저장
	TSet<FTrapCellKey> UniqueCells;
	
	for (const FTrapCellKey& CellKey : CellKeys)
	{
		UniqueCells.Add(CellKey);
	}
	
	TArray<FTrapCellKey> OccupiedCellList = UniqueCells.Array();
	
	for (const FTrapCellKey& CellKey : OccupiedCellList)
	{
		CellToTrap.Add(CellKey, Trap); // CellToTrap 등록
	}

	TrapToCells.Add(TrapKey, OccupiedCellList); // TrapToCells 등록

	// OnDestroyed 연결
	Trap->OnDestroyed.AddUniqueDynamic(
		this,
		&AGridManager::HandleTrapDestroyed
	);

	return true;
}

void AGridManager::ReleaseTrap(ATrapBase* Trap)
{
	// Server
	
	if (!HasAuthority() || !Trap) { return; }

	const FObjectKey TrapKey(Trap);

	TArray<FTrapCellKey> OccupiedCellList;
	if (!TrapToCells.RemoveAndCopyValue(TrapKey,OccupiedCellList)) { return; }

	for (const FTrapCellKey& CellKey : OccupiedCellList)
	{
		const TWeakObjectPtr<ATrapBase>* FoundTrap = CellToTrap.Find(CellKey);

		if (FoundTrap && FoundTrap->Get() == Trap)
		{
			CellToTrap.Remove(CellKey);
		}
	}

	Trap->OnDestroyed.RemoveDynamic(
		this,
		&AGridManager::HandleTrapDestroyed
	);
}

void AGridManager::RegisterClientOccupiedCells(ATrapBase* Trap, const TArray<FTrapCellKey>& CellKeys)
{
	if (HasAuthority() || !Trap)
	{
		return;
	}

	for (const FTrapCellKey& CellKey : CellKeys)
	{
		CellToTrap.Add(CellKey, Trap);
	}
}

void AGridManager::ReleaseClientTrap(ATrapBase* Trap)
{
	if (HasAuthority() || !Trap)
	{
		return;
	}

	for (auto It = CellToTrap.CreateIterator(); It; ++It)
	{
		if (It.Value().Get() == Trap)
		{
			It.RemoveCurrent();
		}
	}
}

bool AGridManager::IsTrapRegistered(const ATrapBase* Trap) const
{
	return Trap && TrapToCells.Contains(FObjectKey(Trap));
}

void AGridManager::HandleTrapDestroyed(AActor* DestroyedActor)
{
	if (ATrapBase* DestroyedTrap = Cast<ATrapBase>(DestroyedActor))
	{
		ReleaseTrap(DestroyedTrap);
	}
}
