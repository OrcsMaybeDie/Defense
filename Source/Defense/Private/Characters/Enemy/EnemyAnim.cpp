// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyAnim.h"

#include "Characters/Enemy/EnemyBase.h"

void UEnemyAnim::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Enemy = Cast<AEnemyBase>(TryGetPawnOwner());
}

void UEnemyAnim::PlayAttackMotion()
{
	Montage_Play(AttackMontage);
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
		Enemy->AttackTarget();
	}
}
