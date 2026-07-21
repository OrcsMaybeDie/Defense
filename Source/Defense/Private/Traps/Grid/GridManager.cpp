#include "Traps/Grid/GridManager.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Traps/TrapBase.h"
#include "EngineUtils.h"
#include "Traps/Grid/GridSurfaceComponent.h"


namespace
{
	constexpr float PlaneCoordinateUnitCm = 0.1f;
	constexpr float AxisAlignedThreshold = 0.999f;
	constexpr float GridAlignmentToleranceCm = 0.1f;
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

	bool IsSurfaceTypeCompatible(const ETrapGridSurface SurfaceType, const ETrapPlaneAxis PlaneAxis, const ETrapPlaneNormal PlaneNormal)
	{
		switch (SurfaceType)
		{
		case ETrapGridSurface::Floor:
			return PlaneAxis == ETrapPlaneAxis::Z && PlaneNormal == ETrapPlaneNormal::Positive;
		case ETrapGridSurface::Ceiling:
			return PlaneAxis == ETrapPlaneAxis::Z && PlaneNormal == ETrapPlaneNormal::Negative;
		case ETrapGridSurface::Wall:
			return PlaneAxis == ETrapPlaneAxis::X || PlaneAxis == ETrapPlaneAxis::Y;
		default:
			return false;
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

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UGridSurfaceComponent*> SurfaceComponents;
		It->GetComponents(SurfaceComponents);

		for (UGridSurfaceComponent* Surface : SurfaceComponents)
		{
			TArray<FTrapCellKey> CellKeys;
			if (BuildCellKeysForSurface(Surface, CellKeys))
			{
				RegisteredSurfaces.Add(Surface);
				RegisterValidCells(CellKeys);
			}
		}
	}
}

// GridSurface 하나를 Valid Cell 목록으로 등록
// (Surface 하나를 월드 Grid의 FTrapCellKey 배열로 변환)
bool AGridManager::BuildCellKeysForSurface(
	const UGridSurfaceComponent* Surface,
	TArray<FTrapCellKey>& OutCellKeys
) const
{
	OutCellKeys.Reset();

	if (!Surface || !Surface->bEnabled)
	{
		return false;
	}

	// Scale이 (1,1,1)이 아니면 실패
	const FTransform SurfaceTransform = Surface->GetComponentTransform();
	if (!SurfaceTransform.GetScale3D().Equals(FVector::OneVector, KINDA_SMALL_NUMBER))
	{
		return false;
	}

	// X/Y 방향 Cell 개수 계산
	const float SafeCellSize = FMath::Max(CellSize, 1.f);
	const FVector2D SurfaceSize = Surface->SurfaceSizeCm;
	const float CellCountUFloat = SurfaceSize.X / SafeCellSize;
	const float CellCountVFloat = SurfaceSize.Y / SafeCellSize;
	const int32 CellCountU = FMath::RoundToInt(CellCountUFloat);
	const int32 CellCountV = FMath::RoundToInt(CellCountVFloat);

	// 정수로 나누어떨어지지 않으면 실패 (반쪽 Cell -> 현재 Grid 규칙으로 정의 X)
	if (
		CellCountU <= 0
		|| CellCountV <= 0
		|| !FMath::IsNearlyEqual(CellCountUFloat, static_cast<float>(CellCountU))
		|| !FMath::IsNearlyEqual(CellCountVFloat, static_cast<float>(CellCountV))
	)
	{
		return false;
	}
	
	// Surface local +Z 검사 (Floor/Wall/Ceiling 평면 축, 바깥 방향 결정)
	// SurfaceType과 실제 회전이 일치하는지 검사
	ETrapPlaneAxis PlaneAxis;
	ETrapPlaneNormal PlaneNormal;
	if (!ResolveWorldPlane(SurfaceTransform.GetUnitAxis(EAxis::Z), PlaneAxis, PlaneNormal)
		|| !IsSurfaceTypeCompatible(Surface->SurfaceType, PlaneAxis, PlaneNormal))
	{
		return false;
	}

	// Surface의 가로·세로 방향 - Wall에서도 같은 코드로 Cell을 만들 수 있는 핵심
	const FVector WorldAxisU = SurfaceTransform.GetUnitAxis(EAxis::X);
	const FVector WorldAxisV = SurfaceTransform.GetUnitAxis(EAxis::Y);

	// 각 X/Y Cell 순회
	for (int32 UIndex = 0; UIndex < CellCountU; ++UIndex)
	{
		const float OffsetU = -SurfaceSize.X * 0.5f + (UIndex + 0.5f) * SafeCellSize;

		for (int32 VIndex = 0; VIndex < CellCountV; ++VIndex)
		{
			const float OffsetV = -SurfaceSize.Y * 0.5f + (VIndex + 0.5f) * SafeCellSize;
			
			// Cell 중심의 월드 좌표 계산
			const FVector WorldCellCenter = SurfaceTransform.GetLocation()
				+ WorldAxisU * OffsetU
				+ WorldAxisV * OffsetV;

			// 월드 Grid 주소 생성
			const FTrapCellKey CellKey = WorldToCellKey(
				WorldCellCenter,
				PlaneAxis,
				PlaneNormal
			);

			// CellKey를 다시 월드 중심으로 바꿨을 때, 원래 계산한 Cell 중심과 일치하지 않으면 실패 -> GridSurface가 100cm 월드 Grid에 정확히 정렬되었는가? 를 보장
			if (!CellKeyToWorldCenter(CellKey).Equals(WorldCellCenter, GridAlignmentToleranceCm))
			{
				OutCellKeys.Reset();
				return false;
			}

			// OutCellKeys에 추가
			OutCellKeys.Add(CellKey);
		}
	}

	return true;
}

// Hit 위치를 Trap의 2×2 Footprint anchor로 해석
// (Trap의 2×2 Footprint 시작 Cell로 변환)
bool AGridManager::TryGetCellKeyForSurface(
	const UGridSurfaceComponent* Surface,
	const FVector& WorldLocation,
	FTrapCellKey& OutCellKey
) const
{
	if (!Surface || !Surface->bEnabled)
	{
		return false;
	}

	// Hit된 월드 위치를 Surface의 Local 좌표로 변환
	const FTransform SurfaceTransform = Surface->GetComponentTransform();
	const FVector LocalLocation = SurfaceTransform.InverseTransformPosition(WorldLocation);
	
	// 정말 이 GridSurface 내부를 조준했는가?를 판단
	const FVector2D SurfaceHalfSize = Surface->SurfaceSizeCm * 0.5f;

	if (
		FMath::Abs(LocalLocation.Z) > SurfaceHitPlaneToleranceCm
		|| FMath::Abs(LocalLocation.X) > SurfaceHalfSize.X
		|| FMath::Abs(LocalLocation.Y) > SurfaceHalfSize.Y
	)
	{
		return false;
	}

	ETrapPlaneAxis PlaneAxis;
	ETrapPlaneNormal PlaneNormal;
	if (!ResolveWorldPlane(SurfaceTransform.GetUnitAxis(EAxis::Z), PlaneAxis, PlaneNormal))
	{
		return false;
	}

	const FVector SurfaceLocation = SurfaceTransform.TransformPosition(
		FVector(LocalLocation.X, LocalLocation.Y, 0.f)
	);
	const FTrapCellKey RequestedAnchor = WorldToTrapAnchorCellKey(SurfaceLocation, PlaneAxis, PlaneNormal);
	bool bFoundValidFootprint = false;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	// Surface 경계에서는 Trap 외곽이 경계에 맞닿는 마지막 2x2 위치를 선택
	// 점유된 위치를 피해 이동하지는 X. 점유 여부는 호출자가 별도로 검사
	for (int32 UOffset = -1; UOffset <= 1; ++UOffset)
	{
		for (int32 VOffset = -1; VOffset <= 1; ++VOffset)
		{
			FTrapCellKey CandidateAnchor = RequestedAnchor;
			CandidateAnchor.Cell += FIntPoint(UOffset, VOffset);

			TArray<FTrapCellKey> CandidateFootprint;
			GetTrapFootprintCells(CandidateAnchor, CandidateFootprint);
			if (!AreCellsValid(CandidateFootprint))
			{
				continue;
			}

			const float DistanceSquared = FVector::DistSquared(GetTrapFootprintCenter(CandidateAnchor), SurfaceLocation);
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
	ETrapPlaneNormal PlaneNormal
) const
{
	FTrapCellKey AnchorCell = WorldToCellKey(WorldLocation, PlaneAxis, PlaneNormal);
	const float SafeCellSize = FMath::Max(GetCellSize(), 1.f);
	const FVector RelativeLocation = WorldLocation - GetActorLocation();
	const int32 HalfFootprintCellCount = TrapFootprintCellCount / 2;

	switch (PlaneAxis)
	{
	case ETrapPlaneAxis::X:
		AnchorCell.Cell = FIntPoint(
			FMath::RoundToInt(RelativeLocation.Y / SafeCellSize) - HalfFootprintCellCount,
			FMath::RoundToInt(RelativeLocation.Z / SafeCellSize) - HalfFootprintCellCount
		);
		break;

	case ETrapPlaneAxis::Y:
		AnchorCell.Cell = FIntPoint(
			FMath::RoundToInt(RelativeLocation.X / SafeCellSize) - HalfFootprintCellCount,
			FMath::RoundToInt(RelativeLocation.Z / SafeCellSize) - HalfFootprintCellCount
		);
		break;

	case ETrapPlaneAxis::Z:
	default:
		AnchorCell.Cell = FIntPoint(
			FMath::RoundToInt(RelativeLocation.X / SafeCellSize) - HalfFootprintCellCount,
			FMath::RoundToInt(RelativeLocation.Y / SafeCellSize) - HalfFootprintCellCount
		);
		break;
	}

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

	for (const TWeakObjectPtr<UGridSurfaceComponent>& SurfacePtr : RegisteredSurfaces)
	{
		const UGridSurfaceComponent* Surface = SurfacePtr.Get();
		if (!Surface || Surface->SurfaceType != TrapData->GridSurface)
		{
			continue;
		}

		if (Surface->GetAttachParent() == HitComponent
			&& TryGetCellKeyForSurface(Surface, HitLocation, OutCellKey))
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
			&& TryGetCellKeyForSurface(Surface, WorldLocation, OutCellKey))
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
	const FTrapCellKey& AnchorCell,
	TArray<FTrapCellKey>& OutCellKeys
) const
{
	OutCellKeys.Reset(TrapFootprintCellCount * TrapFootprintCellCount);

	for (int32 UIndex = 0; UIndex < TrapFootprintCellCount; ++UIndex)
	{
		for (int32 VIndex = 0; VIndex < TrapFootprintCellCount; ++VIndex)
		{
			FTrapCellKey CellKey = AnchorCell;
			CellKey.Cell += FIntPoint(UIndex, VIndex);
			OutCellKeys.Add(CellKey);
		}
	}
}

FVector AGridManager::GetTrapFootprintCenter(const FTrapCellKey& AnchorCell) const
{
	FVector Center = CellKeyToWorldCenter(AnchorCell);
	const float Offset = (TrapFootprintCellCount - 1) * GetCellSize() * 0.5f;

	switch (AnchorCell.PlaneAxis)
	{
	case ETrapPlaneAxis::X:
		Center += FVector(0.f, Offset, Offset);
		break;
	case ETrapPlaneAxis::Y:
		Center += FVector(Offset, 0.f, Offset);
		break;
	case ETrapPlaneAxis::Z:
	default:
		Center += FVector(Offset, Offset, 0.f);
		break;
	}

	return Center;
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

