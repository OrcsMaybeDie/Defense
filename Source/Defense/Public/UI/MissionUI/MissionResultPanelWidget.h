#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Mission/MissionCompletionResult.h"
#include "MissionResultPanelWidget.generated.h"

class UMissionResultEntryWidget;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class DEFENSE_API UMissionResultPanelWidget
	: public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> MissionResultScrollBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> MissionResultList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmptyResultText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EarnedSealText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CurrentSealText;

	// 목록에 반복 생성할 한 줄짜리 Entry WBP
	UPROPERTY(EditDefaultsOnly, Category = "Mission")
	TSubclassOf<UMissionResultEntryWidget> MissionResultEntryWidgetClass;

public:
	void RefreshMissionResults(const TArray<FMissionCompletionResult>& Results, int32 CurrentSeal);
};
