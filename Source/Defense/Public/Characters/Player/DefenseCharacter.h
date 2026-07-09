// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Player/StatusComponent.h"
#include "Characters/Player/WeaponComponent.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "DefenseCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

class ULoadoutComponent;
class UBuildComponent;

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class ADefenseCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStatusComponent> StatusComp;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWeaponComponent> WeaponComp;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ULoadoutComponent> LoadoutComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBuildComponent> BuildComp;

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
	TObjectPtr<UInputAction> IA_LClick;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> IA_RClick;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> IA_Sell;
	
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> IA_LoadoutIdx;

public:
	/** Constructor */
	ADefenseCharacter();	

protected:
	virtual void Tick(float DeltaSeconds) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);
	
	void SelectLoadoutIdx(const FInputActionValue& Value);

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
	void HandleLClickTriggered();

	UFUNCTION(BlueprintCallable, Category="Input")
	void HandleRClick();

	UFUNCTION(BlueprintCallable, Category="Input")
	void FireWeapon();
	
	UFUNCTION(BlueprintCallable, Category="Input")
	void Attack();	
	UFUNCTION(BlueprintCallable, Category="Input")
	void AltAttack();

	UFUNCTION(BlueprintCallable, Category="Input")
	void SellTrap();
	
public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	
	FORCEINLINE class UStatusComponent* GetStatusComp() const { return StatusComp; }
	FORCEINLINE class ULoadoutComponent* GetLoadoutComponent() const { return LoadoutComp; }
	FORCEINLINE class UBuildComponent* GetBuildComp() const { return BuildComp; }
	
	// test
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	
	UFUNCTION(BlueprintImplementableEvent, Category="Weapon")
	void OnAttackAccepted(EWeaponAttackType AttackType);

	UPROPERTY(BlueprintReadOnly, Category="Weapon")
	float TimeSinceFiredWeapon = 999.f;

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void NotifyFireWeapon();
};

