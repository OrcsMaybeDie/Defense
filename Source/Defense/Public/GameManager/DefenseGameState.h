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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDestScoreChanged, int32, NewDestScore);

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

	UPROPERTY(ReplicatedUsing=OnRep_DestScore, BlueprintReadOnly)
	int32 DestScore = 20;

	// DestinationUI에서 이 델리게이트에 UIupdate 함수 등록함.
	UPROPERTY(BlueprintAssignable)
	FOnDestScoreChanged OnDestScoreChanged;

	UFUNCTION()
	void OnRep_DestScore();

	void SetDestScore(int32 NewDestScore);
	
};
