// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyDestroyTask.h"

#include "Characters/Enemy/EnemyDestroy.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FEnemyDestroyTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	InstanceData.bDestroyFinished = false;
	InstanceData.bDestroyStarted = false;
	InstanceData.bDestroyTaskCompleted = false;
	InstanceData.CachedEnemy = Cast<AEnemyDestroy>(GetAIEnemy(Context));

	AEnemyDestroy* AIEnemy = InstanceData.CachedEnemy.Get();
	if (!AIEnemy)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!AIEnemy->HasAuthority())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (AIEnemy->EnemyMode != EEnemyMode::Combat || !AIEnemy->CanTryDestroy())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!AIEnemy->TryFindDestroyTarget())
	{
		AIEnemy->MarkDestroyFinished(false);
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.DestroyDuration = AIEnemy->GetDestroyDuration(InstanceData.DestroyDuration);
	const float DestroyHitTime = InstanceData.DestroyDuration
		* FMath::Clamp(InstanceData.DestroyHitTimeRatio, 0.f, 1.f);

	InstanceData.bDestroyStarted = true;
	AIEnemy->EnemyState = EEnemyState::Destroy;
	AIEnemy->ClearSuspendedAction();
	AIEnemy->BeginTrackedAction(EEnemyState::Destroy, InstanceData.DestroyDuration, DestroyHitTime);
	AIEnemy->MulticastRPC_DestroyMotion();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEnemyDestroyTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyDestroy* AIEnemy = InstanceData.CachedEnemy.Get();
	if (!AIEnemy || !AIEnemy->HasAuthority() || AIEnemy->EnemyMode != EEnemyMode::Combat)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (AIEnemy->EnemyState != EEnemyState::Destroy)
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.ElapsedTime += DeltaTime;

	const float DestroyHitTime = InstanceData.DestroyDuration
		* FMath::Clamp(InstanceData.DestroyHitTimeRatio, 0.f, 1.f);
	if (!InstanceData.bDestroyFinished && InstanceData.ElapsedTime >= DestroyHitTime)
	{
		InstanceData.bDestroyFinished = true;
		AIEnemy->DestroyTargetTrap();
	}

	AIEnemy->UpdateTrackedAction(InstanceData.ElapsedTime, InstanceData.bDestroyFinished);

	if (InstanceData.ElapsedTime >= InstanceData.DestroyDuration)
	{
		InstanceData.bDestroyTaskCompleted = true;
		AIEnemy->MarkDestroyFinished(true);
		AIEnemy->CompleteTrackedAction();
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FEnemyDestroyTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyDestroy* AIEnemy = InstanceData.CachedEnemy.Get();
	if (!InstanceData.bDestroyStarted
		|| InstanceData.bDestroyTaskCompleted
		|| !AIEnemy
		|| !AIEnemy->HasAuthority()
		|| AIEnemy->EnemyMode != EEnemyMode::Combat
		|| AIEnemy->bDeathHandled)
	{
		return;
	}

	AIEnemy->MarkDestroyFinished(InstanceData.bDestroyFinished);
	AIEnemy->CompleteTrackedAction();
	InstanceData.bDestroyTaskCompleted = true;
}

#if WITH_EDITOR
FText FEnemyDestroyTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Destroy</b>"));
}
#endif
