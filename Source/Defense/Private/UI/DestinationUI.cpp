// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/DestinationUI.h"

#include "Components/TextBlock.h"
#include "GameManager/DefenseGameState.h"
#include "TimerManager.h"

void UDestinationUI::UpdateUI(int32 newScore)
{
	DestinationCount->SetText(FText::AsNumber(newScore));
}

void UDestinationUI::NativeConstruct()
{
	Super::NativeConstruct();
	TryBindGameState();
}

void UDestinationUI::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindGameStateTimerHandle);

		if (ADefenseGameState* DefenseGS = World->GetGameState<ADefenseGameState>())
		{
			DefenseGS->OnDestScoreChanged.RemoveDynamic(this, &UDestinationUI::UpdateUI);
		}
	}

	Super::NativeDestruct();
}

void UDestinationUI::TryBindGameState()
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
			&UDestinationUI::TryBindGameState,
			0.1f,
			false
		);
		return;
	}

	World->GetTimerManager().ClearTimer(BindGameStateTimerHandle);

	if (!DefenseGS->OnDestScoreChanged.IsAlreadyBound(this, &UDestinationUI::UpdateUI))
	{
		DefenseGS->OnDestScoreChanged.AddDynamic(this, &UDestinationUI::UpdateUI);
	}

	UpdateUI(DefenseGS->DestScore);
}
