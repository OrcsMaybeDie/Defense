// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Player/StatusComponent.h"
#include "Combat/WeaponData.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "DefenseCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class ABuildGridSurface;
class ADefenseArrowProjectile;
class ATrapBase;
class UTrapData;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class ADefenseCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStatusComponent> StatusComp;
	
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> LClickAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> RClickAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> ModeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap")
	TObjectPtr<UTrapData> EquippedTrapData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap|Preview", meta=(ClampMin="1"))
	float TrapPlacementTraceRange = 5000.f;

	UPROPERTY(Transient)
	TObjectPtr<ATrapBase> TrapPreviewActor;

	bool bTrapPlacementMode = false;

public:

	/** Constructor */
	ADefenseCharacter();	

protected:

	virtual void Tick(float DeltaSeconds) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();
	
	UFUNCTION(BlueprintCallable, Category="Input")
	void HandleLClick();

	UFUNCTION(BlueprintCallable, Category="Input")
	void HandleRClick();

	UFUNCTION(BlueprintCallable, Category="Input")
	void Attack();	
	UFUNCTION(BlueprintCallable, Category="Input")
	void AltAttack();

	UFUNCTION(BlueprintCallable, Category="Trap")
	void ToggleTrapPlacementMode();

	UFUNCTION(BlueprintCallable, Category="Trap")
	void PlaceTrap();

	UFUNCTION(BlueprintCallable, Category="Trap")
	void RecoverTrap();
	
public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	
	FORCEINLINE class UStatusComponent* GetStatComp() const { return StatusComp; }
	
	// test
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	
	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestAttack(EWeaponAttackType AttackType);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_PlayAttack(EWeaponAttackType AttackType);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_SpawnArrowVisual(TSubclassOf<ADefenseArrowProjectile> ProjectileClass, FVector SpawnLocation, FRotator SpawnRotation, FVector LaunchVelocity);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_SpawnArrowTrail(FVector SpawnLocation, FRotator SpawnRotation, FVector LaunchVelocity);

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon")
	void OnAttackAccepted(EWeaponAttackType AttackType);

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestPlaceTrap(ABuildGridSurface* BuildSurface, FVector_NetQuantize HitLocation);

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestRecoverTrap(ABuildGridSurface* BuildSurface, FVector_NetQuantize HitLocation);

	void HitscanAttack(const FAttackData& AttackData);

	void ProjectileAttack(const FAttackData& AttackData);

	bool TraceTrapPlacement(FHitResult& OutHit, ABuildGridSurface*& OutBuildSurface) const;
	void UpdateTrapPreview();
	void DestroyTrapPreview();

	float LastAttackServerTime = -BIG_NUMBER;
	float LastAltAttackServerTime = -BIG_NUMBER;

	// test
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UWeaponData> DefaultWeaponData;

};

