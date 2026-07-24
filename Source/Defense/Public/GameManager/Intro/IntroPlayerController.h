// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "IntroPlayerController.generated.h"

class UIntroMenu;
class UInputAction;
class UInputMappingContext;
class UESCUI;

UCLASS()
class DEFENSE_API AIntroPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	UFUNCTION(Server, Reliable)
	void ServerRPC_SetGuestReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestStartGame();

	UFUNCTION(Server, Reliable)
	void ServerRPC_SelectMap(class UMapConfigData* SelectedMapConfigData);

	UFUNCTION(Server, Reliable)
	void ServerRPC_SubmitClientIdentity(const FString& ClientIdentity);

	UFUNCTION(Client, Reliable)
	void ClientRPC_ShowIntroLoading();

	UFUNCTION(Client, Reliable)
	void ClientRPC_ShowFullWarning();

	void RefreshIntroMenu();

protected:
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> IA_ESC;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Intro|UI")
	TSubclassOf<UIntroMenu> IntroMenuClass;

	UPROPERTY()
	TObjectPtr<UIntroMenu> IntroMenu;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Intro|UI")
	TSubclassOf<UESCUI> ESCUIClass;

	UPROPERTY()
	TObjectPtr<UESCUI> ESCUI;

	UPROPERTY()
	bool bShouldShowFullWarning = false;

	void ShowIntroMenu();
	void ShowFullWarning();
	void UpdateIntroEntryUI();
	void SubmitClientIdentity();
	void ToggleESCUI();
};
