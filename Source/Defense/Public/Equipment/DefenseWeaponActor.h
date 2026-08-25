#pragma once

#include "CoreMinimal.h"
#include "Equipment/WeaponData.h"
#include "GameFramework/Actor.h"
#include "DefenseWeaponActor.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMeshComponent;

UCLASS(Blueprintable)
class DEFENSE_API ADefenseWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	ADefenseWeaponActor();
	virtual void OnConstruction(const FTransform& Transform) override;

	// 기존 Skeletal 무기 Blueprint와의 호환을 위해 이름과 타입을 유지한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	// Static Mesh가 지정되면 이를 우선 사용하고, 없으면 WeaponMesh를 사용한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UStaticMeshComponent> StaticWeaponMesh;

	/** 현재 실제로 표시 중인 Static/Skeletal Mesh 컴포넌트를 반환한다. */
	UFUNCTION(BlueprintPure, Category="Weapon")
	UMeshComponent* GetActiveWeaponMesh() const;

	// VFX, SFX처럼 즉시 확인할 수 있는 표현은 무기 Blueprint에서 구현한다.
	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Presentation")
	void OnWeaponAction(
		EWeaponActionType ActionType,
		EWeaponActionPhase Phase,
		EWeaponChargeStage ChargeStage,
		float ChargeRatio
	);

private:
	void RefreshActiveWeaponMesh();
};
