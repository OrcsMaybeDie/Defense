#pragma once

#include "CoreMinimal.h"
#include "Traps/TrapBase.h"
#include "FireTrap.generated.h"

/**
 * Trap that delegates damage-over-time ownership to AEnemyBase.
 * Damage and DamageInterval come from the trap data asset; BurnDuration is set
 * on the fire-trap Blueprint/class defaults.
 */
UCLASS()
class DEFENSE_API AFireTrap : public ATrapBase
{
	GENERATED_BODY()

public:
	AFireTrap();

protected:
	virtual bool ApplyWallHitEffect(AEnemyBase* Enemy, const FVector& EffectStart, const FVector& EffectEnd) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire Trap|Burn", meta=(ClampMin="0.1", Units="s"))
	float BurnDuration = 5.f;
};
