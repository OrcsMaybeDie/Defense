// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyChaseTask.h"

#include "StateTreeExecutionContext.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/AI/EnemyController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Traps/Barricade.h"
#include "Traps/BarricadeTrap.h"

namespace
{
	float CalculateTargetDistance(const AEnemyBase* Enemy)
	{
		if (!Enemy || !IsValid(Enemy->Target))
		{
			return MAX_flt;
		}

		if (const ABarricade* Barricade = Cast<ABarricade>(Enemy->Target))
		{
			return Barricade->GetDistanceToSurface(Enemy->GetActorLocation());
		}

		if (const ABarricadeTrap* BarricadeTrap = Cast<ABarricadeTrap>(Enemy->Target))
		{
			return BarricadeTrap->GetDistanceToSurface(Enemy->GetActorLocation());
		}

		return Enemy->GetDistanceTo(Enemy->Target);
	}
}

EStateTreeRunStatus FEnemyChaseTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	AEnemyController* AIController = GetAIController(Context);
	if (!AIEnemy || !IsValid(AIEnemy->Target) || !AIController)
	{
		if (AIEnemy)
		{
			AIEnemy->CurrentTargetDistance = MAX_flt;
		}
		InstanceData.MoveTarget.Reset();
		return EStateTreeRunStatus::Failed;
	}

	if (AIEnemy->EnemyMode != EEnemyMode::Combat)
	{
		AIEnemy->CurrentTargetDistance = MAX_flt;
		return EStateTreeRunStatus::Failed;
	}

	AIEnemy->EnemyState = EEnemyState::Chase;
	AIEnemy->CurrentTargetDistance = CalculateTargetDistance(AIEnemy);
	InstanceData.MoveTarget = AIEnemy->Target;

	const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToActor(
		InstanceData.MoveTarget.Get(),
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
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	AEnemyController* AIController = GetAIController(Context);
	if (!AIEnemy || AIEnemy->EnemyMode != EEnemyMode::Combat || !IsValid(AIEnemy->Target) || !AIController)
	{
		if (AIEnemy)
		{
			AIEnemy->CurrentTargetDistance = MAX_flt;
		}
		InstanceData.MoveTarget.Reset();
		return EStateTreeRunStatus::Failed;
	}

	AIEnemy->CurrentTargetDistance = CalculateTargetDistance(AIEnemy);

	if (InstanceData.MoveTarget.Get() != AIEnemy->Target)
	{
		AIController->StopMovement();
		InstanceData.MoveTarget = AIEnemy->Target;

		const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToActor(
			InstanceData.MoveTarget.Get(),
			InstanceData.AcceptanceRadius,
			true,
			true,
			false
		);

		if (MoveResult == EPathFollowingRequestResult::Failed)
		{
			InstanceData.MoveTarget.Reset();
			return EStateTreeRunStatus::Failed;
		}
	}

	return EStateTreeRunStatus::Running;
}

void FEnemyChaseTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.MoveTarget.Reset();

	if (AEnemyBase* AIEnemy = GetAIEnemy(Context))
	{
		AIEnemy->CurrentTargetDistance = MAX_flt;
	}

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
