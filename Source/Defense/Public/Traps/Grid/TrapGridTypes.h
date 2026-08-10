#pragma once

#include "CoreMinimal.h"
#include "TrapGridTypes.generated.h"

class UPrimitiveComponent;


/**
- 공통 데이터 형식
- Grid 시스템 전체에서 사용하는 “주소 형식”을 정의
*/


/* 함정 설치 가능 면 */
UENUM(BlueprintType)
enum class ETrapGridSurface : uint8
{
	Floor,
	Wall,
	Ceiling
};


/* 연결된 설치 평면 하나의 Bake 결과 */
USTRUCT(BlueprintType)
struct FTrapGridRegion
{
	GENERATED_BODY()

	// Bake 결과에서 Region을 식별하는 안정 ID
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Trap Grid|Region")
	FGuid RegionId;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Trap Grid|Region")
	ETrapGridSurface SurfaceType = ETrapGridSurface::Floor;

	// Region Local (0, 0)의 월드 위치
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Trap Grid|Region")
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Trap Grid|Region")
	FVector AxisU = FVector::ForwardVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Trap Grid|Region")
	FVector AxisV = FVector::RightVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Trap Grid|Region")
	FVector Normal = FVector::UpVector;

	// Region의 U/V 전체 길이
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Trap Grid|Region")
	FVector2D Size = FVector2D::ZeroVector;

	// 이 Region을 구성하는 레벨 Primitive
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Trap Grid|Region")
	TArray<TObjectPtr<UPrimitiveComponent>> SourceComponents;
};


/* Trap Grid에서 Cell 하나를 식별하는 주소 */
USTRUCT(BlueprintType)
struct FTrapCellKey
{
	GENERATED_BODY()

	// 연결된 설치 영역 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap Grid")
	FGuid RegionId;
	
	// Region 위의 2D Cell 좌표
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap Grid")
	FIntPoint Cell = FIntPoint::ZeroValue;
	
	// 두 주소가 완전히 같은지 검사
	bool operator==(const FTrapCellKey& Other) const
	{
		return RegionId == Other.RegionId
		&& Cell == Other.Cell;
	}
	
};


/* FTrapCellKey를 TSet 또는 TMap의 Key로 사용하기 위한 Hash 함수 */
FORCEINLINE uint32 GetTypeHash(const FTrapCellKey& Key)
{
	uint32 Hash = GetTypeHash(Key.RegionId);
	
	Hash = HashCombineFast(Hash, GetTypeHash(Key.Cell));
	
	return Hash;
}
