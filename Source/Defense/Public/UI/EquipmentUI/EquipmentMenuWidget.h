#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentMenuWidget.generated.h"

class UEquipmentCardWidget;
class UProfileSubsystem;
class UWrapBox;

UCLASS()
class DEFENSE_API UEquipmentMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWrapBox> EquipmentList; // card container

	UPROPERTY(EditDefaultsOnly, Category="Equipment")
	TSubclassOf<UEquipmentCardWidget> EquipmentCardWidgetClass;

	UProfileSubsystem* GetProfileSubsystem() const;

public:
	UFUNCTION(BlueprintCallable, Category="Equipment")
	void RefreshEquipmentCards();
};
