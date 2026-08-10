// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Barricade.generated.h"

class AEnemyBase;
class UBoxComponent;
class UNavModifierComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS()
class DEFENSE_API ABarricade : public AActor
{
	GENERATED_BODY()

public:
	ABarricade();

	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser
	) override;

	float GetDistanceToSurface(const FVector& FromLocation) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Barricade|Components")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Barricade|Components")
	TObjectPtr<UStaticMeshComponent> Cube;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Barricade|Components")
	TObjectPtr<UBoxComponent> Sensor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Barricade|Components")
	TObjectPtr<UNavModifierComponent> NavModifier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Barricade|Sensor", meta=(ClampMin="0.0"))
	float SensorActivationDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Barricade|AI", meta=(ClampMin="0.0"))
	float PatrolNotifyRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Barricade|Health", meta=(ClampMin="0.0"))
	float HP = 50.0f;

	FTimerHandle SensorActivationTimer;
	TSet<TWeakObjectPtr<AEnemyBase>> OverlappingEnemies;
	bool bReleasedEnemies = false;

	void ActivateSensor();
	void EngageEnemy(AEnemyBase* Enemy);
	void ReleaseEnemy(AEnemyBase* Enemy);
	void ReleaseAllEnemies();
	void NotifyNearbyWaitingRunEnemies();

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
