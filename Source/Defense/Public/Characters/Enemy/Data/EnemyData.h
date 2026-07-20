// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyData.generated.h"

/**
 * 
 */

UENUM()
enum class EEnemyType  : uint8 // 적타입에 따라 다른 공격 task
{
	Run,
	Attack,
	Destroy
};

UCLASS()
class DEFENSE_API UEnemyData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Stat")
	EEnemyType EnemyType = EEnemyType::Attack;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Stat")
	float MaxHP = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Reward")
	int32 KillCoinReward = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement")
	float PreviewMoveSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement")
	float CombatMoveSpeed = 600.f;
};

UCLASS()
class DEFENSE_API UEnemyAttackData : public UEnemyData
{
	GENERATED_BODY()

public:
	UEnemyAttackData();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack")
	float AttackDist = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack")
	float DamageNum = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack")
	float AttackCooldown = 1.2f;
};

UCLASS()
class DEFENSE_API UEnemyDestroyData : public UEnemyData
{
	GENERATED_BODY()

public:
	UEnemyDestroyData();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Destroy")
	float SearchCooldown = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Destroy")
	float DestroyCooldown = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Destroy")
	float DestroyRadius = 100.f;
};
