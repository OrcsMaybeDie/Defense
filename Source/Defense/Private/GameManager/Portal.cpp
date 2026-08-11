// Fill out your copyright notice in the Description page of Project Settings.


#include "GameManager/Portal.h"

#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "Characters/Enemy/EnemySpawner.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameManager/DefenseGameMode.h"

namespace
{
	constexpr ECollisionChannel EnemyCollisionChannel = ECC_GameTraceChannel1;
}

APortal::APortal()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	PortalPlane = CreateDefaultSubobject<USceneComponent>(TEXT("PortalPlane"));
	SetRootComponent(PortalPlane);

	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
	PortalMesh->SetupAttachment(PortalPlane);
	PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PortalMesh->SetCollisionResponseToAllChannels(ECR_Ignore);

	EntrySensor = CreateDefaultSubobject<UBoxComponent>(TEXT("EntrySensor"));
	EntrySensor->SetupAttachment(PortalPlane);
	EntrySensor->SetRelativeLocation(FVector(75.f, 0.f, 0.f));
	EntrySensor->SetBoxExtent(FVector(125.f, PortalHalfWidth, PortalHalfHeight));
	EntrySensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EntrySensor->SetCollisionObjectType(ECC_WorldDynamic);
	EntrySensor->SetCollisionResponseToAllChannels(ECR_Ignore);
	EntrySensor->SetCollisionResponseToChannel(EnemyCollisionChannel, ECR_Overlap);
	EntrySensor->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EntrySensor->SetGenerateOverlapEvents(true);

	EntryPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EntryPoint"));
	EntryPoint->SetupAttachment(PortalPlane);
	EntryPoint->SetRelativeLocation(FVector(75.f, 0.f, -PortalHalfHeight));

	ReturnPoolSensor = CreateDefaultSubobject<UBoxComponent>(TEXT("ReturnPoolSensor"));
	ReturnPoolSensor->SetupAttachment(PortalPlane);
	ReturnPoolSensor->SetRelativeLocation(FVector(-250.f, 0.f, 0.f));
	ReturnPoolSensor->SetBoxExtent(FVector(50.f, PortalHalfWidth, PortalHalfHeight));
	ReturnPoolSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ReturnPoolSensor->SetCollisionObjectType(ECC_WorldDynamic);
	ReturnPoolSensor->SetCollisionResponseToAllChannels(ECR_Ignore);
	ReturnPoolSensor->SetCollisionResponseToChannel(EnemyCollisionChannel, ECR_Overlap);
	ReturnPoolSensor->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ReturnPoolSensor->SetGenerateOverlapEvents(true);

	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(PortalPlane);
	ExitPoint->SetRelativeLocation(FVector(-350.f, 0.f, 0.f));
}

FVector APortal::GetEntryPointLocation() const
{
	return EntryPoint ? EntryPoint->GetComponentLocation() : GetActorLocation();
}

FVector APortal::GetExitPointLocation() const
{
	return ExitPoint ? ExitPoint->GetComponentLocation() : GetActorLocation();
}

void APortal::BeginPlay()
{
	Super::BeginPlay();

	EntrySensor->OnComponentBeginOverlap.AddUniqueDynamic(this, &APortal::OnEntrySensorBeginOverlap);
	EntrySensor->OnComponentEndOverlap.AddUniqueDynamic(this, &APortal::OnEntrySensorEndOverlap);
	ReturnPoolSensor->OnComponentBeginOverlap.AddUniqueDynamic(this, &APortal::OnReturnPoolSensorBeginOverlap);
	EnemyPool = GetWorld()->GetSubsystem<UEnemyPoolSubsystem>();
}

void APortal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const float SafeHalfWidth = FMath::Max(PortalHalfWidth, 1.f);
	const float SafeHalfHeight = FMath::Max(PortalHalfHeight, 1.f);
	if (EntrySensor)
	{
		const FVector CurrentExtent = EntrySensor->GetUnscaledBoxExtent();
		EntrySensor->SetBoxExtent(FVector(CurrentExtent.X, SafeHalfWidth, SafeHalfHeight));
	}
	if (ReturnPoolSensor)
	{
		const FVector CurrentExtent = ReturnPoolSensor->GetUnscaledBoxExtent();
		ReturnPoolSensor->SetBoxExtent(FVector(CurrentExtent.X, SafeHalfWidth, SafeHalfHeight));
	}
}

void APortal::OnEntrySensorBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!HasAuthority())
	{
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(OtherActor);
	if (!Enemy || OtherComp != Enemy->GetCapsuleComponent())
	{
		return;
	}

	const FVector PortalScale = PortalPlane->GetComponentScale();
	const float ScaledHalfWidth = PortalHalfWidth * FMath::Abs(PortalScale.Y);
	const float ScaledHalfHeight = PortalHalfHeight * FMath::Abs(PortalScale.Z);

	if (!Enemy->TryBeginPortalEntry(
		this,
		GetExitPointLocation(),
		PortalMesh->GetComponentLocation(),
		PortalPlane->GetForwardVector(),
		PortalPlane->GetRightVector(),
		PortalPlane->GetUpVector(),
		ScaledHalfWidth,
		ScaledHalfHeight))
	{
		return;
	}
}

void APortal::OnEntrySensorEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
)
{
	if (!HasAuthority())
	{
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(OtherActor);
	if (!Enemy
		|| OtherComp != Enemy->GetCapsuleComponent()
		|| Enemy->EnemyMode != EEnemyMode::Preview)
	{
		return;
	}

	if (!EnemyPool)
	{
		EnemyPool = GetWorld()->GetSubsystem<UEnemyPoolSubsystem>();
	}

	if (!EnemyPool || !Enemy->FinishPortalEntry(this))
	{
		return;
	}

	if (AEnemySpawner* Spawner = Enemy->OwningSpawner)
	{
		Spawner->RemoveActiveEnemy(Enemy);
	}

	EnemyPool->ReturnToPool(Enemy);
}

void APortal::OnReturnPoolSensorBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!HasAuthority())
	{
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(OtherActor);
	if (!Enemy)
	{
		return;
	}

	const bool bShouldCompleteCombatDestination = Enemy->EnemyMode == EEnemyMode::Combat
		&& Enemy->EnemyState != EEnemyState::Die
		&& Enemy->EnemyState != EEnemyState::StoneDie;

	if (!Enemy->FinishPortalEntry(this))
	{
		return;
	}

	if (bShouldCompleteCombatDestination)
	{
		if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
		{
			GameMode->NotifyEnemyRemoved(Enemy, EEnemyRemoveReason::ReachedDestination);
			GameMode->ApplyDestinationDamage(1);
		}
	}

	if (!EnemyPool)
	{
		EnemyPool = GetWorld()->GetSubsystem<UEnemyPoolSubsystem>();
	}

	if (EnemyPool)
	{
		EnemyPool->ReturnToPool(Enemy);
	}
}

