// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyDamageTask.h"

#include "StateTreeExecutionContext.h"
#include "Characters/Enemy/EnemyBase.h"

EStateTreeRunStatus FEnemyDamageTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;

	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	if (!AIEnemy)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	AIEnemy->EnemyState = EEnemyState::Damage;
	AIEnemy->ClearSuspendedAction();
	AIEnemy->BeginTrackedAction(EEnemyState::Damage, InstanceData.DamageDuration);
	AIEnemy->MulticastRPC_DamageMotion();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEnemyDamageTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	if (!AIEnemy)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (AIEnemy->EnemyState != EEnemyState::Damage)
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.ElapsedTime += DeltaTime;

	if (AIEnemy)
	{
		AIEnemy->UpdateTrackedAction(InstanceData.ElapsedTime, false);
		if (InstanceData.ElapsedTime >= InstanceData.DamageDuration)
		{
			AIEnemy->CompleteTrackedAction();
		}
	}

	return InstanceData.ElapsedTime >= InstanceData.DamageDuration
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
	
}

#if WITH_EDITOR
FText FEnemyDamageTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Damage</b>"));
}
#endif
