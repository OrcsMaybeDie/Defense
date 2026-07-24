// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemy/AI/EnemyBaseTask.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeAsyncExecutionContext.h"
#include "EnemyPatrolTask.generated.h"

USTRUCT()
struct FEnemyPatrolTaskInstanceData : public FEnemyBaseTaskInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY()
	int32 CurrentWaypointIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadius = 50.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float DestinationAcceptanceRadius = 50.f;

	UPROPERTY()
	bool bMovingToDestination = false;

	FDelegateHandle MoveFinishedHandle;
	FAIRequestID MoveRequestID;
	FStateTreeWeakExecutionContext WeakExecutionContext;
};

USTRUCT(meta = (DisplayName = "Enemy Patrol", Category = "Enemy|AI"))
struct DEFENSE_API FEnemyPatrolTask : public FEnemyBaseTask
{
	GENERATED_BODY()

	FEnemyPatrolTask();

	using FInstanceDataType = FEnemyPatrolTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	static void MoveToCurrentWaypoint(TWeakObjectPtr<class AEnemyController> WeakController, FEnemyPatrolTaskInstanceData* InstanceData);
	static void AdvanceToNextWaypoint(TWeakObjectPtr<class AEnemyController> WeakController, FEnemyPatrolTaskInstanceData* InstanceData);
	static void FinishPatrolTask(TWeakObjectPtr<class AEnemyController> WeakController, FEnemyPatrolTaskInstanceData* InstanceData, EStateTreeFinishTaskType FinishType);
	static void UnbindMoveFinished(class AEnemyController* AIController, FEnemyPatrolTaskInstanceData& InstanceData);

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
