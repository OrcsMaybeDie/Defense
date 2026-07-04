#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LoadoutComponent.generated.h"

class UEquipmentData;
class UWeaponData;
class UTrapData;
class UItemData;


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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
	int32 SelectedSlotIdx = 0;
	
public:
	UFUNCTION(BlueprintPure, Category="Loadout")
	UEquipmentData* GetCurEquipment() const;

	UFUNCTION(BlueprintPure, Category="Loadout")
	UWeaponData* GetCurWeapon() const;

	UFUNCTION(BlueprintPure, Category="Loadout")
	UTrapData* GetCurTrap() const;

	UFUNCTION(BlueprintPure, Category="Loadout")
	UItemData* GetCurItem() const;
};
