#pragma once

#include "CoreMinimal.h"
#include "Equipment/EquipmentData.h"
#include "WeaponData.generated.h"

class UAnimSequenceBase;
class UAnimMontage;
class UAnimInstance;
class ADefenseWeaponActor;

UENUM(BlueprintType)
enum class EWeaponActionType : uint8
{
	Fire,
	ChargedFire
};

UENUM(BlueprintType)
enum class EWeaponActionPhase : uint8
{
	Started,
	Executed,
	Cancelled
};

UENUM(BlueprintType)
enum class EWeaponChargeStage : uint8
{
	None,
	Stage1,
	Stage2,
	Stage3
};

USTRUCT(BlueprintType)
struct FWeaponShotData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire", meta=(ClampMin="0"))
	float Damage = 10.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire", meta=(ClampMin="0", Units="cm"))
	float Range = 3000.f;
	
	// 권총/라이플 모두 이 값으로 연사 간격을 조절한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire", meta=(ClampMin="0", Units="s"))
	float Cooldown = 0.5f;

	// 조준선 중심에서 조금 벗어난 적도 명중시키는 플레이어 조준 보정 반경이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire|Aim Assist", meta=(ClampMin="0", Units="cm"))
	float AimAssistRadius = 0.f;
};

USTRUCT(BlueprintType)
struct FWeaponFireData : public FWeaponShotData
{
	GENERATED_BODY()

	// 일반/마나 공격이 함께 사용하는 무기당 단 하나의 발사 애니메이션이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire|Animation")
	TObjectPtr<UAnimSequenceBase> Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire|Animation")
	FName AnimationSlotName = TEXT("DefaultSlot");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire|Animation")
	TObjectPtr<UAnimMontage> Montage;
};

USTRUCT(BlueprintType)
struct FWeaponChargeStageData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge Stage", meta=(ClampMin="0"))
	float ManaCost = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge Stage")
	FWeaponShotData ReleaseFire;

	// 공격 실행 후 이동/점프/공격/차지/장비 변경/건설 입력을 막는 행동 경직 시간이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge Stage|Recovery", meta=(ClampMin="0", Units="s"))
	float MovementLockDuration = 0.f;
};

USTRUCT(BlueprintType)
struct FWeaponChargeData
{
	GENERATED_BODY()

	FWeaponChargeData()
	{
		Stage1.ManaCost = 10.f;
		Stage2.ManaCost = 20.f;
		Stage3.ManaCost = 30.f;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge")
	bool bEnabled = true;

	// 전체 차지 시간. 1/3, 2/3, 3/3 지점에서 Stage1, Stage2, Stage3가 확정된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge", meta=(ClampMin="0.01", Units="s"))
	float MaxChargeTime = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge")
	FWeaponChargeStageData Stage1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge")
	FWeaponChargeStageData Stage2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge")
	FWeaponChargeStageData Stage3;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	TSubclassOf<UAnimInstance> EquippedAnimLayerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Fire")
	FWeaponFireData Fire;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Fire")
	FWeaponChargeData ChargedFire;
};
