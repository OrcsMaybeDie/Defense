// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameManager/DefenseGameState.h"
#include "TimerManager.h"
#include "DefenseGameMode.generated.h"

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
	void DecreaseCurrentEnemyCount();
	void NotifyEnemyActivated(class AEnemyBase* Enemy);
	void NotifyEnemyRemoved(class AEnemyBase* Enemy, EEnemyRemoveReason Reason);
	void NotifySpawnerFinished(class AEnemySpawner* Spawner);
	void TryFinishWave();
	void ApplyDestinationDamage(int32 DamageAmount);
	
	virtual void BeginPlay() override;
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override; // init coin set

protected:
	bool AreAllPlayersReady() const;
	void ResetAllPlayersReady();
	
	// 플레이어와 적들이 스폰되었는지 확인 - 그동안 UI로 로딩중 보여주기
	void GameStart();
	
	// 모든 웨이브가 끝나고 입력 멈춤 + 캐릭터 댄스애니메이션 + 결과 화면 UI를 보여줌
	void GameEnd();
	
	void SetGamePhase(EGamePhase NewPhase);
	void Preparation();
	void WaveStart();
	void WaveEnd();
	void CleanupCurrentWave();
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
	
	UPROPERTY()
	int32 CurrentWave = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	int32 MaxWave = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	TArray<int32> AutoStartWaves = {4, 6};

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

	FTimerHandle AutoWaveCountdownTimerHandle;
	FTimerHandle ReadyWaveCountdownTimerHandle;
	FTimerHandle EnemyCleanupTimerHandle;

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

	UFUNCTION()
	int32 GetCurrentWave();
	
	UFUNCTION()
	int32 GetMaxWave();
	
	
	/* Player 재화 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Economy")
	int32 InitCoin = 500;

	void AwardEnemyKillCoin(class AEnemyBase* Enemy, AActor* DamageCauser, AController* EventInstigator);
	// 추가할 것 : wave 보상
};
