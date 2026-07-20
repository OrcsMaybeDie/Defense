// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MapConfigData.generated.h"

UCLASS()
class DEFENSE_API UMapConfigData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	int32 InitialDestScore = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
	int32 InitCoin = 3000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<class UWaveData> WaveData;
};
