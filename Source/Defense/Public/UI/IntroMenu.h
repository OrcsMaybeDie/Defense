// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameManager/Intro/IntroPlayerState.h"
#include "IntroMenu.generated.h"

/**
 * 
 */
UCLASS()
class DEFENSE_API UIntroMenu : public UUserWidget
{
	GENERATED_BODY()
	
public:
	
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category="Intro|UI")
	void RefreshMenuState();

	UFUNCTION(BlueprintCallable, Category="Intro|UI")
	void ShowIntroLoading();
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UWidgetSwitcher> SwitcherIntroLoading;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class UWidget> WBP_EquipEntryButton;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UScrollBox> ScrollBoxMaps;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UWidgetSwitcher> SwitcherHostGuest;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UWidgetSwitcher> SwitcherGuest;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UButton> ButtonStart;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UButton> ButtonReady;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UButton> ButtonReadyEnd;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Intro|Map")
	TSubclassOf<class UMapWidget> MapWidgetClass;

	UPROPERTY()
	TArray<TObjectPtr<UMapWidget>> MapWidgets;

	UPROPERTY()
	bool bPendingGuestReady = false;

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleReadyClicked();

	UFUNCTION()
	void HandleIntroPlayersChanged();

	UFUNCTION()
	void HandleGuestReadyChanged(bool bGuestReady);

	UFUNCTION()
	void HandleIntroRoleChanged(EIntroPlayerRole NewRole);

	UFUNCTION()
	void HandleIntroReadyChanged(bool bReady);

	UFUNCTION()
	void HandleSelectedMapChanged(class UMapConfigData* SelectedMapConfigData);

	UFUNCTION()
	void HandleMapWidgetSelected(class UMapConfigData* SelectedMapConfigData);

	void BindIntroStateDelegates();
	void UnbindIntroStateDelegates();
	void RebuildMapList();
	void RefreshMapWidgets();
};
