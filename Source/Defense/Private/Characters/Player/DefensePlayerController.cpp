// Copyright Epic Games, Inc. All Rights Reserved.


#include "Characters/Player/DefensePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Defense.h"
#include "EnhancedInputComponent.h"
#include "Characters/Player/DefenseGameMode.h"
#include "Characters/Player/DefensePlayerState.h"
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
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogDefense, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
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


