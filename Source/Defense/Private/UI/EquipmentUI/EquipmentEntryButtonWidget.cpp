#include "UI/EquipmentUI/EquipmentEntryButtonWidget.h"

#include "Components/Button.h"
#include "UI/EquipmentUI/EquipmentMainMenuWidget.h"

void UEquipmentEntryButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Equip)
	{
		Button_Equip->OnClicked.AddUniqueDynamic(
			this,
			&UEquipmentEntryButtonWidget::HandleEquipmentButtonClicked);
	}
}

void UEquipmentEntryButtonWidget::NativeDestruct()
{
	if (Button_Equip)
	{
		Button_Equip->OnClicked.RemoveDynamic(
			this,
			&UEquipmentEntryButtonWidget::HandleEquipmentButtonClicked);
	}

	if (EquipmentMenuWidget)
	{
		EquipmentMenuWidget->RemoveFromParent();
		EquipmentMenuWidget = nullptr;
	}

	Super::NativeDestruct();
}

void UEquipmentEntryButtonWidget::HandleEquipmentButtonClicked()
{
	if (EquipmentMenuWidget && EquipmentMenuWidget->IsInViewport())
	{
		EquipmentMenuWidget->RemoveFromParent();
		return;
	}

	if (!EquipmentMenuWidget)
	{
		if (!EquipmentMenuWidgetClass.Get())
		{
			return;
		}

		EquipmentMenuWidget = CreateWidget<UEquipmentMainMenuWidget>(GetOwningPlayer(), EquipmentMenuWidgetClass);
	}

	if (EquipmentMenuWidget)
	{
		EquipmentMenuWidget->SetVisibility(ESlateVisibility::Visible);
		EquipmentMenuWidget->AddToViewport(100);
	}
}
