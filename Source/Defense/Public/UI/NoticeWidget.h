#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NoticeWidget.generated.h"


class ADefenseGameState;
class ADefensePlayerState;
class UTextBlock;


UCLASS()
class DEFENSE_API UNoticeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindReadyState(
		ADefenseGameState* InGameState,
		ADefensePlayerState* InPlayerState
	);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> ReadyText;

private:
	UPROPERTY()
	TObjectPtr<ADefenseGameState> BoundGameState;

	UPROPERTY()
	TObjectPtr<ADefensePlayerState> BoundPlayerState;

	UFUNCTION()
	void HandleReadyStateChanged(bool bValue);

	void RefreshReadyText();
};
