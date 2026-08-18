#include "UI/EquipmentUI/EquipmentCardWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "InputCoreTypes.h"
#include "UI/EquipmentUI/EquipmentDragDrop.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Equipment/EquipmentData.h"

FReply UEquipmentCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!EquipmentData || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	// DetectDragIfPressed 등록
	return UWidgetBlueprintLibrary::DetectDragIfPressed(
		InMouseEvent,
		this,
		EKeys::LeftMouseButton).NativeReply;

}

void UEquipmentCardWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!EquipmentData)
	{
		return;
	}

	UEquipmentDragDrop* DragOperation = NewObject<UEquipmentDragDrop>(this);

	if (!DragOperation)
	{
		return;
	}

	DragOperation->EquipmentData = EquipmentData;
	DragOperation->Pivot = EDragPivot::MouseDown;

	UEquipmentCardWidget* DragVisual = CreateWidget<UEquipmentCardWidget>(GetOwningPlayer(), GetClass());

	if (DragVisual)
	{
		DragVisual->SetEquipmentData(EquipmentData);
		DragVisual->SetRenderOpacity(0.75f);

		DragOperation->DefaultDragVisual = DragVisual;
	}

	OutOperation = DragOperation;
}

void UEquipmentCardWidget::SetEquipmentData(UEquipmentData* InEquipmentData)
{
	EquipmentData = InEquipmentData;

	if (EquipmentNameText)
	{
		EquipmentNameText->SetText(EquipmentData ? EquipmentData->DisplayName : FText::GetEmpty());
	}

	if (!EquipmentIconImage)
	{
		return;
	}

	if (EquipmentData && EquipmentData->EquipmentIcon)
	{
		EquipmentIconImage->SetBrushFromTexture(EquipmentData->EquipmentIcon);

		EquipmentIconImage->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		EquipmentIconImage->SetVisibility(ESlateVisibility::Hidden);
	}
}
