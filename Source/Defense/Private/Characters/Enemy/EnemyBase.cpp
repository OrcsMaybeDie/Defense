// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyBase.h"

#include "StateTreeEvents.h"
#include "Characters/Enemy/EnemyAnim.h"
#include "Characters/Enemy/AI/EnemyController.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "UI/EnemyHPUI.h"

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

	const TCHAR* LexToString(const EEnemyState State)
	{
		switch (State)
		{
		case EEnemyState::Idle:
			return TEXT("Idle");
		case EEnemyState::Patrol:
			return TEXT("Patrol");
		case EEnemyState::Chase:
			return TEXT("Chase");
		case EEnemyState::Damage:
			return TEXT("Damage");
		case EEnemyState::Attack:
			return TEXT("Attack");
		case EEnemyState::Die:
			return TEXT("Die");
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
	HpComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HpComp"));
	HpComp->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	HpComp->SetVisibility(false);
	EnemyController = Cast<AEnemyController>(GetController());
	AnimInst = Cast<UEnemyAnim>(GetMesh()->GetAnimInstance());
	EnemyMesh = GetMesh();
	CurHP = MaxHP;
	
	// 서버에서만 AIPerception이 동작하도록 / 클라이언트에서는 비활성화하고 HPUI 정의
	if (!HasAuthority())
	{
		if (AIComp)
		{
			AIComp->Deactivate();
			AIComp->SetComponentTickEnabled(false);
			HPUI = Cast<UEnemyHPUI>(HpComp->GetWidget());
		}
		return;
	}
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

	// 서버에서는 UI가 클라이언트 방향으로 회전하는 것 제외
	if (GetNetMode() == NM_DedicatedServer || !HpComp || !bHpUIVisible)
	{
		return;
	}

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	const FVector CamLoc = PlayerController->PlayerCameraManager->GetCameraLocation();
	FVector Dir = CamLoc - HpComp->GetComponentLocation();
	Dir.Z = 0;
	HpComp->SetWorldRotation(Dir.GetSafeNormal().ToOrientationRotator());
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
			if (PreviewMaterial)
			{
				EnemyMesh->SetMaterial(0, PreviewMaterial);
				EnemyMesh->SetMaterial(1, PreviewMaterial);
			}
		}
	}
	
	if (HasAuthority())
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
			if (CombatMaterial)
			{
				EnemyMesh->SetMaterial(0, CombatMaterial);
				EnemyMesh->SetMaterial(1, CombatMaterial);
			}
		}
	}
	
	if (HasAuthority())
	{
		if (EnemyController && EnemyController->StateTreeAIComp)
		{
			EnemyController->StateTreeAIComp->StartLogic();
			EnemyController->StateTreeAIComp->SetComponentTickEnabled(true);
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
	else
	{
		
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
	if (HpComp)
	{
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
	}
}

// AIPerception으로 감지 후 업데이트
void AEnemyBase::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!HasAuthority())
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

	UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Updated | Enemy=%s Actor=%s Sensed=%d EnemyMode=%s EnemyState=%s CurrentTarget=%s Distance=%.1f Strength=%.2f"),
		*GetNameSafe(this),
		*GetNameSafe(Actor),
		Stimulus.WasSuccessfullySensed() ? 1 : 0,
		LexToString(EnemyMode),
		LexToString(EnemyState),
		*GetNameSafe(Target),
		Actor ? GetDistanceTo(Actor) : -1.0f,
		Stimulus.Strength
	);
	
	if (EnemyMode != EEnemyMode::Combat)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Ignored | Enemy=%s Reason=NotCombat EnemyMode=%s EnemyState=%s Actor=%s"),
			*GetNameSafe(this),
			LexToString(EnemyMode),
			LexToString(EnemyState),
			*GetNameSafe(Actor)
		);
		return;
	}

	ADefenseCharacter* PerceivedCharacter = Cast<ADefenseCharacter>(Actor);
	if (!PerceivedCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Ignored | Enemy=%s Reason=ActorNotDefenseCharacter EnemyMode=%s EnemyState=%s Actor=%s"),
			*GetNameSafe(this),
			LexToString(EnemyMode),
			LexToString(EnemyState),
			*GetNameSafe(Actor)
		);
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		if (EnemyState == EEnemyState::Patrol)
		{
			Target = PerceivedCharacter;
			UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Target Updated | Enemy=%s NewTarget=%s EnemyState=%s Event=TargetFind"),
				*GetNameSafe(this),
				*GetNameSafe(Target),
				LexToString(EnemyState)
			);
			SendStateTreeEvent(FName("AI.Event.TargetFind"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Sensed But Not Tracking | Enemy=%s Actor=%s EnemyState=%s RequiredState=Patrol CurrentTarget=%s"),
				*GetNameSafe(this),
				*GetNameSafe(Actor),
				LexToString(EnemyState),
				*GetNameSafe(Target)
			);
		}
	}
	else
	{
		if (Target != PerceivedCharacter)
		{
			UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Lost Ignored | Enemy=%s LostActor=%s CurrentTarget=%s EnemyState=%s"),
				*GetNameSafe(this),
				*GetNameSafe(PerceivedCharacter),
				*GetNameSafe(Target),
				LexToString(EnemyState)
			);
			return;
		}
		
		Target = nullptr;
		UE_LOG(LogTemp, Warning, TEXT("Enemy Perception Target Updated | Enemy=%s NewTarget=None EnemyState=%s Event=TargetLost"),
			*GetNameSafe(this),
			LexToString(EnemyState)
		);
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

void AEnemyBase::MulticastRPC_StopAllMontages_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (AnimInst)
	{
		AnimInst->Montage_Stop(0.f);
	}
}

// 체력 UI 업데이트
void AEnemyBase::OnRep_UpdateUI()
{
	if (IsRunningDedicatedServer() || nullptr == HPUI)
	{
		return;
	}
	if (!bHpUIVisible && EnemyMode == EEnemyMode::Combat && EnemyState != EEnemyState::Die)
	{
		bHpUIVisible = true;
		HpComp->SetVisibility(true);
	}
	
	HPUI->UpdateHPBar(CurHP, MaxHP);
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

	if (EnemyState == EEnemyState::Die)
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

	/*ADefenseCharacter* AttackingCharacter = Cast<ADefenseCharacter>(DamageCauser);
	if (!AttackingCharacter && EventInstigator)
	{
		AttackingCharacter = Cast<ADefenseCharacter>(EventInstigator->GetPawn());
	}

	if (AttackingCharacter)
	{
		Target = AttackingCharacter;
	}*/
	
	if (CurHP <= 0.0f)
	{
		SendStateTreeEvent(FName("AI.Event.Die"));
		HpComp->SetVisibility(false);
		bHpUIVisible = false;
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

