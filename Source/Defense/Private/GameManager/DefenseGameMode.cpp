// Copyright Epic Games, Inc. All Rights Reserved.


#include "GameManager/DefenseGameMode.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemySpawner.h"
#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "Characters/Player/DefensePlayerController.h"
#include "Characters/Player/DefensePlayerState.h"
#include "EngineUtils.h"
#include "GameManager/DefenseGameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Traps/TrapBase.h"
#include "Traps/Grid/GridManager.h"

namespace
{
	const TCHAR* LexToString(const EEnemyRemoveReason Reason)
	{
		switch (Reason)
		{
		case EEnemyRemoveReason::Killed:
			return TEXT("Killed");
		case EEnemyRemoveReason::ReachedDestination:
			return TEXT("ReachedDestination");
		case EEnemyRemoveReason::InvalidState:
			return TEXT("InvalidState");
		case EEnemyRemoveReason::OutOfBounds:
			return TEXT("OutOfBounds");
		case EEnemyRemoveReason::ForcedCleanup:
			return TEXT("ForcedCleanup");
		default:
			return TEXT("Unknown");
		}
	}
}

ADefenseGameMode::ADefenseGameMode()
{
	PlayerStateClass = ADefensePlayerState::StaticClass();
	GameStateClass = ADefenseGameState::StaticClass();
}

void ADefenseGameMode::BeginPlay()
{
	Super::BeginPlay();

	// GridManager (없으면) 초기화
	for (TActorIterator<AGridManager> It(GetWorld()); It; ++It)
	{
		GridManager = *It;
		break;
	}

	if (!GridManager)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// 월드 원점 (0,0,0)에 Spawn
		GridManager = GetWorld()->SpawnActor<AGridManager>(
			AGridManager::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParams
		);
	}
}

// Spawner마다 init을 하면 적 배열이 원하는대로 안 생길 수 있어서 GameMode에서만 한 번 init하도록 함.
// Spawner의 BeginPlay에서 대입하는 변수에 접근시 타이밍상 문제 생길 수 있음.
// 여기서는 Controller에 있는 Route에 값을 대입하지 않도록 함.
void ADefenseGameMode::StartPlay()
{
	Super::StartPlay();

	DefenseGameState = GetGameState<ADefenseGameState>();
	if (DefenseGameState)
	{
		DefenseGameState->SetDestScore(InitialDestScore);
		DefenseGameState->CurrentWave = CurrentWave;
		DefenseGameState->MaxWave = MaxWave;
		DefenseGameState->CountdownRemaining = 0;
	}

	UEnemyPoolSubsystem* EnemyPool = GetWorld()->GetSubsystem<UEnemyPoolSubsystem>();
	if (!EnemyPool)
	{
		//UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode StartPlay failed | EnemyPool null"));
		return;
	}

	EnemySpawners.Empty();

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		AEnemySpawner::StaticClass(),
		FoundActors
	);

	for (AActor* Actor : FoundActors)
	{
		if (AEnemySpawner* Spawner = Cast<AEnemySpawner>(Actor))
		{
			EnemySpawners.Add(Spawner);
			Spawner->SetEnemyPool(EnemyPool);
			EnemyPool->InitPool(Spawner->EnemyFactory, Spawner->EnemyCount);
		}
	}

	SetGamePhase(EGamePhase::GameStart);
}

void ADefenseGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ADefensePlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ADefensePlayerState>() : nullptr;
	if (!PS) return;

	PS->SetCoin(InitCoin);
}

bool ADefenseGameMode::AreAllPlayersReady() const
{
	AGameStateBase* GS = GameState;
	if (!GS) return false;
	
	// 혼자 플레이할 때도 시작하게 하기 위해 주석처리함.
	//if (GS->PlayerArray.Num() < 2) return false;
	
	// 전체 Player 검사
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		const ADefensePlayerState* PS = Cast<ADefensePlayerState>(PlayerState);
		if (!PS || !PS->IsReady()) return false;
	}
	
	return true;
}

void ADefenseGameMode::ResetAllPlayersReady()
{
	AGameStateBase* GS = GameState;
	if (!GS) return;

	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		if (ADefensePlayerState* PS = Cast<ADefensePlayerState>(PlayerState))
		{
			PS->SetReady(false);
		}
	}
}

void ADefenseGameMode::GameStart()
{
	SetGamePhase(EGamePhase::Preparation);
}

void ADefenseGameMode::GameEnd()
{
	CleanupCurrentWave();

	if (DefenseGameState)
	{
		DefenseGameState->CountdownRemaining = 0;
		
		// TODO : UI만 갱신되면 여기선 필요 X
		//DefenseGameState->OnRep_CountdownRemaining();
	}
	
	// 모든 플레이어 레디 초기화
	ResetAllPlayersReady();
	
	bool bGameClear = false;
	// 모든 클라이언트에게 ShowGameEndUI 실행시키기
	if (DefenseGameState->DestScore > 0 && CurrentWave >= MaxWave)
	{
		bGameClear = true;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADefensePlayerController* PC = Cast<ADefensePlayerController>(It->Get()))
		{
			PC->ClientRPC_ShowGameEndUI(bGameClear);
		}
	}
	
	
}

void ADefenseGameMode::RetryGame()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADefensePlayerController* PC = Cast<ADefensePlayerController>(It->Get()))
		{
			PC->ClientRPC_HideGameEndUI();
		}
	}
	
	//const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	GetWorld()->ServerTravel("/Game/ThirdPerson/L_BetaMap");
}

// 플레이어가 G키(준비)를 누르면 호출됨 -> 모든 플레이어가 준비됐는지 확인하고 StartWave를 함.
void ADefenseGameMode::HandlePlayerReadyChanged()
{
	if (!DefenseGameState)
	{
		DefenseGameState = GetGameState<ADefenseGameState>();
	}

	if (!DefenseGameState)
	{
		return;
	}

	if (DefenseGameState->GamePhase == EGamePhase::GameEnded)
	{
		if (AreAllPlayersReady())
		{
			RetryGame();
		}

		return;
	}

	if (DefenseGameState->GamePhase != EGamePhase::Preparation)
	{
		return;
	}

	if (IsAutoStartWave(CurrentWave))
	{
		return;
	}

	if (AreAllPlayersReady() && !GetWorldTimerManager().IsTimerActive(ReadyWaveCountdownTimerHandle))
	{
		StartReadyWaveCountdown();
	}
}

void ADefenseGameMode::SetGamePhase(EGamePhase NewPhase)
{
	if (!DefenseGameState)
	{
		DefenseGameState = GetGameState<ADefenseGameState>();
	}
	
	if (!DefenseGameState || DefenseGameState->GamePhase == NewPhase)
	{
		return;
	}

	DefenseGameState->GamePhase = NewPhase;
	DefenseGameState->OnRep_GamePhase(); // 클라이언트에서 UI 갱신

	switch (NewPhase)
	{
	case EGamePhase::GameStart:
		GameStart();
		break;
	case EGamePhase::Preparation:
		Preparation();
		break;
	case EGamePhase::WaveStart:
		WaveStart();
		break;
	case EGamePhase::WaveEnded:
		WaveEnd();
		break;
	case EGamePhase::GameEnded:
		GameEnd();
		break;
	default:
		break;
	}
}

void ADefenseGameMode::Preparation()
{
	bIsWaveActive = false;
	CurrentEnemyCount = 0; // 맵에 남은 적 수 초기화
	ActiveWaveEnemies.Empty();
	ParticipatingSpawners.Empty();
	FinishedSpawners.Empty();
	ResetAllPlayersReady(); // 플레이어의 준비 초기화
	GetWorldTimerManager().ClearTimer(AutoWaveCountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(ReadyWaveCountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(EnemyCleanupTimerHandle);

	if (DefenseGameState)
	{
		DefenseGameState->CurrentWave = CurrentWave;
		DefenseGameState->OnRep_CurrentWave();
		DefenseGameState->MaxWave = MaxWave;
		DefenseGameState->CountdownRemaining = 0;
		DefenseGameState->OnRep_CountdownRemaining();
	}

	if (EnemySpawners.Num() == 0)
	{
		return;
	}

	if (IsAutoStartWave(CurrentWave))
	{
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode Preparation | Spawners=%d"), EnemySpawners.Num());

	for (AEnemySpawner* Spawner : EnemySpawners)
	{
		if (Spawner)
		{
			Spawner->StartPreviewSpawn(CurrentWave);
		}
	}
}


// Combat 상태의 적들 호출 
void ADefenseGameMode::WaveStart()
{
	if (bIsWaveActive)
	{
		return;
	}

	bIsWaveActive = true;
	GetWorldTimerManager().ClearTimer(AutoWaveCountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(ReadyWaveCountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(EnemyCleanupTimerHandle);
	ActiveWaveEnemies.Empty();
	ParticipatingSpawners.Empty();
	FinishedSpawners.Empty();

	if (DefenseGameState)
	{
		DefenseGameState->CurrentWave = CurrentWave;
		DefenseGameState->CountdownRemaining = 0;
		
		// TODO : UI 갱신만 하면 서버에서 필요없음.
		DefenseGameState->OnRep_CurrentWave();
		DefenseGameState->OnRep_CountdownRemaining();
		//-------------------------------------------
	}

	int32 PlannedEnemyCount = 0;
	for (AEnemySpawner* Spawner : EnemySpawners)
	{
		if (Spawner)
		{
			const int32 SpawnerPlanCount = Spawner->PrepareCombatSpawnPlans(CurrentWave);
			if (SpawnerPlanCount > 0)
			{
				ParticipatingSpawners.Add(Spawner);
				PlannedEnemyCount += SpawnerPlanCount;
			}
		}
	}
	CurrentEnemyCount = 0;

	/*const FString Message = FString::Printf(
		TEXT("All players ready. Start wave. | Wave=%d/%d ActiveEnemies=%d PlannedEnemies=%d Spawners=%d"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount,
		PlannedEnemyCount,
		ParticipatingSpawners.Num()
	);
	
	UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode WaveStart | Wave=%d/%d ActiveEnemies=%d PlannedEnemies=%d ParticipatingSpawners=%d"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount,
		PlannedEnemyCount,
		ParticipatingSpawners.Num()
	);*/
	
	//UKismetSystemLibrary::PrintString(this, Message, true, true, FLinearColor::Green, 3.0f);
	
	for (AEnemySpawner* Spawner : ParticipatingSpawners)
	{
		if (Spawner)
		{
			Spawner->StartCombatSpawn(CurrentWave);
		}
	}

	if (bEnableInvalidEnemyCleanup && InvalidEnemyCleanupInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			EnemyCleanupTimerHandle,
			this,
			&ADefenseGameMode::CleanupInvalidActiveEnemies,
			InvalidEnemyCleanupInterval,
			true
		);
	}

	TryFinishWave();
	
}

void ADefenseGameMode::WaveEnd()
{
	if (!bIsWaveActive)
	{
		return;
	}

	CleanupCurrentWave();
	
	// 게임오버시 리턴
	if (DefenseGameState->GamePhase == EGamePhase::GameEnded)
		return;
	/*const FString Message = FString::Printf(
		TEXT("Wave End | Wave=%d/%d RemainingEnemies=%d"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount
	);*/
	/*UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode WaveEnd | Wave=%d/%d RemainingEnemies=%d"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount
	);*/
	//UKismetSystemLibrary::PrintString(this, Message, true, true, FLinearColor::Green, 3.0f);

	AdvanceToNextWave();
}

void ADefenseGameMode::CleanupCurrentWave()
{
	bIsWaveActive = false;
	GetWorldTimerManager().ClearTimer(AutoWaveCountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(ReadyWaveCountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(EnemyCleanupTimerHandle);

	for (AEnemySpawner* Spawner : EnemySpawners)
	{
		if (Spawner)
		{
			Spawner->EndWave();
		}
	}

	ActiveWaveEnemies.Empty();
	ParticipatingSpawners.Empty();
	FinishedSpawners.Empty();
	CurrentEnemyCount = 0;
}

// 적의 수 감소 -> Destination에 overlap했을 때, 적이 처치됐을 때 호출
void ADefenseGameMode::DecreaseCurrentEnemyCount()
{
	CurrentEnemyCount = FMath::Max(0, CurrentEnemyCount - 1);

	/*UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode DecreaseCurrentEnemyCount fallback | Wave=%d/%d RemainingEnemies=%d ActiveEnemies=%d"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount,
		ActiveWaveEnemies.Num()
	);*/

	TryFinishWave();
}

void ADefenseGameMode::NotifyEnemyActivated(AEnemyBase* Enemy)
{
	if (!bIsWaveActive || !IsValid(Enemy) || Enemy->EnemyMode != EEnemyMode::Combat)
	{
		return;
	}

	ActiveWaveEnemies.Add(Enemy);
	CurrentEnemyCount = ActiveWaveEnemies.Num();

	/*UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode EnemyActivated | Wave=%d/%d ActiveEnemies=%d Enemy=%s Spawner=%s Location=%s"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount,
		*GetNameSafe(Enemy),
		*GetNameSafe(Enemy->OwningSpawner),
		*Enemy->GetActorLocation().ToString()
	);*/
}

void ADefenseGameMode::NotifyEnemyRemoved(AEnemyBase* Enemy, EEnemyRemoveReason Reason)
{
	if (!Enemy)
	{
		return;
	}

	AEnemySpawner* OwningSpawner = Enemy->OwningSpawner;
	const int32 RemovedCount = ActiveWaveEnemies.Remove(Enemy);
	if (OwningSpawner)
	{
		OwningSpawner->RemoveActiveEnemy(Enemy);
	}

	CurrentEnemyCount = ActiveWaveEnemies.Num();

	/*UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode EnemyRemoved | Wave=%d/%d ActiveEnemies=%d Removed=%d Reason=%s Enemy=%s Spawner=%s Location=%s"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount,
		RemovedCount,
		LexToString(Reason),
		*GetNameSafe(Enemy),
		*GetNameSafe(OwningSpawner),
		*Enemy->GetActorLocation().ToString()
	);*/

	TryFinishWave();
}

void ADefenseGameMode::NotifySpawnerFinished(AEnemySpawner* Spawner)
{
	if (!bIsWaveActive || !Spawner)
	{
		return;
	}

	if (!ParticipatingSpawners.Contains(Spawner))
	{
		return;
	}

	FinishedSpawners.Add(Spawner);

	/*UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode SpawnerFinished | Wave=%d/%d FinishedSpawners=%d/%d ActiveEnemies=%d Spawner=%s"),
		CurrentWave,
		MaxWave,
		FinishedSpawners.Num(),
		ParticipatingSpawners.Num(),
		ActiveWaveEnemies.Num(),
		*GetNameSafe(Spawner)
	);*/

	TryFinishWave();
}

void ADefenseGameMode::TryFinishWave()
{
	if (!bIsWaveActive)
	{
		return;
	}

	if (FinishedSpawners.Num() < ParticipatingSpawners.Num())
	{
		return;
	}

	if (ActiveWaveEnemies.Num() > 0)
	{
		LogActiveWaveEnemies();
		return;
	}

	SetGamePhase(EGamePhase::WaveEnded);
}

void ADefenseGameMode::ApplyDestinationDamage(int32 DamageAmount)
{
	if (!DefenseGameState)
	{
		DefenseGameState = GetGameState<ADefenseGameState>();
	}

	if (!DefenseGameState)
	{
		return;
	}

	const int32 NewDestScore = FMath::Max(0, DefenseGameState->DestScore - DamageAmount);
	DefenseGameState->SetDestScore(NewDestScore);

	if (NewDestScore <= 0)
	{
		SetGamePhase(EGamePhase::GameEnded);
	}
}

void ADefenseGameMode::AdvanceToNextWave()
{
	if (CurrentWave >= MaxWave)
	{
		SetGamePhase(EGamePhase::GameEnded);
		return;
	}

	++CurrentWave;

	if (DefenseGameState)
	{
		DefenseGameState->CurrentWave = CurrentWave;
		DefenseGameState->OnRep_CurrentWave();
	}

	if (IsAutoStartWave(CurrentWave))
	{
		SetGamePhase(EGamePhase::Preparation);
		StartAutoWaveCountdown();
		return;
	}

	SetGamePhase(EGamePhase::Preparation);
}

bool ADefenseGameMode::IsAutoStartWave(int32 WaveNumber) const
{
	return AutoStartWaves.Contains(WaveNumber);
}

void ADefenseGameMode::StartReadyWaveCountdown()
{
	GetWorldTimerManager().ClearTimer(ReadyWaveCountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(AutoWaveCountdownTimerHandle);

	for (AEnemySpawner* Spawner : EnemySpawners)
	{
		if (Spawner)
		{
			Spawner->ClearPreviewEnemies();
		}
	}

	if (ReadyStartCountdownSeconds <= 0)
	{
		HandleReadyWaveCountdownFinished();
		return;
	}

	if (DefenseGameState)
	{
		DefenseGameState->CountdownRemaining = ReadyStartCountdownSeconds;
		DefenseGameState->OnRep_CountdownRemaining();
	}

	GetWorldTimerManager().SetTimer(
		ReadyWaveCountdownTimerHandle,
		this,
		&ADefenseGameMode::HandleReadyWaveCountdownTick,
		1.0f,
		true
	);
}

void ADefenseGameMode::HandleReadyWaveCountdownTick()
{
	if (!DefenseGameState)
	{
		DefenseGameState = GetGameState<ADefenseGameState>();
	}

	if (!DefenseGameState)
	{
		GetWorldTimerManager().ClearTimer(ReadyWaveCountdownTimerHandle);
		return;
	}

	DefenseGameState->CountdownRemaining = FMath::Max(0, DefenseGameState->CountdownRemaining - 1);
	DefenseGameState->OnRep_CountdownRemaining();

	if (DefenseGameState->CountdownRemaining <= 0)
	{
		HandleReadyWaveCountdownFinished();
	}
}

void ADefenseGameMode::HandleReadyWaveCountdownFinished()
{
	GetWorldTimerManager().ClearTimer(ReadyWaveCountdownTimerHandle);

	if (DefenseGameState)
	{
		DefenseGameState->CountdownRemaining = 0;
		DefenseGameState->OnRep_CountdownRemaining();
	}

	SetGamePhase(EGamePhase::WaveStart);
}

void ADefenseGameMode::StartAutoWaveCountdown()
{
	GetWorldTimerManager().ClearTimer(AutoWaveCountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(ReadyWaveCountdownTimerHandle);
	ResetAllPlayersReady();

	if (AutoStartCountdownSeconds <= 0)
	{
		HandleAutoWaveCountdownFinished();
		return;
	}

	if (DefenseGameState)
	{
		DefenseGameState->CountdownRemaining = AutoStartCountdownSeconds;
		DefenseGameState->OnRep_CountdownRemaining();
	}

	GetWorldTimerManager().SetTimer(
		AutoWaveCountdownTimerHandle,
		this,
		&ADefenseGameMode::HandleAutoWaveCountdownTick,
		1.0f,
		true
	);
}

void ADefenseGameMode::HandleAutoWaveCountdownTick()
{
	if (!DefenseGameState)
	{
		DefenseGameState = GetGameState<ADefenseGameState>();
	}

	if (!DefenseGameState)
	{
		GetWorldTimerManager().ClearTimer(AutoWaveCountdownTimerHandle);
		return;
	}

	DefenseGameState->CountdownRemaining = FMath::Max(0, DefenseGameState->CountdownRemaining - 1);
	DefenseGameState->OnRep_CountdownRemaining();

	if (DefenseGameState->CountdownRemaining <= 0)
	{
		HandleAutoWaveCountdownFinished();
	}
}

void ADefenseGameMode::HandleAutoWaveCountdownFinished()
{
	GetWorldTimerManager().ClearTimer(AutoWaveCountdownTimerHandle);

	if (DefenseGameState)
	{
		DefenseGameState->CountdownRemaining = 0;
		DefenseGameState->OnRep_CountdownRemaining();
	}

	SetGamePhase(EGamePhase::WaveStart);
}

void ADefenseGameMode::CleanupInvalidActiveEnemies()
{
	if (!bIsWaveActive)
	{
		GetWorldTimerManager().ClearTimer(EnemyCleanupTimerHandle);
		return;
	}

	TArray<TObjectPtr<AEnemyBase>> EnemiesToRemove;
	TArray<EEnemyRemoveReason> RemoveReasons;

	for (AEnemyBase* Enemy : ActiveWaveEnemies)
	{
		if (!IsValid(Enemy))
		{
			EnemiesToRemove.Add(Enemy);
			RemoveReasons.Add(EEnemyRemoveReason::InvalidState);
			continue;
		}

		if (Enemy->EnemyMode != EEnemyMode::Combat)
		{
			EnemiesToRemove.Add(Enemy);
			RemoveReasons.Add(EEnemyRemoveReason::InvalidState);
			continue;
		}

		const FVector EnemyLocation = Enemy->GetActorLocation();
		if (EnemyLocation.Z <= InvalidEnemyKillZ)
		{
			EnemiesToRemove.Add(Enemy);
			RemoveReasons.Add(EEnemyRemoveReason::OutOfBounds);
			continue;
		}

		if (MaxDistanceFromOwningSpawner > 0.0f && Enemy->OwningSpawner)
		{
			const float MaxDistanceSq = FMath::Square(MaxDistanceFromOwningSpawner);
			if (FVector::DistSquared(EnemyLocation, Enemy->OwningSpawner->GetActorLocation()) > MaxDistanceSq)
			{
				EnemiesToRemove.Add(Enemy);
				RemoveReasons.Add(EEnemyRemoveReason::ForcedCleanup);
			}
		}
	}

	UEnemyPoolSubsystem* EnemyPool = GetWorld() ? GetWorld()->GetSubsystem<UEnemyPoolSubsystem>() : nullptr;
	for (int32 Index = 0; Index < EnemiesToRemove.Num(); ++Index)
	{
		AEnemyBase* Enemy = EnemiesToRemove[Index];
		if (!Enemy)
		{
			continue;
		}

		const EEnemyRemoveReason Reason = RemoveReasons.IsValidIndex(Index)
			? RemoveReasons[Index]
			: EEnemyRemoveReason::ForcedCleanup;
		NotifyEnemyRemoved(Enemy, Reason);

		if (EnemyPool)
		{
			EnemyPool->ReturnToPool(Enemy);
		}
	}
}

void ADefenseGameMode::LogActiveWaveEnemies() const
{
	/*UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode Wave still active | Wave=%d/%d ActiveEnemies=%d FinishedSpawners=%d/%d"),
		CurrentWave,
		MaxWave,
		ActiveWaveEnemies.Num(),
		FinishedSpawners.Num(),
		ParticipatingSpawners.Num()
	);*/

	for (const AEnemyBase* Enemy : ActiveWaveEnemies)
	{
		if (!IsValid(Enemy))
		{
			//UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode RemainingEnemy | Invalid enemy reference"));
			continue;
		}

		/*UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode RemainingEnemy | Enemy=%s State=%d Mode=%d Spawner=%s Location=%s"),
			*GetNameSafe(Enemy),
			static_cast<int32>(Enemy->EnemyState),
			static_cast<int32>(Enemy->EnemyMode),
			*GetNameSafe(Enemy->OwningSpawner),
			*Enemy->GetActorLocation().ToString()
		);*/
	}
}

int32 ADefenseGameMode::GetCurrentWave()
{
	return CurrentWave;
}

int32 ADefenseGameMode::GetMaxWave()
{
	return MaxWave;
}
	
void ADefenseGameMode::AwardEnemyKillCoin(class AEnemyBase* Enemy, AActor* DamageCauser, AController* EventInstigator)
{
	if (!HasAuthority() || !Enemy) return;
	
	const int32 RewardCoin = Enemy->KillCoinReward;
	if (RewardCoin <= 0) return;
	
	ADefensePlayerState* RewardTarget = nullptr;
	
	if (const ATrapBase* Trap = Cast<ATrapBase>(DamageCauser))
	{
		RewardTarget = Trap->GetOwnerPS();
	}
	else if (EventInstigator)
	{
		RewardTarget = EventInstigator->GetPlayerState<ADefensePlayerState>();
	}
	else if (const APawn* DamageCauserPawn = Cast<APawn>(DamageCauser))
	{
		RewardTarget = DamageCauserPawn->GetPlayerState<ADefensePlayerState>();
	}

	if (!RewardTarget) return;

	RewardTarget->AddCoin(RewardCoin);
}
