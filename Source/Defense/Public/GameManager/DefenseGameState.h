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
	GameStart,
	Preparation,
	WaveStart,
	WaveEnded,
	GameEnded,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDestScoreChanged, int32, NewDestScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCountdownChanged, int32, NewCountdownRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentWaveChanged, int32, NewCurrentWave);

UCLASS()
class DEFENSE_API ADefenseGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(Replicated)
	EGamePhase GamePhase = EGamePhase::Preparation;
	
	UPROPERTY(ReplicatedUsing=OnRep_CurrentWave)
	int32 CurrentWave = 1;
	
	UPROPERTY(Replicated)
	int32 MaxWave = 6;

	UPROPERTY(ReplicatedUsing=OnRep_CountdownRemaining)
	int32 CountdownRemaining = 0;
	
	UPROPERTY(Replicated)
	int32 AlivePlayerCount;

	UPROPERTY(ReplicatedUsing=OnRep_DestScore, BlueprintReadOnly)
	int32 DestScore = 20;

	// DestinationUI에서 이 델리게이트에 UIupdate 함수 등록함.
	UPROPERTY(BlueprintAssignable)
	FOnDestScoreChanged OnDestScoreChanged;

	UPROPERTY(BlueprintAssignable)
	FOnCountdownChanged OnCountdownChanged;

	UPROPERTY(BlueprintAssignable)
	FOnCurrentWaveChanged OnCurrentWaveChanged;

	UFUNCTION()
	void OnRep_DestScore();
	
	UFUNCTION()
	void OnRep_CurrentWave();

	UFUNCTION()
	void OnRep_CountdownRemaining();

	void SetDestScore(int32 NewDestScore);
	
};
