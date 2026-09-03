#include "UI/MissionUI/MissionResultEntryWidget.h"

#include "Components/TextBlock.h"

void UMissionResultEntryWidget::SetMissionResult(const FMissionCompletionResult& Result)
{
	if (MissionNameText)
	{
		MissionNameText->SetText(Result.DisplayName);
	}

	if (SealRewardText)
	{
		const FText RewardText = FText::Format(
			NSLOCTEXT(
				"MissionResult",
				"SealRewardFormat",
				"+{0}"),
			FText::AsNumber(Result.SealReward));

		SealRewardText->SetText(RewardText);
	}
}
