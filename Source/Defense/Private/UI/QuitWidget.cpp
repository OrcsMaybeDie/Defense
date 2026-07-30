// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/QuitWidget.h"

#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"

void UQuitWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ButtonQuit)
	{
		ButtonQuit->OnClicked.AddUniqueDynamic(this, &UQuitWidget::HandleQuitClicked);
	}
}

void UQuitWidget::NativeDestruct()
{
	if (ButtonQuit)
	{
		ButtonQuit->OnClicked.RemoveDynamic(this, &UQuitWidget::HandleQuitClicked);
	}

	Super::NativeDestruct();
}

void UQuitWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(
		this,
		GetOwningPlayer(),
		EQuitPreference::Quit,
		true
	);
}
