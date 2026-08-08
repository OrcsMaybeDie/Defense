// Fill out your copyright notice in the Description page of Project Settings.


#include "Traps/Barricade.h"

#include "Characters/Enemy/Data/EnemyData.h"
#include "Characters/Enemy/EnemyAttack.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavModifierComponent.h"

namespace
{
	constexpr ECollisionChannel BarricadeEnemyCollisionChannel = ECC_GameTraceChannel1;
	constexpr ECollisionChannel BarricadeCollisionChannel = ECC_GameTraceChannel3;
}

ABarricade::ABarricade()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetCanBeDamaged(true);

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetCollisionObjectType(BarricadeCollisionChannel);
	
	Cube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cube"));
	Cube->SetupAttachment(Box);

	Sensor = CreateDefaultSubobject<UBoxComponent>(TEXT("Sensor"));
	Sensor->SetupAttachment(Box);

	Sensor->SetGenerateOverlapEvents(false);
	Sensor->SetAutoActivate(false);
	Sensor->SetCanEverAffectNavigation(false);

	NavModifier = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifier"));
}

float ABarricade::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser
)
{
	if (!HasAuthority() || DamageAmount <= 0.0f || HP <= 0.0f)
	{
		return 0.0f;
	}

	const float AppliedDamage = FMath::Min(HP, DamageAmount);
	Super::TakeDamage(AppliedDamage, DamageEvent, EventInstigator, DamageCauser);

	HP -= AppliedDamage;
	if (HP <= 0.0f)
	{
		HP = 0.0f;
		Destroy();
	}

	return AppliedDamage;
}

float ABarricade::GetDistanceToSurface(const FVector& FromLocation) const
{
	if (!Box)
	{
		return FVector::Distance(FromLocation, GetActorLocation());
	}

	const FTransform& BoxTransform = Box->GetComponentTransform();
	const FVector LocalLocation = BoxTransform.InverseTransformPosition(FromLocation);
	const FVector Extent = Box->GetUnscaledBoxExtent();
	const FVector ClosestLocalPoint(
		FMath::Clamp(LocalLocation.X, -Extent.X, Extent.X),
		FMath::Clamp(LocalLocation.Y, -Extent.Y, Extent.Y),
		FMath::Clamp(LocalLocation.Z, -Extent.Z, Extent.Z)
	);
	const FVector ClosestWorldPoint = BoxTransform.TransformPosition(ClosestLocalPoint);

	return FVector::Distance(FromLocation, ClosestWorldPoint);
}

void ABarricade::BeginPlay()
{
	Super::BeginPlay();

	if (Sensor)
	{
		Sensor->OnComponentBeginOverlap.AddUniqueDynamic(this, &ABarricade::OnSensorBeginOverlap);
		Sensor->OnComponentEndOverlap.AddUniqueDynamic(this, &ABarricade::OnSensorEndOverlap);
	}

	if (SensorActivationDelay <= 0.f)
	{
		ActivateSensor();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SensorActivationTimer,
			this,
			&ABarricade::ActivateSensor,
			SensorActivationDelay,
			false
		);
	}
}

void ABarricade::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SensorActivationTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void ABarricade::Destroyed()
{
	NotifyNearbyWaitingRunEnemies();
	ReleaseAllEnemies();
	Super::Destroyed();
}

void ABarricade::ActivateSensor()
{
	if (!Sensor)
	{
		return;
	}

	Sensor->SetGenerateOverlapEvents(true);
	Sensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sensor->Activate(true);
	Sensor->UpdateOverlaps();
}

void ABarricade::EngageEnemy(AEnemyBase* Enemy)
{
	if (!IsValid(Enemy) || Enemy->EnemyMode != EEnemyMode::Combat)
	{
		return;
	}

	const TWeakObjectPtr<AEnemyBase> WeakEnemy(Enemy);
	if (OverlappingEnemies.Contains(WeakEnemy))
	{
		return;
	}

	OverlappingEnemies.Add(WeakEnemy);

	switch (Enemy->EnemyType)
	{
	case EEnemyType::Attack:
		if (AEnemyAttack* AttackEnemy = Cast<AEnemyAttack>(Enemy))
		{
			AttackEnemy->bLockedTarget = true;
			AttackEnemy->SetTarget(this);
			AttackEnemy->SendStateTreeEvent(TEXT("AI.Event.TargetFind"));
		}
		break;

	case EEnemyType::Run:
	case EEnemyType::Destroy:
		Enemy->EnemyState = EEnemyState::Waiting;
		Enemy->SendStateTreeEvent(TEXT("AI.Event.Waiting"));
		break;
	}
}

void ABarricade::ReleaseEnemy(AEnemyBase* Enemy)
{
	if (!IsValid(Enemy))
	{
		return;
	}

	switch (Enemy->EnemyType)
	{
	case EEnemyType::Attack:
		if (AEnemyAttack* AttackEnemy = Cast<AEnemyAttack>(Enemy))
		{
			if (AttackEnemy->Target == this)
			{
				AttackEnemy->bLockedTarget = false;
				AttackEnemy->SetTarget(nullptr);
				AttackEnemy->SendStateTreeEvent(TEXT("AI.Event.Patrol"));
			}
		}
		break;

	case EEnemyType::Run:
	case EEnemyType::Destroy:
		Enemy->SendStateTreeEvent(TEXT("AI.Event.Patrol"));
		break;
	}
}

void ABarricade::ReleaseAllEnemies()
{
	if (bReleasedEnemies || !HasAuthority())
	{
		return;
	}

	bReleasedEnemies = true;

	for (const TWeakObjectPtr<AEnemyBase>& WeakEnemy : OverlappingEnemies)
	{
		AEnemyBase* Enemy = WeakEnemy.Get();
		if (IsValid(Enemy) && Enemy->EnemyType == EEnemyType::Attack)
		{
			ReleaseEnemy(Enemy);
		}
	}

	OverlappingEnemies.Empty();
}

void ABarricade::NotifyNearbyWaitingRunEnemies()
{
	if (!HasAuthority() || PatrolNotifyRadius <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(BarricadeEnemyCollisionChannel);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BarricadePatrolNotify), false, this);
	QueryParams.AddIgnoredActor(this);

	const bool bHasOverlaps = World->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(PatrolNotifyRadius),
		QueryParams
	);

	if (!bHasOverlaps)
	{
		return;
	}

	TSet<TWeakObjectPtr<AEnemyBase>> NotifiedEnemies;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AEnemyBase* Enemy = Cast<AEnemyBase>(OverlapResult.GetActor());
		if (!IsValid(Enemy)
			|| Enemy->EnemyMode != EEnemyMode::Combat
			|| (Enemy->EnemyType != EEnemyType::Run && Enemy->EnemyType != EEnemyType::Destroy)
			|| Enemy->EnemyState != EEnemyState::Waiting)
		{
			continue;
		}

		const TWeakObjectPtr<AEnemyBase> WeakEnemy(Enemy);
		if (NotifiedEnemies.Contains(WeakEnemy))
		{
			continue;
		}

		NotifiedEnemies.Add(WeakEnemy);
		Enemy->SendStateTreeEvent(TEXT("AI.Event.Patrol"));
	}
}

void ABarricade::OnSensorBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!HasAuthority() || bReleasedEnemies || !IsValid(OtherActor) || OtherActor == this)
	{
		return;
	}



	AEnemyBase* Enemy = Cast<AEnemyBase>(OtherActor);
	if (!Enemy || Enemy->EnemyMode == EEnemyMode::Preview)
	{
		return;
	}

	EngageEnemy(Enemy);
}

void ABarricade::OnSensorEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
)
{
	if (!HasAuthority() || bReleasedEnemies)
	{
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(OtherActor);
	if (!Enemy || Enemy->EnemyMode == EEnemyMode::Preview)
	{
		return;
	}

	const TWeakObjectPtr<AEnemyBase> WeakEnemy(Enemy);
	if (OverlappingEnemies.Remove(WeakEnemy) > 0)
	{
		ReleaseEnemy(Enemy);
	}
}
