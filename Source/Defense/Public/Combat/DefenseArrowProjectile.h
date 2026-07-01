#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DefenseArrowProjectile.generated.h"

class UProjectileMovementComponent;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class DEFENSE_API ADefenseArrowProjectile : public AActor
{
	GENERATED_BODY()

public:
	ADefenseArrowProjectile();

	virtual void Tick(float DeltaSeconds) override;

	void Launch(const FVector& Velocity, float InDamage);
	void IgnoreActor(AActor* ActorToIgnore);
	void SetCosmeticOnly(bool bNewCosmeticOnly);
	void SetDebugTrailEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UFUNCTION()
	void OnProjectileBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void DamageAndDestroy(AActor* OtherActor);
	void EnableProjectileCollision();
	void TraceDamageAlongMovement(const FVector& Start, const FVector& End);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage")
	float Damage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Debug")
	bool bDrawDebugTrailByDefault = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Debug", meta=(ClampMin="0"))
	float DebugTrailLifeTime = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Debug", meta=(ClampMin="0"))
	float DebugTrailThickness = 1.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> IgnoredActors;

	FTimerHandle CollisionEnableTimer;

	bool bHasHit = false;
	FVector LastDebugLocation = FVector::ZeroVector;
	bool bDamageTraceEnabled = false;
	bool bCosmeticOnly = false;
	bool bDrawDebugTrail = true;
};
