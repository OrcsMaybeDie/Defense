// Copyright Epic Games, Inc. All Rights Reserved.


#include "GameManager/DefenseGameMode.h"

#include "Characters/Enemy/EnemySpawner.h"
#include "Characters/Player/DefensePlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

ADefenseGameMode::ADefenseGameMode()
{
	PlayerStateClass = ADefensePlayerState::StaticClass();
}

void ADefenseGameMode::BeginPlay()
{
	Super::BeginPlay();
	
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
		}
	}
}

bool ADefenseGameMode::AreAllPlayersReady() const
{
	AGameStateBase* GS = GameState;
	if (!GS) return false;
	
	if (GS->PlayerArray.Num() < 2) return false;
	
	// 전체 Player 검사
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		const ADefensePlayerState* PS = Cast<ADefensePlayerState>(PlayerState);
		if (!PS || !PS->IsReady()) return false;
	}
	
	return true;
}

void ADefenseGameMode::HandlePlayerReadyChanged()
{
	if (AreAllPlayersReady())
	{
		StartWave();
	}
}

void ADefenseGameMode::StartWave()
{
	const FString Message = TEXT("All players ready. Start wave.");
	
	UE_LOG(LogTemp, Warning, TEXT("%s"), *Message); // console
	
	UKismetSystemLibrary::PrintString(this, Message, true, true, FLinearColor::Green, 3.0f);
	
	if (EnemySpawners.Num() != 0)
	{
		// enemy 스폰 관련
		/*int32 rand = FMath::RandRange(0, EnemySpawners.Num() - 1);
		EnemySpawners[rand]->SpawnTest();*/
		
		for (auto* spawner : EnemySpawners)
		{
			spawner->SpawnTest();
		}
	}
	
}

void ADefenseGameMode::EndWave()
{
	//
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
