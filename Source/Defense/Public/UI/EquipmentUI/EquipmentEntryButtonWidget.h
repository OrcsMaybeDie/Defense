#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentEntryButtonWidget.generated.h"

class UButton;
class UEquipmentMainMenuWidget;

UCLASS()
class DEFENSE_API UEquipmentEntryButtonWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Equip;

	UPROPERTY(EditDefaultsOnly, Category="Equipment")
	TSubclassOf<UEquipmentMainMenuWidget> EquipmentMenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UEquipmentMainMenuWidget> EquipmentMenuWidget;

	UFUNCTION()
	void HandleEquipmentButtonClicked();
};
