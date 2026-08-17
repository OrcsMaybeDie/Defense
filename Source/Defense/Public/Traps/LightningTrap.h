#pragma once

#include "CoreMinimal.h"
#include "Traps/TrapBase.h"
#include "LightningTrap.generated.h"

/**
 * Marker class for lightning traps.
 * Enemy damage handling uses the damage causer type to trigger the electric hit visual.
 */
UCLASS()
class DEFENSE_API ALightningTrap : public ATrapBase
{
	GENERATED_BODY()

public:
	ALightningTrap();
};
