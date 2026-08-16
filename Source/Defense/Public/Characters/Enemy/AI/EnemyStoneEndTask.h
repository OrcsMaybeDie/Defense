#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/AI/EnemyBaseTask.h"
#include "EnemyStoneEndTask.generated.h"

USTRUCT()
struct FEnemyStoneEndTaskInstanceData : public FEnemyBaseTaskInstanceData
{
	GENERATED_BODY()
};

USTRUCT(meta=(DisplayName="Enemy Stone End", Category="Enemy|AI"))
struct DEFENSE_API FEnemyStoneEndTask : public FEnemyBaseTask
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyStoneEndTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
