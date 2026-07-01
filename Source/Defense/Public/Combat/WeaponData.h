#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "Engine/DataAsset.h"
#include "WeaponData.generated.h"

class UAnimSequenceBase;
class ADefenseArrowProjectile;

UENUM(BlueprintType)
enum class EAttackDelivery : uint8
{
	None,
	Hitscan,
	Projectile
};

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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	EAttackDelivery Delivery = EAttackDelivery::Hitscan;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Projectile")
	TSubclassOf<ADefenseArrowProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Projectile", meta=(ClampMin="0"))
	float ProjectileSpeed = 3000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Projectile")
	FName ProjectileSpawnSocketName = TEXT("arrow");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Animation")
	TObjectPtr<UAnimSequenceBase> Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Animation")
	FName AnimationSlotName = TEXT("DefaultSlot");
};


UCLASS(BlueprintType)
class DEFENSE_API UWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	FText DisplayName;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	FAttackData Attack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attack")
	FAttackData AltAttack;
};
