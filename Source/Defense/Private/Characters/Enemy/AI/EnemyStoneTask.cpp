#include "Characters/Enemy/AI/EnemyStoneTask.h"

#include "Characters/Enemy/EnemyBase.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FEnemyStoneTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;

	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	if (!AIEnemy || !AIEnemy->HasAuthority() || AIEnemy->EnemyMode != EEnemyMode::Combat)
	{
		return EStateTreeRunStatus::Failed;
	}

	AIEnemy->EnemyState = EEnemyState::Stone;
	AIEnemy->BeginStoneGameplay();
	AIEnemy->MulticastRPC_EnterStoneVisual();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEnemyStoneTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	if (!AIEnemy || !AIEnemy->HasAuthority() || AIEnemy->EnemyMode != EEnemyMode::Combat)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (AIEnemy->EnemyState != EEnemyState::Stone)
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.ElapsedTime += DeltaTime;
	return InstanceData.ElapsedTime >= InstanceData.StoneDuration
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FEnemyStoneTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Stone</b>"));
}
#endif
