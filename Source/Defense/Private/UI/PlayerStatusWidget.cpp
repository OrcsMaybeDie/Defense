#include "UI/PlayerStatusWidget.h"

#include "Characters/Player/DefenseCharacter.h"
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
	if (BoundStatusComp)
	{
		BoundStatusComp->OnHealthChanged.RemoveDynamic(this, &UPlayerStatusWidget::HandleHealthChanged);
		BoundStatusComp->OnManaChanged.RemoveDynamic(this, &UPlayerStatusWidget::HandleManaChanged);
		BoundStatusComp = nullptr;
	}
	
	Super::NativeDestruct();
}

void UPlayerStatusWidget::BindStatusComp(UStatusComponent* InStatComp)
{
	if (!InStatComp) return;

	if (BoundStatusComp)
	{
		BoundStatusComp->OnHealthChanged.RemoveDynamic(this, &UPlayerStatusWidget::HandleHealthChanged);
		BoundStatusComp->OnManaChanged.RemoveDynamic(this, &UPlayerStatusWidget::HandleManaChanged);
	}

	BoundStatusComp = InStatComp;
	BoundStatusComp->OnHealthChanged.AddDynamic(this, &UPlayerStatusWidget::HandleHealthChanged);
	BoundStatusComp->OnManaChanged.AddDynamic(this, &UPlayerStatusWidget::HandleManaChanged);

	HandleHealthChanged(BoundStatusComp->Health, BoundStatusComp->MaxHealth);
	HandleManaChanged(BoundStatusComp->Mana, BoundStatusComp->MaxMana);
}

void UPlayerStatusWidget::HandleHealthChanged(float CurValue, float MaxValue)
{
	if (!HPBar) return;

	const float Percent = MaxValue > 0.f ? CurValue / MaxValue : 0.f;
	HPBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
}

void UPlayerStatusWidget::HandleManaChanged(float CurValue, float MaxValue)
{
	if (!MPBar) return;

	const float Percent = MaxValue > 0.f ? CurValue / MaxValue : 0.f;
	MPBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
}
