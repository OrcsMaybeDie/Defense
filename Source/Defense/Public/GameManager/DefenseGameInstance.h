// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DefenseGameInstance.generated.h"

class UFullWarning;
class APlayerState;

UCLASS()
class DEFENSE_API UDefenseGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TSoftObjectPtr<class UWorld> IntroMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TObjectPtr<class UMapConfigData> DefaultMapConfigData;

	UPROPERTY(BlueprintReadWrite, Category = "Map")
	TObjectPtr<class UMapConfigData> SelectedMapConfigData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TArray<TObjectPtr<class UMapConfigData>> AvailableMapConfigDataList;

	UFUNCTION(BlueprintCallable, Category = "Map")
	void SetSelectedMapConfigData(class UMapConfigData* InMapConfigData);

	UFUNCTION(BlueprintPure, Category = "Map")
	class UMapConfigData* GetSelectedMapConfigData() const;

	const TArray<TObjectPtr<class UMapConfigData>>& GetAvailableMapConfigDataList() const;

	UFUNCTION(BlueprintPure, Category = "Map")
	FString GetSelectedGameMapPackageName() const;

	UFUNCTION(BlueprintPure, Category = "Map")
	FString GetIntroMapPackageName() const;

	void SaveIntroPlayerRoles(APlayerState* HostPlayerState, APlayerState* GuestPlayerState);
	bool IsSavedHostPlayerState(const APlayerState* PlayerState) const;
	bool IsSavedGuestPlayerState(const APlayerState* PlayerState) const;
	bool HasSavedGuestPlayerId() const;

	void ShowFullWarning();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UFullWarning> FullWarningClass;

	UPROPERTY()
	TObjectPtr<UFullWarning> FullWarningWidget;

	UPROPERTY()
	FString SavedHostPlayerId;

	UPROPERTY()
	FString SavedGuestPlayerId;
};
