// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "IntroGameMode.generated.h"

class AIntroGameState;
class AIntroPlayerState;
class AIntroPlayerController;

UCLASS()
class DEFENSE_API AIntroGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AIntroGameMode();

	virtual void PreLogin(
		const FString& Options,
		const FString& Address,
		const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void HandleGuestReadyChanged(AIntroPlayerState* ReadyPlayerState, bool bReady);
	void HandleStartGameRequested(APlayerController* RequestingPlayer);
	void HandleMapSelected(APlayerController* RequestingPlayer, class UMapConfigData* SelectedMapConfigData);
	void HandleClientIdentitySubmitted(APlayerController* PlayerController);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Intro|Session")
	int32 MaxIntroPlayers = 2;

	void AssignIntroRole(APlayerController* NewPlayer);
	void EnsureDefaultMapSelected();
	void PromoteGuestToHost();
	void ClearGuest(AIntroPlayerState* GuestPlayerState);
	void RefreshIntroPlayerRefs(AIntroPlayerState* IgnoredPlayerState = nullptr);
	void ShowIntroLoadingForAllPlayers();
	void TravelToPendingGameMap();

	AIntroGameState* GetIntroGameState() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Intro|Travel")
	float StartTravelDelay = 0.2f;

	UPROPERTY()
	FString PendingGameMapPackageName;

	FTimerHandle StartTravelTimerHandle;
};
