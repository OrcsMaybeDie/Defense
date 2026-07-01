// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHPUI.generated.h"

/**
 * 
 */
UCLASS()
class DEFENSE_API UEnemyHPUI : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UProgressBar> HPBar;
	
	void UpdateHPBar(int32 Cur, int32 Max);
	
};
