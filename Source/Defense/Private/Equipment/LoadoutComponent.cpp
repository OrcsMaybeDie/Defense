#include "Equipment/LoadoutComponent.h"


#include "Equipment/EquipmentData.h"
#include "Equipment/ItemData.h"
#include "Equipment/WeaponData.h"
#include "Traps/TrapData.h"

ULoadoutComponent::ULoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

