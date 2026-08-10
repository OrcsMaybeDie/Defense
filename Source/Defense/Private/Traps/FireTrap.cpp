#include "Traps/FireTrap.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Components/BoxComponent.h"

AFireTrap::AFireTrap()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFireTrap::BeginPlay()
{
	Super::BeginPlay();

	if (DamageArea)
	{
		DamageArea->OnComponentBeginOverlap.AddUniqueDynamic(this, &AFireTrap::OnFireAreaBeginOverlap);
	}
}

void AFireTrap::InitializePlacedTrap(
	UTrapData* TrapData,
	ADefensePlayerState* InInstalledByPlayerState,
	const TArray<FTrapCellKey>& InOccupiedCells
)
{
	Super::InitializePlacedTrap(TrapData, InInstalledByPlayerState, InOccupiedCells);

	// AEnemyBase owns the burn timers. Do not also run ATrapBase's direct-damage timer.
	StopDamageTimer();
}

void AFireTrap::OnFireAreaBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!HasAuthority() || !IsPlaced() || Damage <= 0.f || DamageInterval <= 0.f || BurnDuration <= 0.f)
	{
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(OtherActor);
	if (!IsValid(Enemy) || Enemy->EnemyMode != EEnemyMode::Combat)
	{
		return;
	}

	Enemy->ApplyBurnEffect(BurnDuration, DamageInterval, Damage, this);
}
