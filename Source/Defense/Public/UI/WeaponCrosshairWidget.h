#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "WeaponCrosshairWidget.generated.h"

class ADefenseCharacter;
class UWeaponComponent;

/**
 * WarZ 크로스헤어의 조준 대상 표시와 발사 피드백을 Defense 무기 구조에 맞게 옮긴 네이티브 위젯.
 * 별도 Widget Blueprint나 텍스처가 없어도 즉시 표시되며, 필요하면 이 클래스를 부모로 한 BP로 색/크기를 조정한다.
 */
UCLASS()
class DEFENSE_API UWeaponCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWeaponCrosshairWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled
	) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Shape", meta=(ClampMin="0"))
	float CenterGap = 14.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Shape", meta=(ClampMin="0"))
	float ArmLength = 7.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Shape", meta=(ClampMin="0.5"))
	float LineThickness = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Aim")
	FSlateBrush AimCrosshairBrush;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Aim", meta=(ClampMin="0"))
	float AimCrosshairRadius = 28.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Feedback", meta=(ClampMin="0.01", Units="s"))
	float AimFlashDuration = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Feedback", meta=(ClampMin="0.01", Units="s"))
	float FireFeedbackDuration = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Feedback", meta=(ClampMin="0"))
	float FireStartGap = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Feedback", meta=(ClampMin="0"))
	float FireEndGap = 36.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Feedback")
	FSlateBrush FireCrosshairBrush;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Feedback")
	FSlateBrush FireEmphasisBrush;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Feedback", meta=(ClampMin="0"))
	float FireEmphasisSizeOffset = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Feedback", meta=(ClampMin="0", ClampMax="1"))
	float FireEmphasisOpacity = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Charge", meta=(ClampMin="0"))
	float ChargeRingRadius = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Charge", meta=(ClampMin="8", ClampMax="128"))
	int32 ChargeRingSegments = 40;

	// 차지 중 현재 확정된 Stage1/2/3를 크로스헤어 아래의 세 칸으로 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Charge", meta=(ClampMin="0"))
	float ChargeStageIndicatorOffset = 42.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Charge", meta=(ClampMin="1"))
	float ChargeStageIndicatorWidth = 7.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Charge", meta=(ClampMin="0"))
	float ChargeStageIndicatorSpacing = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Charge", meta=(ClampMin="1"))
	float ChargeStageIndicatorThickness = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Color")
	FLinearColor IdleColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Color")
	FLinearColor DamageableTargetColor = FLinearColor(1.f, 0.12f, 0.08f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crosshair|Color")
	FLinearColor ChargeColor = FLinearColor(0.15f, 0.8f, 1.f, 1.f);

private:
	void RefreshTargetState();

	UPROPERTY(Transient)
	TObjectPtr<ADefenseCharacter> OwningCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> WeaponComponent;

	bool bDamageableTarget = false;
};
