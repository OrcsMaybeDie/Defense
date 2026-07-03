// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameManager/DefenseGameState.h"
#include "TimerManager.h"
#include "DefenseGameMode.generated.h"

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
	void ApplyDestinationDamage(int32 DamageAmount);
	
	virtual void BeginPlay() override;
	virtual void StartPlay() override;

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
	void AdvanceToNextWave();
	bool IsAutoStartWave(int32 WaveNumber) const;
	void StartAutoWaveCountdown();
	void HandleAutoWaveCountdownTick();
	void HandleAutoWaveCountdownFinished();
	
	UPROPERTY()
	int32 CurrentWave = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	int32 MaxWave = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	TArray<int32> AutoStartWaves = {4, 6};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	int32 AutoStartCountdownSeconds = 10;

	UPROPERTY()
	int32 CurrentEnemyCount = 0;

	UPROPERTY()
	bool bIsWaveActive = false;

	FTimerHandle AutoWaveCountdownTimerHandle;

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 InitialDestScore = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<class AEnemySpawner*> EnemySpawners;
	
	UPROPERTY()
	TObjectPtr<class ADefenseGameState> DefenseGameState;

	UFUNCTION()
	int32 GetCurrentWave();
	
	UFUNCTION()
	int32 GetMaxWave();
	
};

