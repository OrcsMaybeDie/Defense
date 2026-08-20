#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MissionData.generated.h"

class UEquipmentData;

UENUM(BlueprintType)
enum class EMissionConditionType : uint8
{
	TotalKills UMETA(DisplayName = "Total Kills"),
	TimedKillStreak UMETA(DisplayName = "Timed Kill Streak"),
	ClearWithinSeconds UMETA(DisplayName = "Clear Within Seconds"),
	KillTaggedEnemy UMETA(DisplayName = "Kill Tagged Enemy")
};

UENUM(BlueprintType)
enum class EMissionScope : uint8
{
	Player UMETA(DisplayName = "Player"),
	Party UMETA(DisplayName = "Party")
};

UCLASS(BlueprintType)
class DEFENSE_API UMissionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// 저장에 사용하는 고유 ID. 출시 후 변경 금지.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	FName MissionId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	EMissionScope Scope = EMissionScope::Player;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	EMissionConditionType ConditionType = EMissionConditionType::TotalKills;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|Condition", meta = (ClampMin = "0", EditCondition = "ConditionType != EMissionConditionType::ClearWithinSeconds", EditConditionHides))
	int32 RequiredCount = 0;

	// TimedKillStreak에서 각 처치 사이에 허용할 최대 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|Condition", meta = (ClampMin = "0.0", Units = "s", EditCondition = "ConditionType == EMissionConditionType::TimedKillStreak", EditConditionHides))
	float MaxKillIntervalSeconds = 0.f;

	// ClearWithinSeconds 전용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|Condition", meta = (ClampMin = "0.0", Units = "s", EditCondition = "ConditionType == EMissionConditionType::ClearWithinSeconds", EditConditionHides))
	float ClearTimeLimitSeconds = 0.f;

	// KillTaggedEnemy 전용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|Condition", meta = (EditCondition = "ConditionType == EMissionConditionType::KillTaggedEnemy", EditConditionHides))
	FGameplayTag TargetEnemyTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|Reward", meta = (ClampMin = "0"))
	int32 SealReward = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|Reward")
	TArray<TObjectPtr<UEquipmentData>> PurchasableEquipmentRewards;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
