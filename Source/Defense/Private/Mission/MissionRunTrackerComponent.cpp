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
	RunStartTimeSeconds = World ? World->GetTimeSeconds() : -1.0;
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
	}
}

TArray<FMissionCompletionResult> UMissionRunTrackerComponent::CollectMissionResults(ADefensePlayerState* PlayerState, bool bGameClear) const
{
	TArray<FMissionCompletionResult> Results;

	const UWorld* World = GetWorld();

	// 유효하지 않거나 정상 클리어가 아니면 달성 결과 생성 X -> Seal 지급/저장은 소유 클라이언트의 ProfileSubsystem
	if (!PlayerState || !World || !bGameClear)
	{
		return Results;
	}

	// 첫 웨이브 시작부터 게임 종료까지의 실제 플레이 시간
	const double ElapsedSeconds = RunStartTimeSeconds >= 0.0
		? FMath::Max(0.0, World->GetTimeSeconds() - RunStartTimeSeconds)
		: 0.0;

	const TWeakObjectPtr<ADefensePlayerState> PlayerKey = PlayerState;

	// 현재 맵에 지정된 미션을 하나씩 최종 판정
	for (const UMissionData* MissionData : ActiveMissions)
	{
		if (!MissionData || MissionData->MissionId.IsNone())
		{
			continue;
		}

		bool bAchieved = false;

		if (MissionData->ConditionType == EMissionConditionType::ClearWithinSeconds)
		{
			// 처치 진행도 대신 첫 웨이브부터 종료까지 걸린 시간을 제한 시간과 비교
			bAchieved =
				RunStartTimeSeconds >= 0.0
				&& MissionData->ClearTimeLimitSeconds > 0.0
				&& ElapsedSeconds <= MissionData->ClearTimeLimitSeconds;
		}
		else
		{
			// Player 미션은 해당 플레이어 기록, Party 미션은 공용 기록을 선택
			const FPlayerMissionProgress* MissionOwnerProgress = MissionData->Scope == EMissionScope::Player
				? PlayerProgressByPlayer.Find(PlayerKey)
				: &PartyProgress;

			// 선택한 기록에 이 미션의 달성 상태가 있으면 최종 결과에 반영
			if (MissionOwnerProgress)
			{
				if (const FMissionProgressState* Progress = MissionOwnerProgress->MissionStates.Find(MissionData->MissionId))
				{
					bAchieved = Progress->bAchievedThisRun;
				}
			}
		}

		if (!bAchieved)
		{
			continue;
		}

		// 서버가 판정한 MissionId/보상을 소유 클라이언트로 전달할 결과에 추가
		// 실제 중복 달성 검사, Seal 지급, 프로필 저장은 ProfileSubsystem에서 처리
		FMissionCompletionResult Result;
		Result.MissionId = MissionData->MissionId;
		Result.DisplayName = MissionData->DisplayName;
		Result.SealReward = MissionData->SealReward;
		Results.Add(Result);
	}

	return Results;
}
