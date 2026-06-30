// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyRoute.generated.h"

UCLASS()
class DEFENSE_API AEnemyRoute : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEnemyRoute();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class USplineComponent> SplineComp;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SampleInterval = 300.f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FVector> Waypoints;

	UFUNCTION(BlueprintCallable)
	void GenerateWaypoints();
};
