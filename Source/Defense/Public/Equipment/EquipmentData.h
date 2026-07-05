#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EquipmentData.generated.h"


UCLASS(Abstract, BlueprintType)
class DEFENSE_API UEquipmentData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	FText DisplayName;
};
