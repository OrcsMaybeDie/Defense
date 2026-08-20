#include "Profile/ProfileSubsystem.h"

#include "Engine/AssetManager.h"
#include "Equipment/EquipmentData.h"
#include "Kismet/GameplayStatics.h"
#include "Profile/ProfileSaveGame.h"

namespace
{
	const FString ProfileSlotName = TEXT("Profile");
	constexpr int32 ProfileUserIndex = 0;
	constexpr int32 InitialQuickSlotCount = 5;

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
		return;
	}

	if (CurrentProfile->SaveVersion > UProfileSaveGame::CurrentSaveVersion)
	{
		UE_LOG(LogTemp, Error, TEXT("[Profile] Unsupported future save version: %d"), CurrentProfile->SaveVersion);
		return;
	}

	bool bShouldSaveProfile = MigrateProfile();

	if (CurrentProfile->EquippedEquipmentIds.Num() < InitialQuickSlotCount)
	{
		InitializeQuickSlots();
		bShouldSaveProfile = true;
	}

	if (bShouldSaveProfile)
	{
		SaveProfile();
	}
}

int32 UProfileSubsystem::GetSeal() const
{
	return CurrentProfile ? FMath::Max(0, CurrentProfile->Seal) : 0;
}

bool UProfileSubsystem::CanSpendSeal(int32 Amount) const
{
	return Amount > 0 && GetSeal() >= Amount;
}

bool UProfileSubsystem::AddSeal(int32 Amount)
{
	if (!CurrentProfile || Amount <= 0)
	{
		return false;
	}

	const int32 PreviousSeal = CurrentProfile->Seal;

	const int64 NewSeal = static_cast<int64>(PreviousSeal) + Amount;

	// MAX_int32 = 2,147,483,647
	if (NewSeal > MAX_int32)
	{
		return false;
	}

	CurrentProfile->Seal = static_cast<int32>(NewSeal);

	if (!SaveProfile())
	{
		CurrentProfile->Seal = PreviousSeal;
		return false;
	}

	OnSealChanged.Broadcast(CurrentProfile->Seal);

	return true;
}

bool UProfileSubsystem::TrySpendSeal(int32 Amount)
{
	if (!CurrentProfile || !CanSpendSeal(Amount))
	{
		return false;
	}

	const int32 PreviousSeal = CurrentProfile->Seal;

	CurrentProfile->Seal -= Amount;

	if (!SaveProfile())
	{
		CurrentProfile->Seal = PreviousSeal;
		return false;
	}

	OnSealChanged.Broadcast(CurrentProfile->Seal);

	return true;
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

TArray<UEquipmentData*> UProfileSubsystem::GetUnlockedEquipmentData() const
{
	TArray<UEquipmentData*> UnlockedEquipmentData;

	for (UEquipmentData* EquipmentData : GetAllEquipmentData())
	{
		if (IsEquipmentUnlocked(EquipmentData))
		{
			UnlockedEquipmentData.Add(EquipmentData);
		}
	}
	return UnlockedEquipmentData;
}

int32 UProfileSubsystem::GetQuickSlotCount() const
{
	return CurrentProfile ? CurrentProfile->EquippedEquipmentIds.Num() : 0;
}

TArray<FPrimaryAssetId> UProfileSubsystem::GetQuickSlotEquipmentIds() const
{
	if (!CurrentProfile)
	{
		return {};
	}

	return CurrentProfile->EquippedEquipmentIds;
}

bool UProfileSubsystem::ExpandQuickSlots(int32 AddSlotCount)
{
	if (!CurrentProfile || AddSlotCount <= 0)
	{
		return false;
	}

	const int32 PreviousSlotCount = CurrentProfile->EquippedEquipmentIds.Num();

	CurrentProfile->EquippedEquipmentIds.SetNum(PreviousSlotCount + AddSlotCount);

	if (SaveProfile())
	{
		OnQuickSlotsChanged.Broadcast();
		return true;
	}

	// 저장 실패 시 원래 슬롯 개수로 복원
	CurrentProfile->EquippedEquipmentIds.SetNum(PreviousSlotCount);

	return false;
}

UEquipmentData* UProfileSubsystem::GetQuickSlotEquipment(int32 SlotIndex) const
{
	if (!CurrentProfile || !CurrentProfile->EquippedEquipmentIds.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}

	const FPrimaryAssetId& EquipmentId = CurrentProfile->EquippedEquipmentIds[SlotIndex];

	if (!EquipmentId.IsValid())
	{
		return nullptr;
	}

	const FSoftObjectPath AssetPath = UAssetManager::Get().GetPrimaryAssetPath(EquipmentId);

	return Cast<UEquipmentData>(AssetPath.TryLoad());
}

bool UProfileSubsystem::AssignEquipmentToQuickSlot(int32 SlotIndex, const UEquipmentData* EquipmentData)
{
	if (!CurrentProfile || !EquipmentData || !CurrentProfile->EquippedEquipmentIds.IsValidIndex(SlotIndex))
	{
		return false;
	}

	if (!IsEquipmentUnlocked(EquipmentData))
	{
		return false;
	}

	const FPrimaryAssetId EquipmentId = EquipmentData->GetPrimaryAssetId();

	if (!EquipmentId.IsValid())
	{
		return false;
	}

	if (CurrentProfile->EquippedEquipmentIds[SlotIndex] == EquipmentId)
	{
		return false;
	}

	// 저장 실패 시 전체 슬롯 상태를 복원하기 위한 복사본
	const TArray<FPrimaryAssetId> PreviousEquipmentIds = CurrentProfile->EquippedEquipmentIds;

	// 같은 장비가 다른 슬롯에 있다면 기존 슬롯을 비움
	for (FPrimaryAssetId& EquippedId : CurrentProfile->EquippedEquipmentIds)
	{
		if (EquippedId == EquipmentId)
		{
			EquippedId = FPrimaryAssetId();
		}
	}

	CurrentProfile->EquippedEquipmentIds[SlotIndex] = EquipmentId;

	if (SaveProfile())
	{
		OnQuickSlotsChanged.Broadcast();
		return true;
	}

	// 저장 실패 시 모든 슬롯을 이전 상태로 복구
	CurrentProfile->EquippedEquipmentIds = PreviousEquipmentIds;

	return false;
}

bool UProfileSubsystem::ClearQuickSlot(int32 SlotIndex)
{
	if (!CurrentProfile || !CurrentProfile->EquippedEquipmentIds.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FPrimaryAssetId& EquipmentId = CurrentProfile->EquippedEquipmentIds[SlotIndex];

	if (!EquipmentId.IsValid())
	{
		return false;
	}

	const FPrimaryAssetId PreviousEquipmentId = EquipmentId;

	// 해당 ID를 빈 FPrimaryAssetId로 변경
	EquipmentId = FPrimaryAssetId();

	if (SaveProfile())
	{
		OnQuickSlotsChanged.Broadcast();
		return true;
	}

	// 저장 실패 시 기존 장비 복원
	EquipmentId = PreviousEquipmentId;
	return false;
}

bool UProfileSubsystem::MigrateProfile()
{
	if (!CurrentProfile)
	{
		return false;
	}

	bool bWasModified = false;

	// Version 1 -> 2
	if (CurrentProfile->SaveVersion < 2)
	{
		CurrentProfile->Seal = 0;
		CurrentProfile->SaveVersion = 2;
		bWasModified = true;
	}

	// 비정상적인 음수 값 방어
	if (CurrentProfile->Seal < 0)
	{
		CurrentProfile->Seal = 0;
		bWasModified = true;
	}

	return bWasModified;
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
	InitializeQuickSlots();
	InitializeDefaultQuickSlotAssignments();

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

void UProfileSubsystem::InitializeQuickSlots()
{
	if (!CurrentProfile)
	{
		return;
	}

	// 최소 개수 보장
	if (CurrentProfile->EquippedEquipmentIds.Num() < InitialQuickSlotCount)
	{
		CurrentProfile->EquippedEquipmentIds.SetNum(InitialQuickSlotCount);
	}
}

void UProfileSubsystem::InitializeDefaultQuickSlotAssignments()
{
	if (!CurrentProfile)
	{
		return;
	}

	for (const UEquipmentData* EquipmentData : GetAllEquipmentData())
	{
		if (!EquipmentData || !IsEquipmentUnlocked(EquipmentData))
		{
			continue;
		}

		const int32 SlotIndex = EquipmentData->DefaultQuickSlotIndex;

		if (!CurrentProfile->EquippedEquipmentIds.IsValidIndex(SlotIndex))
		{
			continue;
		}

		CurrentProfile->EquippedEquipmentIds[SlotIndex] = EquipmentData->GetPrimaryAssetId();
	}
}

bool UProfileSubsystem::SaveProfile()
{
	if (!CurrentProfile || CurrentProfile->SaveVersion > UProfileSaveGame::CurrentSaveVersion)
	{
		return false;
	}

	return UGameplayStatics::SaveGameToSlot(
		CurrentProfile,
		ProfileSlotName,
		ProfileUserIndex);
}
