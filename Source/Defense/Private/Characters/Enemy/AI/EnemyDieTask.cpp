// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyDieTask.h"

#include "StateTreeExecutionContext.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "Characters/Enemy/EnemySpawner.h"
#include "GameManager/DefenseGameMode.h"

EStateTreeRunStatus FEnemyDieTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	InstanceData.bReturnedToPool = false;

	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	if (!AIEnemy)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	AIEnemy->EnemyState = EEnemyState::Die;
	AIEnemy->MulticastRPC_DieMotion();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEnemyDieTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime += DeltaTime;

	if (InstanceData.ElapsedTime < InstanceData.DamageDuration)
	{
		return EStateTreeRunStatus::Running;
	}

	if (!InstanceData.bReturnedToPool)
	{
		InstanceData.bReturnedToPool = true;

		if (AEnemyBase* AIEnemy = GetAIEnemy(Context))
		{
			if (AEnemySpawner* Spawner = AIEnemy->OwningSpawner)
			{
				Spawner->RemoveActiveEnemy(AIEnemy);
			}

			if (UWorld* World = AIEnemy->GetWorld())
			{
				if (ADefenseGameMode* GameMode = World->GetAuthGameMode<ADefenseGameMode>())
				{
					GameMode->DecreaseCurrentEnemyCount();
				}

				if (UEnemyPoolSubsystem* EnemyPool = World->GetSubsystem<UEnemyPoolSubsystem>())
				{
					EnemyPool->ReturnToPool(AIEnemy);
				}
			}
		}
	}

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FEnemyDieTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Die</b>"));
}
#endif
