// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/Player/DefenseGameMode.h"

#include "Characters/Player/DefensePlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetSystemLibrary.h"

ADefenseGameMode::ADefenseGameMode()
{
	PlayerStateClass = ADefensePlayerState::StaticClass();
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
}
