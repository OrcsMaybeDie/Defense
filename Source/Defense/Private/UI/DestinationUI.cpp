// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/DestinationUI.h"

#include "Components/TextBlock.h"
#include "GameManager/DefenseGameState.h"
#include "TimerManager.h"

void UDestinationUI::UpdateUI(int32 newScore)
{
	if (DestinationCount)
	{
		DestinationCount->SetText(FText::AsNumber(newScore));
	}
}

void UDestinationUI::UpdateCurrentWave(int32 NewCurrentWave)
{
	if (Text_CurWave)
	{
		Text_CurWave->SetText(FText::AsNumber(NewCurrentWave));
	}
}

void UDestinationUI::UpdateMaxWave(int32 NewMaxWave)
{
	if (Text_MaxWave)
	{
		Text_MaxWave->SetText(FText::AsNumber(NewMaxWave));
	}
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
			DefenseGS->OnCurrentWaveChanged.RemoveDynamic(this, &UDestinationUI::UpdateCurrentWave);
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

	if (!DefenseGS->OnCurrentWaveChanged.IsAlreadyBound(this, &UDestinationUI::UpdateCurrentWave))
	{
		DefenseGS->OnCurrentWaveChanged.AddDynamic(this, &UDestinationUI::UpdateCurrentWave);
	}

	UpdateUI(DefenseGS->DestScore);
	UpdateCurrentWave(DefenseGS->CurrentWave);
	UpdateMaxWave(DefenseGS->MaxWave);
}
