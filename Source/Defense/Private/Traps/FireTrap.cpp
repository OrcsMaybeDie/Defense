#include "Traps/FireTrap.h"

#include "Characters/Enemy/Data/EnemyData.h"
#include "Characters/Enemy/EnemyBase.h"

AFireTrap::AFireTrap()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AFireTrap::ApplyWallHitEffect(
	AEnemyBase* Enemy,
	const FVector& EffectStart,
	const FVector& EffectEnd
)
{
	if (!HasAuthority()
		|| !IsPlaced()
		|| !IsValid(Enemy)
		|| Damage <= 0.f
		|| DamageInterval <= 0.f
		|| BurnDuration <= 0.f)
	{
		return false;
	}

	if (Enemy->EnemyMode != EEnemyMode::Combat
		|| Enemy->EnemyType == EEnemyType::Run)
	{
		return false;
	}

	Enemy->ApplyBurnEffect(BurnDuration, DamageInterval, Damage, this);
	Multicast_PlayWallShotVFX(EffectStart, EffectEnd);
	return true;
}
