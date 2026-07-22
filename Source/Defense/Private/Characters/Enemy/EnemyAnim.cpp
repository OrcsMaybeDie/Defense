// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyAnim.h"

#include "Characters/Enemy/EnemyAttack.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemyDestroy.h"

void UEnemyAnim::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Enemy = Cast<AEnemyBase>(TryGetPawnOwner());
}

void UEnemyAnim::PlayAttackMotion()
{
	Montage_Play(AttackMontage);
}

void UEnemyAnim::PlayDestroyMotion()
{
	Montage_Play(DestroyMontage);
}

void UEnemyAnim::PlayDamageMotion()
{
	Montage_Play(DamagedMontage);
}

void UEnemyAnim::PlayDieMotion()
{
	Montage_Play(DieMontage);
}

void UEnemyAnim::AnimNotify_Hit()
{
	if (Enemy && Enemy->HasAuthority())
	{
		auto* EnemyAttack = Cast<AEnemyAttack>(Enemy);
		if (EnemyAttack)
		{
			EnemyAttack->AttackTarget();
		}
	}
}

void UEnemyAnim::AnimNotify_Destroy()
{
	if (Enemy && Enemy->HasAuthority())
	{
		auto* EnemyDestroy = Cast<AEnemyDestroy>(Enemy);
		if (EnemyDestroy)
		{
			EnemyDestroy->DestroyTargetTrap();
		}
	}
}
