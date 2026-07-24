// Fill out your copyright notice in the Description page of Project Settings.

#include "GameManager/Intro/IntroGameState.h"

#include "GameManager/Intro/IntroPlayerState.h"
#include "Net/UnrealNetwork.h"

void AIntroGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AIntroGameState, HostPlayerState);
	DOREPLIFETIME(AIntroGameState, GuestPlayerState);
	DOREPLIFETIME(AIntroGameState, bGuestReady);
	DOREPLIFETIME(AIntroGameState, SelectedMapConfigData);
}

bool AIntroGameState::CanHostStart() const
{
	return HostPlayerState && (!GuestPlayerState || bGuestReady);
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

void AIntroGameState::SetGuestPlayerState(AIntroPlayerState* NewGuestPlayerState)
{
	if (GuestPlayerState == NewGuestPlayerState)
	{
		return;
	}

	GuestPlayerState = NewGuestPlayerState;
	OnRep_GuestPlayerState();
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

void AIntroGameState::OnRep_GuestPlayerState()
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
