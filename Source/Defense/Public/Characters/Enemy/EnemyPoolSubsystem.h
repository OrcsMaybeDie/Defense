// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemyPoolSubsystem.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FPooledEnemyArray
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<TObjectPtr<class AEnemyBase>> PooledEnemies;
};

UCLASS()
class DEFENSE_API UEnemyPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	// 다양한 적을 만들 경우 해당 적 타입별로 배열 만들어 저장.
	UPROPERTY()
	TMap<TSubclassOf<class AEnemyBase>, FPooledEnemyArray> EnemyPools;

	UPROPERTY()
	bool bIsPoolInitialized = false;
	
	// 초기화
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	void InitPool(TSubclassOf<AEnemyBase> factory, int32 initSize);
	void InitPool(TSubclassOf<AEnemyBase> factory, int32 initSize, FVector initLocation, FRotator initRotation);
	void InitPoolFromWaveData(class UWaveData* WaveData);
	// 꺼내기
	TObjectPtr<AEnemyBase> SpawnFromPool(TSubclassOf<AEnemyBase> factory, FVector location, FRotator rotation, bool bAllowCreateNew = true);
	TObjectPtr<AEnemyBase> SpawnFromPool(TSubclassOf<AEnemyBase> factory, FTransform t, bool bAllowCreateNew = true);
	// 돌려놓기
	void ReturnToPool(TObjectPtr<AEnemyBase> enemy);
	
	virtual void Deinitialize() override;
	
};
