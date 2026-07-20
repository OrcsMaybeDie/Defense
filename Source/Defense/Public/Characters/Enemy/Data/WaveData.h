// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WaveData.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FWaveEnemyCount
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TSubclassOf<class AEnemyBase> EnemyClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0", UIMin = "0"))
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct FSpawnerWavePlan
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	FName SpawnerId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TArray<FWaveEnemyCount> EnemyCounts;
};

USTRUCT(BlueprintType)
struct FWaveInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TArray<FSpawnerWavePlan> SpawnerPlans;
};

UCLASS()
class DEFENSE_API UWaveData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxWave = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TArray<int32> AutoStartWaves = {4, 6};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave|Spawn", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PreviewSpawnInterval = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave|Spawn", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CombatSpawnInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave|Spawn", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CombatBatchInterval = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave|Pool", meta = (ClampMin = "0", UIMin = "0"))
	int32 ExtraPoolCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave|Pool")
	FVector PoolInitLocation = FVector(0.0f, 0.0f, -1000.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave|Pool")
	FRotator PoolInitRotator = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TArray<FWaveInfo> Waves;

	const FWaveInfo* FindWaveInfo(int32 WaveNumber) const;
	const FSpawnerWavePlan* FindSpawnerPlan(int32 WaveNumber, FName SpawnerId) const;
	void BuildRequiredPoolCounts(TMap<TSubclassOf<class AEnemyBase>, int32>& OutPoolCounts) const;
};
