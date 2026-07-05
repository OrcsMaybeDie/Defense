// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyAttackTask.h"

#include "Characters/Enemy/EnemyBase.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FEnemyAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;

	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	if (!AIEnemy)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (AIEnemy->EnemyMode != EEnemyMode::Combat)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	AIEnemy->EnemyState = EEnemyState::Attack;
	AIEnemy->MulticastRPC_AttackMotion();
	return EStateTreeRunStatus::Running;
}

// 공격 애니메이션 길이 만큼 딜레이
EStateTreeRunStatus FEnemyAttackTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AEnemyBase* AIEnemy = GetAIEnemy(Context);
	if (!AIEnemy || AIEnemy->EnemyMode != EEnemyMode::Combat)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ElapsedTime += DeltaTime;

	return InstanceData.ElapsedTime >= InstanceData.AttackDuration
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FEnemyAttackTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Attack</b>"));
}
#endif
