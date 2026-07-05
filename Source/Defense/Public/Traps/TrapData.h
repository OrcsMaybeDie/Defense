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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap")
	TSubclassOf<ATrapBase> TrapClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap")
	ETrapGridSurface GridSurface = ETrapGridSurface::Floor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap|Combat", meta=(ClampMin="0"))
	float Damage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap|Combat", meta=(ClampMin="0.1"))
	float DamageInterval = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap|Cost", meta=(ClampMin="0"))
	int32 Cost = 0;
};
