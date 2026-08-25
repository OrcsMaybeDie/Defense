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
class UAnimMontage; // Death
class UAnimSequenceBase;
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
	
	/** 공격 후 캐릭터가 카메라 방향을 따라가는 시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin="0.0"))
	float ViewFollowTime = 2.f;
	
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> IA_LClick;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> IA_RClick;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> IA_Sell;
	
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> IA_LoadoutIdx;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequenceBase> GameClearAnimation;
	
public:
	/** Constructor */
	ADefenseCharacter();	

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	void ApplyFootIKCollisionPolicy();

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);
	
	UFUNCTION()
	void HandleLifeStateChanged(EPlayerLifeState NewLifeState);
	
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
	void HandleFireStarted();

	UFUNCTION(BlueprintCallable, Category="Input")
	void HandleFireTriggered();

	UFUNCTION(BlueprintCallable, Category="Input")
	void HandleChargeStarted();

	UFUNCTION(BlueprintCallable, Category="Input")
	void HandleChargeCompleted();

	UFUNCTION(BlueprintCallable, Category="Input")
	void HandleChargeCanceled();

	UFUNCTION(BlueprintCallable, Category="Input")
	void CancelWeaponCharge();

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
	
	// 애니메이션, 카메라, 사운드 등 캐릭터 쪽 표현을 Blueprint에서 연결한다.
	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Presentation")
	void OnWeaponAction(
		EWeaponActionType ActionType,
		EWeaponActionPhase Phase,
		EWeaponChargeStage ChargeStage,
		float ChargeRatio
	);

	UPROPERTY(BlueprintReadOnly, Category="Weapon")
	float TimeSinceFiredWeapon = 999.f;

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void NotifyWeaponFired();

	// 서버에서 확정된 게임 결과 모션을 모든 클라이언트에 재생
	void PlayGameEndMotion(bool bGameClear);

	/** Stage 공격의 후딜 동안 캐릭터 행동 입력을 잠근다. 카메라 조작은 유지한다. */
	void SetWeaponMovementLocked(bool bLocked);
	bool IsWeaponMovementLocked() const { return bWeaponMovementLocked; }

	/** Hides only this machine's character and locally spawned weapon visuals. */
	UFUNCTION(BlueprintCallable, Category="Cinematic")
	void SetCinematicVisualHidden(bool bShouldHide);

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_PlayGameEndMotion(bool bGameClear);

	bool bWeaponMovementLocked = false;
	bool bCinematicVisualHidden = false;
	bool bMeshWasHiddenBeforeCinematic = false;
};

