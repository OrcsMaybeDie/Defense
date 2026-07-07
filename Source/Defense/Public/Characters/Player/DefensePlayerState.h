#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DefensePlayerState.generated.h"


// Delegate
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoinChanged, int32, NewCoin);


UCLASS()
class DEFENSE_API ADefensePlayerState : public APlayerState
{
	GENERATED_BODY()
	
protected:
	
	UPROPERTY(ReplicatedUsing=OnRep_IsReady, BlueprintReadOnly, Category="State")
	bool bIsReady = false;
	
	UPROPERTY(ReplicatedUsing=OnRep_Coin, BlueprintReadOnly, Category="State|Economy")
	int32 Coin = 0;
	
public:
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	bool IsReady() const { return bIsReady; }
	void SetReady(bool bReady);
	
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
};
