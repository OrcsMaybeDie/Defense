#pragma once

#include "CoreMinimal.h"
#include "TrapGridTypes.generated.h"


/**
- 공통 데이터 형식
- Grid 시스템 전체에서 사용하는 “주소 형식”을 정의
*/


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


/* World Trap Grid에서 Cell 하나를 식별하는 주소 */
// 특정 레벨 모듈의 ID 사용 x -> 같은 평면의 인접 모듈들은 동일한 World Grid를 공유 가능
USTRUCT(BlueprintType)
struct FTrapCellKey
{
	GENERATED_BODY()
	
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
		return PlaneAxis == Other.PlaneAxis
		&& PlaneCoordinate == Other.PlaneCoordinate
		&& PlaneNormal == Other.PlaneNormal
		&& Cell == Other.Cell;
	}
	
};


/* FTrapCellKey를 TSet 또는 TMap의 Key로 사용하기 위한 Hash 함수 */
FORCEINLINE uint32 GetTypeHash(const FTrapCellKey& Key)
{
	uint32 Hash = GetTypeHash(static_cast<uint8>(Key.PlaneAxis));
	
	Hash = HashCombineFast(Hash, GetTypeHash(Key.PlaneCoordinate));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Key.PlaneNormal)));
	Hash = HashCombineFast(Hash, GetTypeHash(Key.Cell));
	
	return Hash;
}
