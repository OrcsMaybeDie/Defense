#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "PlayerHUDWidget.generated.h"


class UQuickSlotBarWidget;
class UNoticeWidget;
class UPlayerStatusWidget;
class UTextBlock;
class ADefenseCharacter;
class APlayerState;


UCLASS()
class DEFENSE_API UPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPlayerStatusWidget> SelfStatus;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPlayerStatusWidget> AllyStatus;

	// 3인 시연용 두 번째 아군 상태. 에디터에서 추가하기 전에도 빌드 가능하다.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UPlayerStatusWidget> AllyStatus2;
	
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AllyName;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UQuickSlotBarWidget> QuickSlotBar;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UNoticeWidget> WBP_Notice;
	
private:
	void TryBindPlayer();
	
	// 주어진 PlayerState를 소유한 플레이어 캐릭터를 현재 클라이언트 월드에서 찾는다.
	ADefenseCharacter* FindCharByPlayerState(APlayerState* TargetPlayerState) const;
	
	FTimerHandle BindPlayersTimerHandle;

	// 로비가 없는 데모에서는 두 번째 플레이어 접속/복제가 늦을 수 있어 제한 시간 동안만 재시도한다.
	UPROPERTY(EditDefaultsOnly, Category="Binding")
	float MaxBindRetrySeconds = 60.f;

	float BindRetryElapsedSeconds = 0.f;
};
