// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/AI/EnemyPatrolTask.h"

#include "StateTreeExecutionContext.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemyRoute.h"
#include "Characters/Enemy/AI/EnemyController.h"
#include "Engine/Engine.h"
#include "Navigation/PathFollowingComponent.h"

FEnemyPatrolTask::FEnemyPatrolTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FEnemyPatrolTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyBase* AIEnemy = GetAIEnemy(Context);
	AEnemyController* AIController = GetAIController(Context);
	if (!AIEnemy || !AIController)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemyPatrolTask EnterState failed | Enemy=%s Controller=%s"),
			//*GetNameSafe(AIEnemy),
			//*GetNameSafe(AIController));
		return EStateTreeRunStatus::Failed;
	}

	AIEnemy->EnemyState = EEnemyState::Patrol;
	//UE_LOG(LogTemp, Warning, TEXT("EnemyPatrolTask EnterState | Enemy=%s Location=%s"),
		//*GetNameSafe(AIEnemy),
		//*AIEnemy->GetActorLocation().ToString());

	if (!AIController->EnemyRoute)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemyPatrolTask failed | EnemyRoute is null | Enemy=%s Controller=%s"),
			//*GetNameSafe(AIEnemy),
			//*GetNameSafe(AIController));
		return EStateTreeRunStatus::Failed;
	}
	
	const TArray<FVector>& Waypoints = AIController->EnemyRoute->Waypoints;
	if (Waypoints.Num() == 0)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemyPatrolTask failed | Waypoints empty | Route=%s"),
			//*GetNameSafe(AIController->EnemyRoute));
		return EStateTreeRunStatus::Failed;
	}

	float BestDistSq = TNumericLimits<float>::Max();
	int32 BestIndex = INDEX_NONE;

	for (int32 i = 0; i < Waypoints.Num(); ++i)
	{
		const float DistSq = FVector::DistSquared2D(AIEnemy->GetActorLocation(), Waypoints[i]);

		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = i;
		}
	}

	if (BestIndex == INDEX_NONE)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	InstanceData.CurrentWaypointIndex = BestIndex + 1;
	if (!Waypoints.IsValidIndex(InstanceData.CurrentWaypointIndex))
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemyPatrolTask completed immediately | BestIndex=%d CurrentWaypointIndex=%d Waypoints=%d"),
			//BestIndex,
			//InstanceData.CurrentWaypointIndex,
			//Waypoints.Num());

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				3.f,
				FColor::Yellow,
				FString::Printf(TEXT("Patrol Waypoint invalid | BestIndex: %d | Waypoints: %d"), BestIndex, Waypoints.Num())
			);
		}
		return EStateTreeRunStatus::Succeeded;
	}

	UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent();
	if (!PathFollowingComp)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemyPatrolTask failed | PathFollowingComponent is null | Controller=%s"),
			//*GetNameSafe(AIController));
		return EStateTreeRunStatus::Failed;
	}

	UnbindMoveFinished(AIController, InstanceData);
	InstanceData.WeakExecutionContext = Context.MakeWeakExecutionContext();

	TWeakObjectPtr<AEnemyController> WeakController = AIController;
	InstanceData.MoveFinishedHandle = PathFollowingComp->OnRequestFinished.AddLambda(
		[WeakController, &InstanceData](FAIRequestID RequestID, const FPathFollowingResult& Result)
		{
			if (!WeakController.IsValid() || RequestID != InstanceData.MoveRequestID)
			{
				return;
			}

			//UE_LOG(LogTemp, Warning, TEXT("EnemyPatrolTask move finished | Enemy=%s RequestID=%u Result=%s"),
				//*GetNameSafe(WeakController->GetPawn()),
				//RequestID.GetID(),
				//*Result.ToString());

			if (Result.IsSuccess())
			{
				AdvanceToNextWaypoint(WeakController, &InstanceData);
				return;
			}

			FinishPatrolTask(WeakController, &InstanceData, EStateTreeFinishTaskType::Failed);
		});

	MoveToCurrentWaypoint(WeakController, &InstanceData);

	return EStateTreeRunStatus::Running;
}

void FEnemyPatrolTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AEnemyController* AIController = GetAIController(Context))
	{
		UnbindMoveFinished(AIController, InstanceData);
		AIController->StopMovement();
	}
}

void FEnemyPatrolTask::MoveToCurrentWaypoint(TWeakObjectPtr<AEnemyController> WeakController, FEnemyPatrolTaskInstanceData* InstanceData)
{
	if (!WeakController.IsValid() || !InstanceData || !WeakController->EnemyRoute)
	{
		FinishPatrolTask(WeakController, InstanceData, EStateTreeFinishTaskType::Failed);
		return;
	}

	AEnemyController* AIController = WeakController.Get();
	AEnemyBase* AIEnemy = Cast<AEnemyBase>(AIController->GetPawn());
	const TArray<FVector>& Waypoints = AIController->EnemyRoute->Waypoints;
	if (!Waypoints.IsValidIndex(InstanceData->CurrentWaypointIndex))
	{
		FinishPatrolTask(WeakController, InstanceData, EStateTreeFinishTaskType::Succeeded);
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("EnemyPatrolTask MoveTo waypoint | Enemy=%s Index=%d Location=%s"),
		//*GetNameSafe(AIEnemy),
		//InstanceData->CurrentWaypointIndex,
		//*Waypoints[InstanceData->CurrentWaypointIndex].ToString());

	const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToLocation(
		Waypoints[InstanceData->CurrentWaypointIndex],
		InstanceData->AcceptanceRadius
	);

	//UE_LOG(LogTemp, Warning, TEXT("EnemyPatrolTask MoveTo waypoint result | Result=%d"), static_cast<int32>(MoveResult));

	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		FinishPatrolTask(WeakController, InstanceData, EStateTreeFinishTaskType::Failed);
		return;
	}

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		AdvanceToNextWaypoint(WeakController, InstanceData);
		return;
	}

	if (const UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent())
	{
		InstanceData->MoveRequestID = PathFollowingComp->GetCurrentRequestId();
	}
}

void FEnemyPatrolTask::AdvanceToNextWaypoint(TWeakObjectPtr<AEnemyController> WeakController, FEnemyPatrolTaskInstanceData* InstanceData)
{
	if (!WeakController.IsValid() || !InstanceData || !WeakController->EnemyRoute)
	{
		FinishPatrolTask(WeakController, InstanceData, EStateTreeFinishTaskType::Failed);
		return;
	}

	AEnemyController* AIController = WeakController.Get();
	++InstanceData->CurrentWaypointIndex;

	const int32 WaypointCount = AIController->EnemyRoute->Waypoints.Num();
	//UE_LOG(LogTemp, Warning, TEXT("EnemyPatrolTask advance waypoint | Enemy=%s NextIndex=%d Waypoints=%d"),
		//*GetNameSafe(AIController->GetPawn()),
		//InstanceData->CurrentWaypointIndex,
		//WaypointCount);

	if (!AIController->EnemyRoute->Waypoints.IsValidIndex(InstanceData->CurrentWaypointIndex))
	{
		FinishPatrolTask(WeakController, InstanceData, EStateTreeFinishTaskType::Succeeded);
		return;
	}

	MoveToCurrentWaypoint(WeakController, InstanceData);
}

void FEnemyPatrolTask::FinishPatrolTask(TWeakObjectPtr<AEnemyController> WeakController, FEnemyPatrolTaskInstanceData* InstanceData, EStateTreeFinishTaskType FinishType)
{
	if (!InstanceData)
	{
		return;
	}

	if (WeakController.IsValid())
	{
		UnbindMoveFinished(WeakController.Get(), *InstanceData);
	}

	InstanceData->WeakExecutionContext.FinishTask(FinishType);
}

void FEnemyPatrolTask::UnbindMoveFinished(AEnemyController* AIController, FEnemyPatrolTaskInstanceData& InstanceData)
{
	if (!AIController || !InstanceData.MoveFinishedHandle.IsValid())
	{
		return;
	}

	if (UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent())
	{
		PathFollowingComp->OnRequestFinished.Remove(InstanceData.MoveFinishedHandle);
	}

	InstanceData.MoveFinishedHandle.Reset();
	InstanceData.MoveRequestID = FAIRequestID();
}

#if WITH_EDITOR
FText FEnemyPatrolTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("<b>Enemy Patrol</b>"));
}
#endif
