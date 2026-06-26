// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DefenseGameState.generated.h"

/**
 * 
 */

UENUM()
enum class EGamePhase : uint8
{
	Preparation,
	WaveActive,
	WaveEnded
};

UCLASS()
class DEFENSE_API ADefenseGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(Replicated)
	EGamePhase GamePhase;
	
	UPROPERTY(Replicated)
	int32 CurrentWave;
	
	int32 MaxWave;
	
	UPROPERTY(Replicated)
	int32 AlivePlayerCount;
	
};
