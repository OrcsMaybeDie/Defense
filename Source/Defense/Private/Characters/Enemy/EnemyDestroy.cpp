// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyDestroy.h"

#include "Characters/Enemy/EnemyAnim.h"
#include "Characters/Enemy/Data/EnemyData.h"
// #include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Traps/TrapBase.h"

// namespace
// {
// 	constexpr float DestroySearchDebugTime = 1.0f;
// 	constexpr int32 DestroySearchDebugSegments = 24;
// }

// Sets default values
AEnemyDestroy::AEnemyDestroy()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AEnemyDestroy::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AEnemyDestroy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AEnemyDestroy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AEnemyDestroy::SetPreview()
{
	Super::SetPreview();
	ClearDestroyTryTimer();
	TargetTraps.Empty();
}

void AEnemyDestroy::SetCombat()
{
	Super::SetCombat();
	ScheduleNextDestroyTry(0.f);
}

void AEnemyDestroy::SetInactive()
{
	Super::SetInactive();
	ClearDestroyTryTimer();
	TargetTraps.Empty();
}

void AEnemyDestroy::ApplyEnemyData()
{
	Super::ApplyEnemyData();
	
	if (const UEnemyDestroyData* EnemyDestroyData = Cast<UEnemyDestroyData>(EnemyData))
	{
		SearchCooldown = EnemyDestroyData->SearchCooldown;
		DestroyCooldown = EnemyDestroyData->DestroyCooldown;
		DestroyRadius = EnemyDestroyData->DestroyRadius;
	}
}

// 화염 vfx도 추가할 것.
void AEnemyDestroy::MulticastRPC_DestroyMotion_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}
	AnimInst->PlayDestroyMotion();
}

bool AEnemyDestroy::CanTryDestroy() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->GetTimeSeconds() >= NextDestroyTryTime;
}

bool AEnemyDestroy::TryFindDestroyTarget()
{
	if (!HasAuthority())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TargetTraps.Empty();

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyDestroyTarget), false, this);
	QueryParams.AddIgnoredActor(this);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	const FVector SearchCenter = GetActorLocation() + GetActorTransform().TransformVectorNoScale(DestroySearchOffset);

	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		SearchCenter,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(DestroyRadius),
		QueryParams
	);

	if (!bHasOverlap)
	{
		// Multicast_DrawDestroySearchDebug(SearchCenter, DestroyRadius, false);
		return false;
	}

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* FoundTrap = OverlapResult.GetActor();
		if (!IsValid(FoundTrap) || !FoundTrap->IsA<ATrapBase>())
		{
			continue;
		}

		TargetTraps.AddUnique(FoundTrap);
	}

	// Multicast_DrawDestroySearchDebug(SearchCenter, DestroyRadius, !TargetTraps.IsEmpty());

	return !TargetTraps.IsEmpty();
}

// void AEnemyDestroy::Multicast_DrawDestroySearchDebug_Implementation(FVector SearchCenter, float SearchRadius, bool bFoundTrap)
// {
// 	if (UWorld* World = GetWorld())
// 	{
// 		DrawDebugSphere(
// 			World,
// 			SearchCenter,
// 			SearchRadius,
// 			DestroySearchDebugSegments,
// 			bFoundTrap ? FColor::Red : FColor::Green,
// 			false,
// 			DestroySearchDebugTime,
// 			0,
// 			2.f
// 		);
// 	}
// }

void AEnemyDestroy::DestroyTargetTrap()
{
	if (!HasAuthority())
	{
		return;
	}

	for (AActor* TargetTrap : TargetTraps)
	{
		if (!IsValid(TargetTrap))
		{
			continue;
		}

		// TrapBase 무력화 함수가 추가되면 여기에서 TargetTrap에 호출한다.
		TargetTrap->Destroy();
	}

	TargetTraps.Empty();
}

void AEnemyDestroy::MarkDestroyFinished(bool bDestroyedTrap)
{
	ScheduleNextDestroyTry(bDestroyedTrap ? DestroyCooldown : SearchCooldown);
}

void AEnemyDestroy::ScheduleNextDestroyTry(float Cooldown)
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float ClampedCooldown = FMath::Max(0.f, Cooldown);
	NextDestroyTryTime = World->GetTimeSeconds() + ClampedCooldown;

	World->GetTimerManager().ClearTimer(DestroyTryTimerHandle);
	World->GetTimerManager().SetTimer(
		DestroyTryTimerHandle,
		this,
		&AEnemyDestroy::SendDestroyEvent,
		FMath::Max(0.01f, ClampedCooldown),
		false
	);
}

void AEnemyDestroy::ClearDestroyTryTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DestroyTryTimerHandle);
	}
}

void AEnemyDestroy::SendDestroyEvent()
{
	if (!HasAuthority() || EnemyMode != EEnemyMode::Combat)
	{
		return;
	}

	SendStateTreeEvent(FName("AI.Event.Destroy"));
}

float AEnemyDestroy::GetDestroyDuration(float DefaultDuration) const
{
	if (AnimInst && AnimInst->DestroyMontage)
	{
		const float MontageLength = AnimInst->DestroyMontage->GetPlayLength();
		if (MontageLength > 0.0f)
		{
			return MontageLength;
		}
	}

	return DefaultDuration;
}
