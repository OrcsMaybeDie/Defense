// Fill out your copyright notice in the Description page of Project Settings.

#include "GameManager/Cinematics/FinalWaveCinematicController.h"

#include "Audio/BackgroundMusicActor.h"
#include "Characters/Enemy/EnemyAttackBoss.h"
#include "Characters/Enemy/EnemySpawner.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Characters/Player/DefensePlayerController.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameManager/Cinematics/DestructibleSetPieceActor.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AFinalWaveCinematicController::AFinalWaveCinematicController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.0f);
}

void AFinalWaveCinematicController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DestructionTimerHandle);
	GetWorldTimerManager().ClearTimer(FinishTimerHandle);
	GetWorldTimerManager().ClearTimer(LocalStartRetryTimerHandle);
	FinishLocalPlayback(false);

	Super::EndPlay(EndPlayReason);
}

void AFinalWaveCinematicController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFinalWaveCinematicController, CinematicState);
	DOREPLIFETIME(AFinalWaveCinematicController, ServerStartTime);
}

bool AFinalWaveCinematicController::CanStartForWave(const int32 WaveNumber) const
{
	if (CinematicState != EFinalWaveCinematicState::Idle)
	{
		return false;
	}

	return WaveNumber == TriggerWaveNumber;
}

bool AFinalWaveCinematicController::TryStartBossCinematic(AEnemyAttackBoss* Boss, const int32 WaveNumber)
{
	if (!HasAuthority()
		|| !Boss
		|| !CinematicSequence
		|| !DestructibleSetPiece
		|| !BossResumePoint
		|| ServerCinematicDuration <= 0.0f
		|| !CanStartForWave(WaveNumber)
		|| !Boss->BeginCinematicHold())
	{
		return false;
	}

	GameplayBoss = Boss;
	ServerStartTime = GetWorld()->GetGameState<AGameStateBase>()
		? GetWorld()->GetGameState<AGameStateBase>()->GetServerWorldTimeSeconds()
		: GetWorld()->GetTimeSeconds();
	CinematicState = EFinalWaveCinematicState::Playing;

	if (bPauseEnemySpawners)
	{
		SetEnemySpawnersPaused(true);
	}

	if (DestructionCueTime <= 0.0f)
	{
		TriggerDestructionNow();
	}
	else if (DestructionCueTime < ServerCinematicDuration)
	{
		GetWorldTimerManager().SetTimer(
			DestructionTimerHandle,
			this,
			&AFinalWaveCinematicController::TriggerDestructionNow,
			DestructionCueTime,
			false
		);
	}

	GetWorldTimerManager().SetTimer(
		FinishTimerHandle,
		this,
		&AFinalWaveCinematicController::FinishBossCinematic,
		ServerCinematicDuration,
		false
	);

	OnRep_CinematicState();
	ForceNetUpdate();
	return true;
}

void AFinalWaveCinematicController::TriggerDestructionNow()
{
	if (!HasAuthority() || CinematicState != EFinalWaveCinematicState::Playing)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(DestructionTimerHandle);
	if (DestructibleSetPiece)
	{
		DestructibleSetPiece->TriggerDestruction();
	}
}

void AFinalWaveCinematicController::FinishBossCinematic()
{
	if (!HasAuthority() || CinematicState != EFinalWaveCinematicState::Playing)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(DestructionTimerHandle);
	GetWorldTimerManager().ClearTimer(FinishTimerHandle);

	if (DestructibleSetPiece)
	{
		if (!DestructibleSetPiece->IsDestroyed())
		{
			DestructibleSetPiece->TriggerDestruction();
		}
		DestructibleSetPiece->ClearDestructionDebris();
	}

	if (GameplayBoss && BossResumePoint)
	{
		GameplayBoss->EndCinematicHold(BossResumePoint->GetActorTransform());
	}

	if (bPauseEnemySpawners)
	{
		SetEnemySpawnersPaused(false);
	}

	GameplayBoss = nullptr;
	CinematicState = EFinalWaveCinematicState::Completed;
	OnRep_CinematicState();
	ForceNetUpdate();
}

void AFinalWaveCinematicController::OnRep_CinematicState()
{
	switch (CinematicState)
	{
	case EFinalWaveCinematicState::Playing:
		StartLocalPlayback();
		if (!bLocalPlaybackActive && GetWorld() && GetNetMode() != NM_DedicatedServer)
		{
			GetWorldTimerManager().SetTimer(
				LocalStartRetryTimerHandle,
				this,
				&AFinalWaveCinematicController::StartLocalPlayback,
				0.25f,
				true
			);
		}
		break;

	case EFinalWaveCinematicState::Completed:
		// Local playback finishes naturally. Its OnFinished callback restores local game state.
		break;

	default:
		break;
	}
}

void AFinalWaveCinematicController::StartLocalPlayback()
{
	if (bLocalPlaybackActive || !CinematicSequence || !GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!LocalPlayerController || !LocalPlayerController->IsLocalController())
	{
		return;
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState<AGameStateBase>();
	const double CurrentServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float ElapsedTime = FMath::Max(0.0, CurrentServerTime - ServerStartTime);
	if (ElapsedTime >= ServerCinematicDuration)
	{
		return;
	}

	bLocalPlaybackActive = true;
	GetWorldTimerManager().ClearTimer(LocalStartRetryTimerHandle);
	LocalCinematicPlayerController = Cast<ADefensePlayerController>(LocalPlayerController);
	if (LocalCinematicPlayerController)
	{
		LocalCinematicPlayerController->SetCinematicHUDHidden(true);
	}

	if (bHideAllPlayerVisuals)
	{
		RefreshHiddenPlayerVisuals();
		GetWorldTimerManager().SetTimer(
			PlayerVisibilityRefreshTimerHandle,
			this,
			&AFinalWaveCinematicController::RefreshHiddenPlayerVisuals,
			0.25f,
			true
		);
	}

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bAutoPlay = false;
	PlaybackSettings.bDisableMovementInput = true;
	PlaybackSettings.bDisableLookAtInput = true;
	PlaybackSettings.bHidePlayer = false;
	PlaybackSettings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceRestoreState;

	ALevelSequenceActor* CreatedSequenceActor = nullptr;
	LocalSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		this,
		CinematicSequence,
		PlaybackSettings,
		CreatedSequenceActor
	);
	LocalSequenceActor = CreatedSequenceActor;

	if (!LocalSequencePlayer)
	{
		FinishLocalPlayback();
		return;
	}

	LocalSequencePlayer->OnFinished.AddDynamic(this, &AFinalWaveCinematicController::HandleLocalSequenceFinished);

	if (ElapsedTime > KINDA_SMALL_NUMBER)
	{
		LocalSequencePlayer->SetPlaybackPosition(
			FMovieSceneSequencePlaybackParams(ElapsedTime, EUpdatePositionMethod::Jump)
		);
	}

	if (bStopBackgroundMusicDuringCinematic)
	{
		LocalBackgroundMusicActor = Cast<ABackgroundMusicActor>(
			UGameplayStatics::GetActorOfClass(this, ABackgroundMusicActor::StaticClass()));
		if (LocalBackgroundMusicActor)
		{
			LocalBackgroundMusicActor->StopMusic();
			bLocalBackgroundMusicStopped = true;
		}
	}

	LocalSequencePlayer->Play();
	OnLocalCinematicStarted();
}

void AFinalWaveCinematicController::FinishLocalPlayback(const bool bRestartBackgroundMusic)
{
	if (!bLocalPlaybackActive
		&& !LocalSequencePlayer
		&& !LocalSequenceActor
		&& !LocalCinematicPlayerController
		&& LocallyHiddenPlayers.Num() == 0
		&& !bLocalBackgroundMusicStopped)
	{
		return;
	}

	bLocalPlaybackActive = false;
	GetWorldTimerManager().ClearTimer(LocalStartRetryTimerHandle);
	GetWorldTimerManager().ClearTimer(PlayerVisibilityRefreshTimerHandle);

	if (LocalSequencePlayer)
	{
		LocalSequencePlayer->OnFinished.RemoveDynamic(this, &AFinalWaveCinematicController::HandleLocalSequenceFinished);
	}

	LocalSequencePlayer = nullptr;
	LocalSequenceActor = nullptr;

	if (LocalCinematicPlayerController)
	{
		LocalCinematicPlayerController->SetCinematicHUDHidden(false);
		LocalCinematicPlayerController = nullptr;
	}

	for (const TWeakObjectPtr<ADefenseCharacter>& HiddenPlayer : LocallyHiddenPlayers)
	{
		if (HiddenPlayer.IsValid())
		{
			HiddenPlayer->SetCinematicVisualHidden(false);
		}
	}
	LocallyHiddenPlayers.Empty();

	if (bLocalBackgroundMusicStopped)
	{
		if (bRestartBackgroundMusic && LocalBackgroundMusicActor)
		{
			LocalBackgroundMusicActor->PlayDefaultMusic();
		}

		bLocalBackgroundMusicStopped = false;
		LocalBackgroundMusicActor = nullptr;
	}

	OnLocalCinematicFinished();
}

void AFinalWaveCinematicController::HandleLocalSequenceFinished()
{
	FinishLocalPlayback();
}

void AFinalWaveCinematicController::RefreshHiddenPlayerVisuals()
{
	if (!bLocalPlaybackActive || !bHideAllPlayerVisuals)
	{
		return;
	}

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (ADefenseCharacter* PlayerCharacter = PlayerState ? Cast<ADefenseCharacter>(PlayerState->GetPawn()) : nullptr)
		{
			PlayerCharacter->SetCinematicVisualHidden(true);
			LocallyHiddenPlayers.Add(PlayerCharacter);
		}
	}
}

void AFinalWaveCinematicController::SetEnemySpawnersPaused(const bool bPaused)
{
	if (!HasAuthority())
	{
		return;
	}

	for (AEnemySpawner* EnemySpawner : EnemySpawnersToPause)
	{
		if (EnemySpawner)
		{
			EnemySpawner->SetCombatSpawnPaused(bPaused);
		}
	}
}
