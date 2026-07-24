#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DefensePlayerState.generated.h"


// Delegate
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoinChanged, int32, NewCoin);

UENUM(BlueprintType)
enum class EDefensePlayerRole : uint8
{
	None UMETA(DisplayName = "None"),
	Host UMETA(DisplayName = "Host"),
	Guest UMETA(DisplayName = "Guest"),
	Spectator UMETA(DisplayName = "Spectator")
};

UCLASS()
class DEFENSE_API ADefensePlayerState : public APlayerState
{
	GENERATED_BODY()
	
protected:
	
	UPROPERTY(ReplicatedUsing=OnRep_IsReady, BlueprintReadOnly, Category="State")
	bool bIsReady = false;
	
	UPROPERTY(ReplicatedUsing=OnRep_Coin, BlueprintReadOnly, Category="State|Economy")
	int32 Coin = 0;

	UPROPERTY(ReplicatedUsing=OnRep_GameRole, BlueprintReadOnly, Category="State|Role")
	EDefensePlayerRole GameRole = EDefensePlayerRole::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="State|Identity")
	FString ClientIdentity;
	
public:
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	bool IsReady() const { return bIsReady; }
	void SetReady(bool bReady);

	UFUNCTION(BlueprintPure, Category="State|Role")
	EDefensePlayerRole GetGameRole() const { return GameRole; }

	UFUNCTION(BlueprintPure, Category="State|Role")
	bool IsHost() const { return GameRole == EDefensePlayerRole::Host; }

	UFUNCTION(BlueprintPure, Category="State|Role")
	bool IsGuest() const { return GameRole == EDefensePlayerRole::Guest; }

	UFUNCTION(BlueprintPure, Category="State|Identity")
	const FString& GetClientIdentity() const { return ClientIdentity; }

	void SetGameRole(EDefensePlayerRole NewRole);
	void SetClientIdentity(const FString& NewClientIdentity);
	
	UPROPERTY(BlueprintAssignable, Category="State|Economy")
	FOnCoinChanged OnCoinChanged; // UI
	
	UFUNCTION(BlueprintPure, Category="Economy")
	int32 GetCoin() const { return Coin; }; // getter
	
	bool TrySpendCoin(int32 Amount); // 서버 권한 확인
	bool CanSpendCoin(int32 Amount) const;
	
	void SetCoin(int32 NewCoin);
	
	void AddCoin(int32 Amount);
	void RefundCoin(int32 Amount); 

protected:
	
	UFUNCTION()
	void OnRep_IsReady();
	
	UFUNCTION()
	void OnRep_Coin();

	UFUNCTION()
	void OnRep_GameRole();
};
