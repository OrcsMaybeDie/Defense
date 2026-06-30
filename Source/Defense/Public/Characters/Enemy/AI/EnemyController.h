// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyController.generated.h"

UCLASS()
class DEFENSE_API AEnemyController : public AAIController
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEnemyController();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UStateTreeAIComponent> StateTreeAIComp;
	
	virtual void OnPossess(APawn* InPawn) override;
	
	UPROPERTY()
	TObjectPtr<class AEnemyRoute> EnemyRoute = nullptr;
};
