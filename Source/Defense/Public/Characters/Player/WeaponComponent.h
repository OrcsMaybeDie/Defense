#pragma once

#include "CoreMinimal.h"
#include "Characters/Player/StatusComponent.h"
#include "Components/ActorComponent.h"
#include "Equipment/WeaponData.h"
#include "WeaponComponent.generated.h"

class ADefenseCharacter;
class ADefenseWeaponActor;
class UEquipmentData;
class UWeaponData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnWeaponChargePreviewChanged,
	EWeaponChargeStage, ChargeStage,
	float, ChargeRatio,
	float, PreviewManaCost
);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEFENSE_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void RequestFire();

	UFUNCTION(BlueprintPure, Category="Weapon")
	bool HasEquippedWeapon() const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	float GetFireRange() const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	float GetFireAimAssistRadius() const;

	UFUNCTION(BlueprintCallable, Category="Weapon|Charge")
	void BeginCharge();

	UFUNCTION(BlueprintCallable, Category="Weapon|Charge")
	void ReleaseCharge();

	UFUNCTION(BlueprintCallable, Category="Weapon|Charge")
	void CancelCharge();

	UFUNCTION(BlueprintPure, Category="Weapon|Charge")
	bool IsCharging() const { return bChargeInputHeld || bServerChargeActive; }

	UFUNCTION(BlueprintPure, Category="Weapon|Charge")
	float GetChargeRatio() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Charge")
	EWeaponChargeStage GetChargeStage() const;

	UFUNCTION(BlueprintPure, Category="Weapon|Charge")
	float GetChargePreviewManaCost() const;

	UPROPERTY(BlueprintAssignable, Category="Weapon|Charge")
	FOnWeaponChargePreviewChanged OnChargePreviewChanged;

	/** Local-only visual override used while a cinematic camera is active. */
	UFUNCTION(BlueprintCallable, Category="Cinematic")
	void SetCinematicVisualHidden(bool bHidden);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;

	// 무기를 들지 않은 상태에서 사용하는 공통 Anim Layer
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Animation")
	TSubclassOf<UAnimInstance> DefaultAnimLayerClass;

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestFire();

	UFUNCTION(Server, Reliable)
	void ServerRPC_BeginCharge();

	UFUNCTION(Server, Reliable)
	void ServerRPC_ReleaseCharge();

	UFUNCTION(Server, Reliable)
	void ServerRPC_CancelCharge();

	UFUNCTION(Client, Reliable)
	void ClientRPC_RejectCharge();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPC_PlayWeaponAction(
		EWeaponActionType ActionType,
		EWeaponActionPhase Phase,
		float ChargeRatio
	);

	ADefenseCharacter* GetOwnerCharacter() const;
	UWeaponData* GetCurWeaponData() const;
	const FWeaponShotData* GetShotData(
		const UWeaponData* WeaponData,
		EWeaponActionType ActionType,
		float ChargeRatio = 0.f
	) const;
	const FWeaponChargeStageData* GetChargeStageData(
		const FWeaponChargeData& ChargeData,
		EWeaponChargeStage ChargeStage
	) const;
	bool CanLocallyFire() const;
	bool CanLocallyBeginCharge(const FWeaponChargeData& ChargeData) const;
	bool TryExecuteServerFire(
		UWeaponData* WeaponData,
		EWeaponActionType ActionType,
		const FWeaponShotData& ShotData,
		float ChargeRatio,
		float ManaCost
	);
	float CalculateChargeRatio(const FWeaponChargeData& ChargeData, float HeldTime) const;
	float ClampChargeRatioToAvailableMana(
		const FWeaponChargeData& ChargeData,
		float ChargeRatio,
		float AvailableMana
	) const;
	EWeaponChargeStage CalculateChargeStage(const FWeaponChargeData& ChargeData, float ChargeRatio) const;
	float CalculateChargePreviewManaCost(const FWeaponChargeData& ChargeData, float ChargeRatio) const;
	void BroadcastChargePreview();
	void PlayWeaponActionCosmetics(
		EWeaponActionType ActionType,
		EWeaponActionPhase Phase,
		float ChargeRatio
	);
	void ResetLocalChargeState();
	void ResetServerChargeState();
	void RejectServerCharge();
	void RefreshEquippedWeapon();
	void SpawnAndAttachWeaponActor(UWeaponData* WeaponData);
	void ApplyWeaponAnimLayer(UWeaponData* WeaponData);
	void PlayFireAnimation(ADefenseCharacter* OwnerCharacter, const FWeaponFireData& FireData);
	void ApplyWeaponMovementLock(float Duration);
	void ClearWeaponMovementLock();
	FVector PerformHitscan(const FWeaponShotData& ShotData);
	void SpawnStage3StormTornado(const FVector& ShotTargetLocation);

	UFUNCTION()
	void HandleSelectedEquipmentChanged(int32 SelectedSlotIdx, UEquipmentData* SelectedEquipment);

	UFUNCTION()
	void HandleLifeStateChanged(EPlayerLifeState NewLifeState);

	TMap<const UWeaponData*, float> NextFireServerTimes;
	TMap<const UWeaponData*, float> NextChargedFireServerTimes;
	TMap<const UWeaponData*, float> NextFireRequestLocalTimes;
	TMap<const UWeaponData*, float> NextChargedFireLocalTimes;

	bool bChargeInputHeld = false;
	float LocalChargeStartTime = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponData> LocalChargeWeaponData;

	bool bServerChargeActive = false;
	float ServerChargeStartTime = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponData> ServerChargeWeaponData;

	UPROPERTY(Transient)
	TObjectPtr<ADefenseWeaponActor> EquippedWeaponActor;

	UPROPERTY(Transient)
	TSubclassOf<ADefenseWeaponActor> EquippedWeaponActorClass;
	
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> AppliedAnimLayerClass;

	bool bCinematicVisualHidden = false;
	bool bWeaponWasHiddenBeforeCinematic = false;

	FTimerHandle WeaponMovementLockTimer;
};
