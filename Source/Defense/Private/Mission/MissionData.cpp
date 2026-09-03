#include "Mission/MissionData.h"

#include "Equipment/EquipmentData.h"

namespace
{
	const FPrimaryAssetType MissionAssetType(TEXT("MissionData"));
}

FPrimaryAssetId UMissionData::GetPrimaryAssetId() const
{
	const FName PrimaryAssetName = MissionId.IsNone() ? GetFName() : MissionId;

	return FPrimaryAssetId(MissionAssetType, PrimaryAssetName);
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UMissionData::IsDataValid(FDataValidationContext& Context) const
{
	bool bIsValid = true;

	if (MissionId.IsNone())
	{
		Context.AddError(
			NSLOCTEXT(
				"MissionValidation",
				"MissingMissionId",
				"MissionId가 지정되지 않았습니다."));

		bIsValid = false;
	}

	if (DisplayName.IsEmpty())
	{
		Context.AddError(
			NSLOCTEXT(
				"MissionValidation",
				"MissingDisplayName",
				"DisplayName이 지정되지 않았습니다."));

		bIsValid = false;
	}

	if (SealReward < 0)
	{
		Context.AddError(
			NSLOCTEXT(
				"MissionValidation",
				"NegativeSealReward",
				"SealReward는 음수일 수 없습니다."));

		bIsValid = false;
	}

	switch (ConditionType)
	{
	case EMissionConditionType::TotalKills:
		if (RequiredCount <= 0)
		{
			Context.AddError(
				NSLOCTEXT(
					"MissionValidation",
					"InvalidTotalKills",
					"전체 처치 목표는 1 이상이어야 합니다."));

			bIsValid = false;
		}
		break;

	case EMissionConditionType::TimedKillStreak:
		if (RequiredCount <= 0)
		{
			Context.AddError(
				NSLOCTEXT(
					"MissionValidation",
					"InvalidTimedKillStreak",
					"연속 처치 목표는 1 이상이어야 합니다."));

			bIsValid = false;
		}

		if (MaxKillIntervalSeconds <= 0.f)
		{
			Context.AddError(
				NSLOCTEXT(
					"MissionValidation",
					"InvalidKillInterval",
					"연속 처치 제한 시간은 0보다 커야 합니다."));

			bIsValid = false;
		}
		break;

	case EMissionConditionType::ClearWithinSeconds:
		if (ClearTimeLimitSeconds <= 0.f)
		{
			Context.AddError(
				NSLOCTEXT(
					"MissionValidation",
					"InvalidTimeLimit",
					"클리어 제한 시간은 0보다 커야 합니다."));

			bIsValid = false;
		}
		break;

	case EMissionConditionType::KillTaggedEnemy:
		if (RequiredCount <= 0)
		{
			Context.AddError(
				NSLOCTEXT(
					"MissionValidation",
					"InvalidTaggedEnemyCount",
					"지정 적 처치 목표는 1 이상이어야 합니다."));

			bIsValid = false;
		}

		if (!TargetEnemyTag.IsValid())
		{
			Context.AddError(
				NSLOCTEXT(
					"MissionValidation",
					"MissingEnemyTag",
					"처치 대상 Enemy Tag가 필요합니다."));

			bIsValid = false;
		}
		break;
	}

	TSet<const UEquipmentData*> UniqueEquipmentRewards;
	for (const UEquipmentData* EquipmentData : PurchasableEquipmentRewards)
	{
		if (!EquipmentData)
		{
			Context.AddError(
				NSLOCTEXT(
					"MissionValidation",
					"NullEquipmentReward",
					"구매 가능 장비 보상에 비어 있는 항목이 있습니다."));

			bIsValid = false;
			continue;
		}

		if (UniqueEquipmentRewards.Contains(EquipmentData))
		{
			Context.AddError(
				NSLOCTEXT(
					"MissionValidation",
					"DuplicateEquipmentReward",
					"동일한 구매 가능 장비 보상이 중복 지정되었습니다."));

			bIsValid = false;
			continue;
		}

		UniqueEquipmentRewards.Add(EquipmentData);
	}

	return bIsValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
