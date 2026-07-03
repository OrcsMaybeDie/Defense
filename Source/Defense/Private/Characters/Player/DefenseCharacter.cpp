// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/Player/DefenseCharacter.h"
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

ADefenseCharacter::ADefenseCharacter ()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false;
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
		
		EnhancedInputComponent->BindAction(LClickAction, ETriggerEvent::Started, this, &ADefenseCharacter::HandleLClick);

		EnhancedInputComponent->BindAction(RClickAction, ETriggerEvent::Started, this, &ADefenseCharacter::HandleRClick);

		EnhancedInputComponent->BindAction(ModeAction, ETriggerEvent::Started, this, &ADefenseCharacter::ToggleBuildMode);

		EnhancedInputComponent->BindAction(SellAction, ETriggerEvent::Started, this, &ADefenseCharacter::SellTrap);
	}
	else
	{
		UE_LOG(LogDefense, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ADefenseCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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

void ADefenseCharacter::DoMove(float Right, float Forward)
{
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
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ADefenseCharacter::DoJumpStart()
{
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
	if (BuildComp && BuildComp->IsBuildMode())
	{
		BuildComp->BuildTrap();
		return;
	}

	Attack();
}

void ADefenseCharacter::HandleRClick()
{
	if (BuildComp->IsBuildMode()) return;

	AltAttack();
}

void ADefenseCharacter::Attack()
{
	if (WeaponComp)
	{
		WeaponComp->Attack(EWeaponAttackType::Attack);
	}
}

void ADefenseCharacter::AltAttack()
{
	if (WeaponComp)
	{
		WeaponComp->Attack(EWeaponAttackType::AltAttack);
	}
}

void ADefenseCharacter::ToggleBuildMode()
{
	if (BuildComp)
	{
		BuildComp->ToggleBuildMode();
	}
}

void ADefenseCharacter::BuildTrap()
{
	if (BuildComp)
	{
		BuildComp->BuildTrap();
	}
}

void ADefenseCharacter::SellTrap()
{
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
