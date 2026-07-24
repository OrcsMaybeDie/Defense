// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/ESCUI.h"

#include "Characters/Player/DefensePlayerController.h"
#include "Components/WidgetSwitcher.h"

void UESCUI::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshESCUI();
}

void UESCUI::RefreshESCUI()
{
	if (!SwitcherToIntroQuit)
	{
		return;
	}

	const ADefensePlayerController* DefensePlayerController = GetOwningPlayer<ADefensePlayerController>();
	const bool bIsHost = !bForceGuestMode && DefensePlayerController && DefensePlayerController->IsGameHostPlayer();
	SwitcherToIntroQuit->SetActiveWidgetIndex(bIsHost ? 0 : 1);
}

void UESCUI::SetForceGuestMode(bool bInForceGuestMode)
{
	bForceGuestMode = bInForceGuestMode;
	RefreshESCUI();
}

void UESCUI::ShowESCLoading()
{
	if (SwitcherESCLoading)
	{
		SwitcherESCLoading->SetActiveWidgetIndex(1);
	}
}
