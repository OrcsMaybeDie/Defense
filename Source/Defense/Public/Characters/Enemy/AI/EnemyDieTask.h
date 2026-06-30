// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/AI/EnemyBaseTask.h"
#include "EnemyDieTask.generated.h"

USTRUCT()
struct FEnemyDieTaskInstanceData : public FEnemyBaseTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float DamageDuration = 3.6f;

	float ElapsedTime = 0.f;
	bool bReturnedToPool = false;
};


USTRUCT(meta = (DisplayName = "Enemy Die", Category = "Enemy|AI"))
struct DEFENSE_API FEnemyDieTask : public FEnemyBaseTask
{
	GENERATED_BODY()
	
	using FInstanceDataType = FEnemyDieTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
