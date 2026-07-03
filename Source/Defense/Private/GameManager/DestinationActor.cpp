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
	EnemySensor->OnComponentBeginOverlap.AddUniqueDynamic(
		this,
		&ADestinationActor::OnEnemySensorBeginOverlap
		);
	
	PlayerSensor->OnComponentBeginOverlap.AddUniqueDynamic(
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
		const bool bAlive = enemy->EnemyState != EEnemyState::Die;

		if (AEnemySpawner* Spawner = enemy->OwningSpawner)
		{
			Spawner->RemoveActiveEnemy(enemy);
		}
		if (EnemyPool)
		{
			EnemyPool->ReturnToPool(enemy);
		}
		if (bWasCombatEnemy && bAlive)
		{
			if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
			{
				GameMode->DecreaseCurrentEnemyCount();
				GameMode->ApplyDestinationDamage(1);
			}
		}
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

