#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FractureDoorDebris.generated.h"

class UGeometryCollectionComponent;
class URadialFalloff;
class USceneComponent;

/**
 * Replicated spawn shell for a locally simulated Geometry Collection.
 * The server owns its lifetime; Chaos debris motion is cosmetic per client.
 */
UCLASS(Blueprintable)
class DEFENSE_API AFractureDoorDebris : public AActor
{
	GENERATED_BODY()

public:
	AFractureDoorDebris();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	/** Called while deferred-spawned so the collection starts at the source mesh. */
	void InitializeDoorTransform(const FTransform& DoorTransform);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fracture Door|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fracture Door|Components")
	TObjectPtr<UGeometryCollectionComponent> FracturedDoor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Physics", meta=(ClampMin="0.0", Units="s"))
	float FractureDelay = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Physics", meta=(ClampMin="0.0", Units="cm"))
	float FractureRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Physics", meta=(ClampMin="0.0"))
	float ExternalStrain = 500000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Physics", meta=(ClampMin="0.0"))
	float RadialImpulseStrength = 800.0f;

	/** Local offset from the Geometry Collection pivot at which strain is applied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Physics")
	FVector FractureOriginOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture Door|Lifetime", meta=(ClampMin="0.1", Units="s"))
	float DebrisLifetime = 8.0f;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Fracture Door")
	void OnFractureActivated(FVector FractureOrigin);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Replicated)
	FTransform DoorWorldTransform = FTransform::Identity;

	UPROPERTY(Replicated)
	bool bHasDoorTransform = false;

	UPROPERTY(Transient)
	TObjectPtr<URadialFalloff> ActiveStrainField = nullptr;

	float FractureElapsed = 0.0f;
	bool bPendingFracture = false;

	void ConfigureFractureComponent(UGeometryCollectionComponent* Component);
	void ActivateFractureComponent(UGeometryCollectionComponent* Component);
	void ApplyDoorTransform();
	void ApplyFracture();
};
