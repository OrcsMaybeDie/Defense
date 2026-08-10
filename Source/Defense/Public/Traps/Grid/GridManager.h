#pragma once

#include "CoreMinimal.h"
#include "TrapGridTypes.h"
#include "UObject/ObjectKey.h"
#include "GameFramework/Actor.h"
#include "GridManager.generated.h"


class ATrapBase;
class USceneComponent;
class UTrapData;


UCLASS()
class DEFENSE_API AGridManager : public AActor
{
	GENERATED_BODY()

public:
	AGridManager();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trap Grid")
	TObjectPtr<USceneComponent> SceneRoot;

	/* Region Bake */
	UFUNCTION(CallInEditor, Category="Trap Grid|Region")
	void BuildRegions();

	UFUNCTION(CallInEditor, Category="Trap Grid|Region")
	void ValidateRegions();

	UFUNCTION(CallInEditor, Category="Trap Grid|Region")
	void ClearRegions();

	UFUNCTION(BlueprintPure, Category="Trap Grid|Region")
	int32 GetBakedRegionCount() const { return BakedRegions.Num(); }

	/* World Location ↔ Cell 변환 */
	FTrapCellKey WorldToCellKey(const FVector& WorldLocation,
		ETrapPlaneAxis PlaneAxis,
		ETrapPlaneNormal PlaneNormal
	) const; // 월드 위치와 설치면 정보 → 월드 위치를 논리적인 Cell 주소
	
	FVector CellKeyToWorldCenter(const FTrapCellKey& CellKey) const; // Cell 주소 → 월드 중심 위치
	
	/* 단일 Cell 조회 */
	bool IsCellValid(const FTrapCellKey& CellKey); // 실제 설치면 Collision이 Cell 전체를 지지함?
	bool IsCellOccupied(const FTrapCellKey& CellKey) const; // 함정이 점유됨?
	
	/* Footprint 전체 조회 */
	bool AreCellsValid(const TArray<FTrapCellKey>& CellKeys); // 함정 Footprint의 모든 Cell이 유효함?
	bool AreCellsAvailable(const TArray<FTrapCellKey>& CellKeys); // 유효하고, 비어있음?

	// Footprint를 구성하는 모든 Cell Key를 계산
	void GetTrapFootprintCells(
		const UTrapData* TrapData,
		const FTrapCellKey& AnchorCell,
		TArray<FTrapCellKey>& OutCellKeys
	) const;

	// Anchor Cell에서 Footprint 전체의 월드 중심을 계산
	FVector GetTrapFootprintCenter(
		const UTrapData* TrapData,
		const FTrapCellKey& AnchorCell
	) const;
	
	// Trap의 Footprint 중심과 설치면 방향으로 최종 Actor Transform을 계산
	FTransform GetTrapFootprintTransform(
		const UTrapData* TrapData,
		const FTrapCellKey& AnchorCell
	) const;

	/* 점유와 해제 (Server에서만 확정) */
	bool TryOccupyCells(const TArray<FTrapCellKey>& CellKeys, ATrapBase* Trap); //  모든 셀이 유효, 비어 있을 때 점유
	void ReleaseTrap(ATrapBase* Trap); // 함정이 차지하던 모든 Cell 해제
	void RegisterClientOccupiedCells(ATrapBase* Trap, const TArray<FTrapCellKey>& CellKeys);
	void ReleaseClientTrap(ATrapBase* Trap);
	
	bool IsTrapRegistered(const ATrapBase* Trap) const;

	bool TryGetCellKeyForHit(
		const UTrapData* TrapData,
		const FHitResult& Hit,
		FTrapCellKey& OutCellKey
	);

	bool IsTrapSurfaceCompatible(
		const UTrapData* TrapData,
		const FTrapCellKey& CellKey
	) const;

	float GetCellSize() const { return CellSize; }

	UFUNCTION(BlueprintPure, Category="Trap Grid")
	int32 GetCachedValidCellCount() const { return ValidCells.Num(); }

	UFUNCTION(BlueprintPure, Category="Trap Grid")
	int32 GetCachedInvalidCellCount() const { return InvalidCells.Num(); }
	

protected:
	
	// 모든 설치면이 공유하는 Snap 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid", meta=(ClampMin="1.0"))
	float CellSize = 100.f;

	// Editor에서 생성한 연결 설치 영역
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Trap Grid|Region")
	TArray<FTrapGridRegion> BakedRegions;

	// 서로 맞닿은 Surface로 판단할 최대 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid|Region", meta=(ClampMin="0.0"))
	float RegionConnectionTolerance = 2.f;

	// 점 접촉만으로 Region이 합쳐지는 것을 막는 최소 접촉 길이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid|Region", meta=(ClampMin="0.0"))
	float RegionMinimumContactLength = 1.f;

	// Collision 검사 결과 캐시. 두 Set에 모두 없으면 아직 검사하지 않은 Cell
	TSet<FTrapCellKey> ValidCells;
	TSet<FTrapCellKey> InvalidCells;
	
	// 이 Cell에 어떤 함정이 있는가? (Trap도 살아 있음? -> 점유 상태)
	TMap<FTrapCellKey, TWeakObjectPtr<ATrapBase>> CellToTrap;
	// 이 함정은 어떤 Cell들을 차지하는가? → 판매 시 직사각형 Footprint 전체를 해제하는 데 사용
	TMap<FObjectKey, TArray<FTrapCellKey>> TrapToCells;
	
	UFUNCTION()
	void HandleTrapDestroyed(AActor* DestroyedActor);

	// 아직 검사하지 않은 Cell을 실제 WorldStatic Collision으로 검사
	bool EvaluateCellStaticValidity(const FTrapCellKey& CellKey) const;
	bool IsPlaceableHit(const FHitResult& Hit, ETrapGridSurface SurfaceType) const;

	// 조준 위치 주변에서 가장 가까운 유효 Footprint Anchor 선택
	bool FindClosestValidAnchor(
		const UTrapData* TrapData,
		const FVector& HitLocation,
		ETrapPlaneAxis PlaneAxis,
		ETrapPlaneNormal PlaneNormal,
		FTrapCellKey& OutCellKey
	);

	FTrapCellKey WorldToTrapAnchorCellKey(
		const FVector& WorldLocation,
		ETrapPlaneAxis PlaneAxis,
		ETrapPlaneNormal PlaneNormal,
		const FIntPoint& FootprintCells
	) const;

	// Cell 외곽 검사 시 경계선에서 안쪽으로 들어올 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid|Collision", meta=(ClampMin="0.0"))
	float CellProbeInset = 2.f;

	// 설치면 바깥에서 안쪽으로 검사할 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid|Collision", meta=(ClampMin="1.0"))
	float SurfaceProbeDistance = 50.f;

	// 인접 모듈 설치면 높이 차이 허용 범위
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid|Collision", meta=(ClampMin="0.0"))
	float SurfacePlaneTolerance = 1.f;
};
