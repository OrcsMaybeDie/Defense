#include "Profile/ProfileSubsystem.h"

#include "Engine/AssetManager.h"
#include "Equipment/EquipmentData.h"
#include "Kismet/GameplayStatics.h"
#include "Profile/ProfileSaveGame.h"

namespace
{
	const FString ProfileSlotName = TEXT("Profile");
	constexpr int32 ProfileUserIndex = 0;
	
	const TArray<FPrimaryAssetType> EquipmentAssetTypes =
	{
		FPrimaryAssetType(TEXT("TrapData")),
		FPrimaryAssetType(TEXT("WeaponData")),
		FPrimaryAssetType(TEXT("ItemData"))
	};
}

void UProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	if (UGameplayStatics::DoesSaveGameExist(ProfileSlotName, ProfileUserIndex))
	{
		CurrentProfile = Cast<UProfileSaveGame>(
			UGameplayStatics::LoadGameFromSlot(
				ProfileSlotName,
				ProfileUserIndex));
	}
	
	if (!CurrentProfile)
	{
		CreateNewProfile();
	}
}

bool UProfileSubsystem::IsEquipmentUnlocked(const UEquipmentData* EquipmentData) const
{
	if (!CurrentProfile || !EquipmentData)
	{
		return false;
	}
	
	const FPrimaryAssetId EquipmentId = EquipmentData->GetPrimaryAssetId();
	
	return CurrentProfile->UnlockedEquipmentIds.Contains(EquipmentId);
}

bool UProfileSubsystem::UnlockEquipment(const UEquipmentData* EquipmentData)
{
	if (!CurrentProfile || !EquipmentData)
	{
		return false;
	}
	
	const FPrimaryAssetId EquipmentId = EquipmentData->GetPrimaryAssetId();

	if (!EquipmentId.IsValid() || CurrentProfile->UnlockedEquipmentIds.Contains(EquipmentId))
	{
		return false;
	}
	
	CurrentProfile->UnlockedEquipmentIds.Add(EquipmentId);
	
	// 저장
	if (SaveProfile())
	{
		OnUnlockedEquipmentChanged.Broadcast();
		return true;
	}
	
	// 저장 실패 시 메모리 변경 되돌림
	CurrentProfile->UnlockedEquipmentIds.Remove(EquipmentId);
	return false;
}

TArray<UEquipmentData*> UProfileSubsystem::GetAllEquipmentData() const
{
	TArray<UEquipmentData*> AllEquipmentData;
	
	UAssetManager& AssetManager = UAssetManager::Get();
	
	for (const FPrimaryAssetType& AssetType : EquipmentAssetTypes)
	{
		TArray<FPrimaryAssetId> AssetIds;
		AssetManager.GetPrimaryAssetIdList(AssetType, AssetIds);
		
		for (const FPrimaryAssetId& AssetId : AssetIds)
		{
			const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
			
			UEquipmentData* EquipmentData = Cast<UEquipmentData>(AssetPath.TryLoad());
		
			if (EquipmentData)
			{
				AllEquipmentData.AddUnique(EquipmentData);
			}
		}
	}
	
	return AllEquipmentData;
}

void UProfileSubsystem::CreateNewProfile()
{
	CurrentProfile = Cast<UProfileSaveGame>(
		UGameplayStatics::CreateSaveGameObject(
			UProfileSaveGame::StaticClass()));

	if (!CurrentProfile)
	{
		return;
	}
	
	InitializeDefaultUnlocks();
	SaveProfile();
}

void UProfileSubsystem::InitializeDefaultUnlocks()
{
	if (!CurrentProfile)
	{
		return;
	}
	
	CurrentProfile->UnlockedEquipmentIds.Reset();
	
	// 전체 장비 조회
	for (const UEquipmentData* EquipmentData : GetAllEquipmentData())
	{
		if (EquipmentData && EquipmentData->bUnlockedByDefault)
		{
			// 기본 해금 장비
			CurrentProfile->UnlockedEquipmentIds.AddUnique(EquipmentData->GetPrimaryAssetId());
		}
	}
}

bool UProfileSubsystem::SaveProfile()
{
	if (!CurrentProfile)
	{
		return false;
	}
	
	return UGameplayStatics::SaveGameToSlot(
		CurrentProfile,
		ProfileSlotName,
		ProfileUserIndex);
}
