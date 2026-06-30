// Fill out your copyright notice in the Description page of Project Settings.


#include "GameManager/DestinationActor.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "Characters/Enemy/EnemySpawner.h"
#include "Components/SphereComponent.h"
#include "GameManager/DefenseGameMode.h"


// Sets default values
ADestinationActor::ADestinationActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	SetRootComponent(MeshComp);
	
	EnemySensor = CreateDefaultSubobject<USphereComponent>(TEXT("EnemySensor"));
	EnemySensor->SetupAttachment(RootComponent);
	
	PlayerSensor = CreateDefaultSubobject<USphereComponent>(TEXT("PlayerSensor"));
	PlayerSensor->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void ADestinationActor::BeginPlay()
{
	Super::BeginPlay();
	EnemySensor->OnComponentBeginOverlap.AddDynamic(
		this,
		&ADestinationActor::OnEnemySensorBeginOverlap
		);
	
	PlayerSensor->OnComponentBeginOverlap.AddDynamic(
		this,
		&ADestinationActor::OnPlayerSensorBeginOverlap
		);
	EnemyPool = GetWorld()->GetSubsystem<UEnemyPoolSubsystem>();
}

// Called every frame
void ADestinationActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ADestinationActor::OnEnemySensorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (AEnemyBase* enemy = Cast<AEnemyBase>(OtherActor))
	{
		const bool bWasCombatEnemy = enemy->EnemyMode == EEnemyMode::Combat;

		if (AEnemySpawner* Spawner = enemy->OwningSpawner)
		{
			Spawner->RemoveActiveEnemy(enemy);
		}
		EnemyPool->ReturnToPool(enemy);
		if (bWasCombatEnemy)
		{
			if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
			{
				GameMode->DecreaseCurrentEnemyCount();
			}
		}
		DestScore -= 1;
	}
}

void ADestinationActor::OnPlayerSensorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}
	
	// TODO: 플레이어 체력회복 
}

