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

	StartPreview();
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

// 플레이어가 G키(준비)를 누르면 호출됨 -> 모든 플레이어가 준비됐는지 확인하고 StartWave를 함.
void ADefenseGameMode::HandlePlayerReadyChanged()
{
	if (AreAllPlayersReady())
	{
		StartWave();
	}
}

// Preview 상태의 적들 호출
void ADefenseGameMode::StartPreview()
{
	if (EnemySpawners.Num() == 0)
	{
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode StartPreview | Spawners=%d"), EnemySpawners.Num());

	for (AEnemySpawner* Spawner : EnemySpawners)
	{
		if (Spawner)
		{
			Spawner->StartPreviewSpawn();
		}
	}
}


// Combat 상태의 적들 호출 
void ADefenseGameMode::StartWave()
{
	if (bIsWaveActive)
	{
		return;
	}

	bIsWaveActive = true;

	const FString Message = TEXT("All players ready. Start wave.");
	
	//UE_LOG(LogTemp, Warning, TEXT("%s"), *Message); // console
	
	UKismetSystemLibrary::PrintString(this, Message, true, true, FLinearColor::Green, 3.0f);

	CurrentEnemyCount = 0;
	for (const AEnemySpawner* Spawner : EnemySpawners)
	{
		if (Spawner)
		{
			CurrentEnemyCount += Spawner->EnemyCount;
		}
	}

	//UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode StartWave | CurrentEnemyCount=%d"), CurrentEnemyCount);
	
	if (EnemySpawners.Num() != 0)
	{
		for (auto* spawner : EnemySpawners)
		{
			spawner->StartCombatSpawn();
		}
	}
	
}

void ADefenseGameMode::EndWave()
{
	bIsWaveActive = false;

	for (AEnemySpawner* Spawner : EnemySpawners)
	{
		if (Spawner)
		{
			Spawner->EndWave();
		}
	}

	const FString Message = TEXT("Wave End");
	//UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
	UKismetSystemLibrary::PrintString(this, Message, true, true, FLinearColor::Green, 3.0f);
}

// 적의 수 감소 -> Destination에 overlap했을 때, 적이 처치됐을 때 호출
void ADefenseGameMode::DecreaseCurrentEnemyCount()
{
	CurrentEnemyCount = FMath::Max(0, CurrentEnemyCount - 1);

	//UE_LOG(LogTemp, Warning, TEXT("DefenseGameMode EnemyCount decreased | CurrentEnemyCount=%d"), CurrentEnemyCount);

	if (CurrentEnemyCount == 0)
	{
		EndWave();
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
		EndWave();
	}
}

void ADefenseGameMode::AddWave()
{
	CurrentWave++;
}

int32 ADefenseGameMode::GetCurrentWave()
{
	return CurrentWave;
}

int32 ADefenseGameMode::GetMaxWave()
{
	return MaxWave;
}
