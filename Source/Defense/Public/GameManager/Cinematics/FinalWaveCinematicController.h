// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FinalWaveCinematicController.generated.h"

class ADefenseCharacter;
class ADefensePlayerController;
class ABackgroundMusicActor;
class ADestructibleSetPieceActor;
class AEnemyAttackBoss;
class AEnemySpawner;
class ALevelSequenceActor;
class ATargetPoint;
class ULevelSequence;
class ULevelSequencePlayer;

UENUM(BlueprintType)
enum class EFinalWaveCinematicState : uint8
{
	Idle,
	Playing,
	Completed
};

/**
 * Server-authoritative coordinator for the final-wave boss introduction.
 * The state and start time replicate, while each client creates and plays its
 * own local Level Sequence player.
 */
UCLASS(BlueprintType)
class DEFENSE_API AFinalWaveCinematicController : public AActor
{
	GENERATED_BODY()

public:
	AFinalWaveCinematicController();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Set this explicitly to the wave that should start the cinematic. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic|Trigger", meta = (ClampMin = "1", UIMin = "1"))
	int32 TriggerWaveNumber = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic")
	TObjectPtr<ULevelSequence> CinematicSequence;

	/** Must match the sequence playback length used for authoritative server completion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float ServerCinematicDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic|Destruction", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DestructionCueTime = 2.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Cinematic|Destruction")
	TObjectPtr<ADestructibleSetPieceActor> DestructibleSetPiece;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Cinematic|Boss")
	TObjectPtr<ATargetPoint> BossResumePoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic|Players")
	bool bHideAllPlayerVisuals = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic|Spawning")
	bool bPauseEnemySpawners = true;

	/** If true, stop the local background music during the cinematic and restart it afterward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic|Audio")
	bool bStopBackgroundMusicDuringCinematic = true;

	/** Placed spawners to pause; assign them directly in the level. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Cinematic|Spawning", meta = (EditCondition = "bPauseEnemySpawners"))
	TArray<TObjectPtr<AEnemySpawner>> EnemySpawnersToPause;

	UFUNCTION(BlueprintPure, Category = "Cinematic")
	bool CanStartForWave(int32 WaveNumber) const;

	/** Called by the enemy spawner after the real boss has been initialized. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cinematic")
	bool TryStartBossCinematic(AEnemyAttackBoss* Boss, int32 WaveNumber);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cinematic|Destruction")
	void TriggerDestructionNow();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cinematic")
	void FinishBossCinematic();

	UFUNCTION(BlueprintPure, Category = "Cinematic")
	EFinalWaveCinematicState GetCinematicState() const { return CinematicState; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Cinematic")
	void OnLocalCinematicStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Cinematic")
	void OnLocalCinematicFinished();

private:
	UFUNCTION()
	void OnRep_CinematicState();

	UFUNCTION()
	void HandleLocalSequenceFinished();

	void StartLocalPlayback();
	void FinishLocalPlayback(bool bRestartBackgroundMusic = true);
	void RefreshHiddenPlayerVisuals();
	void SetEnemySpawnersPaused(bool bPaused);

	UPROPERTY(ReplicatedUsing = OnRep_CinematicState)
	EFinalWaveCinematicState CinematicState = EFinalWaveCinematicState::Idle;

	UPROPERTY(Replicated)
	double ServerStartTime = 0.0;

	UPROPERTY()
	TObjectPtr<AEnemyAttackBoss> GameplayBoss;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> LocalSequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> LocalSequenceActor;

	UPROPERTY(Transient)
	TObjectPtr<ADefensePlayerController> LocalCinematicPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<ABackgroundMusicActor> LocalBackgroundMusicActor;

	TSet<TWeakObjectPtr<ADefenseCharacter>> LocallyHiddenPlayers;
	FTimerHandle DestructionTimerHandle;
	FTimerHandle FinishTimerHandle;
	FTimerHandle LocalStartRetryTimerHandle;
	FTimerHandle PlayerVisibilityRefreshTimerHandle;
	bool bLocalPlaybackActive = false;
	bool bLocalBackgroundMusicStopped = false;
};
