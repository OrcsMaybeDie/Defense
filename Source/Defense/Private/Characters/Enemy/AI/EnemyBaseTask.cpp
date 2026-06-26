// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyBaseTask.h"

#include "Characters/Enemy/EnemyBase.h"
#include "StateTreeExecutionContext.h"

AEnemyBase* FEnemyBaseTask::GetAIEnemy(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	return InstanceData.AIEnemy;
}

#if WITH_EDITOR
FText FEnemyBaseTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Base Task</b>"));
}
#endif
