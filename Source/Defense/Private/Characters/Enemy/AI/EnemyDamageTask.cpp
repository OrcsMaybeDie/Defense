// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyDamageTask.h"

#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FEnemyDamageTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return GetAIEnemy(Context) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

#if WITH_EDITOR
FText FEnemyDamageTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Damage</b>"));
}
#endif
