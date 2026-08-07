// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "TimerManager.h"
#include "EnemyDestroy.generated.h"

UCLASS()
class DEFENSE_API AEnemyDestroy : public AEnemyBase
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemyDestroy();

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
	virtual void OnEnteredPatrol() override;
	
	virtual void ApplyEnemyData() override;
	
	// 애니메이션 재생
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPC_DestroyMotion();

	UFUNCTION(BlueprintPure)
	bool CanTryDestroy() const;

	bool TryFindDestroyTarget();

	UFUNCTION(BlueprintCallable)
	void DestroyTargetTrap();

	/*UFUNCTION(NetMulticast, Unreliable)
	void Multicast_DrawDestroySearchDebug(FVector SearchCenter, float SearchRadius, bool bFoundTrap);*/

	void MarkDestroyFinished(bool bDestroyedTrap);
	float GetDestroyDuration(float DefaultDuration = 1.2f) const;

	void ScheduleNextDestroyTry(float Cooldown);
	void ClearDestroyTryTimer();

	UFUNCTION()
	void SendDestroyEvent();
	
	// 함정 탐색 쿨
	float SearchCooldown = 2.f;
	
	// 파괴 쿨
	float DestroyCooldown = 5.f;
	
	// 파괴하는 범위
	float DestroyRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector DestroySearchOffset = FVector::ZeroVector;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> TargetTraps;

	UPROPERTY()
	float NextDestroyTryTime = -1000000.f;

	FTimerHandle DestroyTryTimerHandle;
	bool bDestroyTryPending = false;
	
	
};
