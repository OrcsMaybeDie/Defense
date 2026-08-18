#include "UI/EquipmentUI/EquipmentMenuWidget.h"

#include "Components/WrapBox.h"
#include "Engine/GameInstance.h"
#include "Equipment/EquipmentData.h"
#include "Profile/ProfileSubsystem.h"
#include "UI/EquipmentUI/EquipmentCardWidget.h"

void UEquipmentMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshEquipmentCards();
}

void UEquipmentMenuWidget::RefreshEquipmentCards()
{
	if (!EquipmentList || !EquipmentCardWidgetClass.Get())
	{
		return;
	}

	EquipmentList->ClearChildren();

	UProfileSubsystem* ProfileSubsystem = GetProfileSubsystem();

	if (!ProfileSubsystem)
	{
		return;
	}

	for (UEquipmentData* EquipData : ProfileSubsystem->GetUnlockedEquipmentData())
	{
		if (!EquipData)
		{
			continue;
		}

		UEquipmentCardWidget* EquipmentCard = CreateWidget<UEquipmentCardWidget>(GetOwningPlayer(), EquipmentCardWidgetClass);

		if (!EquipmentCard)
		{
			continue;
		}

		EquipmentCard->SetEquipmentData(EquipData);
		EquipmentList->AddChildToWrapBox(EquipmentCard);
	}
}

UProfileSubsystem* UEquipmentMenuWidget::GetProfileSubsystem() const
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<UProfileSubsystem>() : nullptr;
}
