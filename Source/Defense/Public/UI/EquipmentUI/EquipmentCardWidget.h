// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentCardWidget.generated.h"

class UEquipmentData;
class UImage;
class UTextBlock;

UCLASS()
class DEFENSE_API UEquipmentCardWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> EquipmentIconImage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> EquipmentNameText;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Equipment")
	TObjectPtr<UEquipmentData> EquipmentData;

public:
	UFUNCTION(BlueprintCallable, Category="Equipment")
	void SetEquipmentData(UEquipmentData* InEquipmentData);

	UFUNCTION(BlueprintPure, Category="Equipment")
	UEquipmentData* GetEquipmentData() const { return EquipmentData; }
};
