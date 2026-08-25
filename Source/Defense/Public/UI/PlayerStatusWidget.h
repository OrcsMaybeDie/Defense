#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Characters/Player/StatusComponent.h"
#include "Equipment/WeaponData.h"
#include "PlayerStatusWidget.generated.h"

class UWeaponComponent;

/**
 * 
 */
UCLASS()
class DEFENSE_API UPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UProgressBar> HPBar;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UProgressBar> MPBar;

	// 같은 위치에 초록 ProgressBar를 추가하면 차지 예정 마나가 부드럽게 증가/감소한다.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class UProgressBar> MPChargeBar;
	
	void BindStatusComp(UStatusComponent* InStatComp);
	
	UFUNCTION()
	void HandleHealthChanged(float CurValue, float MaxValue);
	
	UFUNCTION()
	void HandleManaChanged(float CurValue, float MaxValue);

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon|Charge")
	void OnChargeManaPreviewChanged(
		EWeaponChargeStage ChargeStage,
		float ChargeRatio,
		float PreviewManaCost,
		float CurrentMana,
		float MaxMana
	);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Charge", meta=(ClampMin="0"))
	float ChargePreviewInterpSpeed = 12.f;

private:
	UFUNCTION()
	void HandleChargePreviewChanged(
		EWeaponChargeStage ChargeStage,
		float ChargeRatio,
		float PreviewManaCost
	);

	UPROPERTY()
	TObjectPtr<UStatusComponent> BoundStatusComp;

	UPROPERTY()
	TObjectPtr<UWeaponComponent> BoundWeaponComp;

	EWeaponChargeStage CachedChargeStage = EWeaponChargeStage::None;
	float CachedChargeRatio = 0.f;
	float CachedPreviewManaCost = 0.f;
	float DisplayedChargeRatio = 0.f;
	float TargetChargeRatio = 0.f;
	float DisplayedChargeManaPercent = 0.f;
	float TargetChargeManaPercent = 0.f;
};
