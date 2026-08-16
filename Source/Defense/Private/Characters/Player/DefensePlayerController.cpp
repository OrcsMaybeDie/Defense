// Copyright Epic Games, Inc. All Rights Reserved.


#include "Characters/Player/DefensePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Defense.h"
#include "EnhancedInputComponent.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Player/DefensePlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameManager/DefenseGameMode.h"
#include "GameManager/DefenseGameState.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UI/GameEndUI.h"
#include "UI/ESCUI.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace
{
	FString MakeLocalClientIdentity()
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

void ADefensePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		SubmitClientIdentity();
		bShowMouseCursor = false;

		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
	}

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			MobileControlsWidget->ClearFlags(RF_Transactional);

			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogDefense, Error, TEXT("Could not spawn mobile controls widget."));

		}
	}
	
	// 서버는 UI 없음
	if (IsLocalPlayerController() && HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->ClearFlags(RF_Transactional);
			HUDWidget->AddToPlayerScreen();
		}
	}
}

void ADefensePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	if (MobileControlsWidget)
	{
		MobileControlsWidget->RemoveFromParent();
		MobileControlsWidget = nullptr;
	}

	if (ESCUI)
	{
		ESCUI->RemoveFromParent();
		ESCUI = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ADefensePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(
			ReadyAction,
			ETriggerEvent::Started,
			this,
			&ADefensePlayerController::RequestReady
		);

		if (IA_ESC)
		{
			EnhancedInputComponent->BindAction(
				IA_ESC,
				ETriggerEvent::Started,
				this,
				&ADefensePlayerController::ToggleESCUI
			);
		}
	}
}

void ADefensePlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	SubmitClientIdentity();
}

bool ADefensePlayerController::ShouldUseTouchControls() const
{
	// build error
	if (!IsLocalPlayerController() || GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}
	
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ADefensePlayerController::ClientRPC_ShowGameEndUI_Implementation(bool bGameClear)
{
	bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	// 또는 게임 입력도 살릴 거면 FInputModeGameAndUI
	SetInputMode(InputMode);

	if (!GameEndUI && GameEndUIClass)
	{
		GameEndUI = CreateWidget<UGameEndUI>(this, GameEndUIClass);
	}

	if (GameEndUI && !GameEndUI->IsInViewport())
	{
		GameEndUI->AddToViewport();
	}

	if (GameEndUI)
	{
		if (bGameClear)
		{
			GameEndUI->GameClear();
		}
		else
		{
			GameEndUI->GameOver();
		}
	}
}

void ADefensePlayerController::ClientRPC_HideGameEndUI_Implementation()
{
	/*if (GameEndUI && GameEndUI->IsInViewport())
	{
		GameEndUI->RemoveFromParent();
	}*/
	
	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;

	SetInputMode(InputMode);
	
}

void ADefensePlayerController::ClientRPC_ShowEndLoadingUI_Implementation()
{
	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	if (GameEndUI)
	{
		GameEndUI->ShowEndLoading();
	}
}

void ADefensePlayerController::ClientRPC_ShowESCLoadingUI_Implementation()
{
	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	if (!ESCUI && ESCUIClass)
	{
		ESCUI = CreateWidget<UESCUI>(this, ESCUIClass);
	}

	if (ESCUI && !ESCUI->IsInViewport())
	{
		ESCUI->AddToViewport();
	}

	if (ESCUI)
	{
		ESCUI->ShowESCLoading();
	}
}

void ADefensePlayerController::ClientRPC_ShowRewardPopup_Implementation(AEnemyBase* Enemy, const int32 RewardAmount)
{
	if (IsValid(Enemy))
	{
		Enemy->ShowRewardPopup(RewardAmount);
	}
}

void ADefensePlayerController::RequestReady()
{
	ADefensePlayerState* PS = GetPlayerState<ADefensePlayerState>();
	if (PS && !PS->IsReady())
	{
		ServerRPC_RequestReady();
	}
}

void ADefensePlayerController::RequestGameEndRetry()
{
	if (IsGameHostPlayer() && GameEndUI)
	{
		GameEndUI->ShowEndLoading();
	}

	ServerRPC_RequestGameEndRetry();
}

void ADefensePlayerController::RequestReturnToIntroMap()
{
	if (IsGameHostPlayer() && ESCUI)
	{
		ESCUI->ShowESCLoading();
	}

	ServerRPC_RequestReturnToIntroMap();
}

void ADefensePlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(
		this,
		this,
		EQuitPreference::Quit,
		true
	);
}

void ADefensePlayerController::ToggleESCUI()
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
		bShowMouseCursor = false;

		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		return;
	}

	ESCUI->SetForceGuestMode(false);
	ESCUI->AddToViewport();
	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(ESCUI->TakeWidget());
	SetInputMode(InputMode);
}

void ADefensePlayerController::SubmitClientIdentity()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	ServerRPC_SubmitClientIdentity(MakeLocalClientIdentity());
}

bool ADefensePlayerController::IsGameHostPlayer() const
{
	if (const ADefensePlayerState* DefensePlayerState = GetPlayerState<ADefensePlayerState>())
	{
		return DefensePlayerState->IsHost();
	}

	return false;
}



void ADefensePlayerController::ServerRPC_RequestReady_Implementation()
{
	const ADefenseGameState* DefenseGameState = GetWorld()->GetGameState<ADefenseGameState>();
	if (!DefenseGameState || !DefenseGameState->IsReadyInputRequired()) return;

	ADefensePlayerState* PS = GetPlayerState<ADefensePlayerState>();
	if (!PS) return;

	PS->SetReady(true);
	
	if (ADefenseGameMode* GM = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
	{
		GM->HandlePlayerReadyChanged();
	}
}

void ADefensePlayerController::ServerRPC_RequestGameEndRetry_Implementation()
{
	if (ADefenseGameMode* GM = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
	{
		GM->HandleGameEndRetryRequested(this);
	}
}

void ADefensePlayerController::ServerRPC_RequestReturnToIntroMap_Implementation()
{
	if (ADefenseGameMode* GM = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
	{
		GM->HandleReturnToIntroMapRequested(this);
	}
}

void ADefensePlayerController::ServerRPC_SubmitClientIdentity_Implementation(const FString& ClientIdentity)
{
	if (ADefensePlayerState* DefensePlayerState = GetPlayerState<ADefensePlayerState>())
	{
		DefensePlayerState->SetClientIdentity(ClientIdentity);
	}

	if (ADefenseGameMode* GM = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
	{
		GM->HandleClientIdentitySubmitted(this);
	}
}


