// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/Data/WaveData.h"

#include "Characters/Enemy/EnemyBase.h"

const FWaveInfo* UWaveData::FindWaveInfo(int32 WaveNumber) const
{
	const int32 WaveIndex = WaveNumber - 1;
	if (!Waves.IsValidIndex(WaveIndex))
	{
		return nullptr;
	}

	return &Waves[WaveIndex];
}

const FSpawnerWavePlan* UWaveData::FindSpawnerPlan(int32 WaveNumber, FName SpawnerId) const
{
	const FWaveInfo* WaveInfo = FindWaveInfo(WaveNumber);
	if (!WaveInfo)
	{
		return nullptr;
	}

	return WaveInfo->SpawnerPlans.FindByPredicate([SpawnerId](const FSpawnerWavePlan& SpawnerPlan)
	{
		return SpawnerPlan.SpawnerId == SpawnerId;
	});
}

void UWaveData::BuildRequiredPoolCounts(TMap<TSubclassOf<AEnemyBase>, int32>& OutPoolCounts) const
{
	OutPoolCounts.Empty();

	for (const FWaveInfo& WaveInfo : Waves)
	{
		TMap<TSubclassOf<AEnemyBase>, int32> WavePoolCounts;

		for (const FSpawnerWavePlan& SpawnerPlan : WaveInfo.SpawnerPlans)
		{
			for (const FWaveEnemyCount& EnemyCount : SpawnerPlan.EnemyCounts)
			{
				if (!EnemyCount.EnemyClass || EnemyCount.Count <= 0)
				{
					continue;
				}

				int32& WaveCount = WavePoolCounts.FindOrAdd(EnemyCount.EnemyClass);
				WaveCount += EnemyCount.Count;
			}
		}

		for (const TPair<TSubclassOf<AEnemyBase>, int32>& WavePoolCount : WavePoolCounts)
		{
			int32& PoolCount = OutPoolCounts.FindOrAdd(WavePoolCount.Key);
			PoolCount = FMath::Max(PoolCount, WavePoolCount.Value);
		}
	}

	for (TPair<TSubclassOf<AEnemyBase>, int32>& PoolCount : OutPoolCounts)
	{
		PoolCount.Value += ExtraPoolCount;
	}
}
