#include "Equipment/LoadoutComponent.h"


#include "Equipment/EquipmentData.h"
#include "Equipment/ItemData.h"
#include "Equipment/WeaponData.h"
#include "Net/UnrealNetwork.h"
#include "Traps/TrapData.h"

ULoadoutComponent::ULoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void ULoadoutComponent::BeginPlay()
{
	Super::BeginPlay();

	// 시작 장비 0번 슬롯으로 고정
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		SetSelectedSlotIdx(0);
	}
}

void ULoadoutComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
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

void ULoadoutComponent::OnRep_SelectedSlotIdx()
{
	// Broadcast
	OnSelectedEquipChanged.Broadcast(SelectedSlotIdx, GetCurEquipment());
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
