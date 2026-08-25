#pragma once

#include "CoreMinimal.h"
#include "MissionCompletionResult.generated.h"

USTRUCT(BlueprintType)
struct DEFENSE_API FMissionCompletionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName MissionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 SealReward = 0;
};
