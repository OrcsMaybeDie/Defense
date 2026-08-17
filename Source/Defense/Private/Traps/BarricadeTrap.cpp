// Fill out your copyright notice in the Description page of Project Settings.

#include "Traps/BarricadeTrap.h"

#include "Camera/PlayerCameraManager.h"
#include "Characters/Enemy/Data/EnemyData.h"
#include "Characters/Enemy/EnemyAttack.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Collision/DefenseCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UI/EnemyHPUI.h"

ABarricadeTrap::ABarricadeTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(true);

	HpComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HpComp"));
	HpComp->SetupAttachment(SceneRoot);
	HpComp->SetWidgetSpace(EWidgetSpace::World);
	HpComp->SetDrawAtDesiredSize(false);

	Sensor = CreateDefaultSubobject<UBoxComponent>(TEXT("Sensor"));
	Sensor->SetupAttachment(SceneRoot);
	Sensor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sensor->SetCollisionObjectType(ECC_WorldDynamic);
	Sensor->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sensor->SetCollisionResponseToChannel(DefenseCollisionChannels::Enemy, ECR_Overlap);
	Sensor->SetGenerateOverlapEvents(false);
	Sensor->SetAutoActivate(false);
	Sensor->SetCanEverAffectNavigation(false);
}

void ABarricadeTrap::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsRunningDedicatedServer() || !HpComp || !bHpUIVisible)
	{
		return;
	}

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	FVector Direction = CameraLocation - HpComp->GetComponentLocation();
	Direction.Z = 0.0f;
	HpComp->SetWorldRotation(Direction.GetSafeNormal().ToOrientationRotator());
}

void ABarricadeTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABarricadeTrap, HP);
}

float ABarricadeTrap::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser
)
{
	if (IsValid(Cast<ADefenseCharacter>(DamageCauser)))
	{
		return 0.0f;
	}

	if (!HasAuthority() || !IsPlaced() || DamageAmount <= 0.0f || HP <= 0.0f)
	{
		return 0.0f;
	}

	const float AppliedDamage = FMath::Min(HP, DamageAmount);
	Super::TakeDamage(AppliedDamage, DamageEvent, EventInstigator, DamageCauser);

	HP -= AppliedDamage;
	HP = FMath::Max(0.0f, HP);

	// RepNotify는 서버에서 자동 호출되지 않으므로 Listen Server의 로컬 UI를 직접 갱신한다.
	if (!IsRunningDedicatedServer())
	{
		RefreshHPUI();
	}

	ForceNetUpdate();

	if (HP <= 0.0f)
	{
		Destroy();
	}

	return AppliedDamage;
}

float ABarricadeTrap::GetDistanceToSurface(const FVector& FromLocation) const
{
	UMeshComponent* ActiveMesh = GetActiveTrapMeshComponent();
	FVector BoundsCenter;
	FVector BoundsExtent;
	if (!ActiveMesh || !GetTrapMeshLocalBounds(BoundsCenter, BoundsExtent))
	{
		return FVector::Distance(FromLocation, GetActorLocation());
	}

	const FVector BoundsMin = BoundsCenter - BoundsExtent;
	const FVector BoundsMax = BoundsCenter + BoundsExtent;

	const FTransform& MeshTransform = ActiveMesh->GetComponentTransform();
	const FVector LocalLocation = MeshTransform.InverseTransformPosition(FromLocation);
	const FVector ClosestLocalPoint(
		FMath::Clamp(LocalLocation.X, BoundsMin.X, BoundsMax.X),
		FMath::Clamp(LocalLocation.Y, BoundsMin.Y, BoundsMax.Y),
		FMath::Clamp(LocalLocation.Z, BoundsMin.Z, BoundsMax.Z)
	);

	return FVector::Distance(FromLocation, MeshTransform.TransformPosition(ClosestLocalPoint));
}

void ABarricadeTrap::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDamageAreaExtent();
}

void ABarricadeTrap::BeginPlay()
{
	Super::BeginPlay();
	ApplyDamageAreaExtent();

	if (HasAuthority())
	{
		HP = FMath::Max(1.0f, MaxHP);
	}

	if (HpComp)
	{
		HpComp->SetVisibility(false);
		if (!IsRunningDedicatedServer())
		{
			HPUI = Cast<UEnemyHPUI>(HpComp->GetWidget());
			RefreshHPUI();
		}
	}

	if (Sensor)
	{
		Sensor->OnComponentBeginOverlap.AddUniqueDynamic(this, &ABarricadeTrap::OnSensorBeginOverlap);
		Sensor->OnComponentEndOverlap.AddUniqueDynamic(this, &ABarricadeTrap::OnSensorEndOverlap);
	}
}

void ABarricadeTrap::ApplyDamageAreaExtent()
{
	if (DamageArea)
	{
		DamageArea->SetBoxExtent(DamageAreaExtent);
	}
}

void ABarricadeTrap::OnRep_HP()
{
	RefreshHPUI();
}

void ABarricadeTrap::RefreshHPUI()
{
	if (IsRunningDedicatedServer() || !HpComp)
	{
		return;
	}

	if (!HPUI)
	{
		HPUI = Cast<UEnemyHPUI>(HpComp->GetWidget());
	}

	const bool bShouldShow = IsPlaced() && HP > 0.0f && HP < MaxHP;
	bHpUIVisible = bShouldShow;
	HpComp->SetVisibility(bShouldShow);

	if (HPUI)
	{
		HPUI->UpdateHPBar(HP, MaxHP);
	}
}

void ABarricadeTrap::InitializePlacedTrap(
	UTrapData* TrapData,
	ADefensePlayerState* InInstalledByPlayerState,
	const TArray<FTrapCellKey>& InOccupiedCells
)
{
	Super::InitializePlacedTrap(TrapData, InInstalledByPlayerState, InOccupiedCells);
	ScheduleSensorActivation();
}

void ABarricadeTrap::ScheduleSensorActivation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (SensorActivationDelay <= 0.0f)
	{
		ActivateSensor();
	}
	else
	{
		World->GetTimerManager().SetTimer(
			SensorActivationTimer,
			this,
			&ABarricadeTrap::ActivateSensor,
			SensorActivationDelay,
			false
		);
	}
}

void ABarricadeTrap::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SensorActivationTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void ABarricadeTrap::Destroyed()
{
	if (IsPlaced())
	{
		NotifyNearbyWaitingRunEnemies();
		ReleaseAllEnemies();
	}

	Super::Destroyed();
}

void ABarricadeTrap::ActivateSensor()
{
	if (!Sensor || !IsPlaced())
	{
		return;
	}

	Sensor->SetGenerateOverlapEvents(true);
	Sensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sensor->Activate(true);
	Sensor->UpdateOverlaps();
}

void ABarricadeTrap::EngageEnemy(AEnemyBase* Enemy)
{
	if (!IsValid(Enemy) || Enemy->EnemyMode != EEnemyMode::Combat)
	{
		return;
	}

	const TWeakObjectPtr<AEnemyBase> WeakEnemy(Enemy);
	if (SensorOverlappingEnemies.Contains(WeakEnemy))
	{
		return;
	}

	SensorOverlappingEnemies.Add(WeakEnemy);

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

void ABarricadeTrap::ReleaseEnemy(AEnemyBase* Enemy)
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

void ABarricadeTrap::ReleaseAllEnemies()
{
	if (bReleasedEnemies || !HasAuthority())
	{
		return;
	}

	bReleasedEnemies = true;

	for (const TWeakObjectPtr<AEnemyBase>& WeakEnemy : SensorOverlappingEnemies)
	{
		AEnemyBase* Enemy = WeakEnemy.Get();
		if (IsValid(Enemy) && Enemy->EnemyType == EEnemyType::Attack)
		{
			ReleaseEnemy(Enemy);
		}
	}

	SensorOverlappingEnemies.Empty();
}

void ABarricadeTrap::NotifyNearbyWaitingRunEnemies()
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
	ObjectQueryParams.AddObjectTypesToQuery(DefenseCollisionChannels::Enemy);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BarricadeTrapPatrolNotify), false, this);
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

void ABarricadeTrap::OnSensorBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!HasAuthority() || !IsPlaced() || bReleasedEnemies || !IsValid(OtherActor) || OtherActor == this)
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

void ABarricadeTrap::OnSensorEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
)
{
	if (!HasAuthority() || !IsPlaced() || bReleasedEnemies)
	{
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(OtherActor);
	if (!Enemy || Enemy->EnemyMode == EEnemyMode::Preview)
	{
		return;
	}

	const TWeakObjectPtr<AEnemyBase> WeakEnemy(Enemy);
	if (SensorOverlappingEnemies.Remove(WeakEnemy) > 0)
	{
		ReleaseEnemy(Enemy);
	}
}
