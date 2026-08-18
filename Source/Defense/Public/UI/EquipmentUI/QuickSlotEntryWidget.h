#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuickSlotEntryWidget.generated.h"

class UDragDropOperation;
class UProfileSubsystem;
class UEquipmentData;
class UImage;
class UTextBlock;

UCLASS()
class DEFENSE_API UQuickSlotEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSlotData(int32 InSlotIndex, const UEquipmentData* EquipmentData);

	void SetSelected(bool bSelected);

	int32 GetSlotIndex() const { return SlotIndex; }

private:
	int32 SlotIndex = INDEX_NONE;

	// 프로필 접근
	UProfileSubsystem* GetProfileSubsystem() const;

protected:
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> EquipmentIconImage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> CostText;

	// 선택된 것 에디터에서 조정 - 추후
	// UPROPERTY(meta=(BindWidgetOptional))
	// TObjectPtr<UWidget> SelectedIndicator;
};
