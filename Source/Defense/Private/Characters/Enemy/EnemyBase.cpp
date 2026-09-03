// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyBase.h"
#include "Collision/DefenseCollisionChannels.h"

#include "Animation/AnimMontage.h"
#include "StateTreeEvents.h"
#include "Characters/Enemy/EnemyAnim.h"
#include "Characters/Enemy/BurnDamageType.h"
#include "Characters/Enemy/AI/EnemyController.h"
#include "Characters/Enemy/Data/EnemyData.h"
#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "Characters/Enemy/EnemySpawner.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Characters/Player/DefensePlayerController.h"
#include "Characters/Player/DefensePlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/Enemy/StoneFractureActor.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/PlayerController.h"
#include "GameManager/DefenseGameMode.h"
#include "GameManager/Portal.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Traps/BarricadeTrap.h"
#include "Traps/LightningTrap.h"
#include "UI/EnemyHPUI.h"
#include "UI/RewardUI.h"
#include "Blueprint/UserWidget.h"

namespace
{
	// 충돌 채널은 DefenseCollisionChannels.h에서 통합 관리 (확인 후 주석 제거)
	// constexpr ECollisionChannel BarricadeCollisionChannel = ECC_GameTraceChannel3;
	const FName PortalClipEnabledParameter(TEXT("PortalClipEnabled"));
	const FName PortalPositionParameter(TEXT("PortalPosition"));
	const FName PortalForwardParameter(TEXT("PortalForward"));
	const FName PortalRightParameter(TEXT("PortalRight"));
	const FName PortalUpParameter(TEXT("PortalUp"));
	const FName PortalHalfWidthParameter(TEXT("PortalHalfWidth"));
	const FName PortalHalfHeightParameter(TEXT("PortalHalfHeight"));

	FLinearColor ToMaterialVector(const FVector& Vector)
	{
		return FLinearColor(Vector.X, Vector.Y, Vector.Z, 0.f);
	}

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
	ApplyEnemyCollisionPolicy();
	
	HpComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HpComp"));
	HpComp->SetupAttachment(RootComponent);

	RewardPopupAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("RewardPopupAnchor"));
	RewardPopupAnchor->SetupAttachment(RootComponent);
	RewardPopupAnchor->SetRelativeLocation(FVector(0.f, 0.f, 150.f));

	RewardComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("RewardComp"));
	RewardComp->SetupAttachment(RewardPopupAnchor);
	RewardComp->SetWidgetSpace(EWidgetSpace::Screen);
	RewardComp->SetDrawAtDesiredSize(true);
	RewardComp->SetVisibility(false);

	Weapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));
	Weapon->SetupAttachment(GetMesh());
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Weapon->SetCanEverAffectNavigation(false);
	Weapon->SetHiddenInGame(true);
}

void AEnemyBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (Weapon && GetMesh())
	{
		Weapon->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::KeepRelativeTransform,
			WeaponSocketName
		);
	}
}

// Called when the game starts or when spawned
void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyEnemyCollisionPolicy();
	ApplyEnemyData();
	HpComp->SetVisibility(false);
	RewardPopupInitialRelativeLocation = RewardComp->GetRelativeLocation();
	ResetRewardPopup();
	EnemyController = Cast<AEnemyController>(GetController());
	AnimInst = Cast<UEnemyAnim>(GetMesh()->GetAnimInstance());
	EnemyMesh = GetMesh();
	CurHP = MaxHP;
	InitializeDamageOverlay();
	
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
		PortalActor = Cast<APortal>(UGameplayStatics::GetActorOfClass(GetWorld(), APortal::StaticClass()));
	}

	if (UEnemyPoolSubsystem* EnemyPool = GetWorld()->GetSubsystem<UEnemyPoolSubsystem>())
	{
		EnemyPool->RegisterEnemy(this);
	}
}

void AEnemyBase::ApplyEnemyCollisionPolicy()
{
	GetCapsuleComponent()->SetCollisionObjectType(DefenseCollisionChannels::Enemy);

	GetCapsuleComponent()->SetCollisionResponseToChannel(DefenseCollisionChannels::FootIK, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(DefenseCollisionChannels::FootIK, ECR_Ignore);
}

void AEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearDeathFailsafeTimer();
	ClearPortalEntryFailsafeTimer();
	ClearBurnTimers();
	ClearDamageOutline();
	ClearElectricHit();
	SetBurnVisualActive(false);
	RestoreDamageOverlay();

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
	if (bEnteringPortal && NewTarget)
	{
		return;
	}

	Target = NewTarget;
	CurrentAttackDist = IsValid(Target) && (Target->IsA<ABarricadeTrap>())
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
		|| bEnteringPortal
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
	ClearDeathFailsafeTimer();
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
	UpdateRewardPopup(DeltaTime);

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
	DOREPLIFETIME(AEnemyBase, bIsBurning);
	DOREPLIFETIME(AEnemyBase, bEnteringPortal);
	
	
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
	ResetPortalEntryState();
	ResetRewardPopup();
	ClearDamageOutline();
	ClearElectricHit();
	Weapon->SetHiddenInGame(true);

	if (HasAuthority())
	{
		EndBurnEffect();
	}
	SetBurnVisualActive(false);

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
				for (int32 MaterialIndex = 0; MaterialIndex < EnemyMesh->GetNumMaterials(); ++MaterialIndex)
				{
					EnemyMesh->SetMaterial(MaterialIndex, PreviewMaterial);
				}
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
		
		// The spawner starts StateTree after possession and route setup are complete.
	}
	
	// 충돌 처리
	SetActorEnableCollision(true);
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(DefenseCollisionChannels::Barricade, ECR_Ignore);
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
	ResetPortalEntryState();
	ResetStoneVisual();
	Weapon->SetHiddenInGame(false);

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
			for (int32 MaterialIndex = 0; MaterialIndex < EnemyMesh->GetNumMaterials(); ++MaterialIndex)
			{
				if (CombatMaterials.IsValidIndex(MaterialIndex) && CombatMaterials[MaterialIndex])
				{
					EnemyMesh->SetMaterial(MaterialIndex, CombatMaterials[MaterialIndex]);
				}
			}
		}
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
		SetActorTickEnabled(true); // tick에는 ui 회전만 있어서 데디서버가 아닌 곳에서 켜지게 함.
	}
	
	if (HasAuthority())
	{
		// The spawner starts StateTree after possession and route setup are complete.
	}
	
	SetActorEnableCollision(true);
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		//CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		CapsuleComp->SetCollisionResponseToChannel(DefenseCollisionChannels::Barricade, ECR_Block);
	}
	
	if (EnemyMesh)
	{
		EnemyMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		//EnemyMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
	
}

void AEnemyBase::SetInactive()
{
	ClearDeathFailsafeTimer();
	ResetPortalEntryState();
	ResetRewardPopup();
	ClearDamageOutline();
	ClearElectricHit();

	if (HasAuthority())
	{
		EndBurnEffect();
	}
	SetBurnVisualActive(false);

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

bool AEnemyBase::TryBeginPortalEntry(
	APortal* Portal,
	const FVector& ExitLocation,
	const FVector& PortalPosition,
	const FVector& PortalForward,
	const FVector& PortalRight,
	const FVector& PortalUp,
	const float PortalHalfWidth,
	const float PortalHalfHeight
)
{
	if (!HasAuthority()
		|| !Portal
		|| bEnteringPortal
		|| EnemyMode == EEnemyMode::Inactive
		|| EnemyMode == EEnemyMode::ReturningToPool
		|| EnemyState == EEnemyState::Die
		|| EnemyState == EEnemyState::StoneDie)
	{
		return false;
	}

	bEnteringPortal = true;
	EnteringPortal = Portal;
	SetTarget(nullptr);
	EndBurnEffect();
	ClearDamageOutline();
	ClearElectricHit();
	ApplyPortalCollisionState();

	if (HpComp)
	{
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
	}

	if (!EnemyController)
	{
		EnemyController = Cast<AEnemyController>(GetController());
	}

	if (EnemyController)
	{
		if (EnemyController->StateTreeAIComp)
		{
			EnemyController->StateTreeAIComp->StopLogic(TEXT("Enemy entered portal"));
		}

		EnemyController->StopMovement();
		EnemyController->MoveToLocation(
			ExitLocation,
			5.f,
			false,
			true,
			true,
			false,
			NavigationFilterClass,
			true
		);
	}

	MulticastRPC_BeginPortalClip(
		PortalPosition,
		PortalForward.GetSafeNormal(),
		PortalRight.GetSafeNormal(),
		PortalUp.GetSafeNormal(),
		FMath::Max(PortalHalfWidth, 1.f),
		FMath::Max(PortalHalfHeight, 1.f)
	);
	StartPortalEntryFailsafeTimer();

	return true;
}

bool AEnemyBase::FinishPortalEntry(const APortal* Portal)
{
	if (!HasAuthority() || !bEnteringPortal || !Portal || EnteringPortal != Portal)
	{
		return false;
	}

	bEnteringPortal = false;
	EnteringPortal = nullptr;
	ClearPortalEntryFailsafeTimer();
	RestorePortalCollisionState();
	return true;
}

void AEnemyBase::MulticastRPC_BeginPortalClip_Implementation(
	const FVector PortalPosition,
	const FVector PortalForward,
	const FVector PortalRight,
	const FVector PortalUp,
	const float PortalHalfWidth,
	const float PortalHalfHeight
)
{
	ApplyPortalCollisionState();

	if (IsRunningDedicatedServer())
	{
		return;
	}

	ClearDamageOutline();
	ClearElectricHit();
	SetBurnVisualActive(false);
	if (HpComp)
	{
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
	}

	ApplyPortalClipVisual(
		PortalPosition,
		PortalForward,
		PortalRight,
		PortalUp,
		PortalHalfWidth,
		PortalHalfHeight
	);
}

void AEnemyBase::ApplyPortalClipVisual(
	const FVector& PortalPosition,
	const FVector& PortalForward,
	const FVector& PortalRight,
	const FVector& PortalUp,
	const float PortalHalfWidth,
	const float PortalHalfHeight
)
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh)
	{
		return;
	}

	PortalMaterialInstances.Reset();
	for (int32 MaterialIndex = 0; MaterialIndex < SkeletalMesh->GetNumMaterials(); ++MaterialIndex)
	{
		UMaterialInstanceDynamic* MID = SkeletalMesh->CreateDynamicMaterialInstance(MaterialIndex);
		if (!MID)
		{
			continue;
		}

		MID->SetVectorParameterValue(PortalPositionParameter, ToMaterialVector(PortalPosition));
		MID->SetVectorParameterValue(PortalForwardParameter, ToMaterialVector(PortalForward));
		MID->SetVectorParameterValue(PortalRightParameter, ToMaterialVector(PortalRight));
		MID->SetVectorParameterValue(PortalUpParameter, ToMaterialVector(PortalUp));
		MID->SetScalarParameterValue(PortalHalfWidthParameter, FMath::Max(PortalHalfWidth, 1.f));
		MID->SetScalarParameterValue(PortalHalfHeightParameter, FMath::Max(PortalHalfHeight, 1.f));
		MID->SetScalarParameterValue(PortalClipEnabledParameter, 1.f);
		PortalMaterialInstances.Add(MID);
	}
}

void AEnemyBase::ResetPortalClipVisual()
{
	for (UMaterialInstanceDynamic* MID : PortalMaterialInstances)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(PortalClipEnabledParameter, 0.f);
		}
	}
	PortalMaterialInstances.Reset();
}

void AEnemyBase::ApplyPortalCollisionState()
{
	if (bPortalCollisionSnapshotValid)
	{
		return;
	}

	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsulePawnResponseBeforePortal = CapsuleComp->GetCollisionResponseToChannel(ECC_Pawn);
		CapsuleVisibilityResponseBeforePortal = CapsuleComp->GetCollisionResponseToChannel(ECC_Visibility);
		CapsuleBarricadeResponseBeforePortal = CapsuleComp->GetCollisionResponseToChannel(DefenseCollisionChannels::Barricade);
		CapsuleWorldDynamicResponseBeforePortal = CapsuleComp->GetCollisionResponseToChannel(ECC_WorldDynamic);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(DefenseCollisionChannels::Barricade, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	}

	if (!EnemyMesh)
	{
		EnemyMesh = GetMesh();
	}
	if (EnemyMesh)
	{
		MeshCollisionEnabledBeforePortal = EnemyMesh->GetCollisionEnabled();
		EnemyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	bPortalCollisionSnapshotValid = true;
}

void AEnemyBase::RestorePortalCollisionState()
{
	if (!bPortalCollisionSnapshotValid)
	{
		return;
	}

	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, CapsulePawnResponseBeforePortal);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, CapsuleVisibilityResponseBeforePortal);
		CapsuleComp->SetCollisionResponseToChannel(DefenseCollisionChannels::Barricade, CapsuleBarricadeResponseBeforePortal);
		CapsuleComp->SetCollisionResponseToChannel(ECC_WorldDynamic, CapsuleWorldDynamicResponseBeforePortal);
	}

	if (!EnemyMesh)
	{
		EnemyMesh = GetMesh();
	}
	if (EnemyMesh)
	{
		EnemyMesh->SetCollisionEnabled(MeshCollisionEnabledBeforePortal);
	}

	bPortalCollisionSnapshotValid = false;
}

void AEnemyBase::ResetPortalEntryState()
{
	ClearPortalEntryFailsafeTimer();
	bEnteringPortal = false;
	EnteringPortal = nullptr;
	RestorePortalCollisionState();
	ResetPortalClipVisual();
}

void AEnemyBase::StartDeathFailsafeTimer()
{
	if (!HasAuthority() || DeathFailsafeTimeout <= 0.f)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(DeathFailsafeTimerHandle);
	GetWorldTimerManager().SetTimer(
		DeathFailsafeTimerHandle,
		this,
		&AEnemyBase::HandleDeathFailsafeTimeout,
		DeathFailsafeTimeout,
		false
	);
}

void AEnemyBase::ClearDeathFailsafeTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathFailsafeTimerHandle);
	}
}

void AEnemyBase::HandleDeathFailsafeTimeout()
{
	if (!HasAuthority() || !bDeathHandled || EnemyMode != EEnemyMode::Combat)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Enemy death failsafe forced cleanup | Enemy=%s State=%s"),
		*GetNameSafe(this),
		LexToString(EnemyState));

	ForceReturnToPoolFromFailsafe(false);
}

void AEnemyBase::StartPortalEntryFailsafeTimer()
{
	if (!HasAuthority() || PortalEntryFailsafeTimeout <= 0.f)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(PortalEntryFailsafeTimerHandle);
	GetWorldTimerManager().SetTimer(
		PortalEntryFailsafeTimerHandle,
		this,
		&AEnemyBase::HandlePortalEntryFailsafeTimeout,
		PortalEntryFailsafeTimeout,
		false
	);
}

void AEnemyBase::ClearPortalEntryFailsafeTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PortalEntryFailsafeTimerHandle);
	}
}

void AEnemyBase::HandlePortalEntryFailsafeTimeout()
{
	if (!HasAuthority()
		|| !bEnteringPortal
		|| (EnemyMode != EEnemyMode::Combat && EnemyMode != EEnemyMode::Preview))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Enemy portal-entry failsafe forced cleanup | Enemy=%s Mode=%s Location=%s"),
		*GetNameSafe(this),
		LexToString(EnemyMode),
		*GetActorLocation().ToString());

	ResetPortalEntryState();
	ForceReturnToPoolFromFailsafe(true);
}

void AEnemyBase::ForceReturnToPoolFromFailsafe(const bool bReachedDestination)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!EnemyController)
	{
		EnemyController = Cast<AEnemyController>(GetController());
	}
	if (EnemyController)
	{
		EnemyController->StopMovement();
	}
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bWasCombatEnemy = EnemyMode == EEnemyMode::Combat;
	if (bWasCombatEnemy)
	{
		ADefenseGameMode* AuthGameMode = World->GetAuthGameMode<ADefenseGameMode>();
		if (AuthGameMode)
		{
			GameMode = AuthGameMode;
			AuthGameMode->NotifyEnemyRemoved(
				this,
				bReachedDestination ? EEnemyRemoveReason::ReachedDestination : EEnemyRemoveReason::Killed
			);
		}
		else if (OwningSpawner)
		{
			OwningSpawner->RemoveActiveEnemy(this);
		}
	}
	else if (OwningSpawner)
	{
		OwningSpawner->RemoveActiveEnemy(this);
	}

	if (UEnemyPoolSubsystem* EnemyPool = World->GetSubsystem<UEnemyPoolSubsystem>())
	{
		EnemyPool->ReturnToPool(this);
	}
	else
	{
		SetEnemyMode(EEnemyMode::Inactive);
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

void AEnemyBase::MulticastRPC_BurnReaction_Implementation()
{
	if (IsRunningDedicatedServer() || bStoneVisualActive)
	{
		return;
	}

	if (!AnimInst)
	{
		AnimInst = Cast<UEnemyAnim>(GetMesh() ? GetMesh()->GetAnimInstance() : nullptr);
	}

	if (AnimInst)
	{
		AnimInst->PlayBurnReactionMotion();
	}
}

void AEnemyBase::MulticastRPC_ShowDamageOutline_Implementation()
{
	ShowDamageOutline();
}

void AEnemyBase::MulticastRPC_ShowElectricHit_Implementation()
{
	ShowElectricHit();
}

void AEnemyBase::MulticastRPC_DieMotion_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (NormalDeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, NormalDeathSound, GetActorLocation());
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

	if (StoneDeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, StoneDeathSound, GetActorLocation());
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

void AEnemyBase::ShowRewardPopup(const int32 RewardAmount)
{
	if (IsRunningDedicatedServer() || !RewardComp)
	{
		return;
	}

	RewardComp->InitWidget();
	UUserWidget* RewardWidget = RewardComp->GetWidget();
	RewardUI = Cast<URewardUI>(RewardWidget);
	if (RewardUI)
	{
		RewardUI->SetRewardAmount(RewardAmount);
	}
	else if (RewardWidget)
	{
		// WBP_RewardUI가 아직 URewardUI를 부모로 사용하지 않아도 이름으로 연결한다.
		if (UTextBlock* RewardText = Cast<UTextBlock>(RewardWidget->GetWidgetFromName(TEXT("RewardText"))))
		{
			RewardText->SetText(FText::AsNumber(RewardAmount));
		}
	}

	RewardPopupElapsedTime = 0.f;
	bRewardPopupPlaying = true;
	RewardComp->SetRelativeLocation(RewardPopupInitialRelativeLocation);
	RewardComp->SetVisibility(true);
}

void AEnemyBase::UpdateRewardPopup(const float DeltaTime)
{
	if (!bRewardPopupPlaying || !RewardComp || IsRunningDedicatedServer())
	{
		return;
	}

	RewardPopupElapsedTime += DeltaTime;
	const float Duration = FMath::Max(RewardPopupDuration, UE_KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp(RewardPopupElapsedTime / Duration, 0.f, 1.f);
	const float EasedAlpha = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);
	RewardComp->SetRelativeLocation(
		RewardPopupInitialRelativeLocation + FVector(0.f, 0.f, RewardPopupRiseHeight * EasedAlpha)
	);

	if (Alpha >= 1.f)
	{
		ResetRewardPopup();
	}
}

void AEnemyBase::ResetRewardPopup()
{
	bRewardPopupPlaying = false;
	RewardPopupElapsedTime = 0.f;
	RewardUI = nullptr;

	if (RewardComp)
	{
		RewardComp->SetRelativeLocation(RewardPopupInitialRelativeLocation);
		RewardComp->SetVisibility(false);
	}
}

void AEnemyBase::PrepareForRegularAnimation()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	ResetStoneVisual();
}

const FGameplayTagContainer& AEnemyBase::GetEnemyTags() const
{
	static const FGameplayTagContainer EmptyEnemyTags;

	return EnemyData ? EnemyData->EnemyTags : EmptyEnemyTags;
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
	UpdateDamageOverlayForStoneState();
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
	UpdateDamageOverlayForStoneState();

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
	UpdateDamageOverlayForStoneState();
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

	if (bEnteringPortal)
	{
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
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

	if (EnemyMode != EEnemyMode::Combat || bEnteringPortal)
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
	const UClass* DamageTypeClass = DamageEvent.DamageTypeClass.Get();
	const bool bIsBurnDamage = DamageTypeClass && DamageTypeClass->IsChildOf(UBurnDamageType::StaticClass());
	const bool bIsLightningDamage = IsValid(DamageCauser) && DamageCauser->IsA<ALightningTrap>();

	if (bIsLightningDamage)
	{
		MulticastRPC_ShowElectricHit();
	}

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
		EndBurnEffect();
		bDeathHandled = true;
		bDeathTaskStarted = false;
		PendingDeathType = EnemyState == EEnemyState::Stone
			? EEnemyPendingDeathType::Stone
			: EEnemyPendingDeathType::Normal;
		LastDeathRetryTime = -BIG_NUMBER;
		if (!EnemyController)
		{
			EnemyController = Cast<AEnemyController>(GetController());
		}
		if (EnemyController)
		{
			EnemyController->StopMovement();
		}
		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
		StartDeathFailsafeTimer();
		if (GameMode)
		{
			if (ADefensePlayerState* KillerPlayerState = GameMode->HandleEnemyKilled(this, DamageCauser, EventInstigator))
			{
				if (KillCoinReward > 0)
				{
					if (ADefensePlayerController* PlayerController = Cast<ADefensePlayerController>(KillerPlayerState->GetPlayerController()))
					{
						PlayerController->ClientRPC_ShowRewardPopup(this, KillCoinReward);
					}
				}
			}
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
	else if (!bIsBurnDamage)
	{
		if (!bIsLightningDamage
			&& (EnemyState != EEnemyState::Stone || bShowDamageOutlineWhileStone))
		{
			MulticastRPC_ShowDamageOutline();
		}

		if (EnemyState == EEnemyState::StoneEnd)
		{
			ClearSuspendedAction();
			EnemyState = EEnemyState::Damage;
		}

		if (EnemyState != EEnemyState::Stone)
		{
			SendStateTreeEvent(TEXT("AI.Event.Damage"));
		}
	}

	return ActualDamage;
}

void AEnemyBase::ApplyBurnEffect(
	const float Duration,
	const float DamageInterval,
	const float DamagePerTick,
	AActor* DamageCauser
)
{
	if (!HasAuthority()
		|| EnemyMode != EEnemyMode::Combat
		|| bEnteringPortal
		|| bDeathHandled
		|| EnemyState == EEnemyState::Stone
		|| EnemyState == EEnemyState::StoneDie
		|| EnemyState == EEnemyState::Die
		|| Duration <= 0.f
		|| DamageInterval <= 0.f
		|| DamagePerTick <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bWasBurning = bIsBurning;
	BurnDamagePerTick = DamagePerTick;
	BurnEndTime = World->GetTimeSeconds() + Duration;
	BurnDamageCauser = DamageCauser;
	BurnEventInstigator = DamageCauser ? DamageCauser->GetInstigatorController() : nullptr;

	if (!bWasBurning)
	{
		bIsBurning = true;
		OnRep_IsBurning();
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (!TimerManager.IsTimerActive(BurnDamageTimerHandle)
		|| !FMath::IsNearlyEqual(TimerManager.GetTimerRate(BurnDamageTimerHandle), DamageInterval))
	{
		TimerManager.SetTimer(
			BurnDamageTimerHandle,
			this,
			&AEnemyBase::ApplyBurnDamageTick,
			DamageInterval,
			true,
			DamageInterval
		);
	}

	TimerManager.SetTimer(
		BurnEndTimerHandle,
		this,
		&AEnemyBase::EndBurnEffect,
		Duration,
		false
	);

	// 첫 진입은 즉시 반응하고, 재진입은 지속시간만 갱신해 경계 중첩 피해를 막는다.
	if (!bWasBurning)
	{
		ApplyBurnDamageTick();
	}
}

void AEnemyBase::ApplyBurnDamageTick()
{
	if (!HasAuthority() || !bIsBurning || EnemyMode != EEnemyMode::Combat || bEnteringPortal || bDeathHandled)
	{
		EndBurnEffect();
		return;
	}

	if (const UWorld* World = GetWorld(); !World || World->GetTimeSeconds() >= BurnEndTime)
	{
		EndBurnEffect();
		return;
	}

	AActor* DamageCauser = BurnDamageCauser.Get();
	AController* EventInstigator = BurnEventInstigator.Get();
	const float ActualDamage = UGameplayStatics::ApplyDamage(
		this,
		BurnDamagePerTick,
		EventInstigator,
		DamageCauser,
		UBurnDamageType::StaticClass()
	);

	if (ActualDamage > 0.f && !bDeathHandled)
	{
		MulticastRPC_BurnReaction();
	}
}

void AEnemyBase::EndBurnEffect()
{
	if (!HasAuthority())
	{
		return;
	}

	ClearBurnTimers();
	BurnDamagePerTick = 0.f;
	BurnEndTime = 0.f;
	BurnDamageCauser.Reset();
	BurnEventInstigator.Reset();

	if (bIsBurning)
	{
		bIsBurning = false;
		OnRep_IsBurning();
	}
}

void AEnemyBase::ClearBurnTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurnDamageTimerHandle);
		World->GetTimerManager().ClearTimer(BurnEndTimerHandle);
	}
}

void AEnemyBase::OnRep_IsBurning()
{
	SetBurnVisualActive(bIsBurning);
}

void AEnemyBase::InitializeDamageOverlay()
{
	if (IsRunningDedicatedServer() || (bStoneVisualActive && !bApplyDamageOverlayWhileStone))
	{
		return;
	}

	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh || !DamageOverlayMaterial)
	{
		return;
	}

	if (!DamageOverlayMID)
	{
		DamageOverlayMID = UMaterialInstanceDynamic::Create(DamageOverlayMaterial, this);
		if (!DamageOverlayMID)
		{
			return;
		}

		DamageOverlayMID->SetScalarParameterValue(TEXT("BurnAmount"), bIsBurning ? 1.f : 0.f);
		DamageOverlayMID->SetScalarParameterValue(TEXT("OutlineAmount"), 0.f);
		DamageOverlayMID->SetScalarParameterValue(TEXT("ElectricAmount"), 0.f);
		DamageOverlayMID->SetVectorParameterValue(TEXT("BurnColor"), BurnColor);
		DamageOverlayMID->SetScalarParameterValue(TEXT("BurnSpeed"), BurnPulseSpeed);
	}

	if (SkeletalMesh->GetOverlayMaterial() != DamageOverlayMID)
	{
		OverlayMaterialBeforeDamage = SkeletalMesh->GetOverlayMaterial();
		SkeletalMesh->SetOverlayMaterial(DamageOverlayMID);
	}
}

void AEnemyBase::SetBurnVisualActive(const bool bActive)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	InitializeDamageOverlay();
	if (DamageOverlayMID)
	{
		DamageOverlayMID->SetScalarParameterValue(TEXT("BurnAmount"), bActive ? 1.f : 0.f);
		DamageOverlayMID->SetVectorParameterValue(TEXT("BurnColor"), BurnColor);
		DamageOverlayMID->SetScalarParameterValue(TEXT("BurnSpeed"), BurnPulseSpeed);
	}

	if (bActive)
	{
		return;
	}

	if (!AnimInst)
	{
		AnimInst = Cast<UEnemyAnim>(GetMesh() ? GetMesh()->GetAnimInstance() : nullptr);
	}
	if (AnimInst)
	{
		AnimInst->StopBurnReactionMotion();
	}
}

void AEnemyBase::ShowElectricHit()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	InitializeDamageOverlay();
	UWorld* World = GetWorld();
	if (!DamageOverlayMID || !World)
	{
		return;
	}

	DamageOverlayMID->SetScalarParameterValue(TEXT("ElectricAmount"), 1.f);
	World->GetTimerManager().SetTimer(
		ElectricHitTimerHandle,
		this,
		&AEnemyBase::ClearElectricHit,
		FMath::Max(ElectricHitDuration, UE_KINDA_SMALL_NUMBER),
		false
	);
}

void AEnemyBase::ClearElectricHit()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ElectricHitTimerHandle);
	}

	if (DamageOverlayMID)
	{
		DamageOverlayMID->SetScalarParameterValue(TEXT("ElectricAmount"), 0.f);
	}
}

void AEnemyBase::ShowDamageOutline()
{
	if (IsRunningDedicatedServer()
		|| (bStoneVisualActive
			&& (!bApplyDamageOverlayWhileStone || !bShowDamageOutlineWhileStone)))
	{
		return;
	}

	InitializeDamageOverlay();

	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	UWorld* World = GetWorld();
	if (!DamageOverlayMID || !SkeletalMesh || !World)
	{
		return;
	}

	if (!bDamageOutlineActive)
	{
		bRenderCustomDepthBeforeDamageOutline = SkeletalMesh->bRenderCustomDepth;
	}
	bDamageOutlineActive = true;
	SkeletalMesh->SetRenderCustomDepth(true);
	DamageOverlayMID->SetScalarParameterValue(TEXT("OutlineAmount"), 1.f);

	World->GetTimerManager().SetTimer(
		DamageOutlineTimerHandle,
		this,
		&AEnemyBase::ClearDamageOutline,
		FMath::Max(DamageOutlineDuration, UE_KINDA_SMALL_NUMBER),
		false
	);
}

void AEnemyBase::ClearDamageOutline()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageOutlineTimerHandle);
	}

	if (DamageOverlayMID)
	{
		DamageOverlayMID->SetScalarParameterValue(TEXT("OutlineAmount"), 0.f);
	}

	if (bDamageOutlineActive)
	{
		if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
		{
			SkeletalMesh->SetRenderCustomDepth(bRenderCustomDepthBeforeDamageOutline);
		}
	}

	bDamageOutlineActive = false;
	bRenderCustomDepthBeforeDamageOutline = false;
}

void AEnemyBase::UpdateDamageOverlayForStoneState()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (!bStoneVisualActive || bApplyDamageOverlayWhileStone)
	{
		InitializeDamageOverlay();
		return;
	}

	ClearDamageOutline();
	if (USkeletalMeshComponent* SkeletalMesh = GetMesh();
		SkeletalMesh && SkeletalMesh->GetOverlayMaterial() == DamageOverlayMID)
	{
		SkeletalMesh->SetOverlayMaterial(OverlayMaterialBeforeDamage);
	}
}

void AEnemyBase::RestoreDamageOverlay()
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (SkeletalMesh && SkeletalMesh->GetOverlayMaterial() == DamageOverlayMID)
	{
		SkeletalMesh->SetOverlayMaterial(OverlayMaterialBeforeDamage);
	}

	OverlayMaterialBeforeDamage = nullptr;
	DamageOverlayMID = nullptr;
}

