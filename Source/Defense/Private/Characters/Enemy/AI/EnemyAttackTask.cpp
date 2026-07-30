// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyAttackTask.h"

#include "Characters/Enemy/EnemyBase.h"
#include "StateTreeExecutionContext.h"
#include "Characters/Enemy/EnemyAttack.h"

EStateTreeRunStatus FEnemyAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	InstanceData.bHasAppliedDamage = false;

	AEnemyAttack* AIEnemy = Cast<AEnemyAttack>(GetAIEnemy(Context));
	if (!AIEnemy)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!AIEnemy->HasAuthority())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (AIEnemy->EnemyMode != EEnemyMode::Combat)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!AIEnemy->CanAttack())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.AttackDuration = AIEnemy->GetAttackDuration(InstanceData.AttackDuration);
	
	AIEnemy->EnemyState = EEnemyState::Attack;
	AIEnemy->MulticastRPC_AttackMotion();
	return EStateTreeRunStatus::Running;
}

// 공격 애니메이션 길이 만큼 딜레이
EStateTreeRunStatus FEnemyAttackTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyAttack* AIEnemy = Cast<AEnemyAttack>(GetAIEnemy(Context));
	if (!AIEnemy || !AIEnemy->HasAuthority() || AIEnemy->EnemyMode != EEnemyMode::Combat)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ElapsedTime += DeltaTime;

	const float AttackHitTime = InstanceData.AttackDuration
		* FMath::Clamp(InstanceData.AttackHitTimeRatio, 0.f, 1.f);
	if (!InstanceData.bHasAppliedDamage && InstanceData.ElapsedTime >= AttackHitTime)
	{
		InstanceData.bHasAppliedDamage = true;
		AIEnemy->AttackTarget();
	}

	if (InstanceData.ElapsedTime >= InstanceData.AttackDuration)
	{
		AIEnemy->MarkAttackFinished();
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FEnemyAttackTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Attack</b>"));
}
#endif
