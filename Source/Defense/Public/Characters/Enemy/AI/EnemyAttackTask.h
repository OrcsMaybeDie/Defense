// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/AI/EnemyBaseTask.h"
#include "EnemyAttackTask.generated.h"

USTRUCT()
struct FEnemyAttackTaskInstanceData : public FEnemyBaseTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AttackDuration = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AttackHitTimeRatio = 0.5f;

	float ElapsedTime = 0.f;
	bool bHasAppliedDamage = false;
};

USTRUCT(meta = (DisplayName = "Enemy Attack", Category = "Enemy|AI"))
struct DEFENSE_API FEnemyAttackTask : public FEnemyBaseTask
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyAttackTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
