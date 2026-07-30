// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyDestroyTask.h"

#include "Characters/Enemy/EnemyDestroy.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FEnemyDestroyTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	InstanceData.bDestroyFinished = false;
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

	AIEnemy->EnemyState = EEnemyState::Destroy;
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

	InstanceData.ElapsedTime += DeltaTime;

	if (InstanceData.ElapsedTime < InstanceData.DestroyDuration)
	{
		return EStateTreeRunStatus::Running;
	}

	if (!InstanceData.bDestroyFinished)
	{
		InstanceData.bDestroyFinished = true;
		AIEnemy->DestroyTargetTrap();
		AIEnemy->MarkDestroyFinished(true);
	}

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FEnemyDestroyTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Destroy</b>"));
}
#endif
