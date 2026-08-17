#include "UI/QuickSlotEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Equipment/EquipmentData.h"
#include "Traps/TrapData.h"

void UQuickSlotEntryWidget::SetSlotData(int32 InSlotIndex, const UEquipmentData* EquipmentData)
{

	SlotIndex = InSlotIndex;

	if (EquipmentIconImage)
	{
		UTexture2D* EquipmentIcon = EquipmentData ? EquipmentData->EquipmentIcon : nullptr;

		EquipmentIconImage->SetBrushFromTexture(EquipmentIcon);
		EquipmentIconImage->SetVisibility(EquipmentIcon ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	if (CostText)
	{
		const UTrapData* TrapData = Cast<UTrapData>(EquipmentData);

		if (TrapData)
		{
			CostText->SetText(FText::AsNumber(TrapData->Cost));
			CostText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			CostText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

}

void UQuickSlotEntryWidget::SetSelected(bool bSelected)
{
	SetRenderScale(
		bSelected
		? FVector2D(1.18f, 1.18f)
		: FVector2D(1.f, 1.f));

	SetRenderTranslation(
		bSelected
		? FVector2D(0.f, -8.f)
		: FVector2D::ZeroVector);
}
