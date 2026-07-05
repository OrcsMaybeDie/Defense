// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyChaseTask.h"

#include "StateTreeExecutionContext.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/AI/EnemyController.h"
#include "Navigation/PathFollowingComponent.h"

EStateTreeRunStatus FEnemyChaseTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	AEnemyController* AIController = GetAIController(Context);
	if (!AIEnemy || !AIEnemy->Target || !AIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (AIEnemy->EnemyMode != EEnemyMode::Combat)
	{
		return EStateTreeRunStatus::Failed;
	}

	AIEnemy->EnemyState = EEnemyState::Chase;

	const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToActor(
		AIEnemy->Target,
		InstanceData.AcceptanceRadius,
		true,
		true,
		false
	);

	return MoveResult == EPathFollowingRequestResult::Failed
		? EStateTreeRunStatus::Failed
		: EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEnemyChaseTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const AEnemyBase* AIEnemy = GetAIEnemy(Context);
	return AIEnemy && AIEnemy->EnemyMode == EEnemyMode::Combat && AIEnemy->Target
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

void FEnemyChaseTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AEnemyController* AIController = GetAIController(Context))
	{
		AIController->StopMovement();
	}
}

#if WITH_EDITOR
FText FEnemyChaseTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Chase</b>"));
}
#endif
