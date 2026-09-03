#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentMainMenuWidget.generated.h"

class UButton;
class UWidgetSwitcher;
class UWrapBox;

UCLASS()
class DEFENSE_API UEquipmentMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> EquipmentViewSwitcher;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWrapBox> EquipmentList;

	UFUNCTION()
	void HandleCloseClicked();

public:
	UFUNCTION(BlueprintCallable, Category="Equipment")
	void ShowCatalog();
};