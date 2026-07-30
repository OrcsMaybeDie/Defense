#include "GameManager/DefenseSpectatorController.h"

#include "Blueprint/UserWidget.h"
#include "Characters/Player/DefensePlayerState.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameManager/DefenseGameMode.h"
#include "HAL/PlatformProcess.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UI/ESCUI.h"

namespace
{
	FString MakeSpectatorLocalClientIdentity()
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

void ADefenseSpectatorController::BeginPlay()
{
	APlayerController::BeginPlay();

	if (IsLocalPlayerController())
	{
		SubmitSpectatorClientIdentity();
		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;

		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
	}

	if (HasAuthority() || IsLocalPlayerController())
	{
		SyncViewTargetToHost();

		if (HostViewTargetRefreshInterval > 0.0f)
		{
			GetWorldTimerManager().SetTimer(
				HostViewTargetTimerHandle,
				this,
				&ADefenseSpectatorController::SyncViewTargetToHost,
				HostViewTargetRefreshInterval,
				true
			);
		}
	}
}

void ADefenseSpectatorController::OnRep_PlayerState()
{
	APlayerController::OnRep_PlayerState();

	SubmitSpectatorClientIdentity();
	SyncViewTargetToHost();
}

void ADefenseSpectatorController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(HostViewTargetTimerHandle);

	if (ESCUI)
	{
		ESCUI->RemoveFromParent();
		ESCUI = nullptr;
	}

	APlayerController::EndPlay(EndPlayReason);
}

void ADefenseSpectatorController::SetupInputComponent()
{
	APlayerController::SetupInputComponent();

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : SpectatorMappingContexts)
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
		if (SpectatorESCAction)
		{
			EnhancedInputComponent->BindAction(
				SpectatorESCAction,
				ETriggerEvent::Started,
				this,
				&ADefenseSpectatorController::ToggleESCUI
			);
		}
	}
}

void ADefenseSpectatorController::ToggleESCUI()
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
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;

		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		SyncViewTargetToHost();
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
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
}

void ADefenseSpectatorController::ServerRPC_SubmitSpectatorClientIdentity_Implementation(const FString& ClientIdentity)
{
	if (ADefensePlayerState* DefensePlayerState = GetPlayerState<ADefensePlayerState>())
	{
		DefensePlayerState->SetClientIdentity(ClientIdentity);
	}

	if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
	{
		GameMode->HandleClientIdentitySubmitted(this);
	}
}

void ADefenseSpectatorController::SubmitSpectatorClientIdentity()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	ServerRPC_SubmitSpectatorClientIdentity(MakeSpectatorLocalClientIdentity());
}

void ADefenseSpectatorController::SyncViewTargetToHost()
{
	APawn* HostPawn = FindHostPawn();
	if (!HostPawn || GetViewTarget() == HostPawn)
	{
		return;
	}

	SetViewTargetWithBlend(HostPawn, 0.0f);
}

APawn* ADefenseSpectatorController::FindHostPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* CandidatePawn = *It;
		if (!CandidatePawn || CandidatePawn == GetPawn())
		{
			continue;
		}

		const ADefensePlayerState* DefensePlayerState = CandidatePawn->GetPlayerState<ADefensePlayerState>();
		if (DefensePlayerState && DefensePlayerState->IsHost())
		{
			return CandidatePawn;
		}
	}

	return nullptr;
}
