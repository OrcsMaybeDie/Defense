// Fill out your copyright notice in the Description page of Project Settings.

#include "GameManager/Intro/IntroGameState.h"

#include "GameManager/Intro/IntroPlayerState.h"
#include "Net/UnrealNetwork.h"

void AIntroGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AIntroGameState, HostPlayerState);
	DOREPLIFETIME(AIntroGameState, GuestPlayerStates);
	DOREPLIFETIME(AIntroGameState, bGuestReady);
	DOREPLIFETIME(AIntroGameState, SelectedMapConfigData);
}

AIntroPlayerState* AIntroGameState::GetGuestPlayerState() const
{
	return GuestPlayerStates.IsEmpty() ? nullptr : GuestPlayerStates[0].Get();
}

TArray<AIntroPlayerState*> AIntroGameState::GetGuestPlayerStates() const
{
	TArray<AIntroPlayerState*> Result;
	Result.Reserve(GuestPlayerStates.Num());

	for (AIntroPlayerState* GuestPlayerState : GuestPlayerStates)
	{
		if (GuestPlayerState)
		{
			Result.Add(GuestPlayerState);
		}
	}

	return Result;
}

bool AIntroGameState::CanHostStart() const
{
	return HostPlayerState && (GuestPlayerStates.IsEmpty() || bGuestReady);
}

bool AIntroGameState::IsHostPlayerState(const APlayerState* PlayerState) const
{
	return PlayerState && PlayerState == HostPlayerState;
}

void AIntroGameState::SetHostPlayerState(AIntroPlayerState* NewHostPlayerState)
{
	if (HostPlayerState == NewHostPlayerState)
	{
		return;
	}

	HostPlayerState = NewHostPlayerState;
	OnRep_HostPlayerState();
}

void AIntroGameState::AddGuestPlayerState(AIntroPlayerState* NewGuestPlayerState)
{
	if (!NewGuestPlayerState || GuestPlayerStates.Contains(NewGuestPlayerState))
	{
		return;
	}

	GuestPlayerStates.Add(NewGuestPlayerState);
	OnRep_GuestPlayerStates();
}

void AIntroGameState::RemoveGuestPlayerState(AIntroPlayerState* GuestPlayerStateToRemove)
{
	if (!GuestPlayerStateToRemove || GuestPlayerStates.Remove(GuestPlayerStateToRemove) == 0)
	{
		return;
	}

	OnRep_GuestPlayerStates();
}

void AIntroGameState::SetGuestReady(bool bNewGuestReady)
{
	if (bGuestReady == bNewGuestReady)
	{
		return;
	}

	bGuestReady = bNewGuestReady;
	OnRep_GuestReady();
}

void AIntroGameState::SetSelectedMapConfigData(UMapConfigData* NewSelectedMapConfigData)
{
	if (SelectedMapConfigData == NewSelectedMapConfigData)
	{
		return;
	}

	SelectedMapConfigData = NewSelectedMapConfigData;
	OnRep_SelectedMapConfigData();
}

void AIntroGameState::OnRep_HostPlayerState()
{
	OnIntroPlayersChanged.Broadcast();
}

void AIntroGameState::OnRep_GuestPlayerStates()
{
	OnIntroPlayersChanged.Broadcast();
}

void AIntroGameState::OnRep_GuestReady()
{
	OnGuestReadyChanged.Broadcast(bGuestReady);
}

void AIntroGameState::OnRep_SelectedMapConfigData()
{
	OnSelectedMapChanged.Broadcast(SelectedMapConfigData);
}
