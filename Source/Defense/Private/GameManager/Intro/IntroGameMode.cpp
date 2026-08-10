// Fill out your copyright notice in the Description page of Project Settings.

#include "GameManager/Intro/IntroGameMode.h"

#include "GameFramework/PlayerController.h"
#include "GameManager/DefenseGameInstance.h"
#include "GameManager/Data/MapConfigData.h"
#include "GameManager/Intro/IntroGameState.h"
#include "GameManager/Intro/IntroPlayerState.h"
#include "GameManager/Intro/IntroPlayerController.h"

AIntroGameMode::AIntroGameMode()
{
	GameStateClass = AIntroGameState::StaticClass();
	PlayerStateClass = AIntroPlayerState::StaticClass();
	PlayerControllerClass = AIntroPlayerController::StaticClass();
}

void AIntroGameMode::PreLogin(
	const FString& Options,
	const FString& Address,
	const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
}

void AIntroGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	EnsureDefaultMapSelected();
}

void AIntroGameMode::Logout(AController* Exiting)
{
	AIntroPlayerState* ExitingPlayerState = Exiting ? Exiting->GetPlayerState<AIntroPlayerState>() : nullptr;
	const bool bWasHost = ExitingPlayerState && ExitingPlayerState->IsHost();
	const bool bWasGuest = ExitingPlayerState && ExitingPlayerState->IsGuest();

	Super::Logout(Exiting);

	if (bWasHost)
	{
		PromoteGuestToHost();
	}
	else if (bWasGuest)
	{
		ClearGuest(ExitingPlayerState);
	}

	RefreshIntroPlayerRefs(ExitingPlayerState);
}

void AIntroGameMode::HandleGuestReadyChanged(AIntroPlayerState* ReadyPlayerState, bool bReady)
{
	if (!HasAuthority() || !ReadyPlayerState || !ReadyPlayerState->IsGuest())
	{
		return;
	}

	ReadyPlayerState->SetReady(bReady);
	RefreshGuestReadyState();
}

void AIntroGameMode::HandleStartGameRequested(APlayerController* RequestingPlayer)
{
	if (!HasAuthority() || !RequestingPlayer)
	{
		return;
	}

	AIntroPlayerState* RequestingPlayerState = RequestingPlayer->GetPlayerState<AIntroPlayerState>();
	AIntroGameState* IntroGameState = GetIntroGameState();
	if (!RequestingPlayerState || !IntroGameState)
	{
		return;
	}

	if (!IntroGameState->IsHostPlayerState(RequestingPlayerState) || !IntroGameState->CanHostStart())
	{
		return;
	}

	UDefenseGameInstance* DefenseGameInstance = GetGameInstance<UDefenseGameInstance>();
	const FString GameMapPackageName = DefenseGameInstance
		? DefenseGameInstance->GetSelectedGameMapPackageName()
		: FString();

	if (GameMapPackageName.IsEmpty())
	{
		return;
	}

	TArray<APlayerState*> GuestPlayerStates;
	for (AIntroPlayerState* GuestPlayerState : IntroGameState->GetGuestPlayerStates())
	{
		GuestPlayerStates.Add(GuestPlayerState);
	}

	DefenseGameInstance->SaveIntroPlayerRoles(IntroGameState->GetHostPlayerState(), GuestPlayerStates);

	PendingGameMapPackageName = GameMapPackageName;
	ShowIntroLoadingForAllPlayers();

	if (StartTravelDelay <= 0.0f)
	{
		TravelToPendingGameMap();
		return;
	}

	GetWorldTimerManager().SetTimer(
		StartTravelTimerHandle,
		this,
		&AIntroGameMode::TravelToPendingGameMap,
		StartTravelDelay,
		false
	);
}

void AIntroGameMode::HandleMapSelected(APlayerController* RequestingPlayer, UMapConfigData* SelectedMapConfigData)
{
	if (!HasAuthority() || !RequestingPlayer || !SelectedMapConfigData || SelectedMapConfigData->bNotReady)
	{
		return;
	}

	AIntroPlayerState* RequestingPlayerState = RequestingPlayer->GetPlayerState<AIntroPlayerState>();
	AIntroGameState* IntroGameState = GetIntroGameState();
	if (!RequestingPlayerState || !IntroGameState || !IntroGameState->IsHostPlayerState(RequestingPlayerState))
	{
		return;
	}

	IntroGameState->SetSelectedMapConfigData(SelectedMapConfigData);

	if (UDefenseGameInstance* DefenseGameInstance = GetGameInstance<UDefenseGameInstance>())
	{
		DefenseGameInstance->SetSelectedMapConfigData(SelectedMapConfigData);
	}
}

void AIntroGameMode::HandleClientIdentitySubmitted(APlayerController* PlayerController)
{
	AssignIntroRole(PlayerController);
}

void AIntroGameMode::AssignIntroRole(APlayerController* NewPlayer)
{
	if (!HasAuthority() || !NewPlayer)
	{
		return;
	}

	AIntroGameState* IntroGameState = GetIntroGameState();
	AIntroPlayerState* NewPlayerState = NewPlayer->GetPlayerState<AIntroPlayerState>();
	if (!IntroGameState || !NewPlayerState)
	{
		return;
	}

	if (NewPlayerState->GetIntroRole() != EIntroPlayerRole::None)
	{
		return;
	}

	NewPlayerState->SetReady(false);

	const UDefenseGameInstance* DefenseGameInstance = GetGameInstance<UDefenseGameInstance>();
	if (DefenseGameInstance && DefenseGameInstance->IsSavedHostPlayerState(NewPlayerState))
	{
		NewPlayerState->SetIntroRole(EIntroPlayerRole::Host);
		IntroGameState->SetHostPlayerState(NewPlayerState);
		return;
	}

	if (DefenseGameInstance && DefenseGameInstance->IsSavedGuestPlayerState(NewPlayerState))
	{
		if (IntroGameState->GetGuestPlayerCount() < FMath::Max(0, MaxIntroPlayers - 1))
		{
			NewPlayerState->SetIntroRole(EIntroPlayerRole::Guest);
			IntroGameState->AddGuestPlayerState(NewPlayerState);
			RefreshGuestReadyState();
			return;
		}
	}

	if (!IntroGameState->GetHostPlayerState())
	{
		NewPlayerState->SetIntroRole(EIntroPlayerRole::Host);
		IntroGameState->SetHostPlayerState(NewPlayerState);
		return;
	}

	if (IntroGameState->GetGuestPlayerCount() < FMath::Max(0, MaxIntroPlayers - 1))
	{
		NewPlayerState->SetIntroRole(EIntroPlayerRole::Guest);
		IntroGameState->AddGuestPlayerState(NewPlayerState);
		RefreshGuestReadyState();
		return;
	}

	NewPlayerState->SetIntroRole(EIntroPlayerRole::Spectator);

	if (AIntroPlayerController* IntroPlayerController = Cast<AIntroPlayerController>(NewPlayer))
	{
		IntroPlayerController->ClientRPC_ShowFullWarning();
	}
}

void AIntroGameMode::EnsureDefaultMapSelected()
{
	AIntroGameState* IntroGameState = GetIntroGameState();
	UDefenseGameInstance* DefenseGameInstance = GetGameInstance<UDefenseGameInstance>();
	if (!IntroGameState || !DefenseGameInstance || IntroGameState->GetSelectedMapConfigData())
	{
		return;
	}

	UMapConfigData* DefaultMapConfigData = DefenseGameInstance->GetSelectedMapConfigData();
	if (DefaultMapConfigData && DefaultMapConfigData->bNotReady)
	{
		DefaultMapConfigData = nullptr;
	}

	if (!DefaultMapConfigData)
	{
		for (UMapConfigData* MapConfigData : DefenseGameInstance->GetAvailableMapConfigDataList())
		{
			if (MapConfigData && !MapConfigData->bNotReady)
			{
				DefaultMapConfigData = MapConfigData;
				break;
			}
		}
	}

	if (DefaultMapConfigData)
	{
		IntroGameState->SetSelectedMapConfigData(DefaultMapConfigData);
		DefenseGameInstance->SetSelectedMapConfigData(DefaultMapConfigData);
	}
}

void AIntroGameMode::PromoteGuestToHost()
{
	if (!HasAuthority())
	{
		return;
	}

	AIntroGameState* IntroGameState = GetIntroGameState();
	if (!IntroGameState)
	{
		return;
	}

	AIntroPlayerState* GuestPlayerState = IntroGameState->GetGuestPlayerState();
	if (!GuestPlayerState)
	{
		IntroGameState->SetHostPlayerState(nullptr);
		IntroGameState->SetGuestReady(false);
		return;
	}

	IntroGameState->RemoveGuestPlayerState(GuestPlayerState);
	GuestPlayerState->SetIntroRole(EIntroPlayerRole::Host);
	GuestPlayerState->SetReady(false);
	IntroGameState->SetHostPlayerState(GuestPlayerState);
	RefreshGuestReadyState();
}

void AIntroGameMode::ClearGuest(AIntroPlayerState* GuestPlayerState)
{
	AIntroGameState* IntroGameState = GetIntroGameState();
	if (!IntroGameState || !GuestPlayerState)
	{
		return;
	}

	IntroGameState->RemoveGuestPlayerState(GuestPlayerState);
	RefreshGuestReadyState();
}

void AIntroGameMode::RefreshIntroPlayerRefs(AIntroPlayerState* IgnoredPlayerState)
{
	AIntroGameState* IntroGameState = GetIntroGameState();
	if (!IntroGameState)
	{
		return;
	}

	if (IntroGameState->GetHostPlayerState() == IgnoredPlayerState)
	{
		IntroGameState->SetHostPlayerState(nullptr);
	}

	if (IgnoredPlayerState)
	{
		IntroGameState->RemoveGuestPlayerState(IgnoredPlayerState);
	}

	RefreshGuestReadyState();
}

void AIntroGameMode::RefreshGuestReadyState()
{
	AIntroGameState* IntroGameState = GetIntroGameState();
	if (!IntroGameState)
	{
		return;
	}

	const TArray<AIntroPlayerState*> GuestPlayerStates = IntroGameState->GetGuestPlayerStates();
	bool bAllGuestsReady = !GuestPlayerStates.IsEmpty();

	for (const AIntroPlayerState* GuestPlayerState : GuestPlayerStates)
	{
		if (!GuestPlayerState || !GuestPlayerState->IsReady())
		{
			bAllGuestsReady = false;
			break;
		}
	}

	IntroGameState->SetGuestReady(bAllGuestsReady);
}

void AIntroGameMode::ShowIntroLoadingForAllPlayers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AIntroPlayerController* IntroPlayerController = Cast<AIntroPlayerController>(It->Get()))
		{
			IntroPlayerController->ClientRPC_ShowIntroLoading();
		}
	}
}

void AIntroGameMode::TravelToPendingGameMap()
{
	if (PendingGameMapPackageName.IsEmpty())
	{
		return;
	}

	GetWorld()->ServerTravel(PendingGameMapPackageName);
}

AIntroGameState* AIntroGameMode::GetIntroGameState() const
{
	return GetGameState<AIntroGameState>();
}
