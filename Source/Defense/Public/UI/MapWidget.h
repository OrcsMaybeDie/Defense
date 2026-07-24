// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "MapWidget.generated.h"

class UImage;
class UMapConfigData;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMapWidgetSelected, UMapConfigData*, MapConfigData);

UCLASS()
class DEFENSE_API UMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	void SetupMapWidget(UMapConfigData* InMapConfigData, int32 InMapIndex, bool bInCanSelect);
	void SetSelected(bool bInSelected);
	void SetCanSelect(bool bInCanSelect);
	UMapConfigData* GetMapConfigData() const { return MapConfigData; }

	UPROPERTY(BlueprintAssignable, Category="Map")
	FOnMapWidgetSelected OnMapWidgetSelected;

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> TextMapNum;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> ImageEdge;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> ImageMap;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> ImageNotReady;

	UPROPERTY()
	TObjectPtr<UMapConfigData> MapConfigData;

	UPROPERTY()
	bool bCanSelect = false;
};
