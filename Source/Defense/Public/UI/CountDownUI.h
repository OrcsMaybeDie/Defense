// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "CountDownUI.generated.h"

UENUM(BlueprintType)
enum class ECountdownDisplayRange : uint8
{
	LongCountdown UMETA(DisplayName = "10 To 4 Seconds"),
	FinalThreeSeconds UMETA(DisplayName = "3 To 1 Seconds")
};

/**
 * 
 */
UCLASS()
class DEFENSE_API UCountDownUI : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UTextBlock> Text_CountDown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown")
	ECountdownDisplayRange DisplayRange = ECountdownDisplayRange::FinalThreeSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown", meta=(ClampMin="0.0"))
	float StartTextDisplaySeconds = 1.0f;

	UFUNCTION()
	void UpdateCountdown(int32 RemainingSeconds);
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	void TryBindGameState();

private:
	void HideCountdown();
	bool ShouldShowForRemainingSeconds(int32 RemainingSeconds) const;

	FTimerHandle BindGameStateTimerHandle;
	FTimerHandle StartTextHideTimerHandle;
	bool bHasShownCountdownValue = false;
};
