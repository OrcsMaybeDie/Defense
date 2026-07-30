// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/MapWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameManager/Data/MapConfigData.h"

void UMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetSelected(false);
}

FReply UMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bCanSelect && MapConfigData && !MapConfigData->bNotReady)
	{
		OnMapWidgetSelected.Broadcast(MapConfigData);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UMapWidget::SetupMapWidget(UMapConfigData* InMapConfigData, int32 InMapIndex, bool bInCanSelect)
{
	MapConfigData = InMapConfigData;
	bCanSelect = bInCanSelect;

	if (TextMapNum)
	{
		TextMapNum->SetText(FText::AsNumber(InMapIndex + 1));
	}

	if (ImageMap && MapConfigData && !MapConfigData->Thumbnail.IsNull())
	{
		if (UTexture2D* ThumbnailTexture = MapConfigData->Thumbnail.LoadSynchronous())
		{
			ImageMap->SetBrushFromTexture(ThumbnailTexture);
		}
	}

	if (ImageNotReady)
	{
		const bool bNotReady = MapConfigData && MapConfigData->bNotReady;
		ImageNotReady->SetVisibility(bNotReady ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	SetSelected(false);
}

void UMapWidget::SetSelected(bool bInSelected)
{
	if (ImageEdge)
	{
		const bool bCanShowSelected = bInSelected && MapConfigData && !MapConfigData->bNotReady;
		ImageEdge->SetVisibility(bCanShowSelected ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

void UMapWidget::SetCanSelect(bool bInCanSelect)
{
	bCanSelect = bInCanSelect;
}
