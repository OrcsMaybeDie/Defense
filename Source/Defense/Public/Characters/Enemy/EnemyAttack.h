// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "EnemyAttack.generated.h"

UCLASS()
class DEFENSE_API AEnemyAttack : public AEnemyBase
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemyAttack();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	virtual void SetPreview() override;
	virtual void SetCombat() override;
	virtual void SetInactive() override;
	
	
	//-----------AI Perception-------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UAIPerceptionComponent> AIComp;

	
	// 시야로 적 감지 (일정 거리 이내)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UAISenseConfig_Sight> SightConfig;
	
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, struct FAIStimulus Stimulus);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Target")
	bool bLockedTarget = false;

	// 애니메이션 재생
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPC_AttackMotion();
	
	// Data Asset에서 가져옴
	// 타겟에 가할 데미지
	float DamageNum = 10.f;
	// 공격 쿨타임
	float AttackCooldown = 1.2f;

	UPROPERTY()
	float LastAttackFinishedTime = -1000000.f;
	
	// 공격 StateTree Task에서 실행. 서버에서만 실행. Enemy가 서버에서 스폰되기때문에 RPC지정X
	UFUNCTION(BlueprintCallable)
	void AttackTarget();

	bool CanAttack() const;
	void MarkAttackFinished();
	float GetAttackDuration(float DefaultDuration = 1.2f) const;
	
	virtual void ApplyEnemyData() override;
};
