#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UObject/PrimaryAssetId.h"
#include "ProfileSaveGame.generated.h"


UCLASS()
class DEFENSE_API UProfileSaveGame : public USaveGame
{
	GENERATED_BODY()
	
public:
	
	static constexpr int32 CurrentSaveVersion = 3;

	UPROPERTY(SaveGame)
	int32 SaveVersion = CurrentSaveVersion;

	// 케르베로스의 인장
	UPROPERTY(SaveGame)
	int32 Seal = 0;
	
	// 해금된 장비 목록
	UPROPERTY(SaveGame)
	TArray<FPrimaryAssetId> UnlockedEquipmentIds;
	
	// 배열 인덱스 = 퀵슬롯 인덱스
	UPROPERTY(SaveGame)
	TArray<FPrimaryAssetId> EquippedEquipmentIds;

	// 영구 완료된 미션 ID
	UPROPERTY(SaveGame)
	TArray<FName> CompletedMissionIds;
};
