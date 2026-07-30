// Fill out your copyright notice in the Description page of Project Settings.

#include "GameManager/Intro/IntroPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameManager/DefenseGameInstance.h"
#include "GameManager/Intro/IntroGameMode.h"
#include "GameManager/Intro/IntroPlayerState.h"
#include "HAL/PlatformProcess.h"
#include "InputMappingContext.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UI/ESCUI.h"
#include "UI/IntroMenu.h"

namespace
{
	FString MakeIntroLocalClientIdentity()
	{
		FString ClientIdentity = FPlatformProcess::ComputerName();

		FString LocalClientIndex;
		if (FParse::Value(FCommandLine::Get(), TEXT("LocalClientIndex="), LocalClientIndex) && !LocalClientIndex.IsEmpty())
		{
			ClientIdentity += TEXT("_");
			ClientIdentity += LocalClientIndex;
		}

		return ClientIdentity;
	}
}

void AIntroPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		SubmitClientIdentity();
		UpdateIntroEntryUI();
	}
}

void AIntroPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	SubmitClientIdentity();
	UpdateIntroEntryUI();
}

void AIntroPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ESCUI)
	{
		ESCUI->RemoveFromParent();
		ESCUI = nullptr;
	}

	if (IntroMenu)
	{
		IntroMenu->RemoveFromParent();
		IntroMenu = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AIntroPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				if (CurrentContext)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_ESC)
		{
			EnhancedInputComponent->BindAction(
				IA_ESC,
				ETriggerEvent::Started,
				this,
				&AIntroPlayerController::ToggleESCUI
			);
		}
	}
}

void AIntroPlayerController::ServerRPC_SetGuestReady_Implementation(bool bReady)
{
	AIntroPlayerState* IntroPlayerState = GetPlayerState<AIntroPlayerState>();
	if (!IntroPlayerState || !IntroPlayerState->IsGuest())
	{
		return;
	}

	if (AIntroGameMode* IntroGameMode = GetWorld()->GetAuthGameMode<AIntroGameMode>())
	{
		IntroGameMode->HandleGuestReadyChanged(IntroPlayerState, bReady);
	}
}

void AIntroPlayerController::ServerRPC_RequestStartGame_Implementation()
{
	if (AIntroGameMode* IntroGameMode = GetWorld()->GetAuthGameMode<AIntroGameMode>())
	{
		IntroGameMode->HandleStartGameRequested(this);
	}
}

void AIntroPlayerController::ServerRPC_SelectMap_Implementation(UMapConfigData* SelectedMapConfigData)
{
	if (AIntroGameMode* IntroGameMode = GetWorld()->GetAuthGameMode<AIntroGameMode>())
	{
		IntroGameMode->HandleMapSelected(this, SelectedMapConfigData);
	}
}

void AIntroPlayerController::ServerRPC_SubmitClientIdentity_Implementation(const FString& ClientIdentity)
{
	if (AIntroPlayerState* IntroPlayerState = GetPlayerState<AIntroPlayerState>())
	{
		IntroPlayerState->SetClientIdentity(ClientIdentity);
	}

	if (AIntroGameMode* IntroGameMode = GetWorld()->GetAuthGameMode<AIntroGameMode>())
	{
		IntroGameMode->HandleClientIdentitySubmitted(this);
	}
}

void AIntroPlayerController::ClientRPC_ShowIntroLoading_Implementation()
{
	if (!IntroMenu)
	{
		ShowIntroMenu();
	}

	if (IntroMenu)
	{
		IntroMenu->ShowIntroLoading();
	}

	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
}

void AIntroPlayerController::ClientRPC_ShowFullWarning_Implementation()
{
	bShouldShowFullWarning = true;
	ShowFullWarning();
}

void AIntroPlayerController::RefreshIntroMenu()
{
	if (IntroMenu)
	{
		IntroMenu->RefreshMenuState();
	}
}

void AIntroPlayerController::ShowIntroMenu()
{
	if (GetNetMode() == NM_DedicatedServer || !IntroMenuClass)
	{
		return;
	}

	const AIntroPlayerState* IntroPlayerState = GetPlayerState<AIntroPlayerState>();
	if (bShouldShowFullWarning || (IntroPlayerState && IntroPlayerState->GetIntroRole() == EIntroPlayerRole::Spectator))
	{
		ShowFullWarning();
		return;
	}

	if (!IntroMenu)
	{
		IntroMenu = CreateWidget<UIntroMenu>(this, IntroMenuClass);
	}

	if (!IntroMenu)
	{
		return;
	}

	if (!IntroMenu->IsInViewport())
	{
		IntroMenu->AddToViewport();
	}

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(IntroMenu->TakeWidget());
	SetInputMode(InputMode);
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);

	RefreshIntroMenu();
}

void AIntroPlayerController::ShowFullWarning()
{
	if (IntroMenu)
	{
		IntroMenu->RemoveFromParent();
		IntroMenu = nullptr;
	}

	if (UDefenseGameInstance* DefenseGameInstance = GetGameInstance<UDefenseGameInstance>())
	{
		DefenseGameInstance->ShowFullWarning();
	}
}

void AIntroPlayerController::UpdateIntroEntryUI()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	const AIntroPlayerState* IntroPlayerState = GetPlayerState<AIntroPlayerState>();
	if (bShouldShowFullWarning || (IntroPlayerState && IntroPlayerState->GetIntroRole() == EIntroPlayerRole::Spectator))
	{
		bShouldShowFullWarning = true;
		ShowFullWarning();
		return;
	}

	ShowIntroMenu();
}

void AIntroPlayerController::SubmitClientIdentity()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	ServerRPC_SubmitClientIdentity(MakeIntroLocalClientIdentity());
}

void AIntroPlayerController::ToggleESCUI()
{
	if (!IsLocalPlayerController() || !ESCUIClass)
	{
		return;
	}

	if (!ESCUI)
	{
		ESCUI = CreateWidget<UESCUI>(this, ESCUIClass);
	}

	if (!ESCUI)
	{
		return;
	}

	if (ESCUI->IsInViewport())
	{
		ESCUI->RemoveFromParent();
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;

		FInputModeGameAndUI InputMode;
		if (IntroMenu && IntroMenu->IsInViewport())
		{
			InputMode.SetWidgetToFocus(IntroMenu->TakeWidget());
		}
		SetInputMode(InputMode);
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
		return;
	}

	ESCUI->SetForceGuestMode(true);
	ESCUI->AddToViewport();
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(ESCUI->TakeWidget());
	SetInputMode(InputMode);
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
}
