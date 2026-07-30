// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameManager/DefenseGameState.h"
#include "TimerManager.h"
#include "DefenseGameMode.generated.h"

class AGridManager;
class ADefenseCharacter;

UENUM(BlueprintType)
enum class EEnemyRemoveReason : uint8
{
	Killed UMETA(DisplayName = "Killed"),
	ReachedDestination UMETA(DisplayName = "Reached Destination"),
	InvalidState UMETA(DisplayName = "Invalid State"),
	OutOfBounds UMETA(DisplayName = "Out Of Bounds"),
	ForcedCleanup UMETA(DisplayName = "Forced Cleanup")
};

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class ADefenseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	ADefenseGameMode();
	
	void HandlePlayerReadyChanged();
	void NotifyPlayerDied(ADefenseCharacter* DeadCharacter);

	void DecreaseCurrentEnemyCount();
	void NotifyEnemyActivated(class AEnemyBase* Enemy);
	void NotifyEnemyRemoved(class AEnemyBase* Enemy, EEnemyRemoveReason Reason);
	void NotifySpawnerFinished(class AEnemySpawner* Spawner);
	void TryFinishWave();
	void ApplyDestinationDamage(int32 DamageAmount);
	void HandleGameEndRetryRequested(APlayerController* RequestingPlayer);
	void HandleReturnToIntroMapRequested(APlayerController* RequestingPlayer);
	void HandleClientIdentitySubmitted(APlayerController* PlayerController);
	
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override; // init coin set
	virtual void Logout(AController* Exiting) override;
	virtual APlayerController* SpawnPlayerController(ENetRole InRemoteRole, const FString& Options) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

protected:
	
	// 게임 인스턴스에서 게임에 필요한 초기 설정 가져옴(재화, 웨이브 수 등) 
	void ApplyDataAssets();
	
	bool AreAllPlayersReady() const;
	void ResetAllPlayersReady();
	
	bool AreAllActivePlayersDead() const;
	void RespawnDeadPlayer(AController* Controller);

	// 플레이어와 적들이 스폰되었는지 확인
	void GameStart();
	
	// 모든 웨이브가 끝나고 입력 멈춤
	void GameEnd();
	
	// 게임 재시작 -> 맵 오픈
	void RetryGame();
	bool IsHostPlayer(APlayerController* PlayerController) const;
	
	// 게임 상태 적용 및 시작
	void SetGamePhase(EGamePhase NewPhase);
	
	void Preparation();
	void WaveStart();
	void WaveEnd();
	
	// 맵에 남아있는 적 제거
	void CleanupCurrentWave();
	// 다음 웨이브 진행
	void AdvanceToNextWave();
	
	// 자동시작 되는 웨이브인지 확인
	bool IsAutoStartWave(int32 WaveNumber) const;
	void StartReadyWaveCountdown();
	void HandleReadyWaveCountdownTick();
	void HandleReadyWaveCountdownFinished();
	void StartAutoWaveCountdown();
	void HandleAutoWaveCountdownTick();
	void HandleAutoWaveCountdownFinished();
	void CleanupInvalidActiveEnemies();
	void LogActiveWaveEnemies() const;
	void TryStartGameAfterPlayerJoined();
	void AssignGameRole(APlayerController* NewPlayer);
	void PromoteRemainingGuestToHost();
	bool ShouldSpawnSpectatorController() const;
	
	UPROPERTY()
	int32 CurrentWave = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	int32 MaxWave = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	TArray<int32> AutoStartWaves = {4, 6};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data")
	TObjectPtr<class UWaveData> WaveData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	int32 AutoStartCountdownSeconds = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	int32 ReadyStartCountdownSeconds = 3;

	UPROPERTY()
	int32 CurrentEnemyCount = 0;

	UPROPERTY()
	TSet<TObjectPtr<class AEnemyBase>> ActiveWaveEnemies;

	UPROPERTY()
	TSet<TObjectPtr<class AEnemySpawner>> ParticipatingSpawners;

	UPROPERTY()
	TSet<TObjectPtr<class AEnemySpawner>> FinishedSpawners;

	UPROPERTY()
	bool bIsWaveActive = false;

	UPROPERTY()
	bool bHasGameStarted = false;

	UPROPERTY()
	bool bHasStartPlayInitialized = false;

	UPROPERTY(EditDefaultsOnly, Category="Spectator")
	TSubclassOf<APlayerController> SpectatorPlayerControllerClass;

	FTimerHandle AutoWaveCountdownTimerHandle;
	FTimerHandle ReadyWaveCountdownTimerHandle;
	FTimerHandle EnemyCleanupTimerHandle;

	float RespawnDelay = 3.f;

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 InitialDestScore = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave|Cleanup")
	bool bEnableInvalidEnemyCleanup = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave|Cleanup", meta=(ClampMin="0.1"))
	float InvalidEnemyCleanupInterval = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave|Cleanup")
	float InvalidEnemyKillZ = -5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave|Cleanup", meta=(ClampMin="0.0"))
	float MaxDistanceFromOwningSpawner = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<class AEnemySpawner*> EnemySpawners;
	
	UPROPERTY()
	TObjectPtr<class ADefenseGameState> DefenseGameState;

	UPROPERTY()
	TObjectPtr<AGridManager> GridManager;

	UFUNCTION()
	int32 GetCurrentWave();
	
	UFUNCTION()
	int32 GetMaxWave();
	
	
	/* Player 재화 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy")
	int32 InitCoin = 3000;

	void AwardEnemyKillCoin(class AEnemyBase* Enemy, AActor* DamageCauser, AController* EventInstigator);
	// 추가할 것 : wave 보상
};
