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
#include "Kismet/GameplayStatics.h"

ADefenseCharacter::ADefenseCharacter ()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
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
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	
	StatusComp = CreateDefaultSubobject<UStatusComponent>(TEXT("StatusComp"));
	
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
		
		// Attack
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ADefenseCharacter::Attack);
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

void ADefenseCharacter::Attack()
{
	if (!DefaultWeaponData) return;
	ServerRPC_RequestAttack(EWeaponAttackType::Attack);
}

void ADefenseCharacter::AltAttack()
{
	if (!DefaultWeaponData) return;
	ServerRPC_RequestAttack(EWeaponAttackType::AltAttack);
}

void ADefenseCharacter::ServerRPC_RequestAttack_Implementation(EWeaponAttackType AttackType)
{
	if (!HasAuthority()) return;
	if (!DefaultWeaponData) return;

	const FAttackData* AttackData = nullptr;
	float* LastAttackTime = nullptr;

	switch (AttackType)
	{
	case EWeaponAttackType::Attack:
		AttackData = &DefaultWeaponData->Attack;
		LastAttackTime = &LastAttackServerTime;
		break;

	case EWeaponAttackType::AltAttack:
		AttackData = &DefaultWeaponData->AltAttack;
		LastAttackTime = &LastAltAttackServerTime;
		break;

	default:
		ensureMsgf(false, TEXT("Unhandled WeaponAttackType"));
		return;
	}

	if (!AttackData || !LastAttackTime) return;

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - *LastAttackTime < AttackData->Cooldown)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Attack rejected by server cooldown. Remaining: %.2f"),
			AttackData->Cooldown - (CurrentTime - *LastAttackTime));
		return;
	}

	if (StatusComp && !StatusComp->TrySpendMana(AttackData->ManaCost))
	{
		UE_LOG(LogTemp, Verbose, TEXT("Attack rejected by server mana. Cost: %.1f / Mana: %.1f"),
			AttackData->ManaCost,
			StatusComp->Mana);
		return;
	}

	*LastAttackTime = CurrentTime;
	
	switch (AttackData->Delivery)
	{
	case EAttackDelivery::Hitscan:
		HitscanAttack(*AttackData);
		break;

	case EAttackDelivery::Projectile:
		break;

	case EAttackDelivery::None:
		// 이동스킬/직접 발동형
		break;

	default:
		ensureMsgf(false, TEXT("Unhandled AttackDelivery"));
		break;
	}
}

void ADefenseCharacter::HitscanAttack(const FAttackData& AttackData)
{
	AController* OwningController = GetController();
	if (!OwningController) return;

	FVector ViewLocation;
	FRotator ViewRotation;
	
	// 시점 위치/회전 채움
	OwningController->GetPlayerViewPoint(ViewLocation, ViewRotation); 
	
	// Trace Range
	const FVector Start = ViewLocation;
	const FVector End = Start + ViewRotation.Vector() * AttackData.Range;
	
	const float TraceRadius = FMath::Max(AttackData.Radius, 1.f);
	
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HitscanAttack), false, this);
	Params.AddIgnoredActor(this);
	
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(TraceRadius),
		Params
	);
	
#if ENABLE_DRAW_DEBUG
	const FColor DebugColor = bHit ? FColor::Red : FColor::Green;
	DrawDebugLine(GetWorld(), Start, End, DebugColor, false, 1.0f, 0, 1.0f);
	DrawDebugSphere(GetWorld(), bHit ? Hit.ImpactPoint : End, TraceRadius, 16, DebugColor, false, 1.0f);
#endif

	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();

		UE_LOG(LogTemp, Warning, TEXT("SphereTrace Hit: %s / Damage: %.1f"),
			*GetNameSafe(HitActor),
			AttackData.Damage);

		if (Cast<ADefenseCharacter>(HitActor)) return;
		
		UGameplayStatics::ApplyDamage(
		   HitActor,
		   AttackData.Damage,
		   GetController(),
		   this,
		   UDamageType::StaticClass()
	   );
	}
}
