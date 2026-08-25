#pragma once

#include "CoreMinimal.h"
#include "Traps/TrapBase.h"
#include "FireTrap.generated.h"

/** DamageArea 진입 시 분사하고, 지속시간 종료 후 쿨타임 진입 */
UCLASS()
class DEFENSE_API AFireTrap : public ATrapBase
{
	GENERATED_BODY()

public:
	AFireTrap();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void StartDamageTimer() override;
	virtual void StopDamageTimer() override;
	virtual void HandleEnemyEnteredDamageArea(AEnemyBase* Enemy) override;

	void TryStartFireAttack();
	void EndFireAttack();
	void FinishFireCooldown();
	void ApplyFireToEnemy(AEnemyBase* Enemy, float Duration);
	bool IsValidFireTarget(const AEnemyBase* Enemy) const;

	UFUNCTION()
	void OnRep_FireActive();

	// 분사 및 화상 지속시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire Trap|Attack",
		meta=(DisplayName="Fire Duration", ClampMin="0.1", Units="s"))
	float BurnDuration = 5.f;

	// 분사 중 화상 피해 간격
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire Trap|Attack",
		meta=(DisplayName="Fire Damage Interval", ClampMin="0.1", Units="s"))
	float BurnDamageInterval = 0.5f;

	UPROPERTY(ReplicatedUsing=OnRep_FireActive, VisibleInstanceOnly, BlueprintReadOnly, Category="Fire Trap")
	bool bFireActive = false;

	FTimerHandle FireDurationTimerHandle;
	float FireEndTime = 0.f;
};
