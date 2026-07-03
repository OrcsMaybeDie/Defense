// Copyright Epic Games, Inc. All Rights Reserved.


#include "GameManager/DefenseGameMode.h"

#include "Characters/Enemy/EnemySpawner.h"
#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "Characters/Player/DefensePlayerState.h"
#include "GameManager/DefenseGameState.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

ADefenseGameMode::ADefenseGameMode()
{
	PlayerStateClass = ADefensePlayerState::StaticClass();
	GameStateClass = ADefenseGameState::StaticClass();
}

void ADefenseGameMode::BeginPlay()
{
	Super::BeginPlay();
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
	GetWorldTimerManager().ClearTimer(AutoWaveCountdownTimerHandle);
	bIsWaveActive = false;

	if (DefenseGameState)
	{
		DefenseGameState->CountdownRemaining = 0;
		
		// TODO : UI만 갱신되면 여기선 필요 X
		DefenseGameState->OnRep_CountdownRemaining();
	}
	WaveEnd();
	const FString Message = TEXT("Game End");
	UKismetSystemLibrary::PrintString(this, Message, true, true, FLinearColor::Green, 3.0f);
}

// 플레이어가 G키(준비)를 누르면 호출됨 -> 모든 플레이어가 준비됐는지 확인하고 StartWave를 함.
void ADefenseGameMode::HandlePlayerReadyChanged()
{
	if (!DefenseGameState)
	{
		DefenseGameState = GetGameState<ADefenseGameState>();
	}

	if (!DefenseGameState || DefenseGameState->GamePhase != EGamePhase::Preparation || IsAutoStartWave(CurrentWave))
	{
		return;
	}

	if (AreAllPlayersReady())
	{
		SetGamePhase(EGamePhase::WaveStart);
	}
}

void ADefenseGameMode::SetGamePhase(EGamePhase NewPhase)
{
	if (!DefenseGameState)
	{
		DefenseGameState = GetGameState<ADefenseGameState>();
	}

	if (!DefenseGameState)
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
	default:
		break;
	}
}

void ADefenseGameMode::Preparation()
{
	bIsWaveActive = false;
	CurrentEnemyCount = 0; // 맵에 남은 적 수 초기화
	ResetAllPlayersReady(); // 플레이어의 준비 초기화
	GetWorldTimerManager().ClearTimer(AutoWaveCountdownTimerHandle);

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

	//UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode Preparation | Spawners=%d"), EnemySpawners.Num());

	for (AEnemySpawner* Spawner : EnemySpawners)
	{
		if (Spawner)
		{
			Spawner->StartPreviewSpawn();
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

	if (DefenseGameState)
	{
		DefenseGameState->CurrentWave = CurrentWave;
		DefenseGameState->CountdownRemaining = 0;
		
		// TODO : UI 갱신만 하면 서버에서 필요없음.
		DefenseGameState->OnRep_CurrentWave();
		DefenseGameState->OnRep_CountdownRemaining();
		//-------------------------------------------
	}

	// 스폰 되어야할 총 적의 수
	CurrentEnemyCount = 0;
	for (const AEnemySpawner* Spawner : EnemySpawners)
	{
		if (Spawner)
		{
			CurrentEnemyCount += Spawner->EnemyCount;
		}
	}

	const FString Message = FString::Printf(
		TEXT("All players ready. Start wave. | Wave=%d/%d RemainingEnemies=%d"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount
	);
	
	UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode WaveStart | Wave=%d/%d RemainingEnemies=%d"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount
	);
	
	UKismetSystemLibrary::PrintString(this, Message, true, true, FLinearColor::Green, 3.0f);
	
	if (EnemySpawners.Num() != 0)
	{
		for (auto* spawner : EnemySpawners)
		{
			spawner->StartCombatSpawn();
		}
	}
	
}

void ADefenseGameMode::WaveEnd()
{
	if (!bIsWaveActive)
	{
		return;
	}

	bIsWaveActive = false;
	GetWorldTimerManager().ClearTimer(AutoWaveCountdownTimerHandle);

	for (AEnemySpawner* Spawner : EnemySpawners)
	{
		if (Spawner)
		{
			Spawner->EndWave();
		}
	}
	
	// 게임오버시 리턴
	if (DefenseGameState->GamePhase == EGamePhase::GameEnded)
		return;
	const FString Message = FString::Printf(
		TEXT("Wave End | Wave=%d/%d RemainingEnemies=%d"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount
	);
	UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode WaveEnd | Wave=%d/%d RemainingEnemies=%d"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount
	);
	UKismetSystemLibrary::PrintString(this, Message, true, true, FLinearColor::Green, 3.0f);

	AdvanceToNextWave();
}

// 적의 수 감소 -> Destination에 overlap했을 때, 적이 처치됐을 때 호출
void ADefenseGameMode::DecreaseCurrentEnemyCount()
{
	CurrentEnemyCount = FMath::Max(0, CurrentEnemyCount - 1);

	UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode EnemyCount decreased | Wave=%d/%d RemainingEnemies=%d"),
		CurrentWave,
		MaxWave,
		CurrentEnemyCount
	);

	if (CurrentEnemyCount == 0)
	{
		SetGamePhase(EGamePhase::WaveEnded);
	}
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
		GameEnd();
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

void ADefenseGameMode::StartAutoWaveCountdown()
{
	GetWorldTimerManager().ClearTimer(AutoWaveCountdownTimerHandle);
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

int32 ADefenseGameMode::GetCurrentWave()
{
	return CurrentWave;
}

int32 ADefenseGameMode::GetMaxWave()
{
	return MaxWave;
}
