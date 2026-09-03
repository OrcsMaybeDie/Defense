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
	if (AllyStatus2)
	{
		AllyStatus2->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (AllyName)
	{
		AllyName->SetVisibility(ESlateVisibility::Collapsed);
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
	int32 BoundAllyCount = 0;
	
	
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
	
	TArray<UPlayerStatusWidget*> AllyWidgets;
	if (AllyStatus)
	{
		AllyWidgets.Add(AllyStatus);
	}
	if (AllyStatus2)
	{
		AllyWidgets.Add(AllyStatus2);
	}

	for (UPlayerStatusWidget* AllyWidget : AllyWidgets)
	{
		AllyWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (GameState && LocalPlayerState)
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (!PS || PS == LocalPlayerState) continue;

			const ADefensePlayerState* AllyPlayerState = Cast<ADefensePlayerState>(PS);
			if (AllyPlayerState && AllyPlayerState->GetGameRole() == EDefensePlayerRole::Spectator)
			{
				continue;
			}
			
			ADefenseCharacter* AllyChar = FindCharByPlayerState(PS);
			if (!AllyChar) continue;
			if (!AllyWidgets.IsValidIndex(BoundAllyCount))
			{
				break;
			}

			UPlayerStatusWidget* AllyWidget = AllyWidgets[BoundAllyCount++];
			AllyWidget->BindStatusComp(AllyChar->GetStatusComp());
			AllyWidget->SetVisibility(ESlateVisibility::Visible);
		}
	}
	
	constexpr float BindRetryIntervalSeconds = 0.2f;
	const bool bWithinRetryWindow = BindRetryElapsedSeconds < MaxBindRetrySeconds;
	const bool bAllConfiguredAlliesBound = BoundAllyCount >= AllyWidgets.Num();
	
	if (!bSelfBound || (bWithinRetryWindow && !bAllConfiguredAlliesBound))
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
