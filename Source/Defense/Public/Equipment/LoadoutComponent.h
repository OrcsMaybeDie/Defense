#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LoadoutComponent.generated.h"

class UWeaponData;
class UTrapData;
class UItemData;

UENUM(BlueprintType)
enum class ELoadoutSlotType : uint8
{
	Empty,
	Trap,
	Weapon,
	Item
};

USTRUCT(BlueprintType)
struct FLoadoutSlot
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ELoadoutSlotType SlotType = ELoadoutSlotType::Empty;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UWeaponData> WeaponData = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UTrapData> TrapData = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UItemData> ItemData = nullptr;
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
	UFUNCTION(BlueprintCallable, Category="Loadout")
	UWeaponData* GetCurWeapon() const;

	UFUNCTION(BlueprintCallable, Category="Loadout")
	UTrapData* GetCurTrap() const;
	
	UFUNCTION(BlueprintCallable, Category="Loadout")
	UItemData* GetCurItem() const;
};
