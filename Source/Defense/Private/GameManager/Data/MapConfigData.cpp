// Fill out your copyright notice in the Description page of Project Settings.


#include "GameManager/Data/MapConfigData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Mission/MissionData.h"

EDataValidationResult UMapConfigData::IsDataValid(
	FDataValidationContext& Context) const
{
	bool bIsValid = true;
	TSet<FPrimaryAssetId> MissionIds;

	for (const UMissionData* MissionData : Missions)
	{
		if (!MissionData)
		{
			Context.AddError(
				NSLOCTEXT(
					"MapConfigValidation",
					"NullMission",
					"Missions 배열에 비어 있는 항목이 있습니다."));

			bIsValid = false;
			continue;
		}

		const FPrimaryAssetId MissionId = MissionData->GetPrimaryAssetId();
		if (!MissionId.IsValid())
		{
			Context.AddError(
				NSLOCTEXT(
					"MapConfigValidation",
					"InvalidMissionId",
					"유효하지 않은 MissionData가 지정되었습니다."));

			bIsValid = false;
			continue;
		}

		if (MissionIds.Contains(MissionId))
		{
			Context.AddError(
				FText::Format(
					NSLOCTEXT(
						"MapConfigValidation",
						"DuplicateMissionId",
						"동일한 미션 ID가 중복 지정되었습니다: {0}"),
					FText::FromString(MissionId.ToString())));

			bIsValid = false;
			continue;
		}

		MissionIds.Add(MissionId);
	}

	return bIsValid
		? EDataValidationResult::Valid
		: EDataValidationResult::Invalid;
}
#endif
