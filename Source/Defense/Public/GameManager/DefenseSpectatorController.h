#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "DefenseSpectatorController.generated.h"

class UESCUI;
class UInputAction;
class UInputMappingContext;

UCLASS()
class DEFENSE_API ADefenseSpectatorController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	void ToggleESCUI();

protected:
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> SpectatorMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SpectatorESCAction;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UESCUI> ESCUIClass;

	UPROPERTY()
	TObjectPtr<UESCUI> ESCUI;

	UPROPERTY(EditAnywhere, Category="Spectator")
	float HostViewTargetRefreshInterval = 0.25f;

	UFUNCTION(Server, Reliable)
	void ServerRPC_SubmitSpectatorClientIdentity(const FString& ClientIdentity);

	void SubmitSpectatorClientIdentity();
	void SyncViewTargetToHost();
	APawn* FindHostPawn() const;

	FTimerHandle HostViewTargetTimerHandle;
};
