#include "Mission/MissionRunTrackerComponent.h"

#include "Characters/Player/DefensePlayerState.h"
#include "Mission/MissionData.h"

UMissionRunTrackerComponent::UMissionRunTrackerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UMissionRunTrackerComponent::InitializeMissions(
	const TArray<TObjectPtr<UMissionData>>& InMissions)
{
	ActiveMissions.Reset();

	for (UMissionData* MissionData : InMissions)
	{
		if (MissionData)
		{
			ActiveMissions.AddUnique(MissionData);
		}
	}
}

void UMissionRunTrackerComponent::BeginRun()
{
	PlayerProgressByPlayer.Reset();
	PartyProgress.MissionStates.Reset();

	const UWorld* World = GetWorld();
	RunStartTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
}

void UMissionRunTrackerComponent::RecordEnemyKill(
	ADefensePlayerState* KillerPlayerState,
	const FGameplayTagContainer& EnemyTags)
{
	const UWorld* World = GetWorld();

	if (!KillerPlayerState || !World)
	{
		return;
	}

	const double CurrentTimeSeconds = World->GetTimeSeconds();
	const TWeakObjectPtr<ADefensePlayerState> PlayerKey = KillerPlayerState;

	for (const UMissionData* MissionData : ActiveMissions)
	{
		if (!MissionData
			|| MissionData->MissionId.IsNone()
			|| MissionData->RequiredCount <= 0)
		{
			continue;
		}

		FPlayerMissionProgress* MissionOwnerProgress = nullptr;

		if (MissionData->Scope == EMissionScope::Player)
		{
			MissionOwnerProgress = &PlayerProgressByPlayer.FindOrAdd(PlayerKey);
		}
		else
		{
			MissionOwnerProgress = &PartyProgress;
		}

		FMissionProgressState& Progress =
			MissionOwnerProgress->MissionStates.FindOrAdd(
				MissionData->MissionId);

		// 달성 후 현재 연속킬이 끊겨도 완료 상태를 유지
		if (Progress.bAchievedThisRun)
		{
			continue;
		}

		switch (MissionData->ConditionType)
		{
		case EMissionConditionType::TotalKills:
			++Progress.CurrentCount;
			break;

		case EMissionConditionType::TimedKillStreak:
			{
				if (MissionData->MaxKillIntervalSeconds <= 0.0)
				{
					continue;
				}

				const bool bFirstKill = Progress.LastKillTimeSeconds < 0.0;

				const bool bWithinInterval =
					!bFirstKill
					&& CurrentTimeSeconds - Progress.LastKillTimeSeconds
						<= MissionData->MaxKillIntervalSeconds;

				// 제한 시간
				Progress.CurrentCount = bFirstKill || bWithinInterval
					? Progress.CurrentCount + 1
					: 1;

				Progress.LastKillTimeSeconds = CurrentTimeSeconds;
				break;
			}

		case EMissionConditionType::KillTaggedEnemy:
			if (!MissionData->TargetEnemyTag.IsValid()
				|| !EnemyTags.HasTag(MissionData->TargetEnemyTag))
			{
				continue;
			}

			++Progress.CurrentCount;
			break;

		case EMissionConditionType::ClearWithinSeconds:
			// 게임 종료 시 전체 플레이 시간을 기준으로 판정
			continue;
		}

		if (Progress.CurrentCount >= MissionData->RequiredCount)
		{
			Progress.bAchievedThisRun = true;
		}

		// 임시 로그
		/*
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Mission][Progress] Player=%s PlayerState=%s Scope=%s Mission=%s Count=%d/%d Achieved=%s"),
			*KillerPlayerState->GetPlayerName(),
			*GetNameSafe(KillerPlayerState),
			MissionData->Scope == EMissionScope::Player
				? TEXT("Player")
				: TEXT("Party"),
			*MissionData->MissionId.ToString(),
			Progress.CurrentCount,
			MissionData->RequiredCount,
			Progress.bAchievedThisRun
				? TEXT("true")
				: TEXT("false"));
		*/
	}
}
