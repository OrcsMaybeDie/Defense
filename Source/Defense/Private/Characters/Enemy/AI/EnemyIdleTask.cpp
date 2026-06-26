// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyIdleTask.h"

#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FEnemyIdleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return GetAIEnemy(Context) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

#if WITH_EDITOR
FText FEnemyIdleTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Idle</b>"));
}
#endif
