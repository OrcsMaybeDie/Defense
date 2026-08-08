// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyBase.h"

#include "Animation/AnimMontage.h"
#include "StateTreeEvents.h"
#include "Characters/Enemy/EnemyAnim.h"
#include "Characters/Enemy/AI/EnemyController.h"
#include "Characters/Enemy/Data/EnemyData.h"
#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/Enemy/StoneFractureActor.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameManager/DefenseGameMode.h"
#include "GameManager/DestinationActor.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Traps/Barricade.h"
#include "Traps/BarricadeTrap.h"
#include "UI/EnemyHPUI.h"

namespace
{
	constexpr ECollisionChannel BarricadeCollisionChannel = ECC_GameTraceChannel3;

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
		case EEnemyState::Destroy:
			return TEXT("Destroy");
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
	
	HpComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HpComp"));
	HpComp->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyEnemyData();
	HpComp->SetVisibility(false);
	EnemyController = Cast<AEnemyController>(GetController());
	AnimInst = Cast<UEnemyAnim>(GetMesh()->GetAnimInstance());
	EnemyMesh = GetMesh();
	CurHP = MaxHP;
	
	// 서버에서만 AIPerception이 동작하도록 / 클라이언트에서는 비활성화하고 HPUI 정의
	if (!HasAuthority())
	{
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
		HPUI = Cast<UEnemyHPUI>(HpComp->GetWidget());
	}
	else
	{
		GameMode = Cast<ADefenseGameMode>(GetWorld()->GetAuthGameMode());
		DestinationActor = Cast<ADestinationActor>(
	UGameplayStatics::GetActorOfClass(GetWorld(), ADestinationActor::StaticClass())
);
	}

	if (UEnemyPoolSubsystem* EnemyPool = GetWorld()->GetSubsystem<UEnemyPoolSubsystem>())
	{
		EnemyPool->RegisterEnemy(this);
	}
}

void AEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UEnemyPoolSubsystem* EnemyPool = World->GetSubsystem<UEnemyPoolSubsystem>())
		{
			EnemyPool->UnregisterEnemy(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AEnemyBase::ApplyEnemyData()
{
	if (!EnemyData)
	{
		return;
	}
	EnemyType = EnemyData->EnemyType;
	MaxHP = EnemyData->MaxHP;
	KillCoinReward = EnemyData->KillCoinReward;
	PreviewMoveSpeed = EnemyData->PreviewMoveSpeed;
	CombatMoveSpeed = EnemyData->CombatMoveSpeed;
}

void AEnemyBase::SetTarget(AActor* NewTarget)
{
	Target = NewTarget;
	CurrentAttackDist = IsValid(Target) && (Target->IsA<ABarricade>() || Target->IsA<ABarricadeTrap>())
		? BarricadeAttackDist
		: AttackDist;
}

void AEnemyBase::BeginTrackedAction(const EEnemyState ActionState, const float TotalDuration, const float TriggerTime)
{
	TrackedActionData.Reset();
	TrackedActionData.bIsValid = true;
	TrackedActionData.ActionState = ActionState;
	TrackedActionData.TotalDuration = FMath::Max(0.f, TotalDuration);
	TrackedActionData.RemainingTime = TrackedActionData.TotalDuration;
	TrackedActionData.TriggerTime = FMath::Clamp(TriggerTime, 0.f, TrackedActionData.TotalDuration);
}

void AEnemyBase::UpdateTrackedAction(const float ElapsedTime, const bool bActionTriggered)
{
	if (!TrackedActionData.bIsValid)
	{
		return;
	}

	TrackedActionData.ElapsedTime = FMath::Clamp(ElapsedTime, 0.f, TrackedActionData.TotalDuration);
	TrackedActionData.RemainingTime = FMath::Max(0.f, TrackedActionData.TotalDuration - TrackedActionData.ElapsedTime);
	TrackedActionData.bActionTriggered = bActionTriggered;
}

void AEnemyBase::CompleteTrackedAction()
{
	TrackedActionData.Reset();
}

void AEnemyBase::ClearSuspendedAction()
{
	SuspendedActionData.Reset();
}

bool AEnemyBase::TryEnterStone()
{
	if (!HasAuthority()
		|| EnemyMode != EEnemyMode::Combat
		|| bDeathHandled
		|| EnemyState == EEnemyState::Stone
		|| EnemyState == EEnemyState::StoneDie
		|| EnemyState == EEnemyState::Die)
	{
		return false;
	}

	if (EnemyState != EEnemyState::StoneEnd || !SuspendedActionData.bIsValid)
	{
		const EEnemyState PreviousState = EnemyState;
		SuspendedActionData.Reset();

		if (TrackedActionData.bIsValid && TrackedActionData.ActionState == PreviousState)
		{
			SuspendedActionData = TrackedActionData;
		}
		else
		{
			SuspendedActionData.bIsValid = true;
			SuspendedActionData.ActionState = PreviousState;
		}
	}

	EnemyState = EEnemyState::Stone;
	SendStateTreeEvent(TEXT("AI.Event.Stone"));
	return true;
}

void AEnemyBase::BeginStoneGameplay()
{
	if (!HasAuthority() || bStoneGameplayActive)
	{
		return;
	}

	bStoneGameplayActive = true;
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementModeBeforeStone = MovementComponent->MovementMode;
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	if (AController* EnemyAIController = GetController())
	{
		EnemyAIController->StopMovement();
	}
}

void AEnemyBase::EndStoneGameplay()
{
	if (!HasAuthority() || !bStoneGameplayActive)
	{
		return;
	}

	bStoneGameplayActive = false;
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		const EMovementMode ResumeMode = MovementModeBeforeStone == MOVE_None
			? MOVE_Walking
			: static_cast<EMovementMode>(MovementModeBeforeStone.GetValue());
		MovementComponent->SetMovementMode(ResumeMode);
	}
}

void AEnemyBase::ResetStoneStateForPool()
{
	bDeathHandled = false;
	bDeathTaskStarted = false;
	PendingDeathType = EEnemyPendingDeathType::None;
	LastDeathRetryTime = -BIG_NUMBER;
	TrackedActionData.Reset();
	SuspendedActionData.Reset();
	EndStoneGameplay();
}

bool AEnemyBase::TryMarkDeathTaskStarted(const EEnemyPendingDeathType DeathType)
{
	if (!HasAuthority()
		|| !bDeathHandled
		|| bDeathTaskStarted
		|| PendingDeathType != DeathType)
	{
		return false;
	}

	bDeathTaskStarted = true;
	return true;
}

// Called every frame
void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsRunningDedicatedServer() && bPendingLocomotionResume)
	{
		LocomotionResumeWaitTime += DeltaTime;
		if (GetVelocity().SizeSquared2D() > FMath::Square(5.f) || LocomotionResumeWaitTime >= 0.25f)
		{
			bPendingLocomotionResume = false;
			LocomotionResumeWaitTime = 0.f;
			if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
			{
				SkeletalMesh->bPauseAnims = false;
			}
		}
	}

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

	if (UWorld* World = GetWorld())
	{
		if (UEnemyPoolSubsystem* EnemyPool = World->GetSubsystem<UEnemyPoolSubsystem>())
		{
			EnemyPool->NotifyEnemyModeChanged(this);
		}
	}
}

void AEnemyBase::SetEnemyMode(const EEnemyMode NewMode)
{
	EnemyMode = NewMode;
	OnRep_UpdateMode();
}

void AEnemyBase::SetPreview()
{
	ResetStoneVisual();

	if (!EnemyMesh)
	{
		EnemyMesh = GetMesh();
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = PreviewMoveSpeed;
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

	// 그리기 처리
	SetActorHiddenInGame(false);

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
		// 틱 처리
		SetActorTickEnabled(true);
		
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
	}

	
	// 서버에서 처리	
	if (HasAuthority())
	{	if (!EnemyController)
		{
			EnemyController = Cast<AEnemyController>(GetController());
		}
		
		if (EnemyController && EnemyController->StateTreeAIComp)
		{
			EnemyController->StateTreeAIComp->StartLogic();
			EnemyController->StateTreeAIComp->SetComponentTickEnabled(true);
		}
		
		
	}
	
	// 충돌 처리
	SetActorEnableCollision(true);
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(BarricadeCollisionChannel, ECR_Ignore);
	}
	
	if (EnemyMesh)
	{
		EnemyMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		EnemyMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		EnemyMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	
	
}

void AEnemyBase::SetCombat()
{
	ResetStoneVisual();

	if (!EnemyMesh)
	{
		EnemyMesh = GetMesh();
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = CombatMoveSpeed;
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
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
		SetActorTickEnabled(true); // tick에는 ui 회전만 있어서 데디서버가 아닌 곳에서 켜지게 함.
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
		//CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		CapsuleComp->SetCollisionResponseToChannel(BarricadeCollisionChannel, ECR_Block);
	}
	
	if (EnemyMesh)
	{
		EnemyMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		//EnemyMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
	
}

void AEnemyBase::SetInactive()
{
	ResetStoneVisual();

	//UE_LOG(LogTemp, Warning, TEXT("Enemy SetInactive | Enemy=%s Mode=%s(%d) NetMode=%d HasAuthority=%d"),
		//*GetNameSafe(this),
		//LexToString(EnemyMode),
		//static_cast<int32>(EnemyMode),
		//static_cast<int32>(GetNetMode()),
		//HasAuthority() ? 1 : 0);

	if (HasAuthority())
	{
		if (!EnemyController)
		{
			EnemyController = Cast<AEnemyController>(GetController());
		}
		
		if (EnemyController && EnemyController->StateTreeAIComp)
		{
			EnemyController->StateTreeAIComp->StopLogic(TEXT("EnemyMode : Inactive"));
		}
	}
	else
	{
		
	}
	
	// 충돌 처리
	SetActorEnableCollision(false);
	
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	}
	
	if (EnemyMesh)
	{
		//EnemyMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		EnemyMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	// 그리기 처리
	SetActorHiddenInGame(true);
	
	if (!IsRunningDedicatedServer())
	{ // 틱 처리
		SetActorTickEnabled(false);
		if (HpComp)
		{
			bHpUIVisible = false;
			HpComp->SetVisibility(false);
		}
	}
}

void AEnemyBase::OnEnteredPatrol()
{
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

void AEnemyBase::RetryPendingDeathTransition()
{
	if (!HasAuthority()
		|| EnemyMode != EEnemyMode::Combat
		|| !bDeathHandled
		|| bDeathTaskStarted
		|| PendingDeathType == EEnemyPendingDeathType::None)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastDeathRetryTime < DeathRetryInterval)
	{
		return;
	}
	LastDeathRetryTime = CurrentTime;

	if (!EnemyController)
	{
		EnemyController = Cast<AEnemyController>(GetController());
	}
	if (EnemyController && EnemyController->StateTreeAIComp)
	{
		if (!EnemyController->StateTreeAIComp->IsRunning())
		{
			EnemyController->StateTreeAIComp->StartLogic();
		}
		EnemyController->StateTreeAIComp->SetComponentTickEnabled(true);
	}

	if (PendingDeathType == EEnemyPendingDeathType::Stone)
	{
		EnemyState = EEnemyState::StoneDie;
		SendStateTreeEvent(TEXT("AI.Event.StoneDie"));
	}
	else
	{
		EnemyState = EEnemyState::Die;
		SendStateTreeEvent(TEXT("AI.Event.Die"));
	}
}

void AEnemyBase::MulticastRPC_DamageMotion_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}

	PrepareForRegularAnimation();
	if (AnimInst)
	{
		AnimInst->PlayDamageMotion();
	}
}

void AEnemyBase::MulticastRPC_DieMotion_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}

	PrepareForRegularAnimation();
	if (HpComp)
	{
		HpComp->SetVisibility(false);
	}
	bHpUIVisible = false;
	if (AnimInst)
	{
		AnimInst->PlayDieMotion();
	}
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

	ResetStoneVisual();
}

void AEnemyBase::MulticastRPC_EnterStoneVisual_Implementation()
{
	EnterStoneVisual();
}

void AEnemyBase::MulticastRPC_ExitStoneVisual_Implementation(const bool bResumeMontage, const bool bWaitForMovement)
{
	ExitStoneVisual(bResumeMontage, bWaitForMovement);
}

void AEnemyBase::MulticastRPC_StoneDieVisual_Implementation()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	bPendingLocomotionResume = false;
	LocomotionResumeWaitTime = 0.f;
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!bStoneVisualActive)
	{
		EnterStoneVisual();
	}

	bool bFractureActivated = false;
	if (SkeletalMesh && StoneFractureActorClass)
	{
		if (UWorld* World = GetWorld())
		{
			if (UEnemyPoolSubsystem* EnemyPool = World->GetSubsystem<UEnemyPoolSubsystem>())
			{
				bFractureActivated = EnemyPool->SpawnStoneFractureFromPool(
					StoneFractureActorClass,
					SkeletalMesh) != nullptr;
			}
		}
	}

	// 에셋이나 매핑이 빠진 경우에는 기존 석화 포즈를 남겨 디버깅할 수 있게 한다.
	if (bFractureActivated && SkeletalMesh)
	{
		SkeletalMesh->SetVisibility(false, true);
	}
	if (HpComp)
	{
		HpComp->SetVisibility(false);
	}
	bHpUIVisible = false;
}

void AEnemyBase::PrepareForRegularAnimation()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	ResetStoneVisual();
}

void AEnemyBase::EnterStoneVisual()
{
	if (IsRunningDedicatedServer() || bStoneVisualActive)
	{
		return;
	}

	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh)
	{
		return;
	}

	if (!AnimInst)
	{
		AnimInst = Cast<UEnemyAnim>(SkeletalMesh->GetAnimInstance());
	}

	MaterialsBeforeStone.Empty();
	for (int32 MaterialIndex = 0; MaterialIndex < SkeletalMesh->GetNumMaterials(); ++MaterialIndex)
	{
		MaterialsBeforeStone.Add(SkeletalMesh->GetMaterial(MaterialIndex));
		if (StoneMaterial)
		{
			SkeletalMesh->SetMaterial(MaterialIndex, StoneMaterial);
		}
	}

	SuspendedMontage = nullptr;
	SuspendedMontagePosition = 0.f;
	SuspendedMontagePlayRate = 1.f;
	if (AnimInst)
	{
		SuspendedMontage = AnimInst->GetCurrentActiveMontage();
		if (SuspendedMontage)
		{
			SuspendedMontagePosition = AnimInst->Montage_GetPosition(SuspendedMontage);
			SuspendedMontagePlayRate = AnimInst->Montage_GetPlayRate(SuspendedMontage);
			AnimInst->Montage_Pause(SuspendedMontage);
		}
	}

	bPendingLocomotionResume = false;
	LocomotionResumeWaitTime = 0.f;
	bStoneVisualActive = true;
	SkeletalMesh->bPauseAnims = true;
}

void AEnemyBase::ExitStoneVisual(const bool bResumeMontage, const bool bWaitForMovement)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh)
	{
		return;
	}

	RestoreMaterialsBeforeStone();
	bStoneVisualActive = false;

	if (bWaitForMovement)
	{
		if (AnimInst && SuspendedMontage)
		{
			AnimInst->Montage_Stop(0.f, SuspendedMontage);
		}
		SuspendedMontage = nullptr;
		bPendingLocomotionResume = true;
		LocomotionResumeWaitTime = 0.f;
		return;
	}

	bPendingLocomotionResume = false;
	LocomotionResumeWaitTime = 0.f;
	SkeletalMesh->bPauseAnims = false;

	if (AnimInst && SuspendedMontage)
	{
		if (bResumeMontage && AnimInst->Montage_IsActive(SuspendedMontage))
		{
			AnimInst->Montage_SetPosition(SuspendedMontage, SuspendedMontagePosition);
			AnimInst->Montage_SetPlayRate(SuspendedMontage, SuspendedMontagePlayRate);
			AnimInst->Montage_Resume(SuspendedMontage);
		}
		else if (!bResumeMontage)
		{
			AnimInst->Montage_Stop(0.f, SuspendedMontage);
		}
	}

	SuspendedMontage = nullptr;
	SuspendedMontagePosition = 0.f;
	SuspendedMontagePlayRate = 1.f;
}

void AEnemyBase::RestoreMaterialsBeforeStone()
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (SkeletalMesh)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialsBeforeStone.Num(); ++MaterialIndex)
		{
			SkeletalMesh->SetMaterial(MaterialIndex, MaterialsBeforeStone[MaterialIndex]);
		}
	}

	MaterialsBeforeStone.Empty();
}

void AEnemyBase::ResetStoneVisual()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	RestoreMaterialsBeforeStone();
	bStoneVisualActive = false;
	bPendingLocomotionResume = false;
	LocomotionResumeWaitTime = 0.f;

	if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		SkeletalMesh->bPauseAnims = false;
		SkeletalMesh->SetVisibility(true, true);
	}
	if (AnimInst && SuspendedMontage && AnimInst->Montage_IsActive(SuspendedMontage))
	{
		AnimInst->Montage_Stop(0.f, SuspendedMontage);
	}

	SuspendedMontage = nullptr;
	SuspendedMontagePosition = 0.f;
	SuspendedMontagePlayRate = 1.f;
}

// 체력 UI 업데이트
void AEnemyBase::OnRep_UpdateUI()
{
	if (IsRunningDedicatedServer() || nullptr == HPUI)
	{
		return;
	}
	
	if (EnemyMode != EEnemyMode::Combat)
	{
		return;
	}

	if (CurHP <= 0.f || EnemyState == EEnemyState::Die || EnemyState == EEnemyState::StoneDie)
	{
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
		return;
	}
	
	if (CurHP >= MaxHP)
	{
		return;
	}
	
	if (!bHpUIVisible
		&& CurHP > 0.f
		&& EnemyState != EEnemyState::Die
		&& EnemyState != EEnemyState::StoneDie)
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

	if (bDeathHandled)
	{
		RetryPendingDeathTransition();
		return 0.f;
	}

	if (EnemyState == EEnemyState::Die || EnemyState == EEnemyState::StoneDie)
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

	CurHP = FMath::Max(0.f, CurHP - ActualDamage);

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
		bDeathHandled = true;
		bDeathTaskStarted = false;
		PendingDeathType = EnemyState == EEnemyState::Stone
			? EEnemyPendingDeathType::Stone
			: EEnemyPendingDeathType::Normal;
		LastDeathRetryTime = -BIG_NUMBER;
		if (GameMode)
		{
			GameMode->AwardEnemyKillCoin(this, DamageCauser, EventInstigator);
		}
		
		// 재화 얻어지나 테스트--------------------
		/*if (APawn* CauserPawn = Cast<APawn>(DamageCauser))
		{
			AController* CauserController = CauserPawn->GetController();

			if (CauserController)
			{
				// 여기서 컨트롤러 사용
				auto* ps = CauserController->GetPlayerState<ADefensePlayerState>();
				UE_LOG(LogTemp, Error, TEXT("Coin : %d"), ps->GetCoin());
			}
		}*/
		//----------------------------------------------
		
		RetryPendingDeathTransition();
		bHpUIVisible = false;
	}
	else if (EnemyState != EEnemyState::Stone)
	{
		if (EnemyState == EEnemyState::StoneEnd)
		{
			ClearSuspendedAction();
			EnemyState = EEnemyState::Damage;
		}
		SendStateTreeEvent(TEXT("AI.Event.Damage"));
	}

	return ActualDamage;
}

