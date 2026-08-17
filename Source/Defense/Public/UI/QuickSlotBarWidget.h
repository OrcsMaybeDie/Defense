#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuickSlotBarWidget.generated.h"


class UHorizontalBox;
class UQuickSlotEntryWidget;
class UTextBlock;
class ULoadoutComponent;
class UEquipmentData;
class ADefensePlayerState;


UCLASS()
class DEFENSE_API UQuickSlotBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
private:
	UPROPERTY()
	TObjectPtr<ADefensePlayerState> BoundPlayerState;
	
	UPROPERTY()
	TObjectPtr<ULoadoutComponent> BoundLoadoutComp;
	
	UPROPERTY()
	TArray<TObjectPtr<UQuickSlotEntryWidget>> SlotEntries;
	
	UFUNCTION()
	void HandleCoinChanged(int32 NewCoin);
	
	UFUNCTION()
	void HandleSelectedEquipChanged(int32 SelectedSlotIdx, UEquipmentData* SelectedEquipment);
	
	void RefreshCoin();
	void RefreshSlots();
	void RefreshSelectedSlot();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> CoinText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UHorizontalBox> SlotContainer;

	UPROPERTY(EditDefaultsOnly, Category="QuickSlot")
	TSubclassOf<UQuickSlotEntryWidget> SlotEntryWidgetClass;

public:
	void BindPlayerState(ADefensePlayerState* InPlayerState);
	void BindLoadoutComponent(ULoadoutComponent* InLoadoutComp);
};
