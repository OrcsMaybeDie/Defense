// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/Player/DefenseCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Defense.h"
#include "Animation/AnimInstance.h"
#include "Characters/Player/StatusComponent.h"
#include "Combat/DefenseArrowProjectile.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Traps/BuildGridSurface.h"
#include "Traps/TrapBase.h"
#include "Traps/TrapData.h"

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
		
		if (LClickAction)
		{
			EnhancedInputComponent->BindAction(LClickAction, ETriggerEvent::Started, this, &ADefenseCharacter::HandleLClick);
		}

		if (RClickAction)
		{
			EnhancedInputComponent->BindAction(RClickAction, ETriggerEvent::Started, this, &ADefenseCharacter::HandleRClick);
		}

		if (ModeAction)
		{
			EnhancedInputComponent->BindAction(ModeAction, ETriggerEvent::Started, this, &ADefenseCharacter::ToggleTrapPlacementMode);
		}
	}
	else
	{
		UE_LOG(LogDefense, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ADefenseCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled() && bTrapPlacementMode)
	{
		UpdateTrapPreview();
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

void ADefenseCharacter::HandleLClick()
{
	if (bTrapPlacementMode)
	{
		PlaceTrap();
		return;
	}

	Attack();
}

void ADefenseCharacter::HandleRClick()
{
	if (bTrapPlacementMode)
	{
		RecoverTrap();
		return;
	}

	AltAttack();
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

void ADefenseCharacter::ToggleTrapPlacementMode()
{
	bTrapPlacementMode = !bTrapPlacementMode;

	if (!bTrapPlacementMode)
	{
		DestroyTrapPreview();
	}
}

void ADefenseCharacter::PlaceTrap()
{
	if (!bTrapPlacementMode || !EquippedTrapData) return;

	FHitResult Hit;
	ABuildGridSurface* BuildSurface = nullptr;
	if (!TraceTrapPlacement(Hit, BuildSurface) || !BuildSurface)
	{
		return;
	}

	const bool bCanPlace = BuildSurface->CanPlaceTrapAt(Hit.ImpactPoint);
	if (!bCanPlace)
	{
		return;
	}

	BuildSurface->MarkSlotOccupiedLocally(Hit.ImpactPoint);
	if (TrapPreviewActor)
	{
		TrapPreviewActor->SetActorHiddenInGame(true);
	}

	ServerRPC_RequestPlaceTrap(BuildSurface, Hit.ImpactPoint);
}

void ADefenseCharacter::RecoverTrap()
{
	if (!bTrapPlacementMode) return;

	FHitResult Hit;
	ABuildGridSurface* BuildSurface = nullptr;
	if (!TraceTrapPlacement(Hit, BuildSurface) || !BuildSurface)
	{
		return;
	}

	BuildSurface->MarkSlotFreeLocally(Hit.ImpactPoint);
	ServerRPC_RequestRecoverTrap(BuildSurface, Hit.ImpactPoint);
}

float ADefenseCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator,
	AActor* DamageCauser)
{
	if (!HasAuthority()) { return 0.f; }
	if (!StatusComp) { return 0.f; }
	if (bIsDead) { return 0.f; }
	
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	
	if (ActualDamage <= 0) { return 0.f; }
	
	const float AppliedDamage = StatusComp->ApplyDamage(ActualDamage, DamageCauser);
	if (AppliedDamage > 0.f && StatusComp->Health <= 0.f)
	{
		bIsDead = true;
		MulticastRPC_PlayDeath();
	}

	return AppliedDamage;
}

void ADefenseCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADefenseCharacter, bIsDead);
}

void ADefenseCharacter::MulticastRPC_PlayDeath_Implementation()
{
	bIsDead = true;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->DisableMovement();
	}

	if (!DeathMontage || !GetMesh())
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Play(DeathMontage);
	}
}

void ADefenseCharacter::ServerRPC_RequestAttack_Implementation(EWeaponAttackType AttackType)
{
	if (!HasAuthority()) return;
	if (bIsDead) return;
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
	MulticastRPC_PlayAttack(AttackType);
	
	if (AttackData->ProjectileClass || AttackData->Delivery == EAttackDelivery::Projectile)
	{
		ProjectileAttack(*AttackData);
		return;
	}

	switch (AttackData->Delivery)
	{
	case EAttackDelivery::Hitscan:
		HitscanAttack(*AttackData);
		break;

	case EAttackDelivery::Projectile:
		ProjectileAttack(*AttackData);
		break;

	case EAttackDelivery::None:
		// 이동스킬/직접 발동형
		break;

	default:
		ensureMsgf(false, TEXT("Unhandled AttackDelivery"));
		break;
	}
}

void ADefenseCharacter::MulticastRPC_PlayAttack_Implementation(EWeaponAttackType AttackType)
{
	const FAttackData* AttackData = nullptr;

	switch (AttackType)
	{
	case EWeaponAttackType::Attack:
		AttackData = DefaultWeaponData ? &DefaultWeaponData->Attack : nullptr;
		break;

	case EWeaponAttackType::AltAttack:
		AttackData = DefaultWeaponData ? &DefaultWeaponData->AltAttack : nullptr;
		break;

	default:
		break;
	}

	if (AttackData && AttackData->Animation && GetMesh())
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(
				AttackData->Animation,
				AttackData->AnimationSlotName
			);
		}
	}

	OnAttackAccepted(AttackType);
}

void ADefenseCharacter::MulticastRPC_SpawnArrowVisual_Implementation(TSubclassOf<ADefenseArrowProjectile> ProjectileClass, FVector SpawnLocation, FRotator SpawnRotation, FVector LaunchVelocity)
{
	if (GetNetMode() == NM_DedicatedServer || !GetWorld() || !ProjectileClass || LaunchVelocity.IsNearlyZero())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADefenseArrowProjectile* VisualProjectile = GetWorld()->SpawnActor<ADefenseArrowProjectile>(
		ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	if (!VisualProjectile)
	{
		return;
	}

	VisualProjectile->SetCosmeticOnly(true);
	VisualProjectile->SetDebugTrailEnabled(false);
	VisualProjectile->IgnoreActor(this);

	TArray<AActor*> AttachedActorsToIgnore;
	GetAttachedActors(AttachedActorsToIgnore);
	for (AActor* AttachedActor : AttachedActorsToIgnore)
	{
		VisualProjectile->IgnoreActor(AttachedActor);
	}

	VisualProjectile->Launch(LaunchVelocity, 0.f);
}

void ADefenseCharacter::MulticastRPC_SpawnArrowTrail_Implementation(FVector SpawnLocation, FRotator SpawnRotation, FVector LaunchVelocity)
{
	if (GetNetMode() == NM_DedicatedServer || !GetWorld() || LaunchVelocity.IsNearlyZero())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADefenseArrowProjectile* TrailProjectile = GetWorld()->SpawnActor<ADefenseArrowProjectile>(
		ADefenseArrowProjectile::StaticClass(),
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	if (!TrailProjectile)
	{
		return;
	}

	TrailProjectile->SetCosmeticOnly(true);
	TrailProjectile->SetDebugTrailEnabled(true);
	TrailProjectile->IgnoreActor(this);

	TArray<AActor*> AttachedActorsToIgnore;
	GetAttachedActors(AttachedActorsToIgnore);
	for (AActor* AttachedActor : AttachedActorsToIgnore)
	{
		TrailProjectile->IgnoreActor(AttachedActor);
	}

	TrailProjectile->Launch(LaunchVelocity, 0.f);
}

void ADefenseCharacter::ServerRPC_RequestPlaceTrap_Implementation(ABuildGridSurface* BuildSurface, FVector_NetQuantize HitLocation)
{
	if (!HasAuthority() || !BuildSurface || !EquippedTrapData) return;

	BuildSurface->TryPlaceTrap(EquippedTrapData, HitLocation, GetController());
}

void ADefenseCharacter::ServerRPC_RequestRecoverTrap_Implementation(ABuildGridSurface* BuildSurface, FVector_NetQuantize HitLocation)
{
	if (!HasAuthority() || !BuildSurface) return;

	BuildSurface->TryRemoveTrap(HitLocation);
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

void ADefenseCharacter::ProjectileAttack(const FAttackData& AttackData)
{
	if (!GetWorld())
	{
		UE_LOG(LogDefense, Warning, TEXT("ProjectileAttack failed: World is missing."));
		return;
	}

	AController* OwningController = GetController();
	if (!OwningController)
	{
		UE_LOG(LogDefense, Warning, TEXT("ProjectileAttack failed: Controller is missing."));
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	OwningController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector ViewForward = ViewRotation.Vector();
	FVector SpawnLocation = ViewLocation;
	FName UsedSocketName = NAME_None;

	FName SpawnSocketName = AttackData.ProjectileSpawnSocketName;
	if (SpawnSocketName.IsNone())
	{
		SpawnSocketName = TEXT("Arrow");
	}

	auto TryUseSocket = [&SpawnLocation, &UsedSocketName](const UMeshComponent* MeshComponent, FName SocketName)
	{
		if (MeshComponent && MeshComponent->DoesSocketExist(SocketName))
		{
			SpawnLocation = MeshComponent->GetSocketLocation(SocketName);
			UsedSocketName = SocketName;
			return true;
		}

		return false;
	};

	if (GetMesh())
	{
		TryUseSocket(GetMesh(), SpawnSocketName)
			|| TryUseSocket(GetMesh(), TEXT("arrow"))
			|| TryUseSocket(GetMesh(), TEXT("bow"));
	}

	if (UsedSocketName.IsNone())
	{
		TArray<AActor*> AttachedActors;
		GetAttachedActors(AttachedActors);

		for (AActor* AttachedActor : AttachedActors)
		{
			if (!AttachedActor) continue;

			TArray<UMeshComponent*> AttachedMeshComponents;
			AttachedActor->GetComponents<UMeshComponent>(AttachedMeshComponents);

			for (UMeshComponent* AttachedMeshComponent : AttachedMeshComponents)
			{
				if (TryUseSocket(AttachedMeshComponent, SpawnSocketName)
					|| TryUseSocket(AttachedMeshComponent, TEXT("arrow"))
					|| TryUseSocket(AttachedMeshComponent, TEXT("Arrow")))
				{
					break;
				}
			}

			if (!UsedSocketName.IsNone())
			{
				break;
			}
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FVector AimTarget = ViewLocation + ViewForward * AttackData.Range;
	FCollisionQueryParams AimParams(SCENE_QUERY_STAT(DefenseProjectileAim), false, this);
	AimParams.AddIgnoredActor(this);

	TArray<AActor*> AttachedActorsToIgnore;
	GetAttachedActors(AttachedActorsToIgnore);
	for (AActor* AttachedActor : AttachedActorsToIgnore)
	{
		if (AttachedActor)
		{
			AimParams.AddIgnoredActor(AttachedActor);
		}
	}

	FHitResult AimHit;
	if (GetWorld()->LineTraceSingleByChannel(AimHit, ViewLocation, AimTarget, ECC_Visibility, AimParams))
	{
		AimTarget = AimHit.ImpactPoint;
	}

	FVector LaunchDirection = AimTarget - SpawnLocation;
	if (!LaunchDirection.Normalize())
	{
		LaunchDirection = ViewForward;
	}

	const FRotator LaunchRotation = LaunchDirection.Rotation();
	const FVector LaunchVelocity = LaunchDirection * AttackData.ProjectileSpeed;
	TSubclassOf<ADefenseArrowProjectile> ProjectileClass = AttackData.ProjectileClass;
	if (!ProjectileClass)
	{
		ProjectileClass = ADefenseArrowProjectile::StaticClass();
	}

	SetActorRotation(FRotator(0.f, LaunchRotation.Yaw, 0.f));

	ADefenseArrowProjectile* Projectile = GetWorld()->SpawnActor<ADefenseArrowProjectile>(
		ProjectileClass,
		SpawnLocation,
		LaunchRotation,
		SpawnParams
	);

	if (!Projectile)
	{
		UE_LOG(LogDefense, Warning, TEXT("ProjectileAttack failed: native arrow SpawnActor returned null."));
		return;
	}

	Projectile->SetActorHiddenInGame(true);
	Projectile->SetDebugTrailEnabled(false);
	Projectile->IgnoreActor(this);

	for (AActor* AttachedActor : AttachedActorsToIgnore)
	{
		Projectile->IgnoreActor(AttachedActor);
	}

	TArray<UPrimitiveComponent*> ProjectilePrimitiveComponents;
	Projectile->GetComponents<UPrimitiveComponent>(ProjectilePrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : ProjectilePrimitiveComponents)
	{
		if (PrimitiveComponent)
		{
			PrimitiveComponent->IgnoreActorWhenMoving(this, true);
		}
	}

	GetCapsuleComponent()->IgnoreActorWhenMoving(Projectile, true);
	if (GetMesh())
	{
		GetMesh()->IgnoreActorWhenMoving(Projectile, true);
	}

	Projectile->Launch(LaunchVelocity, AttackData.Damage);
	MulticastRPC_SpawnArrowVisual(ProjectileClass, SpawnLocation, LaunchRotation, LaunchVelocity);
	MulticastRPC_SpawnArrowTrail(SpawnLocation, LaunchRotation, LaunchVelocity);
}

bool ADefenseCharacter::TraceTrapPlacement(FHitResult& OutHit, ABuildGridSurface*& OutBuildSurface) const
{
	OutBuildSurface = nullptr;

	AController* OwningController = GetController();
	if (!OwningController || !GetWorld()) return false;

	FVector ViewLocation;
	FRotator ViewRotation;
	OwningController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector Start = ViewLocation;
	const FVector End = Start + ViewRotation.Vector() * TrapPlacementTraceRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TrapPlacementTrace), false, this);
	Params.AddIgnoredActor(this);
	if (TrapPreviewActor)
	{
		Params.AddIgnoredActor(TrapPreviewActor);
	}

	const bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
	if (!bHit) return false;

	OutBuildSurface = Cast<ABuildGridSurface>(OutHit.GetActor());
	return true;
}

void ADefenseCharacter::UpdateTrapPreview()
{
	if (!EquippedTrapData || !EquippedTrapData->TrapClass || !GetWorld())
	{
		DestroyTrapPreview();
		return;
	}

	if (!TrapPreviewActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		TrapPreviewActor = GetWorld()->SpawnActor<ATrapBase>(
			EquippedTrapData->TrapClass,
			GetActorLocation(),
			FRotator::ZeroRotator,
			SpawnParams
		);

		if (TrapPreviewActor)
		{
			TrapPreviewActor->SetReplicates(false);
			TrapPreviewActor->SetPreviewMode(true);
			TrapPreviewActor->SetActorHiddenInGame(true);
		}
	}

	if (!TrapPreviewActor) return;

	FHitResult Hit;
	ABuildGridSurface* BuildSurface = nullptr;
	if (!TraceTrapPlacement(Hit, BuildSurface))
	{
		if (!TrapPreviewActor->IsHidden())
		{
			TrapPreviewActor->SetActorHiddenInGame(true);
		}
		return;
	}

	if (!BuildSurface)
	{
		if (!TrapPreviewActor->IsHidden())
		{
			TrapPreviewActor->SetActorHiddenInGame(true);
		}
		return;
	}

	FVector PreviewLocation = Hit.ImpactPoint;
	const bool bCanPlace = BuildSurface->CanPlaceTrapAt(Hit.ImpactPoint, nullptr, &PreviewLocation);
	if (!bCanPlace)
	{
		if (!TrapPreviewActor->IsHidden())
		{
			TrapPreviewActor->SetActorHiddenInGame(true);
		}
		return;
	}

	if (TrapPreviewActor->IsHidden())
	{
		TrapPreviewActor->SetActorHiddenInGame(false);
	}

	TrapPreviewActor->SetActorLocation(PreviewLocation);
	TrapPreviewActor->SetActorRotation(BuildSurface->GetActorRotation());
}

void ADefenseCharacter::DestroyTrapPreview()
{
	if (TrapPreviewActor)
	{
		TrapPreviewActor->Destroy();
		TrapPreviewActor = nullptr;
	}
}
