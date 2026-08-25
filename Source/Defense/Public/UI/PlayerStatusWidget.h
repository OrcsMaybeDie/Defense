#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Characters/Player/StatusComponent.h"
#include "Equipment/WeaponData.h"
#include "PlayerStatusWidget.generated.h"

class UWeaponComponent;
class ADefenseCharacter;
class UImage;
class UTexture2D;

UCLASS()
class DEFENSE_API UPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UProgressBar> HPBar;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UProgressBar> MPBar;

	// (hold) 초록 ProgressBar를 추가하면 차지 예정 마나가 부드럽게 증가/감소
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class UProgressBar> MPChargeBar;

	// (시연용) 바인딩된 캐릭터의 Appearance 인덱스에 맞춰 자동으로 교체
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> ProfileImage;
	
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

	void RefreshProfileImage();

	UPROPERTY()
	TObjectPtr<UStatusComponent> BoundStatusComp;

	UPROPERTY()
	TObjectPtr<UWeaponComponent> BoundWeaponComp;

	UPROPERTY()
	TObjectPtr<ADefenseCharacter> BoundCharacter;

	UPROPERTY()
	TObjectPtr<UTexture2D> AppliedProfileTexture;

	EWeaponChargeStage CachedChargeStage = EWeaponChargeStage::None;
	float CachedChargeRatio = 0.f;
	float CachedPreviewManaCost = 0.f;
	float DisplayedChargeRatio = 0.f;
	float TargetChargeRatio = 0.f;
	float DisplayedChargeManaPercent = 0.f;
	float TargetChargeManaPercent = 0.f;
};
