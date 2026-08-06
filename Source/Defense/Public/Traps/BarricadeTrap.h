// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TrapBase.h"
#include "BarricadeTrap.generated.h"

class AEnemyBase;
class UBoxComponent;
class UPrimitiveComponent;

UCLASS()
class DEFENSE_API ABarricadeTrap : public ATrapBase
{
	GENERATED_BODY()

public:
	ABarricadeTrap();

	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser
	) override;

	float GetDistanceToSurface(const FVector& FromLocation) const;

	virtual void InitializePlacedTrap(
		UTrapData* TrapData,
		ADefensePlayerState* InInstalledByPlayerState,
		const TArray<FTrapCellKey>& InOccupiedCells
	) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Barricade|Components")
	TObjectPtr<UBoxComponent> Sensor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Barricade|Sensor", meta=(ClampMin="0.0"))
	float SensorActivationDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Barricade|AI", meta=(ClampMin="0.0"))
	float PatrolNotifyRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Barricade|Health", meta=(ClampMin="0.0"))
	float HP = 50.0f;

	FTimerHandle SensorActivationTimer;
	TSet<TWeakObjectPtr<AEnemyBase>> SensorOverlappingEnemies;
	bool bReleasedEnemies = false;

	void ActivateSensor();
	void EngageEnemy(AEnemyBase* Enemy);
	void ReleaseEnemy(AEnemyBase* Enemy);
	void ReleaseAllEnemies();
	void NotifyNearbyWaitingRunEnemies();
	void ScheduleSensorActivation();
	void ApplyBoxExtents();

	UFUNCTION()
	void OnSensorBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void OnSensorEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);
};
