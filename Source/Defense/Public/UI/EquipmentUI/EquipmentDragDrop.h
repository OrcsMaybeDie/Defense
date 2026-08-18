#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "EquipmentDragDrop.generated.h"


class UEquipmentData;
UCLASS()
class DEFENSE_API UEquipmentDragDrop : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Transient, Category="Equipment")
	TObjectPtr<UEquipmentData> EquipmentData;
};
