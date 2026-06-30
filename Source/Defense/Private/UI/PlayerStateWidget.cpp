#include "UI/PlayerStateWidget.h"

#include "Characters/Player/DefenseCharacter.h"
#include "Components/ProgressBar.h"

void UPlayerStateWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ADefenseCharacter* Character = Cast<ADefenseCharacter>(GetOwningPlayerPawn());
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerStatWidget could not find owning DefenseCharacter."));
		return;
	}

	BindStatComp(Character->GetStatComp());
}

void UPlayerStateWidget::BindStatComp(UStatusComponent* InStatComp)
{
	if (!InStatComp) return;

	if (BoundStatComp)
	{
		BoundStatComp->OnHealthChanged.RemoveDynamic(this, &UPlayerStateWidget::HandleHealthChanged);
		BoundStatComp->OnManaChanged.RemoveDynamic(this, &UPlayerStateWidget::HandleManaChanged);
	}

	BoundStatComp = InStatComp;
	BoundStatComp->OnHealthChanged.AddDynamic(this, &UPlayerStateWidget::HandleHealthChanged);
	BoundStatComp->OnManaChanged.AddDynamic(this, &UPlayerStateWidget::HandleManaChanged);

	HandleHealthChanged(BoundStatComp->Health, BoundStatComp->MaxHealth);
	HandleManaChanged(BoundStatComp->Mana, BoundStatComp->MaxMana);
}

void UPlayerStateWidget::HandleHealthChanged(float CurValue, float MaxValue)
{
	if (!HPBar) return;

	const float Percent = MaxValue > 0.f ? CurValue / MaxValue : 0.f;
	HPBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
}

void UPlayerStateWidget::HandleManaChanged(float CurValue, float MaxValue)
{
	if (!MPBar) return;

	const float Percent = MaxValue > 0.f ? CurValue / MaxValue : 0.f;
	MPBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
}
