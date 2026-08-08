#include "Characters/Enemy/AI/EnemyStoneDieTask.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "GameManager/DefenseGameMode.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FEnemyStoneDieTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	InstanceData.bReturnedToPool = false;

	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	if (!AIEnemy || !AIEnemy->HasAuthority())
	{
		return EStateTreeRunStatus::Failed;
	}
	if (!AIEnemy->TryMarkDeathTaskStarted(EEnemyPendingDeathType::Stone))
	{
		return EStateTreeRunStatus::Failed;
	}

	AIEnemy->EnemyState = EEnemyState::StoneDie;
	AIEnemy->BeginStoneGameplay();
	AIEnemy->CompleteTrackedAction();
	AIEnemy->ClearSuspendedAction();
	AIEnemy->MulticastRPC_StoneDieVisual();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEnemyStoneDieTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime += DeltaTime;

	if (InstanceData.ElapsedTime < InstanceData.ReturnDelay)
	{
		return EStateTreeRunStatus::Running;
	}

	if (!InstanceData.bReturnedToPool)
	{
		InstanceData.bReturnedToPool = true;
		if (AEnemyBase* AIEnemy = GetAIEnemy(Context))
		{
			if (UWorld* World = AIEnemy->GetWorld())
			{
				if (ADefenseGameMode* GameMode = World->GetAuthGameMode<ADefenseGameMode>())
				{
					GameMode->NotifyEnemyRemoved(AIEnemy, EEnemyRemoveReason::Killed);
				}

				if (UEnemyPoolSubsystem* EnemyPool = World->GetSubsystem<UEnemyPoolSubsystem>())
				{
					EnemyPool->ReturnToPool(AIEnemy);
				}
			}
		}
	}

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FEnemyStoneDieTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Stone Die</b>"));
}
#endif
