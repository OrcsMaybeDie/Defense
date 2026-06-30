// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyPoolSubsystem.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/AI/EnemyController.h"

void UEnemyPoolSubsystem::InitPool(TSubclassOf<AEnemyBase> factory, int32 initSize)
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
		if(AEnemyBase* enemy = World->SpawnActor<AEnemyBase>(factory, FVector::ZeroVector, FRotator::ZeroRotator))
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
	enemy->CurHP = enemy->MaxHP;
	enemy->Target = nullptr;
	enemy->EnemyMode = EEnemyMode::Preview;
	//UE_LOG(LogTemp, Warning, TEXT("EnemyPool SpawnFromPool set mode | Enemy=%s EnemyMode=Preview HasAuthority=%d"),
		//*GetNameSafe(enemy),
		//enemy->HasAuthority() ? 1 : 0);
	enemy->SetPreview();
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
	
	enemy->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	enemy->EnemyMode = EEnemyMode::Inactive;
	enemy->OwningSpawner = nullptr;
	if (AEnemyController* EnemyController = Cast<AEnemyController>(enemy->GetController()))
	{
		EnemyController->EnemyRoute = nullptr;
	}
	//UE_LOG(LogTemp, Warning, TEXT("EnemyPool ReturnToPool set mode | Enemy=%s EnemyMode=Inactive HasAuthority=%d"),
		//*GetNameSafe(enemy),
		//enemy->HasAuthority() ? 1 : 0);
	enemy->SetInactive();
	Pool.PooledEnemies.Add(enemy);
	//UE_LOG(LogTemp, Warning, TEXT("EnemyPool ReturnToPool | Enemy=%s PoolSize=%d"),
		//*GetNameSafe(enemy),
		//Pool.PooledEnemies.Num());
}

void UEnemyPoolSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();

	if (!World || !World->GetAuthGameMode())
	{
		return;
	}
	
	EnemyPools.Empty();
	Super::Deinitialize();
}
