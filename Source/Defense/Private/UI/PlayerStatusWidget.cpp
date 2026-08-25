#include "UI/PlayerStatusWidget.h"

#include "Characters/Player/DefenseCharacter.h"
#include "Characters/Player/WeaponComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"

void UPlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ADefenseCharacter* Character = Cast<ADefenseCharacter>(GetOwningPlayerPawn());
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerStatusWidget could not find owning DefenseCharacter."));
		return;
	}

	BindStatusComp(Character->GetStatusComp());
}

void UPlayerStatusWidget::NativeDestruct()
{
	if (BoundWeaponComp)
	{
		BoundWeaponComp->OnChargePreviewChanged.RemoveDynamic(
			this,
			&UPlayerStatusWidget::HandleChargePreviewChanged
		);
		BoundWeaponComp = nullptr;
	}

	if (BoundStatusComp)
	{
		BoundStatusComp->OnHealthChanged.RemoveDynamic(this, &UPlayerStatusWidget::HandleHealthChanged);
		BoundStatusComp->OnManaChanged.RemoveDynamic(this, &UPlayerStatusWidget::HandleManaChanged);
		BoundStatusComp = nullptr;
	}

	BoundCharacter = nullptr;
	AppliedProfileTexture = nullptr;
	
	Super::NativeDestruct();
}

void UPlayerStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshProfileImage();

	DisplayedChargeManaPercent = FMath::FInterpTo(
		DisplayedChargeManaPercent,
		TargetChargeManaPercent,
		InDeltaTime,
		ChargePreviewInterpSpeed
	);
	DisplayedChargeRatio = FMath::FInterpTo(
		DisplayedChargeRatio,
		TargetChargeRatio,
		InDeltaTime,
		ChargePreviewInterpSpeed
	);

	if (MPChargeBar)
	{
		MPChargeBar->SetPercent(FMath::Clamp(DisplayedChargeManaPercent, 0.f, 1.f));
	}

	EWeaponChargeStage DisplayedStage = CachedChargeStage;
	if (TargetChargeRatio < DisplayedChargeRatio)
	{
		// 차지가 해제되면 표현 단계가 Stage3 -> Stage2 -> Stage1 -> None 순서로 내려간다.
		if (DisplayedChargeRatio >= 1.f - KINDA_SMALL_NUMBER)
		{
			DisplayedStage = EWeaponChargeStage::Stage3;
		}
		else if (DisplayedChargeRatio >= 2.f / 3.f)
		{
			DisplayedStage = EWeaponChargeStage::Stage2;
		}
		else if (DisplayedChargeRatio >= 1.f / 3.f)
		{
			DisplayedStage = EWeaponChargeStage::Stage1;
		}
		else
		{
			DisplayedStage = EWeaponChargeStage::None;
		}
	}

	const float CurrentMana = BoundStatusComp ? BoundStatusComp->Mana : 0.f;
	const float MaxMana = BoundStatusComp ? BoundStatusComp->MaxMana : 0.f;
	OnChargeManaPreviewChanged(
		DisplayedStage,
		FMath::Clamp(DisplayedChargeRatio, 0.f, 1.f),
		FMath::Clamp(DisplayedChargeManaPercent, 0.f, 1.f) * MaxMana,
		CurrentMana,
		MaxMana
	);
}

void UPlayerStatusWidget::BindStatusComp(UStatusComponent* InStatComp)
{
	if (!InStatComp) return;

	if (BoundWeaponComp)
	{
		BoundWeaponComp->OnChargePreviewChanged.RemoveDynamic(
			this,
			&UPlayerStatusWidget::HandleChargePreviewChanged
		);
		BoundWeaponComp = nullptr;
	}

	if (BoundStatusComp)
	{
		BoundStatusComp->OnHealthChanged.RemoveDynamic(this, &UPlayerStatusWidget::HandleHealthChanged);
		BoundStatusComp->OnManaChanged.RemoveDynamic(this, &UPlayerStatusWidget::HandleManaChanged);
	}

	BoundStatusComp = InStatComp;
	BoundCharacter = Cast<ADefenseCharacter>(InStatComp->GetOwner());
	AppliedProfileTexture = nullptr;
	BoundStatusComp->OnHealthChanged.AddDynamic(this, &UPlayerStatusWidget::HandleHealthChanged);
	BoundStatusComp->OnManaChanged.AddDynamic(this, &UPlayerStatusWidget::HandleManaChanged);

	BoundWeaponComp = InStatComp->GetOwner()
		? InStatComp->GetOwner()->FindComponentByClass<UWeaponComponent>()
		: nullptr;
	if (BoundWeaponComp)
	{
		BoundWeaponComp->OnChargePreviewChanged.AddUniqueDynamic(
			this,
			&UPlayerStatusWidget::HandleChargePreviewChanged
		);
	}

	HandleHealthChanged(BoundStatusComp->Health, BoundStatusComp->MaxHealth);
	HandleManaChanged(BoundStatusComp->Mana, BoundStatusComp->MaxMana);
	HandleChargePreviewChanged(EWeaponChargeStage::None, 0.f, 0.f);
	RefreshProfileImage();
}

void UPlayerStatusWidget::RefreshProfileImage()
{
	if (!ProfileImage || !BoundCharacter)
	{
		return;
	}

	UTexture2D* DesiredTexture = BoundCharacter->GetAppearanceProfileImage();
	if (!DesiredTexture || DesiredTexture == AppliedProfileTexture)
	{
		return;
	}

	ProfileImage->SetBrushFromTexture(DesiredTexture, false);
	AppliedProfileTexture = DesiredTexture;
}

void UPlayerStatusWidget::HandleHealthChanged(float CurValue, float MaxValue)
{
	if (!HPBar) return;

	const float Percent = MaxValue > 0.f ? CurValue / MaxValue : 0.f;
	HPBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
}

void UPlayerStatusWidget::HandleManaChanged(float CurValue, float MaxValue)
{
	if (MPBar)
	{
		const float Percent = MaxValue > 0.f ? CurValue / MaxValue : 0.f;
		MPBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
	}

	TargetChargeManaPercent = MaxValue > 0.f ? CachedPreviewManaCost / MaxValue : 0.f;
}

void UPlayerStatusWidget::HandleChargePreviewChanged(
	EWeaponChargeStage ChargeStage,
	float ChargeRatio,
	float PreviewManaCost
)
{
	CachedChargeStage = ChargeStage;
	CachedChargeRatio = FMath::Clamp(ChargeRatio, 0.f, 1.f);
	CachedPreviewManaCost = FMath::Max(0.f, PreviewManaCost);
	TargetChargeRatio = CachedChargeRatio;

	const float MaxMana = BoundStatusComp ? BoundStatusComp->MaxMana : 0.f;
	TargetChargeManaPercent = MaxMana > 0.f ? CachedPreviewManaCost / MaxMana : 0.f;
}
