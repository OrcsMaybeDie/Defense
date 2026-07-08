#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadoutBarWidget.generated.h"


class UTextBlock;
class UImage;
class ULoadoutComponent;
class UEquipmentData;
class ADefensePlayerState;


UCLASS()
class DEFENSE_API ULoadoutBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
private:
	UPROPERTY()
	TObjectPtr<ADefensePlayerState> BoundPlayerState;
	
	UPROPERTY()
	TObjectPtr<ULoadoutComponent> BoundLoadoutComp;
	
	UPROPERTY()
	TArray<TObjectPtr<UImage>> SlotImages;
	
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotCostTexts;
	
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

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> SlotImage_0;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> SlotImage_1;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> SlotImage_2;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> SlotImage_3;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotCostText_0;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotCostText_1;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotCostText_2;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotCostText_3;
	
public:
	void BindPlayerState(ADefensePlayerState* InPlayerState);
	void BindLoadoutComponent(ULoadoutComponent* InLoadoutComp);
};
