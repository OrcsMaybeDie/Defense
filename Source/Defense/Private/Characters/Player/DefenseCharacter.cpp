// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/Player/DefenseCharacter.h"
#include "Collision/DefenseCollisionChannels.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Defense.h"
#include "Characters/Player/StatusComponent.h"
#include "Characters/Player/WeaponComponent.h"
#include "Equipment/LoadoutComponent.h"
#include "Traps/BuildComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameManager/DefenseGameMode.h"

ADefenseCharacter::ADefenseCharacter ()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Collision (Player Capsule & Mesh)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	ApplyFootIKCollisionPolicy();
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false; // 캐릭터가 컨트롤러 회전을 따라가지 않음
	bUseControllerRotationRoll = false;

	// Configure character movement
	
	// 이동 방향이 아니라 카메라 정면을 기준으로 회전
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	
	// 방향을 바꿀 때 회전 속도
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); 

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 450.0f;
	CameraBoom->SocketOffset = FVector(0.f, 85.f, 55.f);
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	
	StatusComp = CreateDefaultSubobject<UStatusComponent>(TEXT("StatusComp"));
	WeaponComp = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComp"));
	LoadoutComp = CreateDefaultSubobject<ULoadoutComponent>(TEXT("LoadoutComp"));
	BuildComp = CreateDefaultSubobject<UBuildComponent>(TEXT("BuildComp"));
}

void ADefenseCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyFootIKCollisionPolicy();
	
	if (!StatusComp) return;
	
	StatusComp->OnLifeStateChanged.AddUniqueDynamic(this, &ADefenseCharacter::HandleLifeStateChanged);
	
	HandleLifeStateChanged(StatusComp->GetLifeState());
}

void ADefenseCharacter::ApplyFootIKCollisionPolicy()
{
	GetCapsuleComponent()->SetCollisionResponseToChannel(DefenseCollisionChannels::FootIK, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(DefenseCollisionChannels::FootIK, ECR_Ignore);
}

void ADefenseCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TimeSinceFiredWeapon += DeltaSeconds; // Lyra?
	
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();

	// CharacterMovement의 입력 결과 재사용
	const bool bHasMoveInput =
		MoveComp && !MoveComp->GetCurrentAcceleration().IsNearlyZero();

	const bool bRecentlyAttacked =
		TimeSinceFiredWeapon <= ViewFollowTime;

	const bool bShouldFaceControlYaw =
		StatusComp
		&& StatusComp->IsAlive()
		&& (bHasMoveInput || bRecentlyAttacked);
	
	// 이동 및 공격 회전은 CharacterMovement가 담당
	MoveComp->bUseControllerDesiredRotation =
		bShouldFaceControlYaw;
}

void ADefenseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADefenseCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ADefenseCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADefenseCharacter::Look);
		
		EnhancedInputComponent->BindAction(IA_LClick, ETriggerEvent::Started, this, &ADefenseCharacter::HandleLClick);
		EnhancedInputComponent->BindAction(IA_LClick, ETriggerEvent::Triggered, this, &ADefenseCharacter::HandleLClickTriggered);

		EnhancedInputComponent->BindAction(IA_RClick, ETriggerEvent::Started, this, &ADefenseCharacter::HandleRClick);
		EnhancedInputComponent->BindAction(IA_RClick, ETriggerEvent::Triggered, this, &ADefenseCharacter::HandleRClick);

		EnhancedInputComponent->BindAction(IA_Sell, ETriggerEvent::Started, this, &ADefenseCharacter::SellTrap);
		
		EnhancedInputComponent->BindAction(IA_LoadoutIdx, ETriggerEvent::Started, this, &ADefenseCharacter::SelectLoadoutIdx);
	}
	else
	{
		UE_LOG(LogDefense, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ADefenseCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ADefenseCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ADefenseCharacter::SelectLoadoutIdx(const FInputActionValue& Value)
{
	if (!StatusComp->IsAlive()) return;

	const float RawInputValue = Value.Get<float>();
	const int32 SlotNumber = FMath::RoundToInt(RawInputValue);
	const int32 SlotIdx = SlotNumber - 1;
	
	// UE_LOG(LogDefense, Log, TEXT("Loadout input | Character=%s Raw=%.2f SlotNumber=%d SlotIdx=%d HasAuthority=%d LocallyControlled=%d"),
	// 	*GetNameSafe(this),
	// 	RawInputValue,
	// 	SlotNumber,
	// 	SlotIdx,
	// 	HasAuthority() ? 1 : 0,
	// 	IsLocallyControlled() ? 1 : 0);
	
	if (!LoadoutComp) return;

	LoadoutComp->SelectSlot(SlotIdx);
}

void ADefenseCharacter::DoMove(float Right, float Forward)
{
	if (!StatusComp->IsAlive()) return;

	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ADefenseCharacter::DoLook(float Yaw, float Pitch)
{
	if (!StatusComp->IsAlive()) return;

	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ADefenseCharacter::DoJumpStart()
{
	if (!StatusComp->IsAlive()) return;

	// signal the character to jump
	Jump();
}

void ADefenseCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void ADefenseCharacter::HandleLClick()
{
	if (!StatusComp->IsAlive()) return;

	if (BuildComp && BuildComp->HasSelectedTrap())
	{
		BuildComp->BuildTrap();
		return;
	}
	
	Attack();
}

void ADefenseCharacter::HandleLClickTriggered()
{
	if (BuildComp && BuildComp->HasSelectedTrap()) return;
	Attack();
}

void ADefenseCharacter::HandleRClick()
{
	if (BuildComp && BuildComp->HasSelectedTrap()) return;

	AltAttack();
}

void ADefenseCharacter::FireWeapon()
{
	Attack();
}

void ADefenseCharacter::Attack()
{
	if (!StatusComp->IsAlive()) return;

	if (WeaponComp)
	{
		WeaponComp->Attack(EWeaponAttackType::Attack);
	}
}

void ADefenseCharacter::NotifyFireWeapon()
{
	TimeSinceFiredWeapon = 0.f;
}

void ADefenseCharacter::AltAttack()
{
	if (!StatusComp->IsAlive()) return;

	if (WeaponComp)
	{
		WeaponComp->Attack(EWeaponAttackType::AltAttack);
	}
}

void ADefenseCharacter::SellTrap()
{
	if (!StatusComp->IsAlive()) return;

	if (BuildComp)
	{
		BuildComp->SellTrap();
	}
}

float ADefenseCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator,
                                    AActor* DamageCauser)
{
	if (!HasAuthority()) { return 0.f; }
	if (!StatusComp) { return 0.f; }
	
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	
	if (ActualDamage <= 0) { return 0.f; }
	
	return StatusComp->ApplyDamage(ActualDamage, DamageCauser);
}

void ADefenseCharacter::HandleLifeStateChanged(EPlayerLifeState NewLifeState)
{
	const bool bAlive = NewLifeState == EPlayerLifeState::Alive;
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	
	if (bAlive)
	{
		if (AnimInstance && DeathMontage)
		{
			AnimInstance->Montage_Stop(0.15f, DeathMontage);
		}

		SetActorEnableCollision(true);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		
		// test 확인 후 제거
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();

		// Capsule, Mesh 등 이 Actor가 가진 충돌을 모두 끔
		SetActorEnableCollision(false);
		// test 확인 후 제거
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		
		// death montage
		if (AnimInstance && DeathMontage)
		{
			AnimInstance->Montage_Play(DeathMontage);
		}
		
		// alert game mode
		if (HasAuthority())
		{
			if (ADefenseGameMode* GameMode = GetWorld()->GetAuthGameMode<ADefenseGameMode>())
			{
				GameMode->NotifyPlayerDied(this);
			}
		}
	}
	
	// 이동/시점 입력은 소유 클라이언트에서 차단
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (PlayerController->IsLocalController())
		{
			PlayerController->SetIgnoreMoveInput(!bAlive);
			PlayerController->SetIgnoreLookInput(!bAlive);
		}
	}
}
