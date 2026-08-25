#include "Traps/FireTrap.h"

#include "Characters/Enemy/Data/EnemyData.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"

namespace
{
	const FName FireVFXComponentTag(TEXT("FireVFX"));
}

AFireTrap::AFireTrap()
{
	PrimaryActorTick.bCanEverTick = false;
	DamageArea->ComponentTags.AddUnique(ManualDamageAreaTag);
}

void AFireTrap::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFireTrap, bFireActive);
}

void AFireTrap::StartDamageTimer()
{
	if (!HasAuthority()
		|| !IsPlaced()
		|| Damage <= 0.f
		|| DamageInterval <= 0.f
		|| BurnDuration <= 0.f
		|| BurnDamageInterval <= 0.f)
	{
		return;
	}

	TryStartFireAttack();
}

void AFireTrap::StopDamageTimer()
{
	Super::StopDamageTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireDurationTimerHandle);
	}

	FireEndTime = 0.f;
	if (bFireActive)
	{
		bFireActive = false;
		OnRep_FireActive();
	}
}

void AFireTrap::HandleEnemyEnteredDamageArea(AEnemyBase* Enemy)
{
	if (!HasAuthority() || !IsPlaced() || !IsValidFireTarget(Enemy)) return;

	if (!bFireActive)
	{
		TryStartFireAttack();
		return;
	}

	const UWorld* World = GetWorld();
	if (!World) return;

	ApplyFireToEnemy(Enemy, FireEndTime - World->GetTimeSeconds());
}

void AFireTrap::TryStartFireAttack()
{
	UWorld* World = GetWorld();
	if (!HasAuthority()
		|| !IsPlaced()
		|| !World
		|| bFireActive
		|| World->GetTimerManager().IsTimerActive(DamageTimerHandle))
	{
		return;
	}

	bool bHasTarget = false;
	for (auto It = OverlappingEnemies.CreateIterator(); It; ++It)
	{
		AEnemyBase* Enemy = Cast<AEnemyBase>(It->Get());
		if (!IsValid(Enemy))
		{
			It.RemoveCurrent();
			continue;
		}

		if (!IsValidFireTarget(Enemy))
		{
			continue;
		}

		ApplyFireToEnemy(Enemy, BurnDuration);
		bHasTarget = true;
	}

	if (!bHasTarget) return;

	bFireActive = true;
	FireEndTime = World->GetTimeSeconds() + BurnDuration;
	OnRep_FireActive();
	ForceNetUpdate();

	World->GetTimerManager().SetTimer(
		FireDurationTimerHandle,
		this,
		&AFireTrap::EndFireAttack,
		BurnDuration,
		false
	);
}

void AFireTrap::EndFireAttack()
{
	if (!HasAuthority() || !bFireActive) return;

	bFireActive = false;
	FireEndTime = 0.f;
	OnRep_FireActive();
	ForceNetUpdate();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DamageTimerHandle,
			this,
			&AFireTrap::FinishFireCooldown,
			DamageInterval,
			false
		);
	}
}

void AFireTrap::FinishFireCooldown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTimerHandle);
	}

	TryStartFireAttack();
}

void AFireTrap::ApplyFireToEnemy(AEnemyBase* Enemy, const float Duration)
{
	if (!IsValidFireTarget(Enemy) || Duration <= 0.f) return;

	Enemy->ApplyBurnEffect(Duration, BurnDamageInterval, Damage, this);
}

bool AFireTrap::IsValidFireTarget(const AEnemyBase* Enemy) const
{
	return IsValid(Enemy)
		&& Enemy->EnemyMode == EEnemyMode::Combat
		&& Enemy->EnemyType != EEnemyType::Run;
}

void AFireTrap::OnRep_FireActive()
{
	if (GetNetMode() == NM_DedicatedServer) return;

	if (UParticleSystemComponent* FireVFX =
		FindComponentByTag<UParticleSystemComponent>(FireVFXComponentTag))
	{
		if (bFireActive)
		{
			FireVFX->ActivateSystem(true);
		}
		else
		{
			FireVFX->DeactivateSystem();
		}
	}
}
