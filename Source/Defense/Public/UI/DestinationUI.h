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
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UTextBlock> Text_MaxWave;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UTextBlock> Text_CurWave;
	
	UFUNCTION()
	void UpdateUI(int32 newScore);

	UFUNCTION()
	void UpdateCurrentWave(int32 NewCurrentWave);

	void UpdateMaxWave(int32 NewMaxWave);
	
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	void TryBindGameState();

private:
	FTimerHandle BindGameStateTimerHandle;
	
};
