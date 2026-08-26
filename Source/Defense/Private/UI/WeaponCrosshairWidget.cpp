#include "UI/WeaponCrosshairWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Characters/Player/WeaponComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Traps/BuildComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void DrawCrosshairLine(
		FSlateWindowElementList& OutDrawElements,
		const FGeometry& Geometry,
		int32 LayerId,
		const FVector2f& Start,
		const FVector2f& End,
		const FLinearColor& Color,
		float Thickness
	)
	{
		TArray<FVector2f> Points;
		Points.Reserve(2);
		Points.Add(Start);
		Points.Add(End);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			Color,
			true,
			Thickness
		);
	}

	bool DrawFourWayMarker(
		FSlateWindowElementList& OutDrawElements,
		const FGeometry& Geometry,
		int32 LayerId,
		const FSlateBrush& Brush,
		float Radius,
		const FLinearColor& Color
	)
	{
		if (Color.A <= KINDA_SMALL_NUMBER
			|| Brush.GetDrawType() == ESlateBrushDrawType::NoDrawType
			|| Brush.GetResourceObject() == nullptr)
		{
			return false;
		}

		const FVector2D Center = Geometry.GetLocalSize() * 0.5f;
		const FVector2D ImageSize(Brush.ImageSize.X, Brush.ImageSize.Y);
		if (ImageSize.X <= 0.f || ImageSize.Y <= 0.f)
		{
			return false;
		}

		constexpr float MarkerAngles[4] = { 0.f, 90.f, 180.f, 270.f };
		for (const float AngleDegrees : MarkerAngles)
		{
			const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
			const FVector2D Offset(
				Radius * FMath::Sin(AngleRadians),
				-Radius * FMath::Cos(AngleRadians)
			);
			const FVector2D DrawPosition = Center - ImageSize * 0.5f + Offset;

			FSlateDrawElement::MakeRotatedBox(
				OutDrawElements,
				LayerId,
				Geometry.ToPaintGeometry(ImageSize, FSlateLayoutTransform(DrawPosition)),
				&Brush,
				ESlateDrawEffect::None,
				AngleRadians,
				TOptional<FVector2f>(),
				FSlateDrawElement::RelativeToElement,
				Color
			);
		}

		return true;
	}

	bool DrawCenterMarker(
		FSlateWindowElementList& OutDrawElements,
		const FGeometry& Geometry,
		int32 LayerId,
		const FSlateBrush& Brush,
		float SizeOffset,
		const FLinearColor& Color
	)
	{
		if (Color.A <= KINDA_SMALL_NUMBER
			|| Brush.GetDrawType() == ESlateBrushDrawType::NoDrawType
			|| Brush.GetResourceObject() == nullptr)
		{
			return false;
		}

		const FVector2D Center = Geometry.GetLocalSize() * 0.5f;
		const FVector2D BaseSize(Brush.ImageSize.X, Brush.ImageSize.Y);
		if (BaseSize.X <= 0.f || BaseSize.Y <= 0.f)
		{
			return false;
		}

		const FVector2D DrawSize = BaseSize + FVector2D(SizeOffset, SizeOffset);
		const FVector2D DrawPosition = Center - DrawSize * 0.5f;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(DrawSize, FSlateLayoutTransform(DrawPosition)),
			&Brush,
			ESlateDrawEffect::None,
			Color
		);

		return true;
	}
}

UWeaponCrosshairWidget::UWeaponCrosshairWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AimMaterial(
		TEXT("/Game/UI/Hud/Art/MI_UI_Reticles_CrossHair_Rifle.MI_UI_Reticles_CrossHair_Rifle")
	);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FireMaterial(
		TEXT("/Game/UI/Hud/Art/MI_UI_Reticles_EliminationMarker.MI_UI_Reticles_EliminationMarker")
	);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FireEmphasisMaterial(
		TEXT("/Game/UI/Hud/Art/MI_UI_Reticles_HitMarker.MI_UI_Reticles_HitMarker")
	);

	auto ConfigureBrush = [](FSlateBrush& Brush, UMaterialInterface* Material)
	{
		if (!Material) return;

		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetImageSize(FVector2D(32.f, 32.f));
		Brush.SetResourceObject(Material);
	};

	ConfigureBrush(AimCrosshairBrush, AimMaterial.Object);
	ConfigureBrush(FireCrosshairBrush, FireMaterial.Object);
	ConfigureBrush(FireEmphasisBrush, FireEmphasisMaterial.Object);
}

void UWeaponCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ADefenseCharacter* CurrentCharacter = Cast<ADefenseCharacter>(GetOwningPlayerPawn());
	if (OwningCharacter != CurrentCharacter)
	{
		OwningCharacter = CurrentCharacter;
		WeaponComponent = OwningCharacter
			? OwningCharacter->FindComponentByClass<UWeaponComponent>()
			: nullptr;
	}

	RefreshTargetState();
}

int32 UWeaponCrosshairWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled
) const
{
	const int32 BaseLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled
	);

	if (!OwningCharacter)
	{
		return BaseLayer;
	}

	const FVector2f LocalSize(AllottedGeometry.GetLocalSize());
	const FVector2f Center = LocalSize * 0.5f;

	// 무기 Crosshair 렌더는 아래의 기존 코드를 그대로 사용한다.
	if (!WeaponComponent || !WeaponComponent->HasEquippedWeapon())
	{
		const UBuildComponent* CurrentBuildComponent = OwningCharacter->FindComponentByClass<UBuildComponent>();
		if (CurrentBuildComponent && CurrentBuildComponent->HasSelectedTrap())
		{
			constexpr float TrapModeDotSize = 3.f;
			const FVector2D DotSize(TrapModeDotSize, TrapModeDotSize);
			const FVector2D DotPosition = FVector2D(Center.X, Center.Y) - DotSize * 0.5f;
			const FSlateColorBrush DotBrush(IdleColor);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				BaseLayer + 1,
				AllottedGeometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(DotPosition)),
				&DotBrush,
				ESlateDrawEffect::None,
				FLinearColor::White
			);
			return BaseLayer + 1;
		}

		return BaseLayer;
	}

	const float FireAge = OwningCharacter->TimeSinceFiredWeapon;
	const bool bAimFlash = AimFlashDuration > 0.f && FireAge < AimFlashDuration;
	const bool bFireFeedback = FireFeedbackDuration > 0.f
		&& FireAge >= AimFlashDuration
		&& FireAge < AimFlashDuration + FireFeedbackDuration;

	int32 DrawLayer = BaseLayer;
	if (bFireFeedback)
	{
		// WarZ 원본: 중앙 마커 크기가 커지면서 페이드되고, 완료되면 기본 조준선이 다시 나타난다.
		const float FeedbackRatio = FMath::Clamp(
			(FireAge - AimFlashDuration) / FireFeedbackDuration,
			0.f,
			1.f
		);
		const float FireMarkerSize = FMath::Lerp(FireStartGap, FireEndGap, FeedbackRatio);
		const float FireOpacity = 1.f - FeedbackRatio;

		FLinearColor EmphasisColor = IdleColor;
		EmphasisColor.A *= FireOpacity * FireEmphasisOpacity;
		const bool bDrewEmphasis = DrawCenterMarker(
			OutDrawElements,
			AllottedGeometry,
			DrawLayer + 1,
			FireEmphasisBrush,
			FireMarkerSize + FireEmphasisSizeOffset,
			EmphasisColor
		);

		FLinearColor FireColor = IdleColor;
		FireColor.A *= FireOpacity;
		const bool bDrewFireMarker = DrawCenterMarker(
			OutDrawElements,
			AllottedGeometry,
			DrawLayer + 2,
			FireCrosshairBrush,
			FireMarkerSize,
			FireColor
		);

		if (bDrewEmphasis || bDrewFireMarker)
		{
			DrawLayer += 2;
		}
		else
		{
			// 리소스 로드 실패 시에도 동작을 확인할 수 있는 얇은 선 fallback.
			DrawCrosshairLine(OutDrawElements, AllottedGeometry, DrawLayer + 1,
				Center + FVector2f(FireMarkerSize, 0.f), Center + FVector2f(FireMarkerSize + ArmLength, 0.f),
				FireColor, LineThickness);
			DrawCrosshairLine(OutDrawElements, AllottedGeometry, DrawLayer + 1,
				Center - FVector2f(FireMarkerSize, 0.f), Center - FVector2f(FireMarkerSize + ArmLength, 0.f),
				FireColor, LineThickness);
			DrawCrosshairLine(OutDrawElements, AllottedGeometry, DrawLayer + 1,
				Center + FVector2f(0.f, FireMarkerSize), Center + FVector2f(0.f, FireMarkerSize + ArmLength),
				FireColor, LineThickness);
			DrawCrosshairLine(OutDrawElements, AllottedGeometry, DrawLayer + 1,
				Center - FVector2f(0.f, FireMarkerSize), Center - FVector2f(0.f, FireMarkerSize + ArmLength),
				FireColor, LineThickness);
			DrawLayer++;
		}
	}
	else
	{
		const FLinearColor AimColor = bAimFlash
			? DamageableTargetColor
			: (bDamageableTarget ? DamageableTargetColor : IdleColor);

		if (DrawFourWayMarker(
			OutDrawElements,
			AllottedGeometry,
			DrawLayer + 1,
			AimCrosshairBrush,
			AimCrosshairRadius,
			AimColor
		))
		{
			DrawLayer++;
		}
		else
		{
			DrawCrosshairLine(OutDrawElements, AllottedGeometry, DrawLayer + 1,
				Center + FVector2f(CenterGap, 0.f), Center + FVector2f(CenterGap + ArmLength, 0.f),
				AimColor, LineThickness);
			DrawCrosshairLine(OutDrawElements, AllottedGeometry, DrawLayer + 1,
				Center - FVector2f(CenterGap, 0.f), Center - FVector2f(CenterGap + ArmLength, 0.f),
				AimColor, LineThickness);
			DrawCrosshairLine(OutDrawElements, AllottedGeometry, DrawLayer + 1,
				Center + FVector2f(0.f, CenterGap), Center + FVector2f(0.f, CenterGap + ArmLength),
				AimColor, LineThickness);
			DrawCrosshairLine(OutDrawElements, AllottedGeometry, DrawLayer + 1,
				Center - FVector2f(0.f, CenterGap), Center - FVector2f(0.f, CenterGap + ArmLength),
				AimColor, LineThickness);
			DrawLayer++;
		}
	}

	if (!WeaponComponent->IsCharging())
	{
		return DrawLayer;
	}

	const float ChargeRatio = WeaponComponent->GetChargeRatio();
	const int32 SegmentCount = FMath::Clamp(ChargeRingSegments, 8, 128);
	TArray<FVector2f> ChargeBackgroundPoints;
	ChargeBackgroundPoints.Reserve(SegmentCount + 1);
	for (int32 Index = 0; Index <= SegmentCount; ++Index)
	{
		const float SegmentRatio = static_cast<float>(Index) / SegmentCount;
		const float Angle = -HALF_PI + SegmentRatio * UE_TWO_PI;
		ChargeBackgroundPoints.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * ChargeRingRadius);
	}

	FLinearColor ChargeBackgroundColor = ChargeColor;
	ChargeBackgroundColor.A *= 0.2f;
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		DrawLayer + 1,
		AllottedGeometry.ToPaintGeometry(),
		ChargeBackgroundPoints,
		ESlateDrawEffect::None,
		ChargeBackgroundColor,
		true,
		LineThickness
	);

	const EWeaponChargeStage ChargeStage = WeaponComponent->GetChargeStage();
	const int32 ActiveStageCount = ChargeStage == EWeaponChargeStage::None
		? 0
		: static_cast<int32>(ChargeStage);
	const float IndicatorTotalWidth = ChargeStageIndicatorWidth * 3.f
		+ ChargeStageIndicatorSpacing * 2.f;
	const float IndicatorStartX = Center.X - IndicatorTotalWidth * 0.5f;
	const float IndicatorY = Center.Y + ChargeStageIndicatorOffset;

	for (int32 StageIndex = 0; StageIndex < 3; ++StageIndex)
	{
		FLinearColor IndicatorColor = ChargeColor;
		IndicatorColor.A *= StageIndex < ActiveStageCount ? 1.f : 0.2f;
		const float StartX = IndicatorStartX
			+ StageIndex * (ChargeStageIndicatorWidth + ChargeStageIndicatorSpacing);
		DrawCrosshairLine(
			OutDrawElements,
			AllottedGeometry,
			DrawLayer + 2,
			FVector2f(StartX, IndicatorY),
			FVector2f(StartX + ChargeStageIndicatorWidth, IndicatorY),
			IndicatorColor,
			ChargeStageIndicatorThickness
		);
	}

	if (ChargeRatio <= 0.f)
	{
		return DrawLayer + 2;
	}

	const int32 VisibleSegments = FMath::Max(1, FMath::CeilToInt(SegmentCount * ChargeRatio));
	TArray<FVector2f> ChargePoints;
	ChargePoints.Reserve(VisibleSegments + 1);

	for (int32 Index = 0; Index <= VisibleSegments; ++Index)
	{
		const float SegmentRatio = static_cast<float>(Index) / SegmentCount;
		const float Angle = -HALF_PI + SegmentRatio * UE_TWO_PI;
		ChargePoints.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * ChargeRingRadius);
	}

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		DrawLayer + 3,
		AllottedGeometry.ToPaintGeometry(),
		ChargePoints,
		ESlateDrawEffect::None,
		ChargeColor,
		true,
		LineThickness
	);

	return DrawLayer + 3;
}

void UWeaponCrosshairWidget::RefreshTargetState()
{
	bDamageableTarget = false;

	APlayerController* PlayerController = GetOwningPlayer();
	UWorld* World = GetWorld();
	if (!PlayerController || !World || !OwningCharacter || !WeaponComponent
		|| !WeaponComponent->HasEquippedWeapon())
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponCrosshairTrace), false, OwningCharacter);
	QueryParams.AddIgnoredActor(OwningCharacter);

	const float TraceRange = WeaponComponent->GetFireRange();
	if (TraceRange <= 0.f)
	{
		return;
	}

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * TraceRange;
	const float AimAssistRadius = WeaponComponent->GetFireAimAssistRadius();
	const bool bHit = AimAssistRadius > 0.f
		? World->SweepSingleByChannel(
			Hit,
			ViewLocation,
			TraceEnd,
			FQuat::Identity,
			ECC_Visibility,
			FCollisionShape::MakeSphere(AimAssistRadius),
			QueryParams
		)
		: World->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			TraceEnd,
			ECC_Visibility,
			QueryParams
		);

	if (bHit)
	{
		bDamageableTarget = Cast<AEnemyBase>(Hit.GetActor()) != nullptr;
	}
}
