// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyBase.h"

#include "StateTreeEvents.h"
#include "Characters/Enemy/EnemyAnim.h"
#include "Characters/Enemy/AI/EnemyController.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Components/StateTreeAIComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

namespace
{
	const TCHAR* LexToString(const EEnemyMode Mode)
	{
		switch (Mode)
		{
		case EEnemyMode::Preview:
			return TEXT("Preview");
		case EEnemyMode::Combat:
			return TEXT("Combat");
		case EEnemyMode::ReturningToPool:
			return TEXT("ReturningToPool");
		case EEnemyMode::Inactive:
			return TEXT("Inactive");
		default:
			return TEXT("Unknown");
		}
	}
}

// Sets default values
AEnemyBase::AEnemyBase()
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
void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	EnemyController = Cast<AEnemyController>(GetController());
	AnimInst = Cast<UEnemyAnim>(GetMesh()->GetAnimInstance());
	EnemyMesh = GetMesh();
	CurHP = MaxHP;
	
	if (AIComp)
	{
		AIComp->OnTargetPerceptionUpdated.AddDynamic(
			this,
			&AEnemyBase::OnTargetPerceptionUpdated
		);
	}
}

// Called every frame
void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AEnemyBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AEnemyBase::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEnemyBase, EnemyState);
	DOREPLIFETIME(AEnemyBase, EnemyMode);
	DOREPLIFETIME(AEnemyBase, CurHP);
	
	
}

void AEnemyBase::OnRep_UpdateMode()
{
	//UE_LOG(LogTemp, Warning, TEXT("Enemy OnRep_UpdateMode | Enemy=%s Mode=%s(%d) NetMode=%d HasAuthority=%d PreviewMats=%s/%s"),
		//*GetNameSafe(this),
		//LexToString(EnemyMode),
		//static_cast<int32>(EnemyMode),
		//static_cast<int32>(GetNetMode()),
		//HasAuthority() ? 1 : 0,
		//*GetNameSafe(PreviewMaterial0),
		//*GetNameSafe(PreviewMaterial1));

	// preview일 때 투명 머티리얼, Combat일 때는 원래 머티리얼로 변경
	if (EnemyMode == EEnemyMode::Preview)
	{
		SetPreview();
		
	}else if (EnemyMode == EEnemyMode::Combat)
	{
		SetCombat();
	}
	else if (EnemyMode == EEnemyMode::Inactive)
	{
		SetInactive();
	}
}

void AEnemyBase::SetPreview()
{
	if (!EnemyMesh)
	{
		EnemyMesh = GetMesh();
	}

	if (!EnemyController)
	{
		EnemyController = Cast<AEnemyController>(GetController());
	}

	//UE_LOG(LogTemp, Warning, TEXT("Enemy SetPreview | Enemy=%s Mode=%s(%d) NetMode=%d HasAuthority=%d Mesh=%s PreviewMats=%s/%s"),
		//*GetNameSafe(this),
		//LexToString(EnemyMode),
		//static_cast<int32>(EnemyMode),
		//static_cast<int32>(GetNetMode()),
		//HasAuthority() ? 1 : 0,
		//*GetNameSafe(EnemyMesh),
		//*GetNameSafe(PreviewMaterial0),
		//*GetNameSafe(PreviewMaterial1));

	// 틱 처리
	SetActorTickEnabled(true);
	// 그리기 처리
	SetActorHiddenInGame(false);
	// 충돌 처리
	SetActorEnableCollision(true);
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	}
	
	if (EnemyMesh)
	{
		EnemyMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	
	if(!IsRunningDedicatedServer())
	{
		if (EnemyMesh)
		{
			if (PreviewMaterial0)
			{
				EnemyMesh->SetMaterial(0, PreviewMaterial0);
			}
			if (PreviewMaterial1)
			{
				EnemyMesh->SetMaterial(1, PreviewMaterial1);
			}
		}
	}
	else
	{
		if (EnemyController && EnemyController->StateTreeAIComp)
		{
			EnemyController->StateTreeAIComp->StartLogic();
			EnemyController->StateTreeAIComp->SetComponentTickEnabled(true);
		}
	}
}

void AEnemyBase::SetCombat()
{
	if (!EnemyMesh)
	{
		EnemyMesh = GetMesh();
	}

	//UE_LOG(LogTemp, Warning, TEXT("Enemy SetCombat | Enemy=%s Mode=%s(%d) NetMode=%d HasAuthority=%d CombatMats=%s/%s"),
		//*GetNameSafe(this),
		//LexToString(EnemyMode),
		//static_cast<int32>(EnemyMode),
		//static_cast<int32>(GetNetMode()),
		//HasAuthority() ? 1 : 0,
		//*GetNameSafe(CombatMaterial0),
		//*GetNameSafe(CombatMaterial1));

	if (!IsRunningDedicatedServer())
	{
		if (EnemyMesh)
		{
			if (CombatMaterial0)
			{
				EnemyMesh->SetMaterial(0, CombatMaterial0);
			}
			if (CombatMaterial1)
			{
				EnemyMesh->SetMaterial(1, CombatMaterial1);
			}
		}
		else
		{
			if (EnemyController && EnemyController->StateTreeAIComp)
			{
				EnemyController->StateTreeAIComp->StartLogic();
				EnemyController->StateTreeAIComp->SetComponentTickEnabled(true);
			}
		}
	}

	SetActorEnableCollision(true);
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

void AEnemyBase::SetInactive()
{
	if (!EnemyController)
	{
		EnemyController = Cast<AEnemyController>(GetController());
	}

	//UE_LOG(LogTemp, Warning, TEXT("Enemy SetInactive | Enemy=%s Mode=%s(%d) NetMode=%d HasAuthority=%d"),
		//*GetNameSafe(this),
		//LexToString(EnemyMode),
		//static_cast<int32>(EnemyMode),
		//static_cast<int32>(GetNetMode()),
		//HasAuthority() ? 1 : 0);

	if (HasAuthority())
	{
		if (EnemyController && EnemyController->StateTreeAIComp)
		{
			EnemyController->StateTreeAIComp->StopLogic(TEXT("EnemyMode : Inactive"));
		}
	}
	
	// 틱 처리
	SetActorTickEnabled(false);
	// 그리기 처리
	SetActorHiddenInGame(true);
	// 충돌 처리
	SetActorEnableCollision(false);
	
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	}
}

// AIPerception으로 감지 후 업데이트
void AEnemyBase::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (EnemyMode != EEnemyMode::Combat)
	{
		return;
	}

	ADefenseCharacter* PerceivedCharacter = Cast<ADefenseCharacter>(Actor);
	if (!PerceivedCharacter)
	{
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("Enemy Perception | Enemy=%s Actor=%s Sensed=%d Distance=%.1f State=%d"),
		//*GetNameSafe(this),
		//*GetNameSafe(Actor),
		//Stimulus.WasSuccessfullySensed() ? 1 : 0,
		//GetDistanceTo(Actor),
		//static_cast<int32>(EnemyState));

	if (Stimulus.WasSuccessfullySensed())
	{
		if (EnemyState == EEnemyState::Patrol)
		{
			Target = PerceivedCharacter;
			SendStateTreeEvent(FName("AI.Event.TargetFind"));
		}
	}
	else
	{
		if (Target != PerceivedCharacter)
		{
			return;
		}
		
		Target = nullptr;
		SendStateTreeEvent(TEXT("AI.Event.TargetLost"));
	}
}

// Gameplay Tag 이벤트 보내기
void AEnemyBase::SendStateTreeEvent(FName EventTagName) const
{
	if (!Controller)
	{
		return;
	}

	UStateTreeAIComponent* StateTreeAI =
		Controller->FindComponentByClass<UStateTreeAIComponent>();

	if (!StateTreeAI)
	{
		return;
	}

	FStateTreeEvent Event;
	Event.Tag = FGameplayTag::RequestGameplayTag(EventTagName);

	StateTreeAI->SendStateTreeEvent(Event);	
}

void AEnemyBase::MulticastRPC_DamageMotion_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}
	AnimInst->PlayDamageMotion();
}

void AEnemyBase::MulticastRPC_DieMotion_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}
	AnimInst->PlayDieMotion();
}

// 체력 UI 업데이트
void AEnemyBase::OnRep_UpdateUI()
{
	
}

float AEnemyBase::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority())
	{
		return 0.f;
	}

	if (EnemyMode != EEnemyMode::Combat)
	{
		return 0.f;
	}
	
	const float ActualDamage = Super::TakeDamage(
		DamageAmount,
		DamageEvent,
		EventInstigator,
		DamageCauser
	);

	if (ActualDamage <= 0.0f)
	{
		return 0.0f;
	}

	CurHP -= ActualDamage;

	ADefenseCharacter* AttackingCharacter = Cast<ADefenseCharacter>(DamageCauser);
	if (!AttackingCharacter && EventInstigator)
	{
		AttackingCharacter = Cast<ADefenseCharacter>(EventInstigator->GetPawn());
	}

	if (AttackingCharacter)
	{
		Target = AttackingCharacter;
	}
	
	if (CurHP <= 0.0f)
	{
		SendStateTreeEvent(FName("AI.Event.Die"));
	}
	else
	{
		SendStateTreeEvent(FName("AI.Event.Damage"));
	}

	return ActualDamage;
}



void AEnemyBase::AttackTarget()
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


void AEnemyBase::MulticastRPC_AttackMotion_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}
	AnimInst->PlayAttackMotion();
}

