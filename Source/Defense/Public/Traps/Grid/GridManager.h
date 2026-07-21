#pragma once

#include "CoreMinimal.h"
#include "TrapGridTypes.h"
#include "UObject/ObjectKey.h"
#include "GameFramework/Actor.h"
#include "GridManager.generated.h"


class ATrapBase;
class USceneComponent;
class UGridSurfaceComponent;
class UPrimitiveComponent;
class UTrapData;


UCLASS()
class DEFENSE_API AGridManager : public AActor
{
	GENERATED_BODY()

public:
	AGridManager();

	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trap Grid")
	TObjectPtr<USceneComponent> SceneRoot;

	/* World Location ↔ Cell 변환 */
	FTrapCellKey WorldToCellKey(const FVector& WorldLocation,
		ETrapPlaneAxis PlaneAxis,
		ETrapPlaneNormal PlaneNormal
	) const; // 월드 위치와 설치면 정보 → 월드 위치를 논리적인 Cell 주소
	
	FVector CellKeyToWorldCenter(const FTrapCellKey& CellKey) const; // Cell 주소 → 월드 중심 위치
	
	/* 유효 Cell을 등록 */
	void RegisterValidCells(const TArray<FTrapCellKey>& CellKeys);
	
	/* 단일 Cell 조회 */
	bool IsCellValid(const FTrapCellKey& CellKey) const; // 레벨이 제공한 설치 가능 Cell인가?
	bool IsCellOccupied(const FTrapCellKey& CellKey) const; // 함정이 점유됨?
	
	/* Footprint 전체 조회 */
	bool AreCellsValid(const TArray<FTrapCellKey>& CellKeys) const; // 함정 Footprint의 모든 Cell이 유효함?
	bool AreCellsAvailable(const TArray<FTrapCellKey>& CellKeys) const; // 유효하고, 비어있음?
	void GetTrapFootprintCells(const FTrapCellKey& AnchorCell, TArray<FTrapCellKey>& OutCellKeys) const;
	FVector GetTrapFootprintCenter(const FTrapCellKey& AnchorCell) const;
	
	
	/* 점유와 해제 (Server에서만 확정) */
	bool TryOccupyCells(const TArray<FTrapCellKey>& CellKeys, ATrapBase* Trap); //  모든 셀이 유효, 비어 있을 때 점유
	void ReleaseTrap(ATrapBase* Trap); // 함정이 차지하던 모든 Cell 해제
	void RegisterClientOccupiedCells(ATrapBase* Trap, const TArray<FTrapCellKey>& CellKeys);
	void ReleaseClientTrap(ATrapBase* Trap);
	
	bool IsTrapRegistered(const ATrapBase* Trap) const;

	bool TryGetCellKeyForHit(
		const UTrapData* TrapData,
		const UPrimitiveComponent* HitComponent,
		const FVector& HitLocation,
		FTrapCellKey& OutCellKey
	) const;

	bool TryGetCellKeyAtWorldLocation(
		const UTrapData* TrapData,
		const FVector& WorldLocation,
		FTrapCellKey& OutCellKey
	) const;

	float GetCellSize() const { return CellSize; }
	float GetTrapFootprintSizeCm() const { return CellSize * TrapFootprintCellCount; }

	UFUNCTION(BlueprintPure, Category="Trap Grid")
	int32 GetRegisteredValidCellCount() const { return RegisteredValidCellCount; }
	

protected:
	
	// 이 Actor의 Location이 World Grid의 원점이 됨
	// Rotation은 (0,0,0), Scale은 (1,1,1) 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid", meta=(ClampMin="1.0"))
	float CellSize = 100.f;

	// 모든 Trap은 200cm × 200cm 정사각형
	static constexpr int32 TrapFootprintCellCount = 2;
	
	// 설치 가능한 모든 Cell 주소
	// 모듈 경계가 달라도 동일한 주소라면 같은 Cell. Module ID가 Key에 없기 때문에 경계 설치가 가능
	TSet<FTrapCellKey> ValidCells; 
	TArray<TWeakObjectPtr<UGridSurfaceComponent>> RegisteredSurfaces;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Trap Grid")
	int32 RegisteredValidCellCount = 0;
	
	// 이 Cell에 어떤 함정이 있는가? (Trap도 살아 있음? -> 점유 상태)
	TMap<FTrapCellKey, TWeakObjectPtr<ATrapBase>> CellToTrap;
	// 이 함정은 어떤 Cell들을 차지하는가? → 판매 시 2×2, 3×3 Footprint 전체를 해제하는 데 사용
	TMap<FObjectKey, TArray<FTrapCellKey>> TrapToCells;
	
	UFUNCTION()
	void HandleTrapDestroyed(AActor* DestroyedActor);

	void RegisterWorldGridSurfaces();
	
	// GridSurface 하나를 Valid Cell 목록으로 등록
	bool BuildCellKeysForSurface(
		const UGridSurfaceComponent* Surface,
		TArray<FTrapCellKey>& OutCellKeys
	) const;

	// Hit 위치를 Trap의 2×2 Footprint anchor로 해석 - 조준 / 설치 요청
	bool TryGetCellKeyForSurface(
		const UGridSurfaceComponent* Surface,
		const FVector& WorldLocation,
		FTrapCellKey& OutCellKey
	) const;

	FTrapCellKey WorldToTrapAnchorCellKey(
		const FVector& WorldLocation,
		ETrapPlaneAxis PlaneAxis,
		ETrapPlaneNormal PlaneNormal
	) const;
};
