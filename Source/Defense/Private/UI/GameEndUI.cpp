// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/GameEndUI.h"

#include "Characters/Player/DefensePlayerController.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/KismetSystemLibrary.h"

void UGameEndUI::NativeConstruct()
{
	Super::NativeConstruct();
	Button_Exit->OnClicked.AddDynamic(this, &UGameEndUI::ExitGame);
	Button_Retry->OnClicked.AddDynamic(this, &UGameEndUI::RetryGame);
	
	Retry_Switcher->SetActiveWidgetIndex(0);
}

void UGameEndUI::GameClear()
{
	WidgetSwitcher->SetActiveWidgetIndex(0);
}

void UGameEndUI::GameOver()
{
	WidgetSwitcher->SetActiveWidgetIndex(1);
}

void UGameEndUI::ExitGame()
{
	UKismetSystemLibrary::QuitGame(
	this,
	GetWorld()->GetFirstPlayerController(),
	EQuitPreference::Quit,
	true);
}

void UGameEndUI::RetryGame()
{
	if (ADefensePlayerController* PC = GetOwningPlayer<ADefensePlayerController>())
	{
		PC->RequestGameEndRetry();
	}
}

void UGameEndUI::ShowEndLoading()
{
	if (SwitcherEndLoading)
	{
		SwitcherEndLoading->SetActiveWidgetIndex(1);
	}
}
