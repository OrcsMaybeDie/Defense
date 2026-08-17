#include "Equipment/LoadoutComponent.h"


#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Profile/ProfileSubsystem.h"
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
	
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	
	// 이 PC가 직접 조작하는 캐릭터에만 로컬 프로필 적용
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		InitializeSlotsFromProfile();
	}

	// 시작 장비 0번 슬롯으로 고정
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		SetSelectedSlotIdx(0);
	}
}

void ULoadoutComponent::InitializeSlotsFromProfile()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	
	if (!GameInstance)
	{
		return;
	}
	
	UProfileSubsystem* ProfileSubsystem = GameInstance->GetSubsystem<UProfileSubsystem>();
	
	if (!ProfileSubsystem)
	{
		return;
	}
	
	const int32 QuickSlotCount = ProfileSubsystem->GetQuickSlotCount();
	
	EquippedSlots.SetNum(QuickSlotCount);
	
	for (int32 SlotIdx = 0; SlotIdx < QuickSlotCount; SlotIdx++)
	{
		UEquipmentData* EquipmentData =
			ProfileSubsystem->GetQuickSlotEquipment(SlotIdx);

		EquippedSlots[SlotIdx].EquipmentData = EquipmentData;

		// 런타임 Loadout 적용 확인용
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Loadout] EquippedSlots[%d] = %s"),
			SlotIdx,
			*GetNameSafe(EquipmentData));
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
