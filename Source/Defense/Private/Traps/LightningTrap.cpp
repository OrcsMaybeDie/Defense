#include "Traps/LightningTrap.h"

#include "Characters/Enemy/Data/EnemyData.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ALightningTrap::ALightningTrap()
{
	PrimaryActorTick.bCanEverTick = false;
	DamageArea->ComponentTags.AddUnique(ManualDamageAreaTag);
}

void ALightningTrap::StartDamageTimer()
{
	if (!HasAuthority() || !IsPlaced() || Damage <= 0.f || DamageInterval <= 0.f) return;

	TryStrike();
}

void ALightningTrap::HandleEnemyEnteredDamageArea(AEnemyBase* Enemy)
{
	if (!HasAuthority() || !IsPlaced() || !IsValidLightningTarget(Enemy)) return;

	TryStrike();
}

void ALightningTrap::TryStrike()
{
	UWorld* World = GetWorld();
	if (!HasAuthority()
		|| !IsPlaced()
		|| !World
		|| Damage <= 0.f
		|| DamageInterval <= 0.f
		|| World->GetTimerManager().IsTimerActive(DamageTimerHandle))
	{
		return;
	}

	TArray<AEnemyBase*> Targets;
	for (auto It = OverlappingEnemies.CreateIterator(); It; ++It)
	{
		AEnemyBase* Enemy = Cast<AEnemyBase>(It->Get());
		if (!IsValid(Enemy))
		{
			It.RemoveCurrent();
			continue;
		}

		if (IsValidLightningTarget(Enemy))
		{
			Targets.Add(Enemy);
		}
	}

	if (Targets.IsEmpty()) return;

	World->GetTimerManager().SetTimer(
		DamageTimerHandle,
		this,
		&ALightningTrap::FinishCooldown,
		DamageInterval,
		false
	);

	const FVector EffectLocation = DamageArea ? DamageArea->GetComponentLocation() : GetActorLocation();
	Multicast_PlayDamageVFX(EffectLocation);

	for (AEnemyBase* Enemy : Targets)
	{
		UGameplayStatics::ApplyDamage(
			Enemy,
			Damage,
			GetInstigatorController(),
			this,
			UDamageType::StaticClass()
		);
	}
}

void ALightningTrap::FinishCooldown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTimerHandle);
	}

	TryStrike();
}

bool ALightningTrap::IsValidLightningTarget(const AEnemyBase* Enemy) const
{
	return IsValid(Enemy) && Enemy->EnemyMode == EEnemyMode::Combat;
}
