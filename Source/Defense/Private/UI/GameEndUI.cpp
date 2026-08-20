// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/GameEndUI.h"

#include "Characters/Player/DefensePlayerController.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/KismetSystemLibrary.h"

void UGameEndUI::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Exit)
	{
		Button_Exit->OnClicked.AddUniqueDynamic(this, &UGameEndUI::ExitGame);
	}

	if (Button_Menu)
	{
		Button_Menu->OnClicked.AddUniqueDynamic(this, &UGameEndUI::ReturnToIntroMap);
	}

	if (Button_Retry)
	{
		Button_Retry->OnClicked.AddUniqueDynamic(this, &UGameEndUI::RetryGame);
	}

	RefreshHostButtons();
}

void UGameEndUI::GameClear()
{
	WidgetSwitcher->SetActiveWidgetIndex(0);
	RefreshHostButtons();
}

void UGameEndUI::GameOver()
{
	WidgetSwitcher->SetActiveWidgetIndex(1);
	RefreshHostButtons();
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

void UGameEndUI::ReturnToIntroMap()
{
	if (ADefensePlayerController* PC = GetOwningPlayer<ADefensePlayerController>())
	{
		PC->RequestReturnToIntroMap();
	}
}

void UGameEndUI::ShowEndLoading()
{
	if (SwitcherEndLoading)
	{
		SwitcherEndLoading->SetActiveWidgetIndex(1);
	}
}

void UGameEndUI::RefreshHostButtons()
{
	const ADefensePlayerController* PC = GetOwningPlayer<ADefensePlayerController>();
	const bool bIsHost = PC && PC->IsGameHostPlayer();

	if (Button_Menu)
	{
		Button_Menu->SetIsEnabled(bIsHost);
	}

	if (Button_Retry)
	{
		Button_Retry->SetIsEnabled(bIsHost);
	}
}
