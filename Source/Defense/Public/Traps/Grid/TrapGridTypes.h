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


/* 설치면에 수직인 월드 축 */
UENUM(BlueprintType)
enum class ETrapPlaneAxis : uint8
{
	X, // X 방향을 바라보는 벽 (X축에 수직인 벽)
	Y, // Y 방향을 바라보는 벽 (Y축에 수직인 벽)
	Z  // 바닥 또는 천장
};


/* 설치면이 보는 방향 */
UENUM(BlueprintType)
enum class ETrapPlaneNormal : uint8
{
	Negative, // Z + Negative → 아래를 바라보는 천장
	Positive  // Z + Positive → 위를 바라보는 바닥
	// 벽 → X/Y + Positive/Negative 조합으로 네 방향을 표현
};


/* Trap Grid에서 Cell 하나를 식별하는 주소 */
USTRUCT(BlueprintType)
struct FTrapCellKey
{
	GENERATED_BODY()

	// 연결된 설치 영역 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap Grid")
	FGuid RegionId;
	
	// 설치면에 수직인 월드 축
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap Grid")
	ETrapPlaneAxis PlaneAxis = ETrapPlaneAxis::Z;
	
	// GridManager 위치를 기준으로 한 설치 평면 좌표. 단위는 0.1cm(1mm).
	// 메시 윗면처럼 정수 cm가 아닌 설치면도 구분한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap Grid")
	int32 PlaneCoordinate = 0;
	
	// 설치면이 바라보는 방향
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap Grid")
	ETrapPlaneNormal PlaneNormal = ETrapPlaneNormal::Positive;
	
	// 해당 평면 위의 2D Cell 좌표 (Z 평면 → World X/Y)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap Grid")
	FIntPoint Cell = FIntPoint::ZeroValue;
	
	// 두 주소가 완전히 같은지 검사
	bool operator==(const FTrapCellKey& Other) const
	{
		return RegionId == Other.RegionId
		&& PlaneAxis == Other.PlaneAxis
		&& PlaneCoordinate == Other.PlaneCoordinate
		&& PlaneNormal == Other.PlaneNormal
		&& Cell == Other.Cell;
	}
	
};


/* FTrapCellKey를 TSet 또는 TMap의 Key로 사용하기 위한 Hash 함수 */
FORCEINLINE uint32 GetTypeHash(const FTrapCellKey& Key)
{
	uint32 Hash = GetTypeHash(Key.RegionId);
	
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Key.PlaneAxis)));
	Hash = HashCombineFast(Hash, GetTypeHash(Key.PlaneCoordinate));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Key.PlaneNormal)));
	Hash = HashCombineFast(Hash, GetTypeHash(Key.Cell));
	
	return Hash;
}
