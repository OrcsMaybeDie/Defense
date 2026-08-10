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
	
	UPROPERTY(SaveGame)
	int32 SaveVersion = 1;
	
	UPROPERTY(SaveGame)
	TArray<FPrimaryAssetId> UnlockedEquipmentIds;
};
