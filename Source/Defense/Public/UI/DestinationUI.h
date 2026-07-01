// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DestinationUI.generated.h"

/**
 * 
 */
UCLASS()
class DEFENSE_API UDestinationUI : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UTextBlock> DestinationCount;
	
	UFUNCTION()
	void UpdateUI(int32 newScore);
	
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	void TryBindGameState();

private:
	FTimerHandle BindGameStateTimerHandle;
	
};
