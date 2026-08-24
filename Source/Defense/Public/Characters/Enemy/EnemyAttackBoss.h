// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnemyAttack.h"
#include "EnemyAttackBoss.generated.h"

/**
 * 공격형 보스의 게임플레이 인스턴스.
 *
 * 시네마틱 동안에는 서버의 실제 보스를 숨기고 AI를 정지한 채 유지하고,
 * 시네마틱 종료 시 지정된 위치에서 전투를 재개한다.
 */
UCLASS()
class DEFENSE_API AEnemyAttackBoss : public AEnemyAttack
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void SetCombat() override;
	virtual void SetInactive() override;
	virtual void OnTargetPerceptionUpdated(AActor* Actor, struct FAIStimulus Stimulus) override;
	virtual void AttackTarget() override;
	virtual bool CanAttack() const override;
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser
	) override;

	/** 서버의 실제 보스를 시네마틱 대기 상태로 전환한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Boss|Cinematic")
	bool BeginCinematicHold();

	/** 서버의 실제 보스를 지정 위치로 옮기고 전투를 재개한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Boss|Cinematic")
	bool EndCinematicHold(const FTransform& ResumeTransform);

	UFUNCTION(BlueprintPure, Category="Enemy|Boss|Cinematic")
	bool IsInCinematicHold() const { return bCinematicHold; }

protected:
	UPROPERTY(
		ReplicatedUsing=OnRep_CinematicHold,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category="Enemy|Boss|Cinematic"
	)
	bool bCinematicHold = false;

	UFUNCTION()
	void OnRep_CinematicHold();

private:
	void ApplyCinematicHoldVisualState();
	void StopBossLogicForCinematic();
	bool RestartBossLogicAfterCinematic();
};
