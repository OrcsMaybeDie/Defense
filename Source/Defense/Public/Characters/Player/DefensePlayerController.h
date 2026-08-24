// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "GameFramework/PlayerController.h"
#include "Mission/MissionCompletionResult.h"
#include "DefensePlayerController.generated.h"

class UEquipmentMenuWidget;
class UInputMappingContext;
class UUserWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class ADefensePlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	virtual void OnRep_PlayerState() override;

	/** Gameplay cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<class UInputAction> ReadyAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<class UInputAction> IA_ESC;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<class UInputAction> IA_EquipmentMenu;

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestReady();

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestGameEndRetry();

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestReturnToIntroMap();

	UFUNCTION(Server, Reliable)
	void ServerRPC_SubmitClientIdentity(const FString& ClientIdentity);
	
	// UI
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UUserWidget> HUDWidget;

	bool bCinematicHUDHidden = false;
	ESlateVisibility HUDVisibilityBeforeCinematic = ESlateVisibility::Visible;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<class UESCUI> ESCUIClass;

	UPROPERTY()
	TObjectPtr<UESCUI> ESCUI;
	
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UEquipmentMenuWidget> EquipmentMenuWidgetClass;

	UPROPERTY()
	TObjectPtr<UEquipmentMenuWidget> EquipmentMenuWidget;

public:
	void RequestReady();
	void RequestGameEndRetry();
	void RequestReturnToIntroMap();
	void QuitGame();
	bool IsGameHostPlayer() const;
	void ToggleESCUI();
	void SubmitClientIdentity();
	void SetCinematicHUDHidden(bool bShouldHide);

	void ToggleEquipmentMenu(); // 장비창

	// 게임 끝났을 때 UI
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<class UGameEndUI> GameEndUIClass;
	
	UPROPERTY()
	TObjectPtr<UGameEndUI> GameEndUI;

	// 게임 종료가 확정되는 즉시 로컬 플레이 입력 차단
	UFUNCTION(Client, Reliable)
	void ClientRPC_EnterGameEndState();

	// GameEndUI (+ Mission)
	UFUNCTION(Client, Reliable)
	void ClientRPC_ShowGameEndUI(bool bGameClear, const TArray<FMissionCompletionResult>& Results);
	
	// 게임 다시 시작할 때 커서 및 입력모드 되돌리기
	UFUNCTION(Client, Reliable)
	void ClientRPC_HideGameEndUI();

	UFUNCTION(Client, Reliable)
	void ClientRPC_ShowEndLoadingUI();

	UFUNCTION(Client, Reliable)
	void ClientRPC_ShowESCLoadingUI();

	UFUNCTION(Client, Reliable)
	void ClientRPC_ShowRewardPopup(class AEnemyBase* Enemy, int32 RewardAmount);
	
};
