#pragma once

#include "CoreMinimal.h"
#include "Traps/BarricadeTrap.h"
#include "FractureDoor.generated.h"

class AFractureDoorDebris;
class USceneComponent;

/**
 * A barricade that plays a cosmetic hit shake and replaces itself with
 * a short-lived Geometry Collection actor when its HP is depleted.
 */
UCLASS(Blueprintable)
class DEFENSE_API AFractureDoor : public ABarricadeTrap
{
	GENERATED_BODY()

public:
	AFractureDoor();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void HandleDamageApplied(float AppliedDamage, AActor* DamageCauser) override;
	virtual void HandleHPDepleted(AActor* DamageCauser) override;

	/** Visual-only root. DamageArea and Sensor remain stationary while this shakes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fracture Door|Components")
	TObjectPtr<USceneComponent> DoorVisualRoot;

	/** Replicated visual actor spawned by the server when the door breaks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Fracture")
	TSubclassOf<AFractureDoorDebris> FractureDebrisClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Hit Shake", meta=(ClampMin="0.01", Units="s"))
	float HitShakeDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Hit Shake", meta=(ClampMin="0.0", Units="Hz"))
	float HitShakeFrequency = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Hit Shake")
	FVector HitShakeLocationAmplitude = FVector(2.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Hit Shake")
	FRotator HitShakeRotationAmplitude = FRotator(0.0f, 0.0f, 4.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Hit Shake", meta=(ClampMin="0.1"))
	float HitShakeDampingExponent = 1.5f;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitShake();

private:
	FTransform DoorVisualRestTransform = FTransform::Identity;
	float HitShakeElapsed = 0.0f;
	bool bHitShakeActive = false;

	void UpdateHitShake(float DeltaTime);
	void RestoreDoorVisual();
};
