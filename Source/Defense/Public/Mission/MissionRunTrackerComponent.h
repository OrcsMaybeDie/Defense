#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MissionRunTrackerComponent.generated.h"

class ADefensePlayerState;
class UMissionData;

struct FMissionProgressState
{
	int32 CurrentCount = 0;
	double LastKillTimeSeconds = -1.0;
	bool bAchievedThisRun = false;
};

struct FPlayerMissionProgress
{
	TMap<FName, FMissionProgressState> MissionStates;
};

UCLASS(ClassGroup = (Mission))
class DEFENSE_API UMissionRunTrackerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMissionRunTrackerComponent();

	void InitializeMissions(const TArray<TObjectPtr<UMissionData>>& InMissions);

	void BeginRun();

	void RecordEnemyKill(ADefensePlayerState* KillerPlayerState, const FGameplayTagContainer& EnemyTags);

private:

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMissionData>> ActiveMissions;

	TMap<TWeakObjectPtr<ADefensePlayerState>, FPlayerMissionProgress> PlayerProgressByPlayer;

	FPlayerMissionProgress PartyProgress;

	// 게임이 실제 시작된 시각 기록 (Player 입장/준비 시간 제외)
	double RunStartTimeSeconds = 0.0;
};
