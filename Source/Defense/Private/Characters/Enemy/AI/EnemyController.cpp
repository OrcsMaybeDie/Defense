// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/AI/EnemyController.h"

#include "Components/StateTreeAIComponent.h"


// Sets default values
AEnemyController::AEnemyController()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	StateTreeAIComp = CreateDefaultSubobject<UStateTreeAIComponent>(FName("StateTreeAI"));
}

void AEnemyController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	//UE_LOG(LogTemp, Warning, TEXT("EnemyController OnPossess | Controller=%s Pawn=%s StateTree=%s"),
		//*GetNameSafe(this),
		//*GetNameSafe(InPawn),
		//*GetNameSafe(StateTreeAIComp));
	//StateTreeAIComp->RestartLogic(); // State tree 실행
	
}

// Called when the game starts or when spawned
void AEnemyController::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AEnemyController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}



