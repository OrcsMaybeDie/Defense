#include "UI/EquipmentUI/EquipmentMainMenuWidget.h"

#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"

void UEquipmentMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(
			this,
			&UEquipmentMainMenuWidget::HandleCloseClicked);
	}

	ShowCatalog();
}

void UEquipmentMainMenuWidget::NativeDestruct()
{
	if (Button_Close)
	{
		Button_Close->OnClicked.RemoveDynamic(
			this,
			&UEquipmentMainMenuWidget::HandleCloseClicked);
	}

	Super::NativeDestruct();
}

void UEquipmentMainMenuWidget::HandleCloseClicked()
{
	RemoveFromParent();
}

void UEquipmentMainMenuWidget::ShowCatalog()
{
	if (EquipmentViewSwitcher)
	{
		EquipmentViewSwitcher->SetActiveWidgetIndex(0);
	}
}