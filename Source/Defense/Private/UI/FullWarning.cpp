// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FullWarning.h"

#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"

void UFullWarning::NativeConstruct()
{
	Super::NativeConstruct();

	if (ButtonQuit)
	{
		ButtonQuit->OnClicked.AddUniqueDynamic(this, &UFullWarning::HandleQuitClicked);
	}
}

void UFullWarning::NativeDestruct()
{
	if (ButtonQuit)
	{
		ButtonQuit->OnClicked.RemoveDynamic(this, &UFullWarning::HandleQuitClicked);
	}

	Super::NativeDestruct();
}

void UFullWarning::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(
		this,
		GetOwningPlayer(),
		EQuitPreference::Quit,
		true
	);
}
