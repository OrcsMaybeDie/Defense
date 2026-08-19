// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemySpawner.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "Characters/Enemy/EnemyRoute.h"
#include "Characters/Enemy/AI/EnemyController.h"
#include "Characters/Enemy/Data/WaveData.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameManager/DefenseGameMode.h"
#include "TimerManager.h"


// Sets default values
AEnemySpawner::AEnemySpawner()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	BoxComp = CreateDefaultSubobject<UBoxComponent>(FName("BoxComp"));
	SetRootComponent(BoxComp);
	
}

// Called when the game starts or when spawned
void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();
	// 서버에서만 overlap 이벤트 일어나도록 & pool 초기화
	if (HasAuthority())
	{
		BoxComp->SetGenerateOverlapEvents(true);
		
		// 적 스폰 테스트
		/*BoxComp->OnComponentBeginOverlap.AddUniqueDynamic(
		this,
		&AEnemySpawner::OnBoxBeginOverlap
		);*/
	}
	
}

// Called every frame
void AEnemySpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AEnemySpawner::SpawnTest()
{
	if (!HasAuthority())
	{
		return;
	}
	
	//float RandomDelay = FMath::RandRange(2.0f, 5.0f);

	//FPlatformProcess::Sleep(RandomDelay);
	
	if (EnemyFactory && EnemyPool)
	{
		//GetWorld()->SpawnActor<AEnemyBase>(EnemyFactory, GetActorLocation(), GetActorRotation());
		//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner SpawnTest | Spawner=%s Location=%s Routes=%d"),
			//*GetNameSafe(this),
			//*GetActorLocation().ToString(),
			//EnemyRoutes.Num());
		if (AEnemyBase* Enemy = EnemyPool->SpawnFromPool(EnemyFactory, GetActorLocation(), GetActorRotation()))
		{
			AddActiveEnemy(Enemy);
			RestartEnemyLogic(Enemy);
		}
	}
}

void AEnemySpawner::SetEnemyPool(UEnemyPoolSubsystem* InEnemyPool)
{
	EnemyPool = InEnemyPool;
}

void AEnemySpawner::SetWaveData(UWaveData* InWaveData)
{
	WaveData = InWaveData;

	if (!WaveData)
	{
		return;
	}

	PreviewSpawnInterval = WaveData->PreviewSpawnInterval;
	CombatSpawnInterval = WaveData->CombatSpawnInterval;
	CombatBatchInterval = WaveData->CombatBatchInterval;
}

void AEnemySpawner::RemoveActiveEnemy(AEnemyBase* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	ActiveEnemies.Remove(Enemy);
	if (Enemy->OwningSpawner == this)
	{
		Enemy->OwningSpawner = nullptr;
	}
}

void AEnemySpawner::StartPreviewSpawn(int32 WaveNumber)
{
	if (!HasAuthority() || !EnemyPool)
	{
		return;
	}

	PreviewSpawnedCount = 0;
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	PrepareCombatSpawnPlans(WaveNumber); // 적이 진행할 랜덤 루트 배열
	if (CurrentWaveSpawnPlans.Num() == 0)
	{
		return;
	}

	if (InitialSpawnDelay > 0.0f)
	{
		if (PreviewSpawnInterval > 0.0f)
		{
			GetWorldTimerManager().SetTimer(
				SpawnTimerHandle,
				this,
				&AEnemySpawner::SpawnPreviewEnemy,
				PreviewSpawnInterval,
				true,
				InitialSpawnDelay
			);
		}
		else
		{
			GetWorldTimerManager().SetTimer(
				SpawnTimerHandle,
				this,
				&AEnemySpawner::SpawnPreviewEnemy,
				InitialSpawnDelay,
				false
			);
		}
		return;
	}

	SpawnPreviewEnemy();

	if (PreviewSpawnInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&AEnemySpawner::SpawnPreviewEnemy,
			PreviewSpawnInterval,
			true
		);
	}
}

void AEnemySpawner::StopPreviewSpawn()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
}

void AEnemySpawner::ClearPreviewEnemies()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	ReturnActiveEnemiesToPool();
}

void AEnemySpawner::SpawnPreviewEnemy()
{
	if (!HasAuthority() || !EnemyPool)
	{
		StopPreviewSpawn();
		return;
	}

	if (CurrentWaveSpawnPlans.Num() == 0)
	{
		StopPreviewSpawn();
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner SpawnPreviewEnemy | Spawner=%s Count=%d/%d"),
		//*GetNameSafe(this),
		//PreviewSpawnedCount + 1,
		//CurrentWaveSpawnPlans.Num());

	const int32 SpawnPlanIndex = PreviewSpawnedCount % CurrentWaveSpawnPlans.Num();
	if (!CurrentWaveSpawnPlans.IsValidIndex(SpawnPlanIndex))
	{
		StopPreviewSpawn();
		return;
	}

	const FEnemySpawnPlan& SpawnPlan = CurrentWaveSpawnPlans[SpawnPlanIndex];
	if (AEnemyBase* Enemy = EnemyPool->SpawnFromPool(SpawnPlan.EnemyClass, GetActorLocation(), GetActorRotation(), false))
	{
		AddActiveEnemy(Enemy);
		if (ApplySpawnPlanToEnemy(Enemy, SpawnPlanIndex) && RestartEnemyLogic(Enemy))
		{
			++PreviewSpawnedCount;
		}
		else
		{
			RemoveActiveEnemy(Enemy);
			EnemyPool->ReturnToPool(Enemy);
		}
	}
}

// Preview상태의 적들을 Pool로 되돌리고 Combat상태의 적들 스폰
void AEnemySpawner::StartCombatSpawn(int32 WaveNumber)
{
	if (!HasAuthority() || !EnemyPool)
	{
		if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
		{
			GameMode->NotifySpawnerFinished(this);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	ReturnActiveEnemiesToPool();

	CombatSpawnedCount = 0;
	CurrentCombatBatchRemaining = 0;
	CombatInitializedCount = 0;
	CombatInitializationFailedCount = 0;
	CombatInitializationRetryCount = 0;
	CombatSpawnTargetCount = PrepareCombatSpawnPlans(WaveNumber);

	//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner StartCombatSpawn | Spawner=%s TargetCount=%d"),
		//*GetNameSafe(this),
		//CombatSpawnTargetCount);

	if (CombatSpawnTargetCount <= 0)
	{
		if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
		{
			GameMode->NotifySpawnerFinished(this);
		}
		return;
	}

	if (InitialSpawnDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&AEnemySpawner::SpawnCombatBatch,
			InitialSpawnDelay,
			false
		);
		return;
	}

	SpawnCombatBatch();
}

void AEnemySpawner::EndWave()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	ReturnActiveEnemiesToPool();
	CombatSpawnedCount = 0;
	CombatSpawnTargetCount = 0;
	CurrentCombatBatchRemaining = 0;
	CombatInitializedCount = 0;
	CombatInitializationFailedCount = 0;
	CombatInitializationRetryCount = 0;
}

// 일정시간 간격으로 한마리씩 스폰하되, 배치 크기는 2~4개로 랜덤하게 정함.
void AEnemySpawner::SpawnCombatBatch()
{
	if (!HasAuthority() || !EnemyPool)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
		{
			GameMode->NotifySpawnerFinished(this);
		}
		return;
	}

	if (CombatSpawnedCount >= CombatSpawnTargetCount)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		/*UE_LOG(LogTemp, Warning, TEXT("EnemySpawner CombatInit Summary | Spawner=%s Initialized=%d Failed=%d Spawned=%d Target=%d"),
			*GetNameSafe(this),
			CombatInitializedCount,
			CombatInitializationFailedCount,
			CombatSpawnedCount,
			CombatSpawnTargetCount
		);*/
		if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
		{
			GameMode->NotifySpawnerFinished(this);
		}
		return;
	}

	const int32 RemainingCount = CombatSpawnTargetCount - CombatSpawnedCount;
	if (CurrentCombatBatchRemaining <= 0)
	{
		CurrentCombatBatchRemaining = FMath::Min(FMath::RandRange(2, 4), RemainingCount);
	}

	//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner SpawnCombatBatch | Spawner=%s BatchRemaining=%d Spawned=%d/%d Remaining=%d"),
		//*GetNameSafe(this),
		//CurrentCombatBatchRemaining,
		//CombatSpawnedCount,
		//CombatSpawnTargetCount,
		//RemainingCount);

	if (!CurrentWaveSpawnPlans.IsValidIndex(CombatSpawnedCount))
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
		{
			GameMode->NotifySpawnerFinished(this);
		}
		return;
	}

	const FEnemySpawnPlan& SpawnPlan = CurrentWaveSpawnPlans[CombatSpawnedCount];
	AEnemyBase* Enemy = EnemyPool->SpawnFromPool(SpawnPlan.EnemyClass, GetActorLocation(), GetActorRotation(), true);
	if (!Enemy)
	{
		const float RetryDelay = FMath::Max(CombatSpawnInterval, 0.1f);
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner SpawnCombatBatch retry | Spawner=%s Spawned=%d/%d RetryDelay=%.2f"),
			*GetNameSafe(this),
			CombatSpawnedCount,
			CombatSpawnTargetCount,
			RetryDelay);
		GetWorldTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&AEnemySpawner::SpawnCombatBatch,
			RetryDelay,
			false
		);
		return;
	}

	AddActiveEnemy(Enemy);
	const bool bAppliedSpawnPlan = ApplySpawnPlanToEnemy(Enemy, CombatSpawnedCount);
	Enemy->SetEnemyMode(EEnemyMode::Combat);
	const bool bInitComplete = bAppliedSpawnPlan && RestartEnemyLogic(Enemy);
	if (bInitComplete)
	{
		++CombatInitializedCount;
		CombatInitializationRetryCount = 0;
	}
	else
	{
		RemoveActiveEnemy(Enemy);
		EnemyPool->ReturnToPool(Enemy);
		++CombatInitializationRetryCount;

		if (CombatInitializationRetryCount <= MaxCombatInitializationRetries)
		{
			const float RetryDelay = FMath::Max(CombatSpawnInterval, 0.1f);
			UE_LOG(LogTemp, Warning, TEXT("EnemySpawner CombatInit retry | Spawner=%s Plan=%d/%d Attempt=%d/%d EnemyClass=%s"),
				*GetNameSafe(this),
				CombatSpawnedCount + 1,
				CombatSpawnTargetCount,
				CombatInitializationRetryCount,
				MaxCombatInitializationRetries,
				*GetNameSafe(SpawnPlan.EnemyClass));
			GetWorldTimerManager().SetTimer(
				SpawnTimerHandle,
				this,
				&AEnemySpawner::SpawnCombatBatch,
				RetryDelay,
				false
			);
			return;
		}

		++CombatInitializationFailedCount;
		CombatInitializationRetryCount = 0;
		UE_LOG(LogTemp, Error, TEXT("EnemySpawner CombatInit failed | Spawner=%s Plan=%d/%d EnemyClass=%s"),
			*GetNameSafe(this),
			CombatSpawnedCount + 1,
			CombatSpawnTargetCount,
			*GetNameSafe(SpawnPlan.EnemyClass));
	}

	/*UE_LOG(LogTemp, Warning, TEXT("EnemySpawner CombatInit | Spawner=%s Enemy=%s InitComplete=%d Initialized=%d Failed=%d SpawnedNext=%d/%d RouteApplied=%d HadRouteBeforeRestart=%d HasController=%d HasStateTree=%d Route=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Enemy),
		bInitComplete ? 1 : 0,
		CombatInitializedCount,
		CombatInitializationFailedCount,
		CombatSpawnedCount + 1,
		CombatSpawnTargetCount,
		bAppliedSpawnPlan ? 1 : 0,
		bHasRouteBeforeRestart ? 1 : 0,
		EnemyController ? 1 : 0,
		bHasStateTree ? 1 : 0,
		EnemyController ? *GetNameSafe(EnemyController->EnemyRoute) : TEXT("None")
	);*/
	if (bInitComplete)
	{
		if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
		{
			GameMode->NotifyEnemyActivated(Enemy);
		}
	}
	++CombatSpawnedCount;
	--CurrentCombatBatchRemaining;

	if (CombatSpawnedCount >= CombatSpawnTargetCount)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner CombatInit Summary | Spawner=%s Initialized=%d Failed=%d Spawned=%d Target=%d"),
			*GetNameSafe(this),
			CombatInitializedCount,
			CombatInitializationFailedCount,
			CombatSpawnedCount,
			CombatSpawnTargetCount
		);
		if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
		{
			GameMode->NotifySpawnerFinished(this);
		}
		return;
	}

	const float NextSpawnDelay = CurrentCombatBatchRemaining > 0 ? CombatSpawnInterval : CombatBatchInterval;
	GetWorldTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&AEnemySpawner::SpawnCombatBatch,
		NextSpawnDelay,
		false
	);
}

int32 AEnemySpawner::PrepareCombatSpawnPlans(int32 WaveNumber)
{
	if (!ShouldSpawnInWave(WaveNumber))
	{
		CurrentWaveSpawnPlans.Empty();
		PreparedWaveNumber = INDEX_NONE;
		return 0;
	}

	const int32 ExpectedPlanCount = WaveData ? GetWaveDataPlanCount(WaveNumber) : EnemyCount;
	if (PreparedWaveNumber != WaveNumber || CurrentWaveSpawnPlans.Num() != ExpectedPlanCount)
	{
		PreparedWaveNumber = WaveNumber;
		BuildCurrentWaveSpawnPlans();
	}

	UE_LOG(LogTemp, Warning, TEXT("[Spawner] %s Wave=%d Plans=%d"), *SpawnerId.ToString(), WaveNumber, CurrentWaveSpawnPlans.Num());

	return CurrentWaveSpawnPlans.Num();
}

int32 AEnemySpawner::GetCurrentWaveSpawnPlanCount() const
{
	return CurrentWaveSpawnPlans.Num();
}

bool AEnemySpawner::ShouldSpawnInWave(int32 WaveNumber) const
{
	if (WaveData)
	{
		return WaveData->FindSpawnerPlan(WaveNumber, SpawnerId) != nullptr;
	}

	return SpawnWaves.Num() == 0 || SpawnWaves.Contains(WaveNumber);
}

// 현재 맵에 나와있는 적들 Pool로 되돌리기. 적이 스폰된 스포너를 저장하고 있어서 해당 
void AEnemySpawner::ReturnActiveEnemiesToPool()
{
	if (!EnemyPool)
	{
		ActiveEnemies.Empty();
		return;
	}

	for (AEnemyBase* Enemy : ActiveEnemies)
	{
		if (Enemy)
		{
			if (Enemy->OwningSpawner == this)
			{
				Enemy->OwningSpawner = nullptr;
			}
			EnemyPool->ReturnToPool(Enemy);
		}
	}

	ActiveEnemies.Empty();
}

// 맵에 스폰된 적 배열에 추가 / 자신이 스폰된 스포너를 저장.
void AEnemySpawner::AddActiveEnemy(AEnemyBase* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	Enemy->OwningSpawner = this;
	ActiveEnemies.AddUnique(Enemy);
}

// 랜덤하게 갈 루트 순서 배열
void AEnemySpawner::BuildCurrentWaveSpawnPlans()
{
	CurrentWaveSpawnPlans.Empty();

	if (EnemyRoutes.Num() == 0)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner BuildSpawnPlans failed | Spawner=%s EnemyFactory=%s Routes=%d"),
			//*GetNameSafe(this),
			//*GetNameSafe(EnemyFactory),
			//EnemyRoutes.Num());
		return;
	}

	AEnemyRoute* PreviousRoute = nullptr;

	if (WaveData)
	{
		const FSpawnerWavePlan* SpawnerPlan = WaveData->FindSpawnerPlan(PreparedWaveNumber, SpawnerId);
		if (!SpawnerPlan)
		{
			return;
		}

		struct FWeightedEnemyCount
		{
			TSubclassOf<AEnemyBase> EnemyClass;
			int32 InitialCount = 0;
			int32 RemainingCount = 0;
			int32 CurrentWeight = 0;
		};

		TArray<FWeightedEnemyCount> WeightedEnemyCounts;
		int32 TotalWeight = 0;
		for (const FWaveEnemyCount& EnemyCountInfo : SpawnerPlan->EnemyCounts)
		{
			if (!EnemyCountInfo.EnemyClass || EnemyCountInfo.Count <= 0)
			{
				continue;
			}

			FWeightedEnemyCount WeightedEnemyCount;
			WeightedEnemyCount.EnemyClass = EnemyCountInfo.EnemyClass;
			WeightedEnemyCount.InitialCount = EnemyCountInfo.Count;
			WeightedEnemyCount.RemainingCount = EnemyCountInfo.Count;
			WeightedEnemyCounts.Add(WeightedEnemyCount);
			TotalWeight += EnemyCountInfo.Count;
		}

		for (int32 SpawnIndex = 0; SpawnIndex < TotalWeight; ++SpawnIndex)
		{
			int32 BestIndex = INDEX_NONE;
			for (int32 EnemyIndex = 0; EnemyIndex < WeightedEnemyCounts.Num(); ++EnemyIndex)
			{
				FWeightedEnemyCount& WeightedEnemyCount = WeightedEnemyCounts[EnemyIndex];
				if (WeightedEnemyCount.RemainingCount <= 0)
				{
					continue;
				}

				WeightedEnemyCount.CurrentWeight += WeightedEnemyCount.InitialCount;
				if (BestIndex == INDEX_NONE || WeightedEnemyCount.CurrentWeight > WeightedEnemyCounts[BestIndex].CurrentWeight)
				{
					BestIndex = EnemyIndex;
				}
			}

			if (BestIndex == INDEX_NONE)
			{
				break;
			}

			FWeightedEnemyCount& SelectedEnemyCount = WeightedEnemyCounts[BestIndex];
			SelectedEnemyCount.CurrentWeight -= TotalWeight;
			--SelectedEnemyCount.RemainingCount;

			FEnemySpawnPlan SpawnPlan;
			SpawnPlan.EnemyClass = SelectedEnemyCount.EnemyClass;
			SpawnPlan.Route = GetRandomRoute(PreviousRoute);
			CurrentWaveSpawnPlans.Add(SpawnPlan);
			PreviousRoute = SpawnPlan.Route;
		}

		return;
	}

	if (!EnemyFactory)
	{
		return;
	}

	for (int32 i = 0; i < EnemyCount; ++i)
	{
		FEnemySpawnPlan SpawnPlan;
		SpawnPlan.EnemyClass = EnemyFactory;
		SpawnPlan.Route = GetRandomRoute(PreviousRoute);
		CurrentWaveSpawnPlans.Add(SpawnPlan);
		PreviousRoute = SpawnPlan.Route;
	}

	//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner BuildSpawnPlans | Spawner=%s Plans=%d"),
		//*GetNameSafe(this),
		//CurrentWaveSpawnPlans.Num());
}

int32 AEnemySpawner::GetWaveDataPlanCount(int32 WaveNumber) const
{
	if (!WaveData)
	{
		return 0;
	}

	const FSpawnerWavePlan* SpawnerPlan = WaveData->FindSpawnerPlan(WaveNumber, SpawnerId);
	if (!SpawnerPlan)
	{
		return 0;
	}

	int32 TotalCount = 0;
	for (const FWaveEnemyCount& EnemyCountInfo : SpawnerPlan->EnemyCounts)
	{
		if (EnemyCountInfo.EnemyClass && EnemyCountInfo.Count > 0)
		{
			TotalCount += EnemyCountInfo.Count;
		}
	}

	return TotalCount;
}

// 랜덤한 루트 선정
AEnemyRoute* AEnemySpawner::GetRandomRoute(const AEnemyRoute* PreviousRoute) const
{
	if (EnemyRoutes.Num() == 0)
	{
		return nullptr;
	}

	if (EnemyRoutes.Num() == 1)
	{
		return EnemyRoutes[0];
	}

	TArray<AEnemyRoute*> CandidateRoutes;
	CandidateRoutes.Reserve(EnemyRoutes.Num());

	for (const TObjectPtr<AEnemyRoute>& EnemyRoute : EnemyRoutes)
	{
		if (EnemyRoute && EnemyRoute != PreviousRoute)
		{
			CandidateRoutes.Add(EnemyRoute.Get());
		}
	}

	if (CandidateRoutes.Num() == 0)
	{
		return nullptr;
	}

	const int32 RouteIndex = FMath::RandRange(0, CandidateRoutes.Num() - 1);
	return CandidateRoutes[RouteIndex];
}

// 적에게 루트 플랜에 해당하는 루트 참조시킴
bool AEnemySpawner::ApplySpawnPlanToEnemy(AEnemyBase* Enemy, int32 SpawnPlanIndex) const
{
	if (!Enemy || !CurrentWaveSpawnPlans.IsValidIndex(SpawnPlanIndex))
	{
		return false;
	}

	AEnemyController* EnemyController = Cast<AEnemyController>(Enemy->GetController());
	if (!EnemyController)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner ApplySpawnPlan | Controller null, SpawnDefaultController | Enemy=%s"),
			//*GetNameSafe(Enemy));
		Enemy->SpawnDefaultController();
		EnemyController = Cast<AEnemyController>(Enemy->GetController());
	}

	if (!EnemyController || EnemyController->GetPawn() != Enemy)
	{
		return false;
	}
	
	const FEnemySpawnPlan& SpawnPlan = CurrentWaveSpawnPlans[SpawnPlanIndex];
	EnemyController->EnemyRoute = SpawnPlan.Route;

	//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner ApplySpawnPlan | Enemy=%s Index=%d Route=%s"),
		//*GetNameSafe(Enemy),
		//SpawnPlanIndex,
		//*GetNameSafe(EnemyController->EnemyRoute));

	return EnemyController->EnemyRoute != nullptr;
}

// 플랜이 비어있을 때 직접 랜덤하게 루트 적용
void AEnemySpawner::AssignRandomRouteToEnemy(AEnemyBase* Enemy) const
{
	if (!Enemy || EnemyRoutes.Num() == 0)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner AssignRoute failed | Enemy=%s Routes=%d"),
			//*GetNameSafe(Enemy),
			//EnemyRoutes.Num());
		return;
	}

	AEnemyController* EnemyController = Cast<AEnemyController>(Enemy->GetController());
	if (!EnemyController)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner AssignRoute | Controller null, SpawnDefaultController | Enemy=%s"),
			//*GetNameSafe(Enemy));
		Enemy->SpawnDefaultController();
		EnemyController = Cast<AEnemyController>(Enemy->GetController());
	}

	if (!EnemyController || EnemyController->GetPawn() != Enemy)
	{
		return;
	}
	
	const int32 RouteIndex = FMath::RandRange(0, EnemyRoutes.Num() - 1);
	EnemyController->EnemyRoute = EnemyRoutes[RouteIndex];
	const int32 WaypointCount = EnemyController->EnemyRoute ? EnemyController->EnemyRoute->Waypoints.Num() : 0;

	//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner AssignRoute | Enemy=%s Controller=%s Route=%s RouteIndex=%d Waypoints=%d"),
		//*GetNameSafe(Enemy),
		//*GetNameSafe(EnemyController),
		//*GetNameSafe(EnemyController->EnemyRoute),
		//RouteIndex,
		//WaypointCount);
}

// 적의 StateTree 재시작
bool AEnemySpawner::RestartEnemyLogic(AEnemyBase* Enemy) const
{
	if (!Enemy)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner RestartLogic failed | Enemy null"));
		return false;
	}

	AEnemyController* EnemyController = Cast<AEnemyController>(Enemy->GetController());
	if (!EnemyController)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner RestartLogic | Controller null, SpawnDefaultController | Enemy=%s"),
			//*GetNameSafe(Enemy));
		Enemy->SpawnDefaultController();
		EnemyController = Cast<AEnemyController>(Enemy->GetController());
	}

	if (!EnemyController || EnemyController->GetPawn() != Enemy)
	{
		return false;
	}
	
	if (!EnemyController->EnemyRoute)
	{
		AssignRandomRouteToEnemy(Enemy);
	}
	if (!EnemyController->EnemyRoute)
	{
		return false;
	}
	
	if (EnemyController->StateTreeAIComp)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner RestartLogic | Enemy=%s Controller=%s"),
			//*GetNameSafe(Enemy),
			//*GetNameSafe(EnemyController));
		EnemyController->StateTreeAIComp->SetComponentTickEnabled(true);
		EnemyController->StateTreeAIComp->RestartLogic();
		return EnemyController->StateTreeAIComp->IsRunning();
	}

	return false;
}

void AEnemySpawner::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !Cast<ADefenseCharacter>(OtherActor))
	{
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("EnemySpawner Overlap | Spawner=%s OtherActor=%s OtherComp=%s Routes=%d"),
		//*GetNameSafe(this),
		//*GetNameSafe(OtherActor),
		//*GetNameSafe(OtherComp),
		//EnemyRoutes.Num());
	
	if (OtherActor && OtherActor != this && EnemyPool)
	{
		//GetWorld()->SpawnActor<AEnemyBase>(EnemyFactory, GetActorLocation(), GetActorRotation());
		AEnemyBase* Enemy = EnemyPool->SpawnFromPool(EnemyFactory, GetActorLocation(), GetActorRotation());
		AddActiveEnemy(Enemy);
		RestartEnemyLogic(Enemy);
	}
}
