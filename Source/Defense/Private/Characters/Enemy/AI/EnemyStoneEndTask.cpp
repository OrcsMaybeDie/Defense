#include "Characters/Enemy/AI/EnemyStoneEndTask.h"

#include "Characters/Enemy/EnemyAttack.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemyDestroy.h"
#include "StateTreeExecutionContext.h"

namespace
{
	bool IsTimedMontageAction(const EEnemyState State)
	{
		return State == EEnemyState::Attack
			|| State == EEnemyState::Damage
			|| State == EEnemyState::Destroy;
	}

	bool IsLocomotionState(const EEnemyState State)
	{
		return State == EEnemyState::Patrol || State == EEnemyState::Chase;
	}
}

EStateTreeRunStatus FEnemyStoneEndTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	if (!AIEnemy || !AIEnemy->HasAuthority() || AIEnemy->EnemyMode != EEnemyMode::Combat)
	{
		return EStateTreeRunStatus::Failed;
	}

	AIEnemy->EnemyState = EEnemyState::StoneEnd;
	AIEnemy->EndStoneGameplay();

	const FEnemyActionProgressData& SuspendedAction = AIEnemy->SuspendedActionData;
	const bool bResumeMontage = SuspendedAction.bIsValid
		&& IsTimedMontageAction(SuspendedAction.ActionState)
		&& SuspendedAction.RemainingTime > 0.f;
	const bool bWaitForMovement = SuspendedAction.bIsValid && IsLocomotionState(SuspendedAction.ActionState);
	AIEnemy->MulticastRPC_ExitStoneVisual(bResumeMontage, bWaitForMovement);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEnemyStoneEndTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	if (!AIEnemy || !AIEnemy->HasAuthority() || AIEnemy->EnemyMode != EEnemyMode::Combat)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (AIEnemy->EnemyState != EEnemyState::StoneEnd)
	{
		return EStateTreeRunStatus::Running;
	}

	FEnemyActionProgressData& SuspendedAction = AIEnemy->SuspendedActionData;
	if (!SuspendedAction.bIsValid || !IsTimedMontageAction(SuspendedAction.ActionState) || SuspendedAction.TotalDuration <= 0.f)
	{
		AIEnemy->CompleteTrackedAction();
		AIEnemy->ClearSuspendedAction();
		return EStateTreeRunStatus::Succeeded;
	}

	const float PreviousElapsedTime = SuspendedAction.ElapsedTime;
	SuspendedAction.ElapsedTime = FMath::Min(
		SuspendedAction.TotalDuration,
		SuspendedAction.ElapsedTime + DeltaTime
	);
	SuspendedAction.RemainingTime = FMath::Max(0.f, SuspendedAction.TotalDuration - SuspendedAction.ElapsedTime);

	if (!SuspendedAction.bActionTriggered
		&& (SuspendedAction.ActionState == EEnemyState::Attack
			|| SuspendedAction.ActionState == EEnemyState::Destroy))
	{
		const float ActionTriggerTime = FMath::Clamp(
			SuspendedAction.TriggerTime,
			0.f,
			SuspendedAction.TotalDuration
		);
		if (PreviousElapsedTime > ActionTriggerTime)
		{
			SuspendedAction.bActionTriggered = true;
		}
		else if (SuspendedAction.ElapsedTime >= ActionTriggerTime)
		{
			if (SuspendedAction.ActionState == EEnemyState::Attack)
			{
				if (AEnemyAttack* AttackEnemy = Cast<AEnemyAttack>(AIEnemy))
				{
					AttackEnemy->AttackTarget();
				}
			}
			else if (AEnemyDestroy* DestroyEnemy = Cast<AEnemyDestroy>(AIEnemy))
			{
				DestroyEnemy->DestroyTargetTrap();
			}
			SuspendedAction.bActionTriggered = true;
		}
	}

	if (SuspendedAction.RemainingTime > 0.f)
	{
		return EStateTreeRunStatus::Running;
	}

	if (SuspendedAction.ActionState == EEnemyState::Attack)
	{
		if (AEnemyAttack* AttackEnemy = Cast<AEnemyAttack>(AIEnemy))
		{
			AttackEnemy->MarkAttackFinished();
		}
	}
	else if (SuspendedAction.ActionState == EEnemyState::Destroy)
	{
		if (AEnemyDestroy* DestroyEnemy = Cast<AEnemyDestroy>(AIEnemy))
		{
			DestroyEnemy->MarkDestroyFinished(true);
		}
	}

	AIEnemy->CompleteTrackedAction();
	AIEnemy->ClearSuspendedAction();
	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FEnemyStoneEndTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Stone End</b>"));
}
#endif
