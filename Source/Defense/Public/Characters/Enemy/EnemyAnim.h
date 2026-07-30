// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EnemyAnim.generated.h"

/**
 * 
 */
UCLASS()
class DEFENSE_API UEnemyAnim : public UAnimInstance
{
	GENERATED_BODY()
	
	public:
	virtual void NativeInitializeAnimation() override;
	//virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	
	UPROPERTY()
	TObjectPtr<class AEnemyBase> Enemy;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UAnimMontage> AttackMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UAnimMontage> DestroyMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UAnimMontage> DamagedMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UAnimMontage> DieMontage;
	
	void PlayAttackMotion();
	void PlayDestroyMotion();
	void PlayDamageMotion();
	void PlayDieMotion();
	
	/*UFUNCTION()
	void AnimNotify_AttackEnd();*/
	UFUNCTION()
	void AnimNotify_Hit();
	
	UFUNCTION()
	void AnimNotify_Destroy();
	
	/*UFUNCTION()
	void AnimNotify_DamageEnd();
	
	UFUNCTION()
	void AnimNotify_DieEnd();*/
	
	/*void PlayDamageMontage(int32 idx);
	void PlayDieMontage();*/
	
};
