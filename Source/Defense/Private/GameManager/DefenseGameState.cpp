// Fill out your copyright notice in the Description page of Project Settings.


#include "GameManager/DefenseGameState.h"

#include "Net/UnrealNetwork.h"

void ADefenseGameState::AddPlayerState(APlayerState* PlayerState)
{
	const bool bWasAlreadyRegistered = PlayerArray.Contains(PlayerState);
	Super::AddPlayerState(PlayerState);

	if (!bWasAlreadyRegistered && PlayerArray.Contains(PlayerState))
	{
		OnPlayerStateAdded.Broadcast(PlayerState);
	}
}

void ADefenseGameState::RemovePlayerState(APlayerState* PlayerState)
{
	const bool bWasRegistered = PlayerArray.Contains(PlayerState);
	Super::RemovePlayerState(PlayerState);

	if (bWasRegistered)
	{
		OnPlayerStateRemoved.Broadcast(PlayerState);
	}
}

void ADefenseGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADefenseGameState, GamePhase);
	DOREPLIFETIME(ADefenseGameState, CurrentWave);
	DOREPLIFETIME(ADefenseGameState, MaxWave);
	DOREPLIFETIME(ADefenseGameState, CountdownRemaining);
	DOREPLIFETIME(ADefenseGameState, AlivePlayerCount);
	DOREPLIFETIME(ADefenseGameState, DestScore);
	DOREPLIFETIME(ADefenseGameState, bReadyInputRequired);
	DOREPLIFETIME(ADefenseGameState, bGameClear);
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

void ADefenseGameState::SetReadyInputRequired(bool bRequired)
{
	if (!HasAuthority() || bReadyInputRequired == bRequired) return;

	bReadyInputRequired = bRequired;

	// 서버에서도 클라이언트의 RepNotify와 동일한 이벤트 경로 사용
	OnRep_ReadyInputRequired();
}

void ADefenseGameState::OnRep_ReadyInputRequired()
{
	OnReadyInputRequiredChanged.Broadcast(bReadyInputRequired);
}

void ADefenseGameState::SetGameClear(bool bNewGameClear)
{
	if (!HasAuthority() || bGameClear == bNewGameClear)
	{
		return;
	}

	bGameClear = bNewGameClear;
	OnRep_GameClear();
	ForceNetUpdate();
}

void ADefenseGameState::OnRep_GameClear()
{
	OnGameClearChanged.Broadcast(bGameClear);
}
