#include "UI/EquipmentUI/EquipmentCardWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Equipment/EquipmentData.h"

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
