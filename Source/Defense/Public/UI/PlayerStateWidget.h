#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Characters/Player/StatusComponent.h"
#include "PlayerStateWidget.generated.h"

/**
 * 
 */
UCLASS()
class DEFENSE_API UPlayerStateWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UProgressBar> HPBar;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UProgressBar> MPBar;
	
	void BindStatComp(UStatusComponent* InStatComp);
	
	UFUNCTION()
	void HandleHealthChanged(float CurValue, float MaxValue);
	
	UFUNCTION()
	void HandleManaChanged(float CurValue, float MaxValue);

protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY()
	TObjectPtr<UStatusComponent> BoundStatComp;
};
