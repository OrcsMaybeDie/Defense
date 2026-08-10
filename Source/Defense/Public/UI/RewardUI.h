// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RewardUI.generated.h"

/**
 * 
 */
UCLASS()
class DEFENSE_API URewardUI : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UTextBlock> RewardText;

	UFUNCTION(BlueprintCallable, Category="Reward")
	void SetRewardAmount(int32 RewardAmount);
};
