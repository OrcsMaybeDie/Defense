#pragma once

#include "CoreMinimal.h"
#include "Traps/TrapBase.h"
#include "FireTrap.generated.h"

class UPrimitiveComponent;

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

	virtual void InitializePlacedTrap(
		UTrapData* TrapData,
		ADefensePlayerState* InInstalledByPlayerState,
		const TArray<FTrapCellKey>& InOccupiedCells
	) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire Trap|Burn", meta=(ClampMin="0.1", Units="s"))
	float BurnDuration = 5.f;

private:
	UFUNCTION()
	void OnFireAreaBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);
};
