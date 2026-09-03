#include "UI/MissionUI/MissionResultPanelWidget.h"

#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "UI/MissionUI/MissionResultEntryWidget.h"

void UMissionResultPanelWidget::RefreshMissionResults(const TArray<FMissionCompletionResult>& Results, const int32 CurrentSeal)
{
	if (!MissionResultScrollBox
		|| !MissionResultList
		|| !EmptyResultText
		|| !EarnedSealText
		|| !CurrentSealText)
	{
		return;
	}

	// 재시작 전 결과가 중복 표시되지 않도록 기존 Entry 제거
	MissionResultList->ClearChildren();

	const bool bHasResults = !Results.IsEmpty();

	// 결과가 없을 때 목록 영역 전체 숨김
	MissionResultScrollBox->SetVisibility(
		bHasResults
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);

	// 신규 결과가 없을 때 표시
	EmptyResultText->SetVisibility(
		bHasResults
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);

	int64 EarnedSeal = 0;

	for (const FMissionCompletionResult& Result : Results)
	{
		EarnedSeal += Result.SealReward;

		if (!MissionResultEntryWidgetClass.Get())
		{
			continue;
		}

		UMissionResultEntryWidget* Entry = CreateWidget<UMissionResultEntryWidget>(GetOwningPlayer(), MissionResultEntryWidgetClass);

		if (!Entry)
		{
			continue;
		}

		Entry->SetMissionResult(Result);
		MissionResultList->AddChildToVerticalBox(Entry);
	}

	EarnedSealText->SetText(
		FText::Format(
			NSLOCTEXT(
				"MissionResult",
				"EarnedSealFormat",
				"획득: {0}"),
			FText::AsNumber(EarnedSeal)));

	CurrentSealText->SetText(
		FText::Format(
			NSLOCTEXT(
				"MissionResult",
				"CurrentSealFormat",
				"보유: {0}"),
			FText::AsNumber(CurrentSeal)));
}
