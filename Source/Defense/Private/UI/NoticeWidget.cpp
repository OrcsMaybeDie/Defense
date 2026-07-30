#include "UI/NoticeWidget.h"

#include "Characters/Player/DefensePlayerState.h"
#include "Components/TextBlock.h"
#include "GameManager/DefenseGameState.h"


void UNoticeWidget::BindReadyState(ADefenseGameState* InGameState, ADefensePlayerState* InPlayerState)
{
	if (BoundGameState == InGameState && BoundPlayerState == InPlayerState)
	{
		RefreshReadyText();
		return;
	}

	if (BoundGameState)
	{
		BoundGameState->OnReadyInputRequiredChanged.RemoveDynamic(
			this,
			&UNoticeWidget::HandleReadyStateChanged
		);
	}

	if (BoundPlayerState)
	{
		BoundPlayerState->OnReadyChanged.RemoveDynamic(
			this,
			&UNoticeWidget::HandleReadyStateChanged
		);
	}

	BoundGameState = InGameState;
	BoundPlayerState = InPlayerState;

	if (BoundGameState)
	{
		BoundGameState->OnReadyInputRequiredChanged.AddUniqueDynamic(
			this,
			&UNoticeWidget::HandleReadyStateChanged
		);
	}

	if (BoundPlayerState)
	{
		BoundPlayerState->OnReadyChanged.AddUniqueDynamic(
			this,
			&UNoticeWidget::HandleReadyStateChanged
		);
	}
	
	RefreshReadyText();
}

void UNoticeWidget::NativeDestruct()
{
	if (BoundGameState)
	{
		BoundGameState->OnReadyInputRequiredChanged.RemoveDynamic(
			this,
			&UNoticeWidget::HandleReadyStateChanged
		);
	}

	if (BoundPlayerState)
	{
		BoundPlayerState->OnReadyChanged.RemoveDynamic(
			this,
			&UNoticeWidget::HandleReadyStateChanged
		);
	}

	BoundGameState = nullptr;
	BoundPlayerState = nullptr;

	Super::NativeDestruct();
}

void UNoticeWidget::HandleReadyStateChanged(bool bValue)
{
	RefreshReadyText();
}

void UNoticeWidget::RefreshReadyText()
{
	if (!ReadyText) return;

	const bool bShowReadyNotice =
		BoundGameState
		&& BoundPlayerState
		&& BoundGameState->IsReadyInputRequired()
		&& !BoundPlayerState->IsReady();

	ReadyText->SetVisibility(
		bShowReadyNotice
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed
	);
}
