// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "IntroGameState.generated.h"

class AIntroPlayerState;
class UMapConfigData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnIntroPlayersChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGuestReadyChanged, bool, bGuestReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedMapChanged, UMapConfigData*, SelectedMapConfigData);

UCLASS()
class DEFENSE_API AIntroGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Intro|Players")
	AIntroPlayerState* GetHostPlayerState() const { return HostPlayerState; }

	UFUNCTION(BlueprintPure, Category="Intro|Players")
	AIntroPlayerState* GetGuestPlayerState() const;

	UFUNCTION(BlueprintPure, Category="Intro|Players")
	TArray<AIntroPlayerState*> GetGuestPlayerStates() const;

	UFUNCTION(BlueprintPure, Category="Intro|Players")
	int32 GetGuestPlayerCount() const { return GuestPlayerStates.Num(); }

	UFUNCTION(BlueprintPure, Category="Intro|Ready")
	bool IsGuestReady() const { return bGuestReady; }

	UFUNCTION(BlueprintPure, Category="Intro|Start")
	bool CanHostStart() const;

	UFUNCTION(BlueprintPure, Category="Intro|Map")
	UMapConfigData* GetSelectedMapConfigData() const { return SelectedMapConfigData; }

	UFUNCTION(BlueprintPure, Category="Intro|Players")
	bool IsHostPlayerState(const APlayerState* PlayerState) const;

	void SetHostPlayerState(AIntroPlayerState* NewHostPlayerState);
	void AddGuestPlayerState(AIntroPlayerState* NewGuestPlayerState);
	void RemoveGuestPlayerState(AIntroPlayerState* GuestPlayerStateToRemove);
	void SetGuestReady(bool bNewGuestReady);
	void SetSelectedMapConfigData(UMapConfigData* NewSelectedMapConfigData);

	UPROPERTY(BlueprintAssignable, Category="Intro|Players")
	FOnIntroPlayersChanged OnIntroPlayersChanged;

	UPROPERTY(BlueprintAssignable, Category="Intro|Ready")
	FOnGuestReadyChanged OnGuestReadyChanged;

	UPROPERTY(BlueprintAssignable, Category="Intro|Map")
	FOnSelectedMapChanged OnSelectedMapChanged;

protected:
	UPROPERTY(ReplicatedUsing=OnRep_HostPlayerState, BlueprintReadOnly, Category="Intro|Players")
	TObjectPtr<AIntroPlayerState> HostPlayerState;

	UPROPERTY(ReplicatedUsing=OnRep_GuestPlayerStates, BlueprintReadOnly, Category="Intro|Players")
	TArray<TObjectPtr<AIntroPlayerState>> GuestPlayerStates;

	UPROPERTY(ReplicatedUsing=OnRep_GuestReady, BlueprintReadOnly, Category="Intro|Ready")
	bool bGuestReady = false;

	UPROPERTY(ReplicatedUsing=OnRep_SelectedMapConfigData, BlueprintReadOnly, Category="Intro|Map")
	TObjectPtr<UMapConfigData> SelectedMapConfigData;

	UFUNCTION()
	void OnRep_HostPlayerState();

	UFUNCTION()
	void OnRep_GuestPlayerStates();

	UFUNCTION()
	void OnRep_GuestReady();

	UFUNCTION()
	void OnRep_SelectedMapConfigData();
};
