#include "UI/LoadoutBarWidget.h"

#include "Characters/Player/DefensePlayerState.h"
#include "Equipment/EquipmentData.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Equipment/LoadoutComponent.h"
#include "Traps/TrapData.h"


void ULoadoutBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	SlotImages = { SlotImage_0, SlotImage_1, SlotImage_2, SlotImage_3 };
	SlotCostTexts = { SlotCostText_0, SlotCostText_1, SlotCostText_2, SlotCostText_3 };

	RefreshCoin();
	RefreshSlots();
}

void ULoadoutBarWidget::NativeDestruct()
{
	if (BoundPlayerState)
	{
		BoundPlayerState->OnCoinChanged.RemoveDynamic(this, &ULoadoutBarWidget::HandleCoinChanged);
	}

	if (BoundLoadoutComp)
	{
		BoundLoadoutComp->OnSelectedEquipChanged.RemoveDynamic(this, &ULoadoutBarWidget::HandleSelectedEquipChanged);
	}
	
	Super::NativeDestruct();
}

void ULoadoutBarWidget::BindPlayerState(ADefensePlayerState* InPlayerState)
{
	if (BoundPlayerState == InPlayerState) return; // (timer) 중복 방지
	
	// 기존에 듣고 있던 PlayerState가 있으면 구독 해제
	if (BoundPlayerState)
	{
		BoundPlayerState->OnCoinChanged.RemoveDynamic(this, &ULoadoutBarWidget::HandleCoinChanged);
	}
	
	// PlayerState를 저장
	BoundPlayerState = InPlayerState;

	if (BoundPlayerState)
	{
		BoundPlayerState->OnCoinChanged.AddUniqueDynamic(this, &ULoadoutBarWidget::HandleCoinChanged);
	}

	RefreshCoin();
}

void ULoadoutBarWidget::BindLoadoutComponent(ULoadoutComponent* InLoadoutComp)
{
	if (BoundLoadoutComp == InLoadoutComp) return;

	if (BoundLoadoutComp)
	{
		BoundLoadoutComp->OnSelectedEquipChanged.RemoveDynamic(this, &ULoadoutBarWidget::HandleSelectedEquipChanged);
	}

	BoundLoadoutComp = InLoadoutComp;

	if (BoundLoadoutComp)
	{
		BoundLoadoutComp->OnSelectedEquipChanged.AddUniqueDynamic(this, &ULoadoutBarWidget::HandleSelectedEquipChanged);
	}

	RefreshSlots();
}

void ULoadoutBarWidget::HandleCoinChanged(int32 NewCoin)
{
	if (CoinText)
	{
		CoinText->SetText(FText::AsNumber(NewCoin));
	}
}

void ULoadoutBarWidget::RefreshCoin()
{
	const int32 Coin = BoundPlayerState ? BoundPlayerState->GetCoin() : 0;
	HandleCoinChanged(Coin);
}

void ULoadoutBarWidget::HandleSelectedEquipChanged(int32 SelectedSlotIdx, UEquipmentData* SelectedEquipment)
{
	// 현재는 선택 표시만 갱신한다. 슬롯 구성 변경 UI가 필요해지면 RefreshSlots()로 확장.
	RefreshSelectedSlot();
}

void ULoadoutBarWidget::RefreshSlots()
{
	for (int32 i = 0; i < SlotImages.Num(); ++i)
	{
		UEquipmentData* Equip = BoundLoadoutComp ? BoundLoadoutComp->GetEquipAtSlot(i) : nullptr;
		
		const UTrapData* TrapData = Cast<UTrapData>(Equip);
		
		if (SlotCostTexts.IsValidIndex(i) && SlotCostTexts[i])
		{
			if (TrapData)
			{
				SlotCostTexts[i]->SetText(FText::AsNumber(TrapData->Cost));
				SlotCostTexts[i]->SetVisibility(ESlateVisibility::Visible);
			}
			else
			{
				// 무기 또는 빈 슬롯은 비용 표시 없음
				SlotCostTexts[i]->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	
	RefreshSelectedSlot();
}

void ULoadoutBarWidget::RefreshSelectedSlot()
{
	const int32 SelectedIdx = BoundLoadoutComp ? BoundLoadoutComp->GetSelectedSlotIdx() : INDEX_NONE;
	
	for (int32 i = 0; i < SlotImages.Num(); i++)
	{
		if (!SlotImages[i]) continue;
		
		const bool bSelected = i == SelectedIdx;
		SlotImages[i]->SetRenderScale(bSelected ? FVector2D(1.18f, 1.18f) : FVector2D(1.f, 1.f));
		SlotImages[i]->SetRenderTranslation(bSelected ? FVector2D(0.f, -8.f) : FVector2D::ZeroVector);
	}
}

