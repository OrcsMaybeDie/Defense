#include "UI/EquipmentUI/QuickSlotEntryWidget.h"
#include "UI/EquipmentUI/EquipmentDragDrop.h"
#include "Profile/ProfileSubsystem.h"
#include "Engine/GameInstance.h"

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

UProfileSubsystem* UQuickSlotEntryWidget::GetProfileSubsystem() const
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<UProfileSubsystem>() : nullptr;
}

bool UQuickSlotEntryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UEquipmentDragDrop* EquipmentDragOperation = Cast<UEquipmentDragDrop>(InOperation);

	if (!EquipmentDragOperation || !EquipmentDragOperation->EquipmentData || SlotIndex == INDEX_NONE)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	UProfileSubsystem* ProfileSubsystem = GetProfileSubsystem();

	if (!ProfileSubsystem)
	{
		return false;
	}

	return ProfileSubsystem->AssignEquipmentToQuickSlot(SlotIndex, EquipmentDragOperation->EquipmentData);
}
