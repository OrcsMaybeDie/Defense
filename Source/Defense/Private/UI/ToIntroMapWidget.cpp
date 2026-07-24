// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/ToIntroMapWidget.h"

#include "Characters/Player/DefensePlayerController.h"
#include "Components/Button.h"

void UToIntroMapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ButtonIntroMap)
	{
		ButtonIntroMap->OnClicked.AddUniqueDynamic(this, &UToIntroMapWidget::HandleIntroMapClicked);
	}
}

void UToIntroMapWidget::NativeDestruct()
{
	if (ButtonIntroMap)
	{
		ButtonIntroMap->OnClicked.RemoveDynamic(this, &UToIntroMapWidget::HandleIntroMapClicked);
	}

	Super::NativeDestruct();
}

void UToIntroMapWidget::HandleIntroMapClicked()
{
	if (ADefensePlayerController* DefensePlayerController = GetOwningPlayer<ADefensePlayerController>())
	{
		DefensePlayerController->RequestReturnToIntroMap();
	}
}
