#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProfileSubsystem.generated.h"

class UEquipmentData;
class UProfileSaveGame;

// 해금 변경 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUnlockedEquipmentChanged);

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
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UProfileSaveGame> CurrentProfile;
	
	void CreateNewProfile();
	void InitializeDefaultUnlocks(); // 기본 해금 장비
	bool SaveProfile();
};
