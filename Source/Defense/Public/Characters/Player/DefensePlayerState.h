#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DefensePlayerState.generated.h"

UCLASS()
class DEFENSE_API ADefensePlayerState : public APlayerState
{
	GENERATED_BODY()
	
protected:
	
	UPROPERTY(ReplicatedUsing=OnRep_IsReady, BlueprintReadOnly, Category="Ready")
	bool bIsReady = false;
	
public:
	
	bool IsReady() const { return bIsReady; }
	void SetReady(bool bReady);

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_IsReady();
};
