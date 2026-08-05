#include "Traps/Grid/GridManager.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Traps/TrapBase.h"
#include "EngineUtils.h"
#include "Math/RotationMatrix.h"
#include "Traps/Grid/GridSurfaceComponent.h"
#include "Traps/TrapData.h"


namespace
{
	constexpr float PlaneCoordinateUnitCm = 0.1f;
	constexpr float AxisAlignedThreshold = 0.999f;
	constexpr float SurfaceSizeQuantizationToleranceCm = 5.f;
	constexpr float SurfaceHitPlaneToleranceCm = 25.f;

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

	bool IsAxisAlignedTransform(const FTransform& Transform)
	{
		ETrapPlaneAxis AxisX;
		ETrapPlaneAxis AxisY;
		ETrapPlaneAxis AxisZ;
		ETrapPlaneNormal Normal;

		return ResolveWorldPlane(Transform.GetUnitAxis(EAxis::X), AxisX, Normal)
			&& ResolveWorldPlane(Transform.GetUnitAxis(EAxis::Y), AxisY, Normal)
			&& ResolveWorldPlane(Transform.GetUnitAxis(EAxis::Z), AxisZ, Normal)
			&& AxisX != AxisY
			&& AxisX != AxisZ
			&& AxisY != AxisZ;
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
	bAlwaysRelevant = true; // 모든 클라이언트에게
	SetReplicateMovement(false);
	
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AGridManager::BeginPlay()
{
	Super::BeginPlay();

	RegisterWorldGridSurfaces();
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
	const FVector RelativeLocation = WorldLocation - GridOrigin;
	
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
	
	return GridOrigin + RelativeCenter;
}

void AGridManager::RegisterValidCells(const TArray<FTrapCellKey>& CellKeys)
{
	for (const FTrapCellKey& CellKey : CellKeys)
	{
		ValidCells.Add(CellKey);
	}

	RegisteredValidCellCount = ValidCells.Num();
}

void AGridManager::RegisterWorldGridSurfaces()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<UGridSurfaceComponent*> CandidateSurfaces;
	GridOrigin = GetActorLocation();
	bool bHasGridOriginX = false;
	bool bHasGridOriginY = false;
	bool bHasGridOriginZ = false;

	auto IncludeGridOrigin = [](double Value, double& OriginAxis, bool& bHasOriginAxis)
	{
		if (!bHasOriginAxis || Value < OriginAxis)
		{
			OriginAxis = Value;
			bHasOriginAxis = true;
		}
	};

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UGridSurfaceComponent*> SurfaceComponents;
		It->GetComponents(SurfaceComponents);

		for (UGridSurfaceComponent* Surface : SurfaceComponents)
		{
			const UPrimitiveComponent* TargetPrimitive = Surface ? Surface->GetTargetPrimitive() : nullptr;
			if (!Surface || !Surface->bEnabled || !TargetPrimitive
				|| !IsAxisAlignedTransform(TargetPrimitive->GetComponentTransform()))
			{
				continue;
			}

			const FBox WorldBounds = TargetPrimitive->Bounds.GetBox();
			if (!WorldBounds.IsValid)
			{
				continue;
			}

			CandidateSurfaces.Add(Surface);

			switch (Surface->SurfaceType)
			{
			case ETrapGridSurface::Floor:
			case ETrapGridSurface::Ceiling:
				IncludeGridOrigin(WorldBounds.Min.X, GridOrigin.X, bHasGridOriginX);
				IncludeGridOrigin(WorldBounds.Min.Y, GridOrigin.Y, bHasGridOriginY);
				break;

			case ETrapGridSurface::Wall:
				if (WorldBounds.GetSize().X <= WorldBounds.GetSize().Y)
				{
					IncludeGridOrigin(WorldBounds.Min.Y, GridOrigin.Y, bHasGridOriginY);
				}
				else
				{
					IncludeGridOrigin(WorldBounds.Min.X, GridOrigin.X, bHasGridOriginX);
				}
				IncludeGridOrigin(WorldBounds.Min.Z, GridOrigin.Z, bHasGridOriginZ);
				break;

			default:
				break;
			}
		}
	}

	for (UGridSurfaceComponent* Surface : CandidateSurfaces)
	{
		TArray<FTrapCellKey> CellKeys;
		if (BuildCellKeysForSurface(Surface, CellKeys))
		{
			RegisteredSurfaces.Add(Surface);
			SurfacesByPrimitive.FindOrAdd(FObjectKey(Surface->GetTargetPrimitive())).Add(Surface);
			RegisterValidCells(CellKeys);
		}
	}
}

// 부모 Primitive의 World Bounds를 양자화해 Valid Cell을 생성
bool AGridManager::BuildCellKeysForSurface(
	const UGridSurfaceComponent* Surface,
	TArray<FTrapCellKey>& OutCellKeys
) const
{
	OutCellKeys.Reset();

	const UPrimitiveComponent* TargetPrimitive = Surface ? Surface->GetTargetPrimitive() : nullptr;
	if (!Surface || !Surface->bEnabled || !TargetPrimitive
		|| !IsAxisAlignedTransform(TargetPrimitive->GetComponentTransform()))
	{
		return false;
	}

	const FBox WorldBounds = TargetPrimitive->Bounds.GetBox();
	if (!WorldBounds.IsValid)
	{
		return false;
	}

	switch (Surface->SurfaceType)
	{
	case ETrapGridSurface::Floor:
		AppendCellKeysForPlane(WorldBounds, ETrapPlaneAxis::Z, ETrapPlaneNormal::Positive, OutCellKeys);
		break;

	case ETrapGridSurface::Ceiling:
		AppendCellKeysForPlane(WorldBounds, ETrapPlaneAxis::Z, ETrapPlaneNormal::Negative, OutCellKeys);
		break;

	case ETrapGridSurface::Wall:
	{
		// 수평 Bounds 중 짧은 축을 벽 두께로 보고 양쪽 면을 모두 등록
		const FVector BoundsSize = WorldBounds.GetSize();
		const ETrapPlaneAxis WallPlaneAxis = BoundsSize.X <= BoundsSize.Y
			? ETrapPlaneAxis::X
			: ETrapPlaneAxis::Y;

		AppendCellKeysForPlane(WorldBounds, WallPlaneAxis, ETrapPlaneNormal::Negative, OutCellKeys);
		AppendCellKeysForPlane(WorldBounds, WallPlaneAxis, ETrapPlaneNormal::Positive, OutCellKeys);
		break;
	}

	default:
		break;
	}

	return !OutCellKeys.IsEmpty();
}

void AGridManager::AppendCellKeysForPlane(
	const FBox& WorldBounds,
	ETrapPlaneAxis PlaneAxis,
	ETrapPlaneNormal PlaneNormal,
	TArray<FTrapCellKey>& OutCellKeys
) const
{
	const FVector BoundsCenter = WorldBounds.GetCenter();
	const FVector BoundsSize = WorldBounds.GetSize();
	const float SafeCellSize = FMath::Max(CellSize, 1.f);

	float CenterU = 0.f;
	float CenterV = 0.f;
	float SizeU = 0.f;
	float SizeV = 0.f;
	float PlaneCoordinate = 0.f;

	switch (PlaneAxis)
	{
	case ETrapPlaneAxis::X:
		CenterU = BoundsCenter.Y - GridOrigin.Y;
		CenterV = BoundsCenter.Z - GridOrigin.Z;
		SizeU = BoundsSize.Y;
		SizeV = BoundsSize.Z;
		PlaneCoordinate = (PlaneNormal == ETrapPlaneNormal::Positive ? WorldBounds.Max.X : WorldBounds.Min.X) - GridOrigin.X;
		break;

	case ETrapPlaneAxis::Y:
		CenterU = BoundsCenter.X - GridOrigin.X;
		CenterV = BoundsCenter.Z - GridOrigin.Z;
		SizeU = BoundsSize.X;
		SizeV = BoundsSize.Z;
		PlaneCoordinate = (PlaneNormal == ETrapPlaneNormal::Positive ? WorldBounds.Max.Y : WorldBounds.Min.Y) - GridOrigin.Y;
		break;

	case ETrapPlaneAxis::Z:
	default:
		CenterU = BoundsCenter.X - GridOrigin.X;
		CenterV = BoundsCenter.Y - GridOrigin.Y;
		SizeU = BoundsSize.X;
		SizeV = BoundsSize.Y;
		PlaneCoordinate = (PlaneNormal == ETrapPlaneNormal::Positive ? WorldBounds.Max.Z : WorldBounds.Min.Z) - GridOrigin.Z;
		break;
	}

	// 허용 오차보다 큰 나머지 영역은 불완전 Cell로 보고 등록하지 않음
	const int32 CellCountU = FMath::FloorToInt((SizeU + SurfaceSizeQuantizationToleranceCm) / SafeCellSize);
	const int32 CellCountV = FMath::FloorToInt((SizeV + SurfaceSizeQuantizationToleranceCm) / SafeCellSize);
	if (CellCountU <= 0 || CellCountV <= 0)
	{
		return;
	}

	// Bounds 중심에 가장 가까운 연속 Cell 범위를 선택
	const int32 StartCellU = FMath::RoundToInt(CenterU / SafeCellSize - static_cast<float>(CellCountU) * 0.5f);
	const int32 StartCellV = FMath::RoundToInt(CenterV / SafeCellSize - static_cast<float>(CellCountV) * 0.5f);

	for (int32 UIndex = 0; UIndex < CellCountU; ++UIndex)
	{
		for (int32 VIndex = 0; VIndex < CellCountV; ++VIndex)
		{
			FTrapCellKey CellKey;
			CellKey.PlaneAxis = PlaneAxis;
			CellKey.PlaneNormal = PlaneNormal;
			CellKey.PlaneCoordinate = ToPlaneCoordinateKey(PlaneCoordinate);
			CellKey.Cell = FIntPoint(StartCellU + UIndex, StartCellV + VIndex);
			OutCellKeys.Add(CellKey);
		}
	}
}

bool AGridManager::ResolveSurfacePlaneAtLocation(
	const UGridSurfaceComponent* Surface,
	const FVector& WorldLocation,
	ETrapPlaneAxis& OutPlaneAxis,
	ETrapPlaneNormal& OutPlaneNormal,
	FVector& OutSurfaceLocation
) const
{
	const UPrimitiveComponent* TargetPrimitive = Surface ? Surface->GetTargetPrimitive() : nullptr;
	if (!Surface || !Surface->bEnabled || !TargetPrimitive
		|| !IsAxisAlignedTransform(TargetPrimitive->GetComponentTransform()))
	{
		return false;
	}

	const FBox WorldBounds = TargetPrimitive->Bounds.GetBox();
	if (!WorldBounds.IsValid)
	{
		return false;
	}

	auto IsWithinBounds = [](float Value, float Min, float Max)
	{
		return Value >= Min && Value <= Max;
	};

	OutSurfaceLocation = WorldLocation;

	switch (Surface->SurfaceType)
	{
	case ETrapGridSurface::Floor:
		if (!IsWithinBounds(WorldLocation.X, WorldBounds.Min.X, WorldBounds.Max.X)
			|| !IsWithinBounds(WorldLocation.Y, WorldBounds.Min.Y, WorldBounds.Max.Y)
			|| FMath::Abs(WorldLocation.Z - WorldBounds.Max.Z) > SurfaceHitPlaneToleranceCm)
		{
			return false;
		}

		OutPlaneAxis = ETrapPlaneAxis::Z;
		OutPlaneNormal = ETrapPlaneNormal::Positive;
		OutSurfaceLocation.Z = WorldBounds.Max.Z;
		return true;

	case ETrapGridSurface::Ceiling:
		if (!IsWithinBounds(WorldLocation.X, WorldBounds.Min.X, WorldBounds.Max.X)
			|| !IsWithinBounds(WorldLocation.Y, WorldBounds.Min.Y, WorldBounds.Max.Y)
			|| FMath::Abs(WorldLocation.Z - WorldBounds.Min.Z) > SurfaceHitPlaneToleranceCm)
		{
			return false;
		}

		OutPlaneAxis = ETrapPlaneAxis::Z;
		OutPlaneNormal = ETrapPlaneNormal::Negative;
		OutSurfaceLocation.Z = WorldBounds.Min.Z;
		return true;

	case ETrapGridSurface::Wall:
	{
		const FVector BoundsSize = WorldBounds.GetSize();
		OutPlaneAxis = BoundsSize.X <= BoundsSize.Y ? ETrapPlaneAxis::X : ETrapPlaneAxis::Y;

		if (OutPlaneAxis == ETrapPlaneAxis::X)
		{
			if (!IsWithinBounds(WorldLocation.Y, WorldBounds.Min.Y, WorldBounds.Max.Y)
				|| !IsWithinBounds(WorldLocation.Z, WorldBounds.Min.Z, WorldBounds.Max.Z))
			{
				return false;
			}

			const float NegativeDistance = FMath::Abs(WorldLocation.X - WorldBounds.Min.X);
			const float PositiveDistance = FMath::Abs(WorldLocation.X - WorldBounds.Max.X);
			if (FMath::Min(NegativeDistance, PositiveDistance) > SurfaceHitPlaneToleranceCm)
			{
				return false;
			}

			OutPlaneNormal = PositiveDistance <= NegativeDistance
				? ETrapPlaneNormal::Positive
				: ETrapPlaneNormal::Negative;
			OutSurfaceLocation.X = OutPlaneNormal == ETrapPlaneNormal::Positive ? WorldBounds.Max.X : WorldBounds.Min.X;
			return true;
		}

		if (!IsWithinBounds(WorldLocation.X, WorldBounds.Min.X, WorldBounds.Max.X)
			|| !IsWithinBounds(WorldLocation.Z, WorldBounds.Min.Z, WorldBounds.Max.Z))
		{
			return false;
		}

		const float NegativeDistance = FMath::Abs(WorldLocation.Y - WorldBounds.Min.Y);
		const float PositiveDistance = FMath::Abs(WorldLocation.Y - WorldBounds.Max.Y);
		if (FMath::Min(NegativeDistance, PositiveDistance) > SurfaceHitPlaneToleranceCm)
		{
			return false;
		}

		OutPlaneNormal = PositiveDistance <= NegativeDistance
			? ETrapPlaneNormal::Positive
			: ETrapPlaneNormal::Negative;
		OutSurfaceLocation.Y = OutPlaneNormal == ETrapPlaneNormal::Positive ? WorldBounds.Max.Y : WorldBounds.Min.Y;
		return true;
	}

	default:
		return false;
	}
}

// Hit 위치를 Trap Footprint의 시작 Cell(Anchor)로 변환
// 홀수/짝수 크기 모두 Footprint 중심이 조준 위치에 가장 가깝게 Snap
bool AGridManager::TryGetCellKeyForSurface(
	const UTrapData* TrapData,
	const UGridSurfaceComponent* Surface,
	const FVector& WorldLocation,
	FTrapCellKey& OutCellKey
) const
{
	if (!TrapData || !Surface || !Surface->bEnabled)
	{
		return false;
	}

	const FIntPoint Footprint = TrapData->FootprintCells;
	if (Footprint.X <= 0 || Footprint.Y <= 0)
	{
		return false;
	}

	ETrapPlaneAxis PlaneAxis;
	ETrapPlaneNormal PlaneNormal;
	FVector SurfaceLocation;
	if (!ResolveSurfacePlaneAtLocation(
		Surface,
		WorldLocation,
		PlaneAxis,
		PlaneNormal,
		SurfaceLocation
	))
	{
		return false;
	}

	const FTrapCellKey RequestedAnchor = WorldToTrapAnchorCellKey(
		SurfaceLocation,
		PlaneAxis,
		PlaneNormal,
		Footprint
	);
	bool bFoundValidFootprint = false;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	// Surface 경계에서는 Trap 외곽이 경계에 맞닿는 마지막 위치를 선택
	// 점유된 위치를 피해 이동하지는 X. 점유 여부는 호출자가 별도로 검사
	const int32 SearchRadiusU = FMath::Max(1, (Footprint.X + 1) / 2);
	const int32 SearchRadiusV = FMath::Max(1, (Footprint.Y + 1) / 2);

	for (int32 UOffset = -SearchRadiusU; UOffset <= SearchRadiusU; ++UOffset)
	{
		for (int32 VOffset = -SearchRadiusV; VOffset <= SearchRadiusV; ++VOffset)
		{
			FTrapCellKey CandidateAnchor = RequestedAnchor;
			CandidateAnchor.Cell += FIntPoint(UOffset, VOffset);

			TArray<FTrapCellKey> CandidateFootprint;
			GetTrapFootprintCells(TrapData, CandidateAnchor, CandidateFootprint);
			if (!AreCellsValid(CandidateFootprint))
			{
				continue;
			}

			const float DistanceSquared = FVector::DistSquared(
				GetTrapFootprintCenter(TrapData, CandidateAnchor),
				SurfaceLocation
			);
			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				OutCellKey = CandidateAnchor;
				bFoundValidFootprint = true;
			}
		}
	}

	return bFoundValidFootprint;
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
	const FVector RelativeLocation = WorldLocation - GridOrigin;

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

bool AGridManager::TryGetCellKeyForHit(
	const UTrapData* TrapData,
	const UPrimitiveComponent* HitComponent,
	const FVector& HitLocation,
	FTrapCellKey& OutCellKey
) const
{
	if (!TrapData || !HitComponent)
	{
		return false;
	}

	const TArray<TWeakObjectPtr<UGridSurfaceComponent>>* SurfaceList = SurfacesByPrimitive.Find(FObjectKey(HitComponent));
	if (!SurfaceList)
	{
		return false;
	}

	for (const TWeakObjectPtr<UGridSurfaceComponent>& SurfacePtr : *SurfaceList)
	{
		const UGridSurfaceComponent* Surface = SurfacePtr.Get();
		if (!Surface || Surface->SurfaceType != TrapData->GridSurface)
		{
			continue;
		}

		if (TryGetCellKeyForSurface(TrapData, Surface, HitLocation, OutCellKey))
		{
			return true;
		}
	}

	return false;
}

bool AGridManager::TryGetCellKeyAtWorldLocation(
	const UTrapData* TrapData,
	const FVector& WorldLocation,
	FTrapCellKey& OutCellKey
) const
{
	if (!TrapData)
	{
		return false;
	}

	for (const TWeakObjectPtr<UGridSurfaceComponent>& SurfacePtr : RegisteredSurfaces)
	{
		const UGridSurfaceComponent* Surface = SurfacePtr.Get();
		if (Surface && Surface->SurfaceType == TrapData->GridSurface
		&& TryGetCellKeyForSurface(TrapData, Surface, WorldLocation, OutCellKey))
		{
			return true;
		}
	}

	return false;
}

bool AGridManager::IsCellValid(const FTrapCellKey& CellKey) const
{
	return ValidCells.Contains(CellKey);
}

bool AGridManager::IsCellOccupied(const FTrapCellKey& CellKey) const
{
	const TWeakObjectPtr<ATrapBase>* FoundTrap = CellToTrap.Find(CellKey);
	return FoundTrap && FoundTrap->IsValid();
}

bool AGridManager::AreCellsValid(const TArray<FTrapCellKey>& CellKeys) const
{
	if (CellKeys.IsEmpty()) { return false; }
	
	for (const FTrapCellKey& CellKey : CellKeys)
	{
		if (!IsCellValid(CellKey)) { return false; }
	}
	
	return true;
}

bool AGridManager::AreCellsAvailable(const TArray<FTrapCellKey>& CellKeys) const
{
	if (!AreCellsValid(CellKeys)) { return false; }

	for (const FTrapCellKey& CellKey : CellKeys)
	{
		if (IsCellOccupied(CellKey)) { return false; }
	}

	return true;
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
