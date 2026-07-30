#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DefenseWeaponActor.generated.h"

class USkeletalMeshComponent;

UCLASS(Blueprintable)
class DEFENSE_API ADefenseWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	ADefenseWeaponActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|FX")
	void OnFireVFX();
};
