// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ESCUI.generated.h"

/**
 * 
 */
UCLASS()
class DEFENSE_API UESCUI : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category="ESC")
	void RefreshESCUI();

	UFUNCTION(BlueprintCallable, Category="ESC")
	void SetForceGuestMode(bool bInForceGuestMode);

	UFUNCTION(BlueprintCallable, Category="ESC")
	void ShowESCLoading();

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UWidgetSwitcher> SwitcherToIntroQuit;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UWidgetSwitcher> SwitcherESCLoading;

protected:
	UPROPERTY()
	bool bForceGuestMode = false;
};
