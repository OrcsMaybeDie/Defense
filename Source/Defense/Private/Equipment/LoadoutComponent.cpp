#include "Equipment/LoadoutComponent.h"


ULoadoutComponent::ULoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UWeaponData* ULoadoutComponent::GetCurWeapon() const
{
	if (!EquippedSlots.IsValidIndex(SelectedSlotIdx)) return nullptr;

	const FLoadoutSlot& Slot = EquippedSlots[SelectedSlotIdx];
	return Slot.SlotType == ELoadoutSlotType::Weapon ? Slot.WeaponData : nullptr;
}

UTrapData* ULoadoutComponent::GetCurTrap() const
{
	if (!EquippedSlots.IsValidIndex(SelectedSlotIdx)) return nullptr;

	const FLoadoutSlot& Slot = EquippedSlots[SelectedSlotIdx];
	return Slot.SlotType == ELoadoutSlotType::Trap ? Slot.TrapData : nullptr;
}

UItemData* ULoadoutComponent::GetCurItem() const
{
	if (!EquippedSlots.IsValidIndex(SelectedSlotIdx)) return nullptr;

	const FLoadoutSlot& Slot = EquippedSlots[SelectedSlotIdx];
	return Slot.SlotType == ELoadoutSlotType::Item ? Slot.ItemData : nullptr;
}

