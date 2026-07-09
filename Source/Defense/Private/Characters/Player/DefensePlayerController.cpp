// Copyright Epic Games, Inc. All Rights Reserved.


#include "Characters/Player/DefensePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Defense.h"
#include "EnhancedInputComponent.h"

#include "Characters/Player/DefensePlayerState.h"
#include "GameManager/DefenseGameMode.h"
#include "UI/GameEndUI.h"
#include "Widgets/Input/SVirtualJoystick.h"

void ADefensePlayerController::BeginPlay()
{
	Super::BeginPlay();

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
			&ADefensePlayerController::ToggleReady
		);
	}
}

bool ADefensePlayerController::ShouldUseTouchControls() const
{
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

void ADefensePlayerController::ToggleReady()
{
	ADefensePlayerState* PS = GetPlayerState<ADefensePlayerState>();
	if (PS)
	{
		ServerRPC_SetReady(!PS->IsReady());
	}
}



void ADefensePlayerController::ServerRPC_SetReady_Implementation(bool bReady)
{
	ADefensePlayerState* PS = GetPlayerState<ADefensePlayerState>();
	if (!PS) return;

	PS->SetReady(bReady);
	
	if (ADefenseGameMode* GM = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
	{
		GM->HandlePlayerReadyChanged();
	}
}


