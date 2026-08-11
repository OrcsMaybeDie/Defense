// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Portal.generated.h"

UCLASS()
class DEFENSE_API APortal : public AActor
{
	GENERATED_BODY()

public:
	APortal();
	FVector GetEntryPointLocation() const;
	FVector GetExitPointLocation() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	// PortalForward points toward the side enemies approach from. The clipped/exit side is -Forward.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal|Components")
	TObjectPtr<class USceneComponent> PortalPlane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal|Components")
	TObjectPtr<class UStaticMeshComponent> PortalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal|Components")
	TObjectPtr<class UBoxComponent> EntrySensor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal|Components")
	TObjectPtr<class USceneComponent> EntryPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal|Components")
	TObjectPtr<class UBoxComponent> ReturnPoolSensor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal|Components")
	TObjectPtr<class USceneComponent> ExitPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal|Clip", meta=(ClampMin="1.0", Units="cm"))
	float PortalHalfWidth = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal|Clip", meta=(ClampMin="1.0", Units="cm"))
	float PortalHalfHeight = 250.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UEnemyPoolSubsystem> EnemyPool;

	UFUNCTION()
	void OnEntrySensorBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void OnEntrySensorEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

	UFUNCTION()
	void OnReturnPoolSensorBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);
};
