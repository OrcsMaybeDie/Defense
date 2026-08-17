#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EquipmentData.generated.h"

class UTexture2D;


UCLASS(Abstract, BlueprintType)
class DEFENSE_API UEquipmentData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	FText DisplayName;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	bool bUnlockedByDefault = false;

	// Default 퀵슬롯 인덱스 (-1이면 자동 배치 X)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment", meta=(ClampMin="-1"))
	int32 DefaultQuickSlotIndex = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment")
	TObjectPtr<UTexture2D> EquipmentIcon;
};
