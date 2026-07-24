// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "IntroPlayerState.generated.h"

UENUM(BlueprintType)
enum class EIntroPlayerRole : uint8
{
	None UMETA(DisplayName = "None"),
	Host UMETA(DisplayName = "Host"),
	Guest UMETA(DisplayName = "Guest"),
	Spectator UMETA(DisplayName = "Spectator")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIntroRoleChanged, EIntroPlayerRole, NewRole);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIntroReadyChanged, bool, bNewReady);

UCLASS()
class DEFENSE_API AIntroPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Intro|Role")
	EIntroPlayerRole GetIntroRole() const { return IntroRole; }

	UFUNCTION(BlueprintPure, Category="Intro|Role")
	bool IsHost() const { return IntroRole == EIntroPlayerRole::Host; }

	UFUNCTION(BlueprintPure, Category="Intro|Role")
	bool IsGuest() const { return IntroRole == EIntroPlayerRole::Guest; }

	UFUNCTION(BlueprintPure, Category="Intro|Ready")
	bool IsReady() const { return bIsReady; }

	UFUNCTION(BlueprintPure, Category="Intro|Identity")
	const FString& GetClientIdentity() const { return ClientIdentity; }

	void SetIntroRole(EIntroPlayerRole NewRole);
	void SetReady(bool bNewReady);
	void SetClientIdentity(const FString& NewClientIdentity);

	UPROPERTY(BlueprintAssignable, Category="Intro|Role")
	FOnIntroRoleChanged OnIntroRoleChanged;

	UPROPERTY(BlueprintAssignable, Category="Intro|Ready")
	FOnIntroReadyChanged OnIntroReadyChanged;

protected:
	UPROPERTY(ReplicatedUsing=OnRep_IntroRole, BlueprintReadOnly, Category="Intro|Role")
	EIntroPlayerRole IntroRole = EIntroPlayerRole::None;

	UPROPERTY(ReplicatedUsing=OnRep_IsReady, BlueprintReadOnly, Category="Intro|Ready")
	bool bIsReady = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Intro|Identity")
	FString ClientIdentity;

	UFUNCTION()
	void OnRep_IntroRole();

	UFUNCTION()
	void OnRep_IsReady();
};
