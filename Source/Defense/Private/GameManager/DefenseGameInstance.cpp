// Fill out your copyright notice in the Description page of Project Settings.


#include "GameManager/DefenseGameInstance.h"

#include "Blueprint/UserWidget.h"
#include "Characters/Player/DefensePlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameManager/Data/MapConfigData.h"
#include "GameManager/Intro/IntroPlayerState.h"
#include "UI/FullWarning.h"

namespace
{
	FString MakeTravelPlayerId(const APlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return FString();
		}

		if (const AIntroPlayerState* IntroPlayerState = Cast<AIntroPlayerState>(PlayerState))
		{
			if (!IntroPlayerState->GetClientIdentity().IsEmpty())
			{
				return IntroPlayerState->GetClientIdentity();
			}
		}

		if (const ADefensePlayerState* DefensePlayerState = Cast<ADefensePlayerState>(PlayerState))
		{
			if (!DefensePlayerState->GetClientIdentity().IsEmpty())
			{
				return DefensePlayerState->GetClientIdentity();
			}
		}

		return PlayerState->GetPlayerName();
	}
}

void UDefenseGameInstance::SetSelectedMapConfigData(UMapConfigData* InMapConfigData)
{
	SelectedMapConfigData = InMapConfigData;
}

UMapConfigData* UDefenseGameInstance::GetSelectedMapConfigData() const
{
	return SelectedMapConfigData ? SelectedMapConfigData : DefaultMapConfigData;
}

const TArray<TObjectPtr<UMapConfigData>>& UDefenseGameInstance::GetAvailableMapConfigDataList() const
{
	return AvailableMapConfigDataList;
}

FString UDefenseGameInstance::GetSelectedGameMapPackageName() const
{
	const UMapConfigData* MapConfigData = GetSelectedMapConfigData();
	if (!MapConfigData || MapConfigData->GameMap.IsNull())
	{
		return FString();
	}

	return MapConfigData->GameMap.ToSoftObjectPath().GetLongPackageName();
}

FString UDefenseGameInstance::GetIntroMapPackageName() const
{
	if (IntroMap.IsNull())
	{
		return FString();
	}

	return IntroMap.ToSoftObjectPath().GetLongPackageName();
}

void UDefenseGameInstance::SaveIntroPlayerRoles(APlayerState* HostPlayerState, APlayerState* GuestPlayerState)
{
	SavedHostPlayerId = MakeTravelPlayerId(HostPlayerState);
	SavedGuestPlayerId = MakeTravelPlayerId(GuestPlayerState);
}

bool UDefenseGameInstance::IsSavedHostPlayerState(const APlayerState* PlayerState) const
{
	return !SavedHostPlayerId.IsEmpty() && MakeTravelPlayerId(PlayerState) == SavedHostPlayerId;
}

bool UDefenseGameInstance::IsSavedGuestPlayerState(const APlayerState* PlayerState) const
{
	return !SavedGuestPlayerId.IsEmpty() && MakeTravelPlayerId(PlayerState) == SavedGuestPlayerId;
}

void UDefenseGameInstance::ShowFullWarning()
{
	if (!FullWarningClass)
	{
		return;
	}

	APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (!PlayerController)
	{
		return;
	}

	if (!FullWarningWidget)
	{
		FullWarningWidget = CreateWidget<UFullWarning>(PlayerController, FullWarningClass);
	}

	if (!FullWarningWidget)
	{
		return;
	}

	if (!FullWarningWidget->IsInViewport())
	{
		FullWarningWidget->AddToViewport();
	}

	PlayerController->bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(FullWarningWidget->TakeWidget());
	PlayerController->SetInputMode(InputMode);
}
