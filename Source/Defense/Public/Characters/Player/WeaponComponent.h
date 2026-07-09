#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Equipment/WeaponData.h"
#include "WeaponComponent.generated.h"

class ADefenseCharacter;
class ADefenseWeaponActor;
class UEquipmentData;
class UWeaponData;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEFENSE_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UWeaponComponent();

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void Attack(EWeaponAttackType AttackType);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestAttack(EWeaponAttackType AttackType);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_PlayAttack(EWeaponAttackType AttackType);

	ADefenseCharacter* GetOwnerCharacter() const;
	UWeaponData* GetCurWeaponData() const;
	const FAttackData* GetAttackData(UWeaponData* WeaponData, EWeaponAttackType AttackType) const;
	void RefreshEquippedWeapon();
	void SpawnAndAttachWeaponActor(UWeaponData* WeaponData);
	void ApplyWeaponAnimLayer(UWeaponData* WeaponData);
	void PlayAttackAnimation(ADefenseCharacter* OwnerCharacter, const FAttackData& AttackData);
	void HitscanAttack(const FAttackData& AttackData);

	UFUNCTION()
	void HandleSelectedEquipmentChanged(int32 SelectedSlotIdx, UEquipmentData* SelectedEquipment);

	float LastAttackServerTime = -BIG_NUMBER;
	float LastAltAttackServerTime = -BIG_NUMBER;

	UPROPERTY(Transient)
	TObjectPtr<ADefenseWeaponActor> EquippedWeaponActor;

	UPROPERTY(Transient)
	TSubclassOf<ADefenseWeaponActor> EquippedWeaponActorClass;
};
