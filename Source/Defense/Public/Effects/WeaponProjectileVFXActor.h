#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponProjectileVFXActor.generated.h"

class USceneComponent;

/** 실제 공격 판정과 분리된, 총구에서 명중점까지 이동하는 일회성 VFX 운반체다. */
UCLASS(NotBlueprintable)
class DEFENSE_API AWeaponProjectileVFXActor : public AActor
{
	GENERATED_BODY()

public:
	AWeaponProjectileVFXActor();

	void Configure(
		const FVector& InTargetLocation,
		TSubclassOf<AActor> InProjectileVFXActorClass,
		TSubclassOf<AActor> InImpactVFXActorClass,
		float InSpeed,
		float InScale
	);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void FinishProjectile();

	UPROPERTY(VisibleAnywhere, Category="Projectile VFX")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TSubclassOf<AActor> ProjectileVFXActorClass;

	UPROPERTY(Transient)
	TSubclassOf<AActor> ImpactVFXActorClass;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ProjectileEffectActor;

	FVector TargetLocation = FVector::ZeroVector;
	float TravelSpeed = 2400.f;
	float EffectScale = 1.f;
	bool bConfigured = false;
};
