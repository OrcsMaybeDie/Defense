#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "BurnDamageType.generated.h"

/**
 * Damage marker used by the burn-over-time effect.
 * AEnemyBase uses this type to avoid entering its full-body damage StateTree state
 * for every burn tick; the additive burn reaction is played separately.
 */
UCLASS()
class DEFENSE_API UBurnDamageType : public UDamageType
{
	GENERATED_BODY()
};
