#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Equipment/WeaponData.h"
#include "WeaponComponent.generated.h"

class ADefenseCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEFENSE_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UWeaponComponent();

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void Attack(EWeaponAttackType AttackType);

protected:
	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestAttack(EWeaponAttackType AttackType);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_PlayAttack(EWeaponAttackType AttackType);

	ADefenseCharacter* GetOwnerCharacter() const;
	UWeaponData* GetCurWeaponData() const;
	const FAttackData* GetAttackData(UWeaponData* WeaponData, EWeaponAttackType AttackType) const;
	void HitscanAttack(const FAttackData& AttackData);

	float LastAttackServerTime = -BIG_NUMBER;
	float LastAltAttackServerTime = -BIG_NUMBER;
};
