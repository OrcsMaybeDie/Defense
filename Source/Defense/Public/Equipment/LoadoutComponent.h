#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
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
	// ReplicatedEquipmentIds로부터 재구성되는 런타임 캐시
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Loadout")
	TArray<FLoadoutSlot> EquippedSlots;

	// 서버가 검증, 모든 클라이언트에 복제하는 QuickSlot 장비 ID
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedEquipmentIds, BlueprintReadOnly, Category="Loadout")
	TArray<FPrimaryAssetId> ReplicatedEquipmentIds;

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

	// 로컬 Profile의 QuickSlot을 서버에 제출
	void SubmitProfileLoadout();

	// 클라가 제출한 ID 배열 검증
	bool ValidateSubmittedEquipmentIds(const TArray<FPrimaryAssetId>& EquipmentIds) const;

	// 복제된 ID를 실제 장비 데이터로 변환
	void RebuildEquippedSlotsFromReplicatedIds();

	UFUNCTION()
	void OnRep_ReplicatedEquipmentIds(); // 클라가 새 배열을 복제받았을 때 엔진이 호출

	UFUNCTION()
	void HandleProfileQuickSlotsChanged();

	UProfileSubsystem* GetProfileSubsystem() const;

	UFUNCTION()
	void OnRep_SelectedSlotIdx(); // 복제 처리
	
	UFUNCTION(Server, Reliable)
	void ServerRPC_SubmitProfileLoadout(const TArray<FPrimaryAssetId>& EquipmentIds);

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestSelectSlot(int32 SlotIdx); // RPC
	
	void SetSelectedSlotIdx(int32 SlotIdx); // Server-side state update
};
