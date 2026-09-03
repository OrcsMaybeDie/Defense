#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Mission/MissionCompletionResult.h"
#include "MissionResultEntryWidget.generated.h"
class UTextBlock;

UCLASS()
class DEFENSE_API UMissionResultEntryWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MissionNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SealRewardText;

public:
	void SetMissionResult(const FMissionCompletionResult& Result); // Entry Widget
};
