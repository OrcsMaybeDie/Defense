// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CountDownUI.h"

#include "Components/TextBlock.h"
#include "GameManager/DefenseGameState.h"
#include "TimerManager.h"

void UCountDownUI::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::Hidden);
	TryBindGameState();
}

void UCountDownUI::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindGameStateTimerHandle);
		World->GetTimerManager().ClearTimer(StartTextHideTimerHandle);

		if (ADefenseGameState* DefenseGS = World->GetGameState<ADefenseGameState>())
		{
			DefenseGS->OnCountdownChanged.RemoveDynamic(this, &UCountDownUI::UpdateCountdown);
		}
	}

	Super::NativeDestruct();
}

void UCountDownUI::TryBindGameState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ADefenseGameState* DefenseGS = World->GetGameState<ADefenseGameState>();
	if (!DefenseGS)
	{
		World->GetTimerManager().SetTimer(
			BindGameStateTimerHandle,
			this,
			&UCountDownUI::TryBindGameState,
			0.1f,
			false
		);
		return;
	}

	World->GetTimerManager().ClearTimer(BindGameStateTimerHandle);

	if (!DefenseGS->OnCountdownChanged.IsAlreadyBound(this, &UCountDownUI::UpdateCountdown))
	{
		DefenseGS->OnCountdownChanged.AddDynamic(this, &UCountDownUI::UpdateCountdown);
	}

	UpdateCountdown(DefenseGS->CountdownRemaining);
}

void UCountDownUI::UpdateCountdown(int32 RemainingSeconds)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StartTextHideTimerHandle);
	}

	if (DisplayRange == ECountdownDisplayRange::FinalThreeSeconds && RemainingSeconds == 0 && bHasShownCountdownValue)
	{
		if (Text_CountDown)
		{
			Text_CountDown->SetText(FText::FromString(TEXT("시작")));
		}

		SetVisibility(ESlateVisibility::HitTestInvisible);
		bHasShownCountdownValue = false;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				StartTextHideTimerHandle,
				this,
				&UCountDownUI::HideCountdown,
				StartTextDisplaySeconds,
				false
			);
		}
		return;
	}

	if (!ShouldShowForRemainingSeconds(RemainingSeconds))
	{
		if (RemainingSeconds <= 0)
		{
			bHasShownCountdownValue = false;
		}

		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	if (Text_CountDown)
	{
		Text_CountDown->SetText(FText::AsNumber(RemainingSeconds));
	}

	bHasShownCountdownValue = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCountDownUI::HideCountdown()
{
	SetVisibility(ESlateVisibility::Hidden);
}

bool UCountDownUI::ShouldShowForRemainingSeconds(int32 RemainingSeconds) const
{
	switch (DisplayRange)
	{
	case ECountdownDisplayRange::LongCountdown:
		return RemainingSeconds >= 4;
	case ECountdownDisplayRange::FinalThreeSeconds:
		return RemainingSeconds >= 1 && RemainingSeconds <= 3;
	default:
		return false;
	}
}
