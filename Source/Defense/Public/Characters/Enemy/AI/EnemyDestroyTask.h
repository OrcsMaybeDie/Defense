// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/AI/EnemyBaseTask.h"
#include "EnemyDestroyTask.generated.h"

class AEnemyDestroy;

USTRUCT()
struct FEnemyDestroyTaskInstanceData : public FEnemyBaseTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float DestroyDuration = 1.2f;

	UPROPERTY()
	TObjectPtr<AEnemyDestroy> CachedEnemy = nullptr;

	float ElapsedTime = 0.f;
	bool bDestroyFinished = false;
};

USTRUCT(meta = (DisplayName = "Enemy Destroy", Category = "Enemy|AI"))
struct DEFENSE_API FEnemyDestroyTask : public FEnemyBaseTask
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyDestroyTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
