// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/IntroMenu.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "GameManager/DefenseGameInstance.h"
#include "GameManager/Data/MapConfigData.h"
#include "GameManager/Intro/IntroGameState.h"
#include "GameManager/Intro/IntroPlayerController.h"
#include "GameManager/Intro/IntroPlayerState.h"
#include "UI/MapWidget.h"

void UIntroMenu::NativeConstruct()
{
	Super::NativeConstruct();

	if (ButtonReady)
	{
		ButtonReady->OnClicked.AddUniqueDynamic(this, &UIntroMenu::HandleReadyClicked);
	}

	if (ButtonStart)
	{
		ButtonStart->OnClicked.AddUniqueDynamic(this, &UIntroMenu::HandleStartClicked);
	}

	if (ButtonReadyEnd)
	{
		ButtonReadyEnd->SetIsEnabled(false);
	}

	BindIntroStateDelegates();
	RebuildMapList();
	RefreshMenuState();
}

void UIntroMenu::NativeDestruct()
{
	UnbindIntroStateDelegates();

	if (ButtonReady)
	{
		ButtonReady->OnClicked.RemoveDynamic(this, &UIntroMenu::HandleReadyClicked);
	}

	if (ButtonStart)
	{
		ButtonStart->OnClicked.RemoveDynamic(this, &UIntroMenu::HandleStartClicked);
	}

	Super::NativeDestruct();
}

void UIntroMenu::RefreshMenuState()
{
	BindIntroStateDelegates();

	const AIntroPlayerController* IntroPlayerController = GetOwningPlayer<AIntroPlayerController>();
	const AIntroPlayerState* IntroPlayerState = IntroPlayerController
		? IntroPlayerController->GetPlayerState<AIntroPlayerState>()
		: nullptr;
	const AIntroGameState* IntroGameState = GetWorld()
		? GetWorld()->GetGameState<AIntroGameState>()
		: nullptr;

	const bool bIsHost = IntroPlayerState && IntroPlayerState->IsHost();
	const bool bIsGuest = IntroPlayerState && IntroPlayerState->IsGuest();
	const bool bGuestReady = IntroPlayerState && IntroPlayerState->IsReady();

	if (bGuestReady)
	{
		bPendingGuestReady = false;
	}

	const bool bShowGuestReady = bGuestReady || bPendingGuestReady;

	if (SwitcherHostGuest)
	{
		SwitcherHostGuest->SetActiveWidgetIndex(bIsHost ? 0 : 1);
	}

	if (ButtonStart)
	{
		ButtonStart->SetIsEnabled(bIsHost && IntroGameState && IntroGameState->CanHostStart());
	}

	if (SwitcherGuest)
	{
		SwitcherGuest->SetActiveWidgetIndex(bShowGuestReady ? 1 : 0);
	}

	if (ButtonReady)
	{
		ButtonReady->SetIsEnabled(bIsGuest && !bShowGuestReady);
	}

	if (ButtonReadyEnd)
	{
		ButtonReadyEnd->SetIsEnabled(false);
	}

	RefreshMapWidgets();
}

void UIntroMenu::ShowIntroLoading()
{
	if (WBP_EquipEntryButton)
	{
		WBP_EquipEntryButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (SwitcherIntroLoading)
	{
		SwitcherIntroLoading->SetActiveWidgetIndex(1);
	}
}

void UIntroMenu::HandleStartClicked()
{
	AIntroPlayerController* IntroPlayerController = GetOwningPlayer<AIntroPlayerController>();
	if (!IntroPlayerController)
	{
		return;
	}

	IntroPlayerController->ServerRPC_RequestStartGame();
}

void UIntroMenu::HandleReadyClicked()
{
	AIntroPlayerController* IntroPlayerController = GetOwningPlayer<AIntroPlayerController>();
	if (!IntroPlayerController)
	{
		return;
	}

	bPendingGuestReady = true;
	RefreshMenuState();

	IntroPlayerController->ServerRPC_SetGuestReady(true);
}

void UIntroMenu::HandleIntroPlayersChanged()
{
	RefreshMenuState();
}

void UIntroMenu::HandleGuestReadyChanged(bool bGuestReady)
{
	RefreshMenuState();
}

void UIntroMenu::HandleIntroRoleChanged(EIntroPlayerRole NewRole)
{
	RefreshMenuState();
}

void UIntroMenu::HandleIntroReadyChanged(bool bReady)
{
	if (!bReady)
	{
		bPendingGuestReady = false;
	}

	RefreshMenuState();
}

void UIntroMenu::HandleSelectedMapChanged(UMapConfigData* SelectedMapConfigData)
{
	RefreshMapWidgets();
}

void UIntroMenu::HandleMapWidgetSelected(UMapConfigData* SelectedMapConfigData)
{
	AIntroPlayerController* IntroPlayerController = GetOwningPlayer<AIntroPlayerController>();
	const AIntroPlayerState* IntroPlayerState = IntroPlayerController
		? IntroPlayerController->GetPlayerState<AIntroPlayerState>()
		: nullptr;
	if (!IntroPlayerController || !IntroPlayerState || !IntroPlayerState->IsHost())
	{
		return;
	}

	IntroPlayerController->ServerRPC_SelectMap(SelectedMapConfigData);
}

void UIntroMenu::BindIntroStateDelegates()
{
	if (AIntroPlayerController* IntroPlayerController = GetOwningPlayer<AIntroPlayerController>())
	{
		if (AIntroPlayerState* IntroPlayerState = IntroPlayerController->GetPlayerState<AIntroPlayerState>())
		{
			IntroPlayerState->OnIntroRoleChanged.AddUniqueDynamic(this, &UIntroMenu::HandleIntroRoleChanged);
			IntroPlayerState->OnIntroReadyChanged.AddUniqueDynamic(this, &UIntroMenu::HandleIntroReadyChanged);
		}
	}

	if (AIntroGameState* IntroGameState = GetWorld() ? GetWorld()->GetGameState<AIntroGameState>() : nullptr)
	{
		IntroGameState->OnIntroPlayersChanged.AddUniqueDynamic(this, &UIntroMenu::HandleIntroPlayersChanged);
		IntroGameState->OnGuestReadyChanged.AddUniqueDynamic(this, &UIntroMenu::HandleGuestReadyChanged);
		IntroGameState->OnSelectedMapChanged.AddUniqueDynamic(this, &UIntroMenu::HandleSelectedMapChanged);
	}
}

void UIntroMenu::UnbindIntroStateDelegates()
{
	if (AIntroPlayerController* IntroPlayerController = GetOwningPlayer<AIntroPlayerController>())
	{
		if (AIntroPlayerState* IntroPlayerState = IntroPlayerController->GetPlayerState<AIntroPlayerState>())
		{
			IntroPlayerState->OnIntroRoleChanged.RemoveDynamic(this, &UIntroMenu::HandleIntroRoleChanged);
			IntroPlayerState->OnIntroReadyChanged.RemoveDynamic(this, &UIntroMenu::HandleIntroReadyChanged);
		}
	}

	if (AIntroGameState* IntroGameState = GetWorld() ? GetWorld()->GetGameState<AIntroGameState>() : nullptr)
	{
		IntroGameState->OnIntroPlayersChanged.RemoveDynamic(this, &UIntroMenu::HandleIntroPlayersChanged);
		IntroGameState->OnGuestReadyChanged.RemoveDynamic(this, &UIntroMenu::HandleGuestReadyChanged);
		IntroGameState->OnSelectedMapChanged.RemoveDynamic(this, &UIntroMenu::HandleSelectedMapChanged);
	}
	
}

void UIntroMenu::RebuildMapList()
{
	if (!ScrollBoxMaps || !MapWidgetClass)
	{
		return;
	}

	ScrollBoxMaps->ClearChildren();
	MapWidgets.Empty();

	const UDefenseGameInstance* DefenseGameInstance = GetGameInstance<UDefenseGameInstance>();
	if (!DefenseGameInstance)
	{
		return;
	}

	const AIntroPlayerController* IntroPlayerController = GetOwningPlayer<AIntroPlayerController>();
	const AIntroPlayerState* IntroPlayerState = IntroPlayerController
		? IntroPlayerController->GetPlayerState<AIntroPlayerState>()
		: nullptr;
	const bool bCanSelect = IntroPlayerState && IntroPlayerState->IsHost();

	TArray<UMapConfigData*> MapConfigDataList;
	for (UMapConfigData* MapConfigData : DefenseGameInstance->GetAvailableMapConfigDataList())
	{
		if (MapConfigData)
		{
			MapConfigDataList.Add(MapConfigData);
		}
	}

	if (MapConfigDataList.Num() == 0)
	{
		if (UMapConfigData* DefaultMapConfigData = DefenseGameInstance->GetSelectedMapConfigData())
		{
			MapConfigDataList.Add(DefaultMapConfigData);
		}
	}

	for (int32 Index = 0; Index < MapConfigDataList.Num(); ++Index)
	{
		UMapConfigData* MapConfigData = MapConfigDataList[Index];
		if (!MapConfigData)
		{
			continue;
		}

		UMapWidget* MapWidget = CreateWidget<UMapWidget>(GetOwningPlayer(), MapWidgetClass);
		if (!MapWidget)
		{
			continue;
		}

		MapWidget->SetupMapWidget(MapConfigData, Index, bCanSelect);
		MapWidget->OnMapWidgetSelected.AddUniqueDynamic(this, &UIntroMenu::HandleMapWidgetSelected);
		ScrollBoxMaps->AddChild(MapWidget);
		MapWidgets.Add(MapWidget);
	}

	RefreshMapWidgets();
}

void UIntroMenu::RefreshMapWidgets()
{
	const AIntroPlayerController* IntroPlayerController = GetOwningPlayer<AIntroPlayerController>();
	const AIntroPlayerState* IntroPlayerState = IntroPlayerController
		? IntroPlayerController->GetPlayerState<AIntroPlayerState>()
		: nullptr;
	const bool bCanSelect = IntroPlayerState && IntroPlayerState->IsHost();

	const AIntroGameState* IntroGameState = GetWorld()
		? GetWorld()->GetGameState<AIntroGameState>()
		: nullptr;
	const UMapConfigData* SelectedMapConfigData = IntroGameState
		? IntroGameState->GetSelectedMapConfigData()
		: nullptr;

	for (UMapWidget* MapWidget : MapWidgets)
	{
		if (!MapWidget)
		{
			continue;
		}

		MapWidget->SetCanSelect(bCanSelect);
		MapWidget->SetSelected(MapWidget->GetMapConfigData() == SelectedMapConfigData);
	}
}
