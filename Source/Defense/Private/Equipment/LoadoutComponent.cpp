#include "Equipment/LoadoutComponent.h"

#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Profile/ProfileSubsystem.h"
#include "Equipment/EquipmentData.h"
#include "Equipment/ItemData.h"
#include "Equipment/WeaponData.h"
#include "Net/UnrealNetwork.h"
#include "Traps/TrapData.h"

namespace
{
	constexpr int32 MaxSubmittedQuickSlotCount = 12;

	const FPrimaryAssetType TrapDataAssetType(TEXT("TrapData"));
	const FPrimaryAssetType WeaponDataAssetType(TEXT("WeaponData"));
	const FPrimaryAssetType ItemDataAssetType(TEXT("ItemData"));
}

ULoadoutComponent::ULoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void ULoadoutComponent::BeginPlay()
{
	Super::BeginPlay();
	
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	
	// 이 PC가 직접 조작하는 캐릭터에만 로컬 프로필 적용
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		if (UProfileSubsystem* ProfileSubsystem = GetProfileSubsystem())
		{
			ProfileSubsystem->OnQuickSlotsChanged.AddUniqueDynamic(
				this,
				&ULoadoutComponent::HandleProfileQuickSlotsChanged);
		}

		SubmitProfileLoadout();
	}

	// 시작 장비 0번 슬롯으로 고정
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		SetSelectedSlotIdx(0);
	}
}

void ULoadoutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UProfileSubsystem* ProfileSubsystem = GetProfileSubsystem())
	{
		ProfileSubsystem->OnQuickSlotsChanged.RemoveDynamic(
			this,
			&ULoadoutComponent::HandleProfileQuickSlotsChanged);
	}
	
	Super::EndPlay(EndPlayReason);
}

void ULoadoutComponent::SubmitProfileLoadout()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());

	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	UProfileSubsystem* ProfileSubsystem = GetProfileSubsystem();

	if (!ProfileSubsystem)
	{
		return;
	}

	const TArray<FPrimaryAssetId> EquipmentIds = ProfileSubsystem->GetQuickSlotEquipmentIds();

	ServerRPC_SubmitProfileLoadout(EquipmentIds);
}

bool ULoadoutComponent::ValidateSubmittedEquipmentIds(const TArray<FPrimaryAssetId>& EquipmentIds) const
{
	if (EquipmentIds.IsEmpty()
	|| EquipmentIds.Num() > MaxSubmittedQuickSlotCount)
	{
		return false;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	TSet<FPrimaryAssetId> UsedEquipmentIds;

	for (const FPrimaryAssetId& EquipmentId : EquipmentIds)
	{
		// 빈 ID는 열린 빈 슬롯
		if (!EquipmentId.IsValid())
		{
			continue;
		}

		const bool bIsSupportedType =
			EquipmentId.PrimaryAssetType == TrapDataAssetType
			|| EquipmentId.PrimaryAssetType == WeaponDataAssetType
			|| EquipmentId.PrimaryAssetType == ItemDataAssetType;

		if (!bIsSupportedType)
		{
			return false;
		}

		// 동일 장비 중복 등록 거절
		if (UsedEquipmentIds.Contains(EquipmentId))
		{
			return false;
		}

		const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(EquipmentId);

		if (!AssetPath.IsValid())
		{
			return false;
		}

		const UEquipmentData* EquipmentData = Cast<UEquipmentData>(AssetPath.TryLoad());

		if (!EquipmentData || EquipmentData->GetPrimaryAssetId() != EquipmentId)
		{
			return false;
		}

		UsedEquipmentIds.Add(EquipmentId);
	}

	return true;
}

void ULoadoutComponent::RebuildEquippedSlotsFromReplicatedIds()
{
	EquippedSlots.SetNum(ReplicatedEquipmentIds.Num());

	UAssetManager& AssetManager = UAssetManager::Get();

	for (int32 SlotIdx = 0; SlotIdx < ReplicatedEquipmentIds.Num(); ++SlotIdx)
	{
		const FPrimaryAssetId& EquipmentId = ReplicatedEquipmentIds[SlotIdx];

		UEquipmentData* EquipmentData = nullptr;

		if (EquipmentId.IsValid())
		{
			const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(EquipmentId);

			EquipmentData = Cast<UEquipmentData>(AssetPath.TryLoad());
		}

		EquippedSlots[SlotIdx].EquipmentData = EquipmentData;
	}

	OnLoadoutSlotsChanged.Broadcast();

	// 선택 번호가 같아도 선택 슬롯의 장비가 변경될 수 있음
	OnSelectedEquipChanged.Broadcast(SelectedSlotIdx, GetCurEquipment());
}

void ULoadoutComponent::OnRep_ReplicatedEquipmentIds()
{
	RebuildEquippedSlotsFromReplicatedIds();
}

void ULoadoutComponent::HandleProfileQuickSlotsChanged()
{
	SubmitProfileLoadout();
}

void ULoadoutComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ULoadoutComponent, ReplicatedEquipmentIds);
	DOREPLIFETIME(ULoadoutComponent, SelectedSlotIdx);
}

void ULoadoutComponent::SelectSlot(int32 SlotIdx)
{
	if (!CanSelectSlot(SlotIdx)) return;
	
	AActor* OwnerActor = GetOwner(); // ADefenseCharacter
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		SetSelectedSlotIdx(SlotIdx);
		return;
	}
	
	ServerRPC_RequestSelectSlot(SlotIdx);
}

bool ULoadoutComponent::CanSelectSlot(int32 SlotIdx) const
{
	return EquippedSlots.IsValidIndex(SlotIdx) && EquippedSlots[SlotIdx].EquipmentData != nullptr;
}

void ULoadoutComponent::ServerRPC_RequestSelectSlot_Implementation(int32 SlotIdx)
{
	if (!CanSelectSlot(SlotIdx)) return;
	
	SetSelectedSlotIdx(SlotIdx);
}

void ULoadoutComponent::SetSelectedSlotIdx(int32 SlotIdx)
{
	if (SelectedSlotIdx == SlotIdx) return;
	
	SelectedSlotIdx = SlotIdx;
	
	// Broadcast
	OnSelectedEquipChanged.Broadcast(SelectedSlotIdx, GetCurEquipment());
}

UEquipmentData* ULoadoutComponent::GetEquipAtSlot(int32 SlotIdx) const
{
	return EquippedSlots.IsValidIndex(SlotIdx)
	? EquippedSlots[SlotIdx].EquipmentData : nullptr;
}

UProfileSubsystem* ULoadoutComponent::GetProfileSubsystem() const
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<UProfileSubsystem>() : nullptr;
}

void ULoadoutComponent::OnRep_SelectedSlotIdx()
{
	// Broadcast
	OnSelectedEquipChanged.Broadcast(SelectedSlotIdx, GetCurEquipment());
}

void ULoadoutComponent::ServerRPC_SubmitProfileLoadout_Implementation(const TArray<FPrimaryAssetId>& EquipmentIds)
{
	if (!ValidateSubmittedEquipmentIds(EquipmentIds))
	{
		return;
	}

	ReplicatedEquipmentIds = EquipmentIds;

	// RepNotify는 서버에서 자동 호출되지 X. 직접 호출
	RebuildEquippedSlotsFromReplicatedIds();
}

UEquipmentData* ULoadoutComponent::GetCurEquipment() const
{
	if (!EquippedSlots.IsValidIndex(SelectedSlotIdx)) return nullptr;

	return EquippedSlots[SelectedSlotIdx].EquipmentData;
}

UWeaponData* ULoadoutComponent::GetCurWeapon() const
{
	return Cast<UWeaponData>(GetCurEquipment());
}

UTrapData* ULoadoutComponent::GetCurTrap() const
{
	return Cast<UTrapData>(GetCurEquipment());
}

UItemData* ULoadoutComponent::GetCurItem() const
{
	return Cast<UItemData>(GetCurEquipment());
}
