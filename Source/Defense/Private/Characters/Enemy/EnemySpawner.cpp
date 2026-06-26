// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemySpawner.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Components/BoxComponent.h"


// Sets default values
AEnemySpawner::AEnemySpawner()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	BoxComp = CreateDefaultSubobject<UBoxComponent>(FName("BoxComp"));
	SetRootComponent(BoxComp);
	BoxComp->SetGenerateOverlapEvents(true);
	BoxComp->OnComponentBeginOverlap.AddDynamic(
	this,
	&AEnemySpawner::OnBoxBeginOverlap
);
}

// Called when the game starts or when spawned
void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AEnemySpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AEnemySpawner::SpawnTest()
{
	if (!HasAuthority())
	{
		return;
	}
	
	//float RandomDelay = FMath::RandRange(2.0f, 5.0f);

	//FPlatformProcess::Sleep(RandomDelay);
	
	if (EnemyFactory)
	{
		GetWorld()->SpawnActor<AEnemyBase>(EnemyFactory, GetActorLocation(), GetActorRotation());
	}
}

void AEnemySpawner::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || Cast<AEnemyBase>(OtherActor) != nullptr)
	{
		return;
	}
	
	if (OtherActor && OtherActor != this)
	{
		GetWorld()->SpawnActor<AEnemyBase>(EnemyFactory, GetActorLocation(), GetActorRotation());
	}
}

