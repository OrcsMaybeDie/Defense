// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameEndUI.generated.h"

/**
 * 
 */
UCLASS()
class DEFENSE_API UGameEndUI : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UWidgetSwitcher> WidgetSwitcher;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UWidgetSwitcher> Retry_Switcher;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UButton> Button_Exit;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UButton> Button_Retry;
	
	UFUNCTION()
	void GameClear();
	
	UFUNCTION()
	void GameOver();
	
	UFUNCTION()
	void ExitGame();
	
	UFUNCTION()
	void RetryGame();
};
