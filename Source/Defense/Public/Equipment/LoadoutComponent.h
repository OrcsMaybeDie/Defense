#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LoadoutComponent.generated.h"

class UEquipmentData;
class UWeaponData;
class UTrapData;
class UItemData;
class UProfileSubsystem;


// Delegate (event) 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnSelectedEquipmentChanged,
	int32, SelectedSlotIdx,
	UEquipmentData*, SelectedEquipment
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadoutSlotsChanged);

USTRUCT(BlueprintType)
struct FLoadoutSlot
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UEquipmentData> EquipmentData = nullptr;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEFENSE_API ULoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULoadoutComponent();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
	TArray<FLoadoutSlot> EquippedSlots;

	UPROPERTY(ReplicatedUsing=OnRep_SelectedSlotIdx, BlueprintReadOnly, Category="Loadout")
	int32 SelectedSlotIdx = 0;
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category="Loadout")
	void SelectSlot(int32 SlotIdx);
	
	UFUNCTION(BlueprintPure, Category="Loadout")
	bool CanSelectSlot(int32 SlotIdx) const;
	
	UFUNCTION(BlueprintPure, Category="Loadout")
	int32 GetSelectedSlotIdx() const { return SelectedSlotIdx; }
	
	UFUNCTION(BlueprintPure, Category="Loadout")
	UEquipmentData* GetCurEquipment() const;

	UFUNCTION(BlueprintPure, Category="Loadout")
	UWeaponData* GetCurWeapon() const;

	UFUNCTION(BlueprintPure, Category="Loadout")
	UTrapData* GetCurTrap() const;

	UFUNCTION(BlueprintPure, Category="Loadout")
	UItemData* GetCurItem() const;

	// event) 선택 번호 or 현재 선택 장비가 변경됨
	UPROPERTY(BlueprintAssignable, Category="Loadout")
	FOnSelectedEquipmentChanged OnSelectedEquipChanged;

	// event) Slot 개수 or 배치된 장비가 변경됨
	UPROPERTY(BlueprintAssignable, Category="Loadout")
	FOnLoadoutSlotsChanged OnLoadoutSlotsChanged;

	// getter
	UFUNCTION(BlueprintPure, Category="Loadout")
	int32 GetSlotCount() const { return EquippedSlots.Num(); }
	UFUNCTION(BlueprintPure, Category="Loadout")
	UEquipmentData* GetEquipAtSlot(int32 SlotIdx) const;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeSlotsFromProfile(); // 저장된 QuickSlot을 캐릭터의 런타임 LoadoutComp에 적용

	UFUNCTION()
	void HandleProfileQuickSlotsChanged();

	UProfileSubsystem* GetProfileSubsystem() const;

	UFUNCTION()
	void OnRep_SelectedSlotIdx(); // 복제 처리
	
	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestSelectSlot(int32 SlotIdx); // RPC
	
	void SetSelectedSlotIdx(int32 SlotIdx); // Server-side state update
};
