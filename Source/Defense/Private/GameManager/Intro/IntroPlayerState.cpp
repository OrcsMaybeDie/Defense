// Fill out your copyright notice in the Description page of Project Settings.

#include "GameManager/Intro/IntroPlayerState.h"

#include "Net/UnrealNetwork.h"

void AIntroPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AIntroPlayerState, IntroRole);
	DOREPLIFETIME(AIntroPlayerState, bIsReady);
	DOREPLIFETIME(AIntroPlayerState, ClientIdentity);
}

void AIntroPlayerState::SetIntroRole(EIntroPlayerRole NewRole)
{
	if (!HasAuthority() || IntroRole == NewRole)
	{
		return;
	}

	IntroRole = NewRole;
	OnRep_IntroRole();
}

void AIntroPlayerState::SetReady(bool bNewReady)
{
	if (!HasAuthority() || bIsReady == bNewReady)
	{
		return;
	}

	bIsReady = bNewReady;
	OnRep_IsReady();
}

void AIntroPlayerState::SetClientIdentity(const FString& NewClientIdentity)
{
	if (!HasAuthority() || ClientIdentity == NewClientIdentity)
	{
		return;
	}

	ClientIdentity = NewClientIdentity;
}

void AIntroPlayerState::OnRep_IntroRole()
{
	OnIntroRoleChanged.Broadcast(IntroRole);
}

void AIntroPlayerState::OnRep_IsReady()
{
	OnIntroReadyChanged.Broadcast(bIsReady);
}
