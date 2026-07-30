#pragma once

#include "CoreMinimal.h"
#include "Equipment/EquipmentData.h"
#include "WeaponData.generated.h"

class UAnimSequenceBase;
class UAnimMontage;
class UAnimInstance;
class ADefenseWeaponActor;

UENUM(BlueprintType)
enum class EWeaponAttackType : uint8
{
	Attack,
	AltAttack
};

USTRUCT(BlueprintType)
struct FAttackData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="1"))
	int32 Count = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0"))
	float Damage = 10.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0"))
	float Range = 3000.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0"))
	float Cooldown = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0"))
	float ManaCost = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Shape", meta=(ClampMin="0"))
	float Radius = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Animation")
	TObjectPtr<UAnimSequenceBase> Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Animation")
	FName AnimationSlotName = TEXT("DefaultSlot");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Animation")
	TObjectPtr<UAnimMontage> Montage;
};


UCLASS(BlueprintType)
class DEFENSE_API UWeaponData : public UEquipmentData
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Visual")
	TSubclassOf<ADefenseWeaponActor> WeaponActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Visual")
	FName WeaponAttachSocket = TEXT("weapon_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Visual")
	FTransform WeaponAttachTransform = FTransform(
		FRotator(0.f, -89.999988f, 0.f),
		FVector::ZeroVector,
		FVector::OneVector
	);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	TSubclassOf<UAnimInstance> EquippedAnimLayerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	FAttackData Attack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	FAttackData AltAttack;
};
