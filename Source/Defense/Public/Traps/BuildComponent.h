#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Traps/Grid/TrapGridTypes.h"
#include "BuildComponent.generated.h"

class ATrapBase;
class AGridManager;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UEquipmentData;
class UTrapData;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEFENSE_API UBuildComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category="Build")
	bool HasSelectedTrap() const { return GetSelectedTrapData() != nullptr; }

	UFUNCTION(BlueprintCallable, Category="Build")
	void BuildTrap();

	UFUNCTION(BlueprintCallable, Category="Build")
	void SellTrap();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap|Build", meta=(ClampMin="1"))
	float BuildTraceRange = 5000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap|Animation")
	TObjectPtr<UAnimSequenceBase> TrapBuildAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap|Animation")
	FName TrapBuildAnimationSlot = TEXT("UpperBodySlot");

	UPROPERTY(Transient)
	TObjectPtr<ATrapBase> TrapPreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<AGridManager> CachedGridManager;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveTrapBuildMontage;

	TMap<TWeakObjectPtr<UAnimInstance>, float> TrapUpperBodyWeightOverrides;

	bool bTrapBuildAnimationApplied = false;
	FTimerHandle TrapBuildAnimationRefreshTimer;

	UTrapData* GetSelectedTrapData() const;
	APawn* GetOwnerPawn() const;

	bool TraceBuildTarget(FHitResult& OutHit) const;
	bool TraceSellTarget(FHitResult& OutHit) const;
	AGridManager* FindGridManager();
	void UpdateTrapPreview();
	void DestroyTrapPreview();
	void RefreshTrapBuildAnimation();
	void ApplyTrapBuildAnimation(bool bEnable);
	void ApplyTrapUpperBodyWeight(bool bEnable);

	UFUNCTION()
	void HandleSelectedEquipmentChanged(int32 SelectedSlotIdx, UEquipmentData* SelectedEquipment);

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestBuildTrap(const FTrapCellKey& AnchorCell);

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestSellTrap(ATrapBase* Trap);
};
