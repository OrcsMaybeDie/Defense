// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyWaitingTask.h"

#include "Characters/Enemy/AI/EnemyController.h"
#include "Characters/Enemy/EnemyBase.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FEnemyWaitingTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	AEnemyController* AIController = GetAIController(Context);
	if (!AIEnemy || !AIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	AIEnemy->EnemyState = EEnemyState::Waiting;
	AIController->StopMovement();

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEnemyWaitingTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FEnemyWaitingTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Waiting</b>"));
}
#endif
