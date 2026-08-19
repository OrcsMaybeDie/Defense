// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentCardWidget.generated.h"

class UDragDropOperation;
class UEquipmentData;
class UImage;
class UTextBlock;

UCLASS()
class DEFENSE_API UEquipmentCardWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

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
