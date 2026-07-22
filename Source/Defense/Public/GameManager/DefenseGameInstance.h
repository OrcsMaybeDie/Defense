// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DefenseGameInstance.generated.h"

UCLASS()
class DEFENSE_API UDefenseGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TObjectPtr<class UMapConfigData> DefaultMapConfigData;

	UPROPERTY(BlueprintReadWrite, Category = "Map")
	TObjectPtr<class UMapConfigData> SelectedMapConfigData;

	UFUNCTION(BlueprintCallable, Category = "Map")
	void SetSelectedMapConfigData(class UMapConfigData* InMapConfigData);

	UFUNCTION(BlueprintPure, Category = "Map")
	class UMapConfigData* GetSelectedMapConfigData() const;
};
