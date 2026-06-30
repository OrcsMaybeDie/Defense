// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
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
	
	virtual void BeginPlay() override;
	virtual void StartPlay() override;

protected:
	bool AreAllPlayersReady() const;
	
	// 플레이어와 적들이 스폰되었는지 확인 - 그동안 UI로 로딩중 보여주기
	void GameStart();
	
	// 모든 웨이브가 끝나고 입력 멈춤 + 캐릭터 댄스애니메이션 + 결과 화면 UI를 보여줌
	void GameEnd();
	
	// Preview 적
	void StartPreview();
	
	// Combat 적들이 스폰됨
	void StartWave();
	
	// 
	void EndWave();
	
	// 웨이브 증가
	void AddWave();
	
	UPROPERTY()
	int32 CurrentWave;
	
	UPROPERTY()
	int32 MaxWave;

	UPROPERTY()
	int32 CurrentEnemyCount = 0;

	UPROPERTY()
	bool bIsWaveActive = false;

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<class AEnemySpawner*> EnemySpawners;
	
	UPROPERTY()
	TObjectPtr<class ADefenseGameState> DefenseGameState;

	UFUNCTION()
	int32 GetCurrentWave();
	
	UFUNCTION()
	int32 GetMaxWave();
	
};

