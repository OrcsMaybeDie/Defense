#include "UI/QuickSlotBarWidget.h"

#include "Characters/Player/DefensePlayerState.h"
#include "Components/HorizontalBox.h"
#include "Equipment/EquipmentData.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Equipment/LoadoutComponent.h"
#include "Traps/TrapData.h"
#include "UI/QuickSlotEntryWidget.h"


void UQuickSlotBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshCoin();
	RefreshSlots();
}

void UQuickSlotBarWidget::NativeDestruct()
{
	if (BoundPlayerState)
	{
		BoundPlayerState->OnCoinChanged.RemoveDynamic(this, &UQuickSlotBarWidget::HandleCoinChanged);
	}

	if (BoundLoadoutComp)
	{
		BoundLoadoutComp->OnSelectedEquipChanged.RemoveDynamic(this, &UQuickSlotBarWidget::HandleSelectedEquipChanged);

		BoundLoadoutComp->OnLoadoutSlotsChanged.RemoveDynamic(this, &UQuickSlotBarWidget::HandleLoadoutSlotsChanged);
	}
	
	Super::NativeDestruct();
}

void UQuickSlotBarWidget::BindPlayerState(ADefensePlayerState* InPlayerState)
{
	if (BoundPlayerState == InPlayerState) return; // (timer) 중복 방지
	
	// 기존에 듣고 있던 PlayerState가 있으면 구독 해제
	if (BoundPlayerState)
	{
		BoundPlayerState->OnCoinChanged.RemoveDynamic(this, &UQuickSlotBarWidget::HandleCoinChanged);
	}
	
	// PlayerState를 저장
	BoundPlayerState = InPlayerState;

	if (BoundPlayerState)
	{
		BoundPlayerState->OnCoinChanged.AddUniqueDynamic(this, &UQuickSlotBarWidget::HandleCoinChanged);
	}

	RefreshCoin();
}

void UQuickSlotBarWidget::BindLoadoutComponent(ULoadoutComponent* InLoadoutComp)
{
	if (BoundLoadoutComp == InLoadoutComp) return;

	if (BoundLoadoutComp)
	{
		BoundLoadoutComp->OnSelectedEquipChanged.RemoveDynamic(this, &UQuickSlotBarWidget::HandleSelectedEquipChanged);

		BoundLoadoutComp->OnLoadoutSlotsChanged.RemoveDynamic(this, &UQuickSlotBarWidget::HandleLoadoutSlotsChanged);
	}

	BoundLoadoutComp = InLoadoutComp;

	if (BoundLoadoutComp)
	{
		BoundLoadoutComp->OnSelectedEquipChanged.AddUniqueDynamic(this, &UQuickSlotBarWidget::HandleSelectedEquipChanged);

		BoundLoadoutComp->OnLoadoutSlotsChanged.AddUniqueDynamic(this, &UQuickSlotBarWidget::HandleLoadoutSlotsChanged);
	}

	RefreshSlots();
}

void UQuickSlotBarWidget::HandleCoinChanged(int32 NewCoin)
{
	if (CoinText)
	{
		CoinText->SetText(FText::AsNumber(NewCoin));
	}
}

void UQuickSlotBarWidget::RefreshCoin()
{
	const int32 Coin = BoundPlayerState ? BoundPlayerState->GetCoin() : 0;
	HandleCoinChanged(Coin);
}

void UQuickSlotBarWidget::HandleSelectedEquipChanged(int32 SelectedSlotIdx, UEquipmentData* SelectedEquipment)
{
	// 현재는 선택 표시만 갱신한다. 슬롯 구성 변경 UI가 필요해지면 RefreshSlots()로 확장.
	RefreshSelectedSlot();
}

void UQuickSlotBarWidget::HandleLoadoutSlotsChanged()
{
	RefreshSlots();
}

void UQuickSlotBarWidget::RefreshSlots()
{
	if (!SlotContainer)
	{
		return;
	}

	SlotContainer->ClearChildren();
	SlotEntries.Reset();

	if (!BoundLoadoutComp || !SlotEntryWidgetClass)
	{
		return;
	}

	const int32 SlotCount = BoundLoadoutComp->GetSlotCount();

	for (int32 SlotIdx = 0; SlotIdx < SlotCount; ++SlotIdx)
	{
		UQuickSlotEntryWidget* SlotEntry = CreateWidget<UQuickSlotEntryWidget>(GetOwningPlayer(), SlotEntryWidgetClass);

		if (!SlotEntry)
		{
			continue;
		}

		SlotEntry->SetSlotData(SlotIdx, BoundLoadoutComp->GetEquipAtSlot(SlotIdx));

		SlotContainer->AddChildToHorizontalBox(SlotEntry);
		SlotEntries.Add(SlotEntry);
	}

	RefreshSelectedSlot();
}

void UQuickSlotBarWidget::RefreshSelectedSlot()
{
	const int32 SelectedSlotIndex = BoundLoadoutComp ? BoundLoadoutComp->GetSelectedSlotIdx() : INDEX_NONE;

	for (int32 SlotIdx = 0; SlotIdx < SlotEntries.Num(); ++SlotIdx)
	{
		if (SlotEntries[SlotIdx])
		{
			SlotEntries[SlotIdx]->SetSelected(SlotIdx == SelectedSlotIndex);
		}
	}
}

