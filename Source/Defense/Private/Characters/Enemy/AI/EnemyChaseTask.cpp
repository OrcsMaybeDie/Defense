// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyChaseTask.h"

#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FEnemyChaseTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return GetAIEnemy(Context) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

#if WITH_EDITOR
FText FEnemyChaseTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Chase</b>"));
}
#endif
