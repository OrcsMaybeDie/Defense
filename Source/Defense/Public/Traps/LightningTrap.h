#pragma once

#include "CoreMinimal.h"
#include "Traps/TrapBase.h"
#include "LightningTrap.generated.h"

/** DamageArea 안의 적에게 주기적으로 낙뢰 피해 적용 */
UCLASS()
class DEFENSE_API ALightningTrap : public ATrapBase
{
	GENERATED_BODY()

public:
	ALightningTrap();

protected:
	virtual void StartDamageTimer() override;
	virtual void HandleEnemyEnteredDamageArea(AEnemyBase* Enemy) override;

	void TryStrike();
	void FinishCooldown();
	bool IsValidLightningTarget(const AEnemyBase* Enemy) const;
};
