// Copyright Epic Games, Inc. All Rights Reserved.


#include "GameManager/DefenseGameMode.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemySpawner.h"
#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "Characters/Enemy/Data/WaveData.h"
#include "Characters/Player/DefensePlayerController.h"
#include "Characters/Player/DefensePlayerState.h"
#include "EngineUtils.h"
#include "Characters/Player/DefenseCharacter.h"
#include "GameManager/DefenseGameInstance.h"
#include "GameManager/DefenseGameState.h"
#include "GameManager/Portal.h"
#include "GameManager/DefenseSpectatorController.h"
#include "GameManager/Data/MapConfigData.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Traps/TrapBase.h"
#include "Mission/MissionRunTrackerComponent.h"

namespace
{
	constexpr int32 MaxPlayablePlayers = 3;

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
	SpectatorPlayerControllerClass = ADefenseSpectatorController::StaticClass();

	MissionRunTrackerComponent =
		CreateDefaultSubobject<UMissionRunTrackerComponent>(
			TEXT("MissionRunTrackerComponent"));
}

void ADefenseGameMode::ApplyDataAssets()
{
	if (const UDefenseGameInstance* DefenseGameInstance = GetGameInstance<UDefenseGameInstance>())
	{
		if (const UMapConfigData* SelectedMapConfigData = DefenseGameInstance->GetSelectedMapConfigData())
		{
			InitialDestScore = SelectedMapConfigData->InitialDestScore;
			InitCoin = SelectedMapConfigData->InitCoin;
			WaveData = SelectedMapConfigData->WaveData;

			if (MissionRunTrackerComponent)
			{
				MissionRunTrackerComponent->InitializeMissions(SelectedMapConfigData->Missions);
			}
		}
	}

	if (WaveData)
	{
		MaxWave = WaveData->MaxWave;
		AutoStartWaves = WaveData->AutoStartWaves;
	}

	//UE_LOG(LogTemp, Warning, TEXT("[MapConfig] Dest=%d Coin=%d WaveData=%s"), InitialDestScore, InitCoin, *GetNameSafe(WaveData));
}

void ADefenseGameMode::StartPlay()
{
	Super::StartPlay();

	if (!HasAuthority())
	{
		return;
	}

	ApplyDataAssets();

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
			Spawner->SetWaveData(WaveData);
		}
	}

	bHasStartPlayInitialized = true;
	TryStartGameAfterPlayerJoined();
}

void ADefenseGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!HasAuthority())
	{
		return;
	}

	ApplyDataAssets();

	ADefensePlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ADefensePlayerState>() : nullptr;
	if (PS)
	{
		PS->SetCoin(InitCoin);
	}

	AssignGameRole(NewPlayer);
	TryStartGameAfterPlayerJoined();
}

APlayerController* ADefenseGameMode::SpawnPlayerController(ENetRole InRemoteRole, const FString& Options)
{
	if (ShouldSpawnSpectatorController() && SpectatorPlayerControllerClass)
	{
		return SpawnPlayerControllerCommon(
			InRemoteRole,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpectatorPlayerControllerClass
		);
	}

	return Super::SpawnPlayerController(InRemoteRole, Options);
}

void ADefenseGameMode::RestartPlayer(AController* NewPlayer)
{
	if (Cast<ADefenseSpectatorController>(NewPlayer))
	{
		return;
	}

	Super::RestartPlayer(NewPlayer);
}

void ADefenseGameMode::Logout(AController* Exiting)
{
	const ADefensePlayerState* ExitingPlayerState = Exiting ? Exiting->GetPlayerState<ADefensePlayerState>() : nullptr;
	const bool bWasHost = ExitingPlayerState && ExitingPlayerState->IsHost();

	Super::Logout(Exiting);

	if (bWasHost)
	{
		PromoteRemainingGuestToHost();
	}
}

bool ADefenseGameMode::AreAllPlayersReady() const
{
	AGameStateBase* GS = GameState;
	if (!GS) return false;
	
	// 꼭 2인 플레이로 만들려면
	//if (GS->PlayerArray.Num() < 2) return false;
	
	// 전체 Player 검사
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		const ADefensePlayerState* PS = Cast<ADefensePlayerState>(PlayerState);
		if (!PS)
		{
			return false;
		}

		if (PS->GetGameRole() == EDefensePlayerRole::Spectator)
		{
			continue;
		}

		if (!PS->IsReady())
		{
			return false;
		}
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

bool ADefenseGameMode::AreAllActivePlayersDead() const
{
	for (FConstPlayerControllerIterator It =
		GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const ADefenseCharacter* Character = PlayerController
			? Cast<ADefenseCharacter>(PlayerController->GetPawn())
			: nullptr;

		if (!Character)
		{
			continue;
		}

		UStatusComponent* StatusComp = Character->GetStatusComp();
		if (!StatusComp || StatusComp->IsAlive())
		{
			return false;
		}
	}
	return true;
}

void ADefenseGameMode::NotifyPlayerDied(ADefenseCharacter* DeadCharacter)
{
	if (!HasAuthority() || !IsValid(DeadCharacter)) return;

	UStatusComponent* StatusComp = DeadCharacter->GetStatusComp();
	if (!StatusComp || StatusComp->IsAlive()) return;

	if (!DefenseGameState || DefenseGameState->GamePhase == EGamePhase::GameEnded) return;

	AController* Controller = DeadCharacter->GetController();
	if (!Controller) return;

	if (AreAllActivePlayersDead())
	{
		SetGamePhase(EGamePhase::GameEnded);
		return;
	}

	FTimerHandle RespawnTimerHandle;
	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindUObject(
		this,
		&ADefenseGameMode::RespawnDeadPlayer,
		Controller
	);

	GetWorldTimerManager().SetTimer(
		RespawnTimerHandle,
		RespawnDelegate,
		RespawnDelay,
		false
	);
}

void ADefenseGameMode::RespawnDeadPlayer(AController* Controller)
{
	if (!IsValid(Controller)) return;
	if (!DefenseGameState || DefenseGameState->GamePhase == EGamePhase::GameEnded) return;

	ADefenseCharacter* Character = Cast<ADefenseCharacter>(Controller->GetPawn());
	if (!IsValid(Character)) return;

	UStatusComponent* StatusComp = Character->GetStatusComp();
	if (!StatusComp || StatusComp->IsAlive()) return;

	APortal* Destination = nullptr;
	for (TActorIterator<APortal> It(GetWorld()); It; ++It)
	{
		Destination = *It;
		break;
	}

	if (!Destination) return;

	const FRotator DestinationRotation = Destination->GetActorRotation();
	const FTransform RespawnTransform(
		FRotator(0.f, DestinationRotation.Yaw, 0.f),
		Destination->GetActorLocation(),
		FVector::OneVector
	);

	Character->SetActorTransform(RespawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	StatusComp->Revive();
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
		DefenseGameState->SetReadyInputRequired(false);
		
	}
	
	// 모든 플레이어 레디 초기화
	ResetAllPlayersReady();
	
	bool bGameClear = false;
	
	if (DefenseGameState
		&& !AreAllActivePlayersDead()
		&& DefenseGameState->DestScore > 0
		&& CurrentWave >= MaxWave)
	{
		bGameClear = true;
	}
	
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADefensePlayerController* PC = Cast<ADefensePlayerController>(It->Get()))
		{
			ADefensePlayerState* PlayerState = PC->GetPlayerState<ADefensePlayerState>();

			TArray<FMissionCompletionResult> Results;

			if (MissionRunTrackerComponent && PlayerState)
			{
				Results = MissionRunTrackerComponent->CollectMissionResults(PlayerState, bGameClear);
			}

			// 미션, 종료 UI 표시
			PC->ClientRPC_ShowGameEndUI(bGameClear, Results);
		}
	}
	
	
}

void ADefenseGameMode::RetryGame()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADefensePlayerController* PC = Cast<ADefensePlayerController>(It->Get()))
		{
			PC->ClientRPC_ShowEndLoadingUI();
		}
	}
	
	//const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	//GetWorld()->ServerTravel("/Game/ThirdPerson/L_BetaMap");
	GetWorld()->ServerTravel(GetWorld()->URL.Map);
}

void ADefenseGameMode::HandleGameEndRetryRequested(APlayerController* RequestingPlayer)
{
	if (!HasAuthority() || !RequestingPlayer || !DefenseGameState)
	{
		return;
	}

	if (DefenseGameState->GamePhase != EGamePhase::GameEnded)
	{
		return;
	}

	if (!IsHostPlayer(RequestingPlayer))
	{
		return;
	}

	RetryGame();
}

void ADefenseGameMode::HandleReturnToIntroMapRequested(APlayerController* RequestingPlayer)
{
	if (!HasAuthority() || !RequestingPlayer || !IsHostPlayer(RequestingPlayer))
	{
		return;
	}

	const UDefenseGameInstance* DefenseGameInstance = GetGameInstance<UDefenseGameInstance>();
	const FString IntroMapPackageName = DefenseGameInstance
		? DefenseGameInstance->GetIntroMapPackageName()
		: FString();

	if (IntroMapPackageName.IsEmpty())
	{
		return;
	}

	ADefensePlayerState* HostPlayerState = nullptr;
	TArray<APlayerState*> GuestPlayerStates;
	if (GameState)
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (ADefensePlayerState* DefensePlayerState = Cast<ADefensePlayerState>(PlayerState))
			{
				if (DefensePlayerState->IsHost())
				{
					HostPlayerState = DefensePlayerState;
				}
				else if (DefensePlayerState->IsGuest())
				{
					GuestPlayerStates.Add(DefensePlayerState);
				}
			}
		}
	}

	if (UDefenseGameInstance* MutableDefenseGameInstance = GetGameInstance<UDefenseGameInstance>())
	{
		MutableDefenseGameInstance->SaveIntroPlayerRoles(HostPlayerState, GuestPlayerStates);
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADefensePlayerController* PC = Cast<ADefensePlayerController>(It->Get()))
		{
			PC->ClientRPC_ShowESCLoadingUI();
		}
	}

	GetWorld()->ServerTravel(IntroMapPackageName);
}

void ADefenseGameMode::HandleClientIdentitySubmitted(APlayerController* PlayerController)
{
	AssignGameRole(PlayerController);
}

bool ADefenseGameMode::IsHostPlayer(APlayerController* PlayerController) const
{
	if (!PlayerController)
	{
		return false;
	}

	if (const ADefensePlayerState* DefensePlayerState = PlayerController->GetPlayerState<ADefensePlayerState>())
	{
		return DefensePlayerState->IsHost();
	}

	return false;
}

void ADefenseGameMode::AssignGameRole(APlayerController* NewPlayer)
{
	if (!HasAuthority() || !NewPlayer)
	{
		return;
	}

	ADefensePlayerState* DefensePlayerState = NewPlayer->GetPlayerState<ADefensePlayerState>();
	if (!DefensePlayerState)
	{
		return;
	}

	const UDefenseGameInstance* DefenseGameInstance = GetGameInstance<UDefenseGameInstance>();
	if (DefenseGameInstance && DefenseGameInstance->IsSavedHostPlayerState(DefensePlayerState))
	{
		DefensePlayerState->SetGameRole(EDefensePlayerRole::Host);
		return;
	}

	if (DefenseGameInstance && DefenseGameInstance->IsSavedGuestPlayerState(DefensePlayerState))
	{
		DefensePlayerState->SetGameRole(EDefensePlayerRole::Guest);
		return;
	}

	if (Cast<ADefenseSpectatorController>(NewPlayer) || bHasGameStarted)
	{
		DefensePlayerState->SetGameRole(EDefensePlayerRole::Spectator);
		return;
	}

	bool bHasHost = false;
	int32 PlayablePlayerCount = 0;
	if (GameState)
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (const ADefensePlayerState* ExistingDefensePlayerState = Cast<ADefensePlayerState>(PlayerState))
			{
				if (ExistingDefensePlayerState->GetGameRole() != EDefensePlayerRole::Spectator)
				{
					++PlayablePlayerCount;
				}
				bHasHost |= ExistingDefensePlayerState->IsHost();
			}
		}
	}

	if (!bHasHost)
	{
		DefensePlayerState->SetGameRole(EDefensePlayerRole::Host);
		return;
	}

	DefensePlayerState->SetGameRole(
		PlayablePlayerCount <= MaxPlayablePlayers
			? EDefensePlayerRole::Guest
			: EDefensePlayerRole::Spectator);
}

bool ADefenseGameMode::ShouldSpawnSpectatorController() const
{
	if (!bHasGameStarted)
	{
		return false;
	}

	int32 PlayablePlayerCount = 0;
	if (GameState)
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			const ADefensePlayerState* DefensePlayerState = Cast<ADefensePlayerState>(PlayerState);
			if (DefensePlayerState && DefensePlayerState->GetGameRole() != EDefensePlayerRole::Spectator)
			{
				++PlayablePlayerCount;
			}
		}
	}

	if (PlayablePlayerCount >= MaxPlayablePlayers)
	{
		return true;
	}

	const UDefenseGameInstance* DefenseGameInstance = GetGameInstance<UDefenseGameInstance>();
	if (!DefenseGameInstance)
	{
		return true;
	}

	const int32 SavedPlayablePlayerCount = DefenseGameInstance->GetSavedPlayablePlayerCount();
	return SavedPlayablePlayerCount <= 0 || PlayablePlayerCount >= SavedPlayablePlayerCount;
}

void ADefenseGameMode::PromoteRemainingGuestToHost()
{
	if (!HasAuthority() || !GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (ADefensePlayerState* DefensePlayerState = Cast<ADefensePlayerState>(PlayerState))
		{
			if (DefensePlayerState->IsGuest() || DefensePlayerState->GetGameRole() == EDefensePlayerRole::None)
			{
				DefensePlayerState->SetGameRole(EDefensePlayerRole::Host);
				return;
			}
		}
	}
}

// 플레이어가 G키(준비)를 누르면 호출됨 -> 모든 플레이어가 준비됐는지 확인하고 StartWave를 함.
void ADefenseGameMode::HandlePlayerReadyChanged()
{
	if (!HasAuthority())
	{
		return;
	}

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
	if (!HasAuthority())
	{
		return;
	}

	if (!DefenseGameState)
	{
		DefenseGameState = GetGameState<ADefenseGameState>();
	}
	
	if (!DefenseGameState || DefenseGameState->GamePhase == NewPhase)
	{
		return;
	}

	DefenseGameState->GamePhase = NewPhase;

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

void ADefenseGameMode::TryStartGameAfterPlayerJoined()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bHasGameStarted)
	{
		return;
	}

	if (!DefenseGameState)
	{
		DefenseGameState = GetGameState<ADefenseGameState>();
	}

	if (!bHasStartPlayInitialized || !DefenseGameState || DefenseGameState->PlayerArray.Num() < 1)
	{
		return;
	}

	bHasGameStarted = true;
	SetGamePhase(EGamePhase::GameStart);
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

	const bool bReadyInputRequired = !IsAutoStartWave(CurrentWave);

	if (DefenseGameState)
	{
		DefenseGameState->CurrentWave = CurrentWave;
		DefenseGameState->OnRep_CurrentWave();
		DefenseGameState->MaxWave = MaxWave;
		DefenseGameState->CountdownRemaining = 0;
		DefenseGameState->OnRep_CountdownRemaining();
		DefenseGameState->SetReadyInputRequired(bReadyInputRequired);
	}

	if (!bReadyInputRequired)
	{
		return;
	}

	// 초기화 웨이브 player hp reset
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ADefenseCharacter* Character = Cast<ADefenseCharacter>(It->Get()->GetPawn());
		if (!Character) continue;

		if (UStatusComponent* StatusComp = Character->GetStatusComp())
		{
			StatusComp->Heal(StatusComp->MaxHealth);
		}
	}

	if (EnemySpawners.Num() == 0)
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

	if (CurrentWave == 1 && MissionRunTrackerComponent)
	{
		// 첫 웨이브 전투 시작 시 진행도와 플레이 시간을 초기화한다.
		MissionRunTrackerComponent->BeginRun();
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
		/*DefenseGameState->OnRep_CurrentWave();
		DefenseGameState->OnRep_CountdownRemaining();*/
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
	if (!HasAuthority())
	{
		return;
	}

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
	if (!HasAuthority())
	{
		return;
	}

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
	if (!HasAuthority())
	{
		return;
	}

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
	if (!HasAuthority())
	{
		return;
	}

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
	if (!HasAuthority())
	{
		return;
	}

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
	if (!HasAuthority())
	{
		return;
	}

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

	if (DefenseGameState)
	{
		DefenseGameState->SetReadyInputRequired(false);
	}

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

ADefensePlayerState* ADefenseGameMode::ResolveEnemyKillOwner(AActor* DamageCauser, AController* EventInstigator) const
{
	if (const ATrapBase* Trap = Cast<ATrapBase>(DamageCauser))
	{
		return Trap->GetOwnerPS();
	}

	if (EventInstigator)
	{
		if (ADefensePlayerState* PlayerState = EventInstigator->GetPlayerState<ADefensePlayerState>())
		{
			return PlayerState;
		}
	}

	if (const APawn* DamageCauserPawn = Cast<APawn>(DamageCauser))
	{
		return DamageCauserPawn->GetPlayerState<ADefensePlayerState>();
	}

	return nullptr;
}

int32 ADefenseGameMode::GetCurrentWave()
{
	return CurrentWave;
}

int32 ADefenseGameMode::GetMaxWave()
{
	return MaxWave;
}

ADefensePlayerState* ADefenseGameMode::HandleEnemyKilled(AEnemyBase* Enemy, AActor* DamageCauser, AController* EventInstigator)
{
	if (!HasAuthority() || !IsValid(Enemy))
	{
		return nullptr;
	}

	ADefensePlayerState* KillerPlayerState = ResolveEnemyKillOwner(DamageCauser, EventInstigator);

	if (!KillerPlayerState)
	{
		return nullptr;
	}

	// 임시 로그
	/*
	const bool bTrapKill = Cast<ATrapBase>(DamageCauser) != nullptr;
	const bool bInstigatorKill =
		!bTrapKill
		&& EventInstigator
		&& EventInstigator->GetPlayerState<ADefensePlayerState>()
			== KillerPlayerState;

	const TCHAR* KillSource = bTrapKill
		? TEXT("Trap")
		: bInstigatorKill
			? TEXT("EventInstigator")
			: TEXT("DamageCauserPawn");

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Mission][Kill] Player=%s PlayerState=%s Source=%s Causer=%s Enemy=%s Tags=%s"),
		*KillerPlayerState->GetPlayerName(),
		*GetNameSafe(KillerPlayerState),
		KillSource,
		*GetNameSafe(DamageCauser),
		*GetNameSafe(Enemy),
		*Enemy->GetEnemyTags().ToStringSimple());
	*/

	if (MissionRunTrackerComponent)
	{
		MissionRunTrackerComponent->RecordEnemyKill(KillerPlayerState, Enemy->GetEnemyTags());
	}

	if (Enemy->KillCoinReward > 0)
	{
		KillerPlayerState->AddCoin(Enemy->KillCoinReward);
	}

	return KillerPlayerState;
}
