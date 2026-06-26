// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "EnemyBaseTask.generated.h"

class AEnemyBase;

USTRUCT()
struct FEnemyBaseTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyBase> AIEnemy = nullptr;
};

USTRUCT(meta = (DisplayName = "Enemy Base Task", Category = "Enemy|AI"))
struct DEFENSE_API FEnemyBaseTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyBaseTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

protected:
	AEnemyBase* GetAIEnemy(FStateTreeExecutionContext& Context) const;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
