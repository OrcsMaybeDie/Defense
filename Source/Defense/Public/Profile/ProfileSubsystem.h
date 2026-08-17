#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProfileSubsystem.generated.h"

class UEquipmentData;
class UProfileSaveGame;

// 해금 변경 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUnlockedEquipmentChanged);

// QuickSlot 변경 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnQuickSlotsChanged);

UCLASS()
class DEFENSE_API UProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	const UProfileSaveGame* GetCurrentProfile() const { return CurrentProfile; }
	
	// 해금 여부 조회
	UFUNCTION(BlueprintPure, Category = "Profile")
	bool IsEquipmentUnlocked(const UEquipmentData* EquipmentData) const;
	
	// 해금 (+저장)
	UFUNCTION(BlueprintCallable, Category = "Profile")
	bool UnlockEquipment(const UEquipmentData* EquipmentData);
	
	// 해금 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Profile")
	FOnUnlockedEquipmentChanged OnUnlockedEquipmentChanged;
	
	// 전체 장비 조회 (Trap, Weapon, Item)
	UFUNCTION(BlueprintCallable, Category = "Profile")
	TArray<UEquipmentData*> GetAllEquipmentData() const;
	
	// QuickSlot 개수 조회
	UFUNCTION(BlueprintPure, Category = "Profile|QuickSlot")
	int32 GetQuickSlotCount() const;
	
	// QuickSlot 장비 ID 목록 조회
	TArray<FPrimaryAssetId> GetQuickSlotEquipmentIds() const;

	// QuickSlot 확장 (확장 조건은 추후 결정)
	UFUNCTION(BlueprintCallable, Category = "Profile|QuickSlot")
	bool ExpandQuickSlots(int32 AddSlotCount);
	
	// QuickSlot 변경 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Profile|QuickSlot")
	FOnQuickSlotsChanged OnQuickSlotsChanged;
	
	// Slot 장비 조회
	UFUNCTION(BlueprintCallable, Category = "Profile|QuickSlot")
	UEquipmentData* GetQuickSlotEquipment(int32 SlotIndex) const;
	
	// Slot에 장비 장착
	UFUNCTION(BlueprintCallable, Category = "Profile|QuickSlot")
	bool AssignEquipmentToQuickSlot(int32 SlotIndex, const UEquipmentData* EquipmentData);
	
	// Slot 장비 해제
	UFUNCTION(BlueprintCallable, Category = "Profile|QuickSlot")
	bool ClearQuickSlot(int32 SlotIndex);
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UProfileSaveGame> CurrentProfile;
	
	void CreateNewProfile();
	void InitializeDefaultUnlocks(); // 기본 해금 장비
	void InitializeQuickSlots();
	void InitializeDefaultQuickSlotAssignments();
	bool SaveProfile();
};
