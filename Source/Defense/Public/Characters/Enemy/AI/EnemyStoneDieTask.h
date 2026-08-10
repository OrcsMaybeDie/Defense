#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/AI/EnemyBaseTask.h"
#include "EnemyStoneDieTask.generated.h"

USTRUCT()
struct FEnemyStoneDieTaskInstanceData : public FEnemyBaseTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Parameter", meta=(ClampMin="0.0", Units="s"))
	float ReturnDelay = 1.f;

	float ElapsedTime = 0.f;
	bool bReturnedToPool = false;
};

USTRUCT(meta=(DisplayName="Enemy Stone Die", Category="Enemy|AI"))
struct DEFENSE_API FEnemyStoneDieTask : public FEnemyBaseTask
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyStoneDieTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
