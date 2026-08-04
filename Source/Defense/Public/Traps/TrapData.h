#pragma once

#include "CoreMinimal.h"
#include "Equipment/EquipmentData.h"
#include "TrapData.generated.h"

class ATrapBase;
UENUM(BlueprintType)
enum class ETrapGridSurface : uint8
{
	Floor,
	Wall,
	Ceiling
};

UCLASS(BlueprintType)
class DEFENSE_API UTrapData : public UEquipmentData
{
	GENERATED_BODY()

public:
	// 서버가 Spawn할 실제 함정 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap")
	TSubclassOf<ATrapBase> TrapClass;

	// 함정 설치 가능 면
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap")
	ETrapGridSurface GridSurface = ETrapGridSurface::Floor;

	// 함정이 점유하는 Grid Cell 개수 (기본 2×2)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap", meta=(ClampMin="1"))
	FIntPoint FootprintCells = FIntPoint(2, 2);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap|Combat", meta=(ClampMin="0"))
	float Damage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap|Combat", meta=(ClampMin="0.1"))
	float DamageInterval = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap|Cost", meta=(ClampMin="0"))
	int32 Cost = 0;
};
