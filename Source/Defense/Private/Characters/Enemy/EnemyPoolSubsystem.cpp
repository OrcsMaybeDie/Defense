// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyPoolSubsystem.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemyAttack.h"
#include "Characters/Enemy/AI/EnemyController.h"
#include "Characters/Enemy/Data/WaveData.h"
#include "GameManager/DefenseGameInstance.h"
#include "GameManager/Data/MapConfigData.h"

void UEnemyPoolSubsystem::RegisterEnemy(AEnemyBase* Enemy)
{
	if (!IsValid(Enemy))
	{
		return;
	}

	const TWeakObjectPtr<AEnemyBase> EnemyKey(Enemy);
	if (AllEnemies.Contains(EnemyKey))
	{
		return;
	}

	AllEnemies.Add(EnemyKey);
	OnEnemyRegistered.Broadcast(Enemy);
}

void UEnemyPoolSubsystem::UnregisterEnemy(AEnemyBase* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	if (AllEnemies.Remove(TWeakObjectPtr<AEnemyBase>(Enemy)) > 0)
	{
		OnEnemyUnregistered.Broadcast(Enemy);
	}
}

void UEnemyPoolSubsystem::NotifyEnemyModeChanged(AEnemyBase* Enemy)
{
	if (!IsValid(Enemy))
	{
		return;
	}

	RegisterEnemy(Enemy);
	OnEnemyModeChanged.Broadcast(Enemy);
}

void UEnemyPoolSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (bIsPoolInitialized || !InWorld.GetAuthGameMode())
	{
		return;
	}

	const UDefenseGameInstance* DefenseGameInstance = InWorld.GetGameInstance<UDefenseGameInstance>();
	const UMapConfigData* SelectedMapConfigData = DefenseGameInstance
		? DefenseGameInstance->GetSelectedMapConfigData()
		: nullptr;

	if (!SelectedMapConfigData)
	{
		//UE_LOG(LogTemp, Error, TEXT("[Pool] Missing MapConfigData"));
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("[Pool] Init from WaveData: %s"), *GetNameSafe(SelectedMapConfigData->WaveData));
	InitPoolFromWaveData(SelectedMapConfigData->WaveData);
}

void UEnemyPoolSubsystem::InitPool(TSubclassOf<AEnemyBase> factory, int32 initSize)
{
	InitPool(factory, initSize, FVector(0, 0, -1000), FRotator::ZeroRotator);
}

void UEnemyPoolSubsystem::InitPoolFromWaveData(UWaveData* WaveData)
{
	if (!WaveData || bIsPoolInitialized)
	{
		return;
	}

	TMap<TSubclassOf<AEnemyBase>, int32> RequiredPoolCounts;
	WaveData->BuildRequiredPoolCounts(RequiredPoolCounts);

	//UE_LOG(LogTemp, Warning, TEXT("[Pool] Classes=%d Extra=%d"), RequiredPoolCounts.Num(), WaveData->ExtraPoolCount);

	for (const TPair<TSubclassOf<AEnemyBase>, int32>& RequiredPoolCount : RequiredPoolCounts)
	{
		InitPool(
			RequiredPoolCount.Key,
			RequiredPoolCount.Value,
			WaveData->PoolInitLocation,
			WaveData->PoolInitRotator
		);
	}

	bIsPoolInitialized = true;
}

void UEnemyPoolSubsystem::InitPool(TSubclassOf<AEnemyBase> factory, int32 initSize, FVector initLocation, FRotator initRotation)
{
	
	UWorld* World = GetWorld();

	if (!World || !World->GetAuthGameMode())
	{
		return;
	}
	
	if (nullptr == factory || 0 == initSize)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemyPool InitPool skipped | Factory=%s InitSize=%d"),
			//*GetNameSafe(factory),
			//initSize);
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("EnemyPool InitPool | Factory=%s InitSize=%d"),
		//*GetNameSafe(factory),
		//initSize);
	
	for (int32 i = 0; i < initSize; i++)
	{
		if(AEnemyBase* enemy = World->SpawnActor<AEnemyBase>(factory, initLocation, initRotation))
		{
			//UE_LOG(LogTemp, Warning, TEXT("EnemyPool InitPool spawned | Enemy=%s Index=%d"),
				//*GetNameSafe(enemy),
				//i);
			ReturnToPool(enemy);
		}
		
	}
}

TObjectPtr<AEnemyBase> UEnemyPoolSubsystem::SpawnFromPool(TSubclassOf<AEnemyBase> factory, FVector location, FRotator rotation, bool bAllowCreateNew)
{
	UWorld* World = GetWorld();

	if (!World || !World->GetAuthGameMode())
	{
		return nullptr;
	}
	
	if (nullptr == factory)
	{
		return nullptr;
	}
	
	AEnemyBase* enemy = nullptr;
	
	// EnemyPools에 factory가 있는지? 값이 존재하는지?
	if (EnemyPools.Contains(factory) && EnemyPools[factory].PooledEnemies.Num() > 0)
	{
		enemy = EnemyPools[factory].PooledEnemies.Pop();
		//UE_LOG(LogTemp, Warning, TEXT("EnemyPool SpawnFromPool reused | Enemy=%s Remaining=%d Location=%s"),
			//*GetNameSafe(enemy),
			//EnemyPools[factory].PooledEnemies.Num(),
			//*location.ToString());
	}
	else
	{
		if (!bAllowCreateNew)
		{
			//UE_LOG(LogTemp, Warning, TEXT("EnemyPool SpawnFromPool skipped | Pool empty and create disabled | Factory=%s Location=%s"),
				//*GetNameSafe(factory),
				//*location.ToString());
			return nullptr;
		}

		// 풀이 비어있으면 새로 생성해서 반환
		enemy = World->SpawnActor<AEnemyBase>(factory, FVector::ZeroVector, FRotator::ZeroRotator);
		//UE_LOG(LogTemp, Warning, TEXT("EnemyPool SpawnFromPool created new | Enemy=%s Location=%s"),
			//*GetNameSafe(enemy),
			//*location.ToString());
	}
	// 초기화 처리
	if (!enemy)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemyPool SpawnFromPool failed | Enemy null | Factory=%s"),
			//*GetNameSafe(factory));
		return nullptr;
	}
	enemy->SetActorLocationAndRotation(location, rotation);
	enemy->EnemyState = EEnemyState::Idle;
	enemy->SetTarget(nullptr);
	if (AEnemyAttack* AttackEnemy = Cast<AEnemyAttack>(enemy))
	{
		AttackEnemy->bLockedTarget = false;
	}
	enemy->CurHP = enemy->MaxHP;
	//UE_LOG(LogTemp, Warning, TEXT("EnemyPool SpawnFromPool set mode | Enemy=%s EnemyMode=Preview HasAuthority=%d"),
		//*GetNameSafe(enemy),
		//enemy->HasAuthority() ? 1 : 0);
	enemy->SetEnemyMode(EEnemyMode::Preview);
	//UE_LOG(LogTemp, Warning, TEXT("EnemyPool SpawnFromPool activated | Enemy=%s Mode=Preview Location=%s Controller=%s"),
		//*GetNameSafe(enemy),
		//*enemy->GetActorLocation().ToString(),
		//*GetNameSafe(enemy->GetController()));
	
	return enemy;
}

TObjectPtr<AEnemyBase> UEnemyPoolSubsystem::SpawnFromPool(TSubclassOf<AEnemyBase> factory, FTransform t, bool bAllowCreateNew)
{
	return SpawnFromPool(factory, t.GetLocation(), t.GetRotation().Rotator(), bAllowCreateNew);
}

void UEnemyPoolSubsystem::ReturnToPool(TObjectPtr<AEnemyBase> enemy)
{
	UWorld* World = GetWorld();

	if (!World || !World->GetAuthGameMode())
	{
		return;
	}
	
	if (nullptr == enemy)
	{
		return;
	}

	FPooledEnemyArray& Pool = EnemyPools.FindOrAdd(enemy->GetClass());
	
	enemy->SetActorLocationAndRotation(FVector(0,0,-1000), FRotator::ZeroRotator);
	enemy->MulticastRPC_StopAllMontages();
	enemy->bHpUIVisible = false;
	enemy->OwningSpawner = nullptr;
	if (AEnemyController* EnemyController = Cast<AEnemyController>(enemy->GetController()))
	{
		EnemyController->EnemyRoute = nullptr;
	}
	//UE_LOG(LogTemp, Warning, TEXT("EnemyPool ReturnToPool set mode | Enemy=%s EnemyMode=Inactive HasAuthority=%d"),
		//*GetNameSafe(enemy),
		//enemy->HasAuthority() ? 1 : 0);
	enemy->SetEnemyMode(EEnemyMode::Inactive);
	Pool.PooledEnemies.Add(enemy);
	//UE_LOG(LogTemp, Warning, TEXT("EnemyPool ReturnToPool | Enemy=%s PoolSize=%d"),
		//*GetNameSafe(enemy),
		//Pool.PooledEnemies.Num());
}

void UEnemyPoolSubsystem::Deinitialize()
{
	EnemyPools.Empty();
	AllEnemies.Empty();
	OnEnemyRegistered.Clear();
	OnEnemyUnregistered.Clear();
	OnEnemyModeChanged.Clear();
	bIsPoolInitialized = false;
	Super::Deinitialize();
}
