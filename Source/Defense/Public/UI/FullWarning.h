// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FullWarning.generated.h"

/**
 * 
 */
UCLASS()
class DEFENSE_API UFullWarning : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UButton> ButtonQuit;

protected:
	UFUNCTION()
	void HandleQuitClicked();
};
