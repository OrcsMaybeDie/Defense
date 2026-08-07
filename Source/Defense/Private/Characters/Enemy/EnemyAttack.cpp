// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyAttack.h"

#include "Characters/Enemy/EnemyAnim.h"
#include "Characters/Enemy/Data/EnemyData.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"


// Sets default values
AEnemyAttack::AEnemyAttack()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	AIComp = CreateDefaultSubobject<UAIPerceptionComponent>("AIPerception");
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>("AI Sight config");

	SightConfig->Implementation = UAISense_Sight::StaticClass();
	SightConfig->SightRadius = 600.0f;
	SightConfig->LoseSightRadius = 800.0f;
	SightConfig->PeripheralVisionAngleDegrees = 180.0f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	AIComp->ConfigureSense(*SightConfig);
	AIComp->SetDominantSense(SightConfig->GetSenseImplementation());
}

// Called when the game starts or when spawned
void AEnemyAttack::BeginPlay()
{
	Super::BeginPlay();
	// 서버에서만 AIPerception이 동작하도록 / 클라이언트에서는 비활성화하고 HPUI 정의
	if (!HasAuthority())
	{
		if (AIComp)
		{
			AIComp->Deactivate();
			AIComp->SetComponentTickEnabled(false);
		}
		return;
	}
	
	if (AIComp)
	{
		AIComp->OnTargetPerceptionUpdated.AddDynamic(
			this,
			&AEnemyAttack::OnTargetPerceptionUpdated
		);
	}
}

// Called every frame
void AEnemyAttack::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AEnemyAttack::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AEnemyAttack::SetPreview()
{
	Super::SetPreview();
	
	// 서버에서 처리	
	if (HasAuthority())
	{	
		if (AIComp)
		{
			AIComp->ForgetAll();
			AIComp->Deactivate();
			AIComp->SetComponentTickEnabled(false);
		}
		
	}
}

void AEnemyAttack::SetCombat()
{
	Super::SetCombat();
	
	if (HasAuthority())
	{
		
		if (AIComp)
		{
			AIComp->ForgetAll();
			AIComp->Activate(true);
			AIComp->SetComponentTickEnabled(true);
			AIComp->RequestStimuliListenerUpdate();
		}
	}
}

void AEnemyAttack::SetInactive()
{
	Super::SetInactive();
	
	if (HasAuthority())
	{
		if (AIComp)
		{
			AIComp->ForgetAll();
			AIComp->Deactivate();
			AIComp->SetComponentTickEnabled(false);
		}
		
	}
}

void AEnemyAttack::OnTargetPerceptionUpdated(AActor* Actor, struct FAIStimulus Stimulus)
{
	if (!HasAuthority() || bLockedTarget)
	{
		return;
	}

	if (!OwningSpawner || IsHidden() || EnemyMode == EEnemyMode::Inactive || EnemyMode == EEnemyMode::ReturningToPool)
	{
		return;
	}

	if (Actor == this || (Actor && Actor->IsA(AEnemyBase::StaticClass())))
	{
		return;
	}

	/*UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Updated | Enemy=%s Actor=%s Sensed=%d EnemyMode=%s EnemyState=%s CurrentTarget=%s Distance=%.1f Strength=%.2f"),
		*GetNameSafe(this),
		*GetNameSafe(Actor),
		Stimulus.WasSuccessfullySensed() ? 1 : 0,
		LexToString(EnemyMode),
		LexToString(EnemyState),
		*GetNameSafe(Target),
		Actor ? GetDistanceTo(Actor) : -1.0f,
		Stimulus.Strength
	);*/
	
	if (EnemyMode != EEnemyMode::Combat)
	{
		/*UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Ignored | Enemy=%s Reason=NotCombat EnemyMode=%s EnemyState=%s Actor=%s"),
			*GetNameSafe(this),
			LexToString(EnemyMode),
			LexToString(EnemyState),
			*GetNameSafe(Actor)
		);*/
		return;
	}

	ADefenseCharacter* PerceivedCharacter = Cast<ADefenseCharacter>(Actor);
	if (!PerceivedCharacter)
	{
		/*UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Ignored | Enemy=%s Reason=ActorNotDefenseCharacter EnemyMode=%s EnemyState=%s Actor=%s"),
			*GetNameSafe(this),
			LexToString(EnemyMode),
			LexToString(EnemyState),
			*GetNameSafe(Actor)
		);*/
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		if (EnemyState == EEnemyState::Patrol)
		{
			SetTarget(PerceivedCharacter);
			/*UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Target Updated | Enemy=%s NewTarget=%s EnemyState=%s Event=TargetFind"),
				*GetNameSafe(this),
				*GetNameSafe(Target),
				LexToString(EnemyState)
			);*/
			SendStateTreeEvent(FName("AI.Event.TargetFind"));
		}
		else
		{
			/*UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Sensed But Not Tracking | Enemy=%s Actor=%s EnemyState=%s RequiredState=Patrol CurrentTarget=%s"),
				*GetNameSafe(this),
				*GetNameSafe(Actor),
				LexToString(EnemyState),
				*GetNameSafe(Target)
			);*/
		}
	}
	else
	{
		if (Target != PerceivedCharacter)
		{
			/*UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Lost Ignored | Enemy=%s LostActor=%s CurrentTarget=%s EnemyState=%s"),
				*GetNameSafe(this),
				*GetNameSafe(PerceivedCharacter),
				*GetNameSafe(Target),
				LexToString(EnemyState)
			);*/
			return;
		}
		
		SetTarget(nullptr);
		/*UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Target Updated | Enemy=%s NewTarget=None EnemyState=%s Event=TargetLost"),
			*GetNameSafe(this),
			LexToString(EnemyState)
		);*/
		SendStateTreeEvent(TEXT("AI.Event.TargetLost"));
	}	
}

void AEnemyAttack::MulticastRPC_AttackMotion_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}
	AnimInst->PlayAttackMotion();
}

void AEnemyAttack::AttackTarget()
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

	if (bLockedTarget && IsValid(Target))
	{
		UGameplayStatics::ApplyDamage(Target, DamageNum, GetController(), this, UDamageType::StaticClass());
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyAttackTarget), false, this);
	QueryParams.AddIgnoredActor(this);

	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(AttackDist),
		QueryParams
	);

	if (!bHasOverlap)
	{
		return;
	}

	const FVector EnemyLocation = GetActorLocation();
	FVector EnemyForward = GetActorForwardVector();
	EnemyForward.Z = 0.0f;
	EnemyForward.Normalize();

	AActor* ClosestTarget = nullptr;
	const float AttackDistSq = AttackDist * AttackDist;
	float ClosestDistSq = AttackDistSq;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* FoundTarget = OverlapResult.GetActor();
		if (!IsValid(FoundTarget) || !FoundTarget->IsA<ADefenseCharacter>())
		{
			continue;
		}

		FVector ToTarget = FoundTarget->GetActorLocation() - EnemyLocation;
		ToTarget.Z = 0.0f;
		const float DistSq = ToTarget.SizeSquared();
		if (DistSq <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector DirectionToTarget = ToTarget.GetSafeNormal();
		if (FVector::DotProduct(EnemyForward, DirectionToTarget) <= 0.0f)
		{
			continue;
		}

		if (DistSq <= ClosestDistSq)
		{
			ClosestTarget = FoundTarget;
			ClosestDistSq = DistSq;
		}
	}

	if (ClosestTarget)
	{
		UGameplayStatics::ApplyDamage(ClosestTarget, DamageNum, GetController(), this, UDamageType::StaticClass());
	}
}

bool AEnemyAttack::CanAttack() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->GetTimeSeconds() - LastAttackFinishedTime >= AttackCooldown;
}

void AEnemyAttack::MarkAttackFinished()
{
	if (const UWorld* World = GetWorld())
	{
		LastAttackFinishedTime = World->GetTimeSeconds();
	}
}

float AEnemyAttack::GetAttackDuration(float DefaultDuration) const
{
	if (AnimInst && AnimInst->AttackMontage)
	{
		const float MontageLength = AnimInst->AttackMontage->GetPlayLength();
		if (MontageLength > 0.0f)
		{
			return MontageLength;
		}
	}

	return DefaultDuration;
}

void AEnemyAttack::ApplyEnemyData()
{
	Super::ApplyEnemyData();
	if (const UEnemyAttackData* EnemyAttackData = Cast<UEnemyAttackData>(EnemyData))
	{
		AttackDist = EnemyAttackData->AttackDist;
		BarricadeAttackDist = EnemyAttackData->BarricadeAttackDist;
		SetTarget(Target);
		DamageNum = EnemyAttackData->DamageNum;
		AttackCooldown = EnemyAttackData->AttackCooldown;
	}
}

