// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestructibleSetPieceActor.generated.h"

class UGeometryCollectionComponent;
class URadialFalloff;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Replicated set piece that swaps an intact static mesh for a Geometry Collection.
 * The server owns the destruction state so late-joining clients also keep the
 * intact mesh hidden after the cinematic has finished.
 */
UCLASS(BlueprintType)
class DEFENSE_API ADestructibleSetPieceActor : public AActor
{
	GENERATED_BODY()

public:
	ADestructibleSetPieceActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Starts the replicated destruction once. Must be called on the server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cinematic|Destruction")
	bool TriggerDestruction();

	/** Removes only the fractured GC while preserving the destroyed intact-mesh state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cinematic|Destruction")
	bool ClearDestructionDebris();

	UFUNCTION(BlueprintPure, Category = "Cinematic|Destruction")
	bool IsDestroyed() const { return bDestroyed; }

	UFUNCTION(BlueprintPure, Category = "Cinematic|Destruction")
	bool IsDestructionDebrisCleared() const { return bDestructionDebrisCleared; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The original mesh shown before the cinematic destruction cue. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> IntactMesh;

	/** Geometry Collection placed at the same local transform as IntactMesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGeometryCollectionComponent> GeometryCollection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic|Destruction")
	FVector DestructionOriginOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic|Destruction", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RadialImpulseRadius = 500.0f;

	/** Strain applied on the tick after the local Chaos proxy has been activated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic|Destruction", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ExternalStrain = 500000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic|Destruction")
	float RadialImpulseStrength = 1500.0f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Destroyed();

	UFUNCTION()
	void OnRep_DestructionDebrisCleared();

	/** Runs once on every machine when this set piece enters its destroyed state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cinematic|Destruction")
	void OnDestructionStarted();

private:
	void ApplyIntactState();
	void ApplyDestroyedState();
	void ApplyDestructionDebrisClearedState();
	void ActivateLocalGeometryCollection();
	void ApplyLocalFracture();

	UPROPERTY(ReplicatedUsing = OnRep_Destroyed)
	bool bDestroyed = false;

	UPROPERTY(ReplicatedUsing = OnRep_DestructionDebrisCleared)
	bool bDestructionDebrisCleared = false;

	UPROPERTY(Transient)
	TObjectPtr<URadialFalloff> ActiveStrainField;

	FTimerHandle LocalFractureTimerHandle;
	bool bDestroyedStateApplied = false;
};
