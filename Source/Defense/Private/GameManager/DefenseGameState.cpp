// Fill out your copyright notice in the Description page of Project Settings.


#include "GameManager/DefenseGameState.h"

#include "Net/UnrealNetwork.h"

void ADefenseGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADefenseGameState, GamePhase);
	DOREPLIFETIME(ADefenseGameState, CurrentWave);
	DOREPLIFETIME(ADefenseGameState, MaxWave);
	DOREPLIFETIME(ADefenseGameState, CountdownRemaining);
	DOREPLIFETIME(ADefenseGameState, AlivePlayerCount);
	DOREPLIFETIME(ADefenseGameState, DestScore);
}

void ADefenseGameState::OnRep_DestScore()
{
	OnDestScoreChanged.Broadcast(DestScore);
}

void ADefenseGameState::SetDestScore(int32 NewDestScore)
{
	DestScore = NewDestScore;
	OnRep_DestScore();
}

void ADefenseGameState::OnRep_CurrentWave()
{
	OnCurrentWaveChanged.Broadcast(CurrentWave);
}

void ADefenseGameState::OnRep_CountdownRemaining()
{
	OnCountdownChanged.Broadcast(CountdownRemaining);
}
