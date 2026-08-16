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

void UEnemyAnim::PlayBurnReactionMotion()
{
	if (BurnReactionMontage && !Montage_IsPlaying(BurnReactionMontage))
	{
		Montage_Play(BurnReactionMontage);
	}
}

void UEnemyAnim::StopBurnReactionMotion(const float BlendOutTime)
{
	if (BurnReactionMontage && Montage_IsActive(BurnReactionMontage))
	{
		Montage_Stop(BlendOutTime, BurnReactionMontage);
	}
}

void UEnemyAnim::AnimNotify_Hit()
{
	// Damage is applied by the authoritative attack StateTree task.
}

void UEnemyAnim::AnimNotify_Destroy()
{
	// Trap destruction is handled by the authoritative destroy StateTree task.
}
