#include "UI/PlayerHUDWidget.h"

#include "EngineUtils.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameManager/DefenseGameState.h"
#include "TimerManager.h"
#include "Characters/Player/DefensePlayerState.h"
#include "UI/EquipmentUI/QuickSlotBarWidget.h"
#include "UI/NoticeWidget.h"
#include "UI/PlayerStatusWidget.h"

void UPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindRetryElapsedSeconds = 0.f;
	
	// 초기화
	if (AllyStatus)
	{
		AllyStatus->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (AllyName)
	{
		AllyName->SetVisibility(ESlateVisibility::Collapsed); // 이후에 합칠것
	}
	
	TryBindPlayer();
}

void UPlayerHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		// 위젯이 사라질 때 재시도 타이머 취소
		World->GetTimerManager().ClearTimer(BindPlayersTimerHandle);
	}
	
	Super::NativeDestruct();
}

void UPlayerHUDWidget::TryBindPlayer()
{
	UWorld* World = GetWorld();
	APlayerController* OwningPC = GetOwningPlayer();
	
	if (!World || !OwningPC) return;
	
	bool bSelfBound = false;
	bool bAllyBound = false;
	
	
	// bind mine
	if (ADefenseCharacter* SelfChar = Cast<ADefenseCharacter>(OwningPC->GetPawn()))
	{
		if (SelfStatus)
		{
			SelfStatus->BindStatusComp(SelfChar->GetStatusComp());
			bSelfBound = true;
		}
		
		if (QuickSlotBar)
		{
			QuickSlotBar->BindLoadoutComponent(SelfChar->GetLoadoutComponent());
		}
	}
	
	AGameStateBase* GameState = World->GetGameState();
	APlayerState* LocalPlayerState = OwningPC->PlayerState;
	ADefensePlayerState* DefensePlayerState = Cast<ADefensePlayerState>(LocalPlayerState);
	
	if (QuickSlotBar && DefensePlayerState)
	{
		QuickSlotBar->BindPlayerState(DefensePlayerState);
	}

	if (WBP_Notice)
	{
		WBP_Notice->BindReadyState(
			Cast<ADefenseGameState>(GameState),
			DefensePlayerState
		);
	}
	
	if (GameState && LocalPlayerState && AllyStatus)
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (!PS || PS == LocalPlayerState) continue;
			
			// Ally 가 있으면 Visible
			ADefenseCharacter* AllyChar = FindCharByPlayerState(PS);
			if (!AllyChar) continue;
				
			AllyStatus->BindStatusComp(AllyChar->GetStatusComp());
			AllyStatus->SetVisibility(ESlateVisibility::Visible);
			
			if (AllyName)
			{
				AllyName->SetText(FText::FromString(PS->GetPlayerName()));
				AllyName->SetVisibility(ESlateVisibility::Visible);
			}

			bAllyBound = true;
			break;
		}
	}
	
	if (!bAllyBound)
	{
		// 로비가 없는 데모에서는 동료 PlayerState/Character 복제 타이밍이 늦을 수 있음
		// 이번 시도에서 동료를 못 찾았다면 이전에 보였던 동료 UI를 다시 숨김
		// 로비 (2인 매칭 완료 후에만 게임에 진입) 구조로 변경 후 제거 고려
		if (AllyStatus)
		{
			AllyStatus->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (AllyName)
		{
			AllyName->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	
	constexpr float BindRetryIntervalSeconds = 0.2f;
	const bool bWithinRetryWindow = BindRetryElapsedSeconds < MaxBindRetrySeconds;
	const bool bExpectAlly = GameState && GameState->PlayerArray.Num() > 1;
	
	if (!bSelfBound || (bWithinRetryWindow && !bAllyBound) || (bExpectAlly && !bAllyBound))
	{
		// 로비가 없는 데모에서는 PlayerArray가 1명에서 2명으로 늦게 늘어날 수 있으므로 잠깐 더 재시도
		BindRetryElapsedSeconds += BindRetryIntervalSeconds;
		World->GetTimerManager().SetTimer(
			BindPlayersTimerHandle,
			this,
			&UPlayerHUDWidget::TryBindPlayer,
			BindRetryIntervalSeconds,
			false
			);
	}
	else
	{
		World->GetTimerManager().ClearTimer(BindPlayersTimerHandle);
	}
}

ADefenseCharacter* UPlayerHUDWidget::FindCharByPlayerState(APlayerState* TargetPlayerState) const
{
	if (!TargetPlayerState) return nullptr;
	
	UWorld* World = GetWorld();
	if (!World) return nullptr;
	
	for (TActorIterator<ADefenseCharacter> It(World); It; ++It)
	{
		ADefenseCharacter* Char = *It;
		if (Char && Char->GetPlayerState() == TargetPlayerState) return Char;
	}
	
	return nullptr;
}
