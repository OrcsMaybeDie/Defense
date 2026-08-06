// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/MinimapUI.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Characters/Enemy/Data/EnemyData.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Enemy/EnemyPoolSubsystem.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameManager/Data/MapConfigData.h"
#include "GameManager/DefenseGameInstance.h"
#include "GameManager/DefenseGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

void UMinimapUI::SetMapConfigData(UMapConfigData* InMapConfigData)
{
	MapConfigData = InMapConfigData;
	ApplyMapConfigData();
}

void UMinimapUI::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyMapConfigData();
	BindEnemyRegistry();
	BindPlayerRegistry();
}

void UMinimapUI::NativeDestruct()
{
	UnbindEnemyRegistry();
	UnbindPlayerRegistry();

	for (TPair<TWeakObjectPtr<AEnemyBase>, FEnemyMinimapMarkerEntry>& MarkerPair : EnemyMarkerMap)
	{
		if (UImage* MarkerImage = MarkerPair.Value.Image.Get())
		{
			MarkerImage->RemoveFromParent();
		}
	}

	EnemyMarkerMap.Empty();
	ActiveMarkers.Empty();
	PendingStateChanges.Empty();
	PendingMarkerRemovals.Empty();

	for (TPair<TWeakObjectPtr<APlayerState>, FPlayerMinimapMarkerEntry>& MarkerPair : PlayerMarkerMap)
	{
		if (UCanvasPanel* MarkerRoot = MarkerPair.Value.MarkerRoot.Get())
		{
			MarkerRoot->RemoveFromParent();
		}
	}
	PlayerMarkerMap.Empty();
	bUpdatingMarkers = false;

	Super::NativeDestruct();
}

void UMinimapUI::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	for (TPair<TWeakObjectPtr<APlayerState>, FPlayerMinimapMarkerEntry>& MarkerPair : PlayerMarkerMap)
	{
		if (!UpdatePlayerMarker(MarkerPair.Value))
		{
			if (UCanvasPanel* MarkerRoot = MarkerPair.Value.MarkerRoot.Get())
			{
				MarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	bUpdatingMarkers = true;
	for (const TWeakObjectPtr<AEnemyBase>& EnemyKey : ActiveMarkers)
	{
		AEnemyBase* Enemy = EnemyKey.Get();
		FEnemyMinimapMarkerEntry* Entry = EnemyMarkerMap.Find(EnemyKey);
		if (!Enemy || !Entry)
		{
			PendingMarkerRemovals.AddUnique(EnemyKey);
			continue;
		}

		if (!Entry->bDesiredActive)
		{
			continue;
		}

		if (UpdateMarkerPosition(Enemy, *Entry))
		{
			if (UImage* MarkerImage = Entry->Image.Get())
			{
				MarkerImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
		else if (UImage* MarkerImage = Entry->Image.Get())
		{
			MarkerImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	bUpdatingMarkers = false;

	FlushPendingMarkerChanges();
}

void UMinimapUI::ApplyMapConfigData()
{
	if (!MapConfigData)
	{
		if (const UDefenseGameInstance* DefenseGameInstance = GetGameInstance<UDefenseGameInstance>())
		{
			MapConfigData = DefenseGameInstance->GetSelectedMapConfigData();
		}
	}

	if (!MapConfigData)
	{
		return;
	}

	if (MapImage && !MapConfigData->MinimapImage.IsNull())
	{
		if (UTexture2D* MinimapTexture = MapConfigData->MinimapImage.LoadSynchronous())
		{
			MapImage->SetBrushFromTexture(MinimapTexture, false);
		}
	}

	const FVector2D DesiredSize = MapConfigData->MinimapSize;
	if (DesiredSize.X <= 0.0 || DesiredSize.Y <= 0.0)
	{
		return;
	}

	// 별도의 내부 MinimapCanvas가 있으면 그 크기를, 루트 Canvas를 사용하는
	// 단순 위젯이면 실제 MapImage 슬롯의 크기를 변경한다.
	if (MinimapCanvas)
	{
		if (UCanvasPanelSlot* MinimapSlot = Cast<UCanvasPanelSlot>(MinimapCanvas->Slot))
		{
			MinimapSlot->SetAutoSize(false);
			MinimapSlot->SetSize(DesiredSize);
			return;
		}
	}

	if (MapImage)
	{
		if (UCanvasPanelSlot* MapSlot = Cast<UCanvasPanelSlot>(MapImage->Slot))
		{
			MapSlot->SetAutoSize(false);
			MapSlot->SetSize(DesiredSize);
		}
	}
}

void UMinimapUI::BindEnemyRegistry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UEnemyPoolSubsystem* EnemyPool = World->GetSubsystem<UEnemyPoolSubsystem>();
	if (!EnemyPool)
	{
		return;
	}

	EnemyPoolSubsystem = EnemyPool;
	EnemyPool->OnEnemyRegistered.AddUObject(this, &UMinimapUI::HandleEnemyRegistered);
	EnemyPool->OnEnemyUnregistered.AddUObject(this, &UMinimapUI::HandleEnemyUnregistered);
	EnemyPool->OnEnemyModeChanged.AddUObject(this, &UMinimapUI::HandleEnemyModeChanged);

	for (const TWeakObjectPtr<AEnemyBase>& Enemy : EnemyPool->GetAllEnemies())
	{
		if (Enemy.IsValid())
		{
			ApplyEnemyMode(Enemy.Get());
		}
	}
}

void UMinimapUI::UnbindEnemyRegistry()
{
	if (UEnemyPoolSubsystem* EnemyPool = EnemyPoolSubsystem.Get())
	{
		EnemyPool->OnEnemyRegistered.RemoveAll(this);
		EnemyPool->OnEnemyUnregistered.RemoveAll(this);
		EnemyPool->OnEnemyModeChanged.RemoveAll(this);
	}

	EnemyPoolSubsystem.Reset();
}

void UMinimapUI::HandleEnemyRegistered(AEnemyBase* Enemy)
{
	ApplyEnemyMode(Enemy);
}

void UMinimapUI::HandleEnemyUnregistered(AEnemyBase* Enemy)
{
	const TWeakObjectPtr<AEnemyBase> EnemyKey(Enemy);
	if (bUpdatingMarkers)
	{
		PendingMarkerRemovals.AddUnique(EnemyKey);
		return;
	}

	RemoveMarker(EnemyKey);
}

void UMinimapUI::HandleEnemyModeChanged(AEnemyBase* Enemy)
{
	ApplyEnemyMode(Enemy);
}

void UMinimapUI::ApplyEnemyMode(AEnemyBase* Enemy)
{
	if (!IsValid(Enemy))
	{
		return;
	}

	const bool bShouldShow = Enemy->EnemyMode == EEnemyMode::Preview || Enemy->EnemyMode == EEnemyMode::Combat;
	if (!bShouldShow)
	{
		const TWeakObjectPtr<AEnemyBase> EnemyKey(Enemy);
		if (FEnemyMinimapMarkerEntry* Entry = EnemyMarkerMap.Find(EnemyKey))
		{
			if (UImage* MarkerImage = Entry->Image.Get())
			{
				MarkerImage->SetVisibility(ESlateVisibility::Collapsed);
			}
			RequestMarkerActive(Enemy, false);
		}
		return;
	}

	FEnemyMinimapMarkerEntry* Entry = EnsureMarker(Enemy);
	if (!Entry)
	{
		return;
	}

	const FLinearColor MarkerColor = Enemy->EnemyMode == EEnemyMode::Preview
		? PreviewMarkerColor
		: (Enemy->EnemyData ? Enemy->EnemyData->Color : FLinearColor::Red);

	if (UImage* MarkerImage = Entry->Image.Get())
	{
		MarkerImage->SetColorAndOpacity(MarkerColor);
	}

	RequestMarkerActive(Enemy, true);
	if (UpdateMarkerPosition(Enemy, *Entry))
	{
		if (UImage* MarkerImage = Entry->Image.Get())
		{
			MarkerImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

FEnemyMinimapMarkerEntry* UMinimapUI::EnsureMarker(AEnemyBase* Enemy)
{
	if (!IsValid(Enemy) || !MinimapCanvas || !WidgetTree)
	{
		return nullptr;
	}

	const TWeakObjectPtr<AEnemyBase> EnemyKey(Enemy);
	if (FEnemyMinimapMarkerEntry* ExistingEntry = EnemyMarkerMap.Find(EnemyKey))
	{
		return ExistingEntry;
	}

	const FVector2D MarkerSize = Enemy->EnemyData
		? Enemy->EnemyData->Size
		: FVector2D(6.0f, 6.0f);

	UImage* MarkerImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (!MarkerImage)
	{
		return nullptr;
	}

	if (EnemyMarkerTexture)
	{
		MarkerImage->SetBrushFromTexture(EnemyMarkerTexture, false);
	}
	else
	{
		const float MarkerRadius = static_cast<float>(FMath::Min(MarkerSize.X, MarkerSize.Y) * 0.5);
		MarkerImage->SetBrush(FSlateRoundedBoxBrush(
			FLinearColor::White,
			MarkerRadius,
			FVector2f(static_cast<float>(MarkerSize.X), static_cast<float>(MarkerSize.Y))));
	}

	MarkerImage->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* MarkerSlot = MinimapCanvas->AddChildToCanvas(MarkerImage);
	if (!MarkerSlot)
	{
		return nullptr;
	}

	MarkerSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	MarkerSlot->SetAutoSize(false);
	MarkerSlot->SetSize(MarkerSize);
	MarkerSlot->SetZOrder(EnemyMarkerZOrder);

	FEnemyMinimapMarkerEntry NewEntry;
	NewEntry.Image = MarkerImage;
	NewEntry.CanvasSlot = MarkerSlot;
	return &EnemyMarkerMap.Add(EnemyKey, MoveTemp(NewEntry));
}

void UMinimapUI::RemoveMarker(const TWeakObjectPtr<AEnemyBase>& EnemyKey)
{
	FEnemyMinimapMarkerEntry* Entry = EnemyMarkerMap.Find(EnemyKey);
	if (!Entry)
	{
		return;
	}

	if (Entry->ActiveIndex != INDEX_NONE)
	{
		RemoveActiveMarker(EnemyKey, *Entry);
	}

	if (UImage* MarkerImage = Entry->Image.Get())
	{
		MarkerImage->RemoveFromParent();
	}

	EnemyMarkerMap.Remove(EnemyKey);
}

void UMinimapUI::RequestMarkerActive(AEnemyBase* Enemy, const bool bActive)
{
	const TWeakObjectPtr<AEnemyBase> EnemyKey(Enemy);
	FEnemyMinimapMarkerEntry* Entry = EnemyMarkerMap.Find(EnemyKey);
	if (!Entry)
	{
		return;
	}

	Entry->bDesiredActive = bActive;
	if (!bActive)
	{
		if (UImage* MarkerImage = Entry->Image.Get())
		{
			MarkerImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (bUpdatingMarkers)
	{
		if (!Entry->bStateChangeQueued)
		{
			Entry->bStateChangeQueued = true;
			PendingStateChanges.Add(EnemyKey);
		}
		return;
	}

	ReconcileMarkerActiveState(EnemyKey);
}

void UMinimapUI::ReconcileMarkerActiveState(const TWeakObjectPtr<AEnemyBase>& EnemyKey)
{
	FEnemyMinimapMarkerEntry* Entry = EnemyMarkerMap.Find(EnemyKey);
	if (!Entry)
	{
		return;
	}

	Entry->bStateChangeQueued = false;
	if (Entry->bDesiredActive && Entry->ActiveIndex == INDEX_NONE)
	{
		Entry->ActiveIndex = ActiveMarkers.Add(EnemyKey);
	}
	else if (!Entry->bDesiredActive && Entry->ActiveIndex != INDEX_NONE)
	{
		RemoveActiveMarker(EnemyKey, *Entry);
	}
}

void UMinimapUI::RemoveActiveMarker(
	const TWeakObjectPtr<AEnemyBase>& EnemyKey,
	FEnemyMinimapMarkerEntry& Entry)
{
	const int32 RemoveIndex = Entry.ActiveIndex;
	if (!ActiveMarkers.IsValidIndex(RemoveIndex))
	{
		Entry.ActiveIndex = INDEX_NONE;
		return;
	}

	const int32 LastIndex = ActiveMarkers.Num() - 1;
	const TWeakObjectPtr<AEnemyBase> MovedEnemyKey = ActiveMarkers[LastIndex];
	ActiveMarkers.RemoveAtSwap(RemoveIndex, EAllowShrinking::No);
	Entry.ActiveIndex = INDEX_NONE;

	if (RemoveIndex != LastIndex)
	{
		if (FEnemyMinimapMarkerEntry* MovedEntry = EnemyMarkerMap.Find(MovedEnemyKey))
		{
			MovedEntry->ActiveIndex = RemoveIndex;
		}
	}
}

void UMinimapUI::FlushPendingMarkerChanges()
{
	TArray<TWeakObjectPtr<AEnemyBase>> StateChanges = MoveTemp(PendingStateChanges);
	PendingStateChanges.Reset();
	for (const TWeakObjectPtr<AEnemyBase>& EnemyKey : StateChanges)
	{
		ReconcileMarkerActiveState(EnemyKey);
	}

	TArray<TWeakObjectPtr<AEnemyBase>> MarkerRemovals = MoveTemp(PendingMarkerRemovals);
	PendingMarkerRemovals.Reset();
	for (const TWeakObjectPtr<AEnemyBase>& EnemyKey : MarkerRemovals)
	{
		RemoveMarker(EnemyKey);
	}
}

void UMinimapUI::BindPlayerRegistry()
{
	UWorld* World = GetWorld();
	ADefenseGameState* GameState = World ? World->GetGameState<ADefenseGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	PlayerGameState = GameState;
	GameState->OnPlayerStateAdded.AddUObject(this, &UMinimapUI::HandlePlayerStateAdded);
	GameState->OnPlayerStateRemoved.AddUObject(this, &UMinimapUI::HandlePlayerStateRemoved);

	// 위젯보다 먼저 생성된 플레이어는 처음 한 번만 등록한다.
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		HandlePlayerStateAdded(PlayerState);
	}
}

void UMinimapUI::UnbindPlayerRegistry()
{
	if (ADefenseGameState* GameState = PlayerGameState.Get())
	{
		GameState->OnPlayerStateAdded.RemoveAll(this);
		GameState->OnPlayerStateRemoved.RemoveAll(this);
	}

	for (const TWeakObjectPtr<APlayerState>& PlayerStateKey : BoundPlayerStates)
	{
		if (APlayerState* PlayerState = PlayerStateKey.Get())
		{
			PlayerState->OnPawnSet.RemoveDynamic(this, &UMinimapUI::HandlePlayerPawnSet);
		}
	}

	BoundPlayerStates.Empty();
	PlayerGameState.Reset();
}

void UMinimapUI::HandlePlayerStateAdded(APlayerState* PlayerState)
{
	if (!IsValid(PlayerState))
	{
		return;
	}

	const TWeakObjectPtr<APlayerState> PlayerStateKey(PlayerState);
	if (!BoundPlayerStates.Contains(PlayerStateKey))
	{
		BoundPlayerStates.Add(PlayerStateKey);
		PlayerState->OnPawnSet.AddUniqueDynamic(this, &UMinimapUI::HandlePlayerPawnSet);
	}

	HandlePlayerPawnSet(PlayerState, PlayerState->GetPawn(), nullptr);
}

void UMinimapUI::HandlePlayerStateRemoved(APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	PlayerState->OnPawnSet.RemoveDynamic(this, &UMinimapUI::HandlePlayerPawnSet);
	const TWeakObjectPtr<APlayerState> PlayerStateKey(PlayerState);
	BoundPlayerStates.Remove(PlayerStateKey);
	RemovePlayerMarker(PlayerStateKey);
}

void UMinimapUI::HandlePlayerPawnSet(APlayerState* PlayerState, APawn* NewPawn, APawn* OldPawn)
{
	if (!IsValid(PlayerState))
	{
		return;
	}

	ADefenseCharacter* Character = Cast<ADefenseCharacter>(NewPawn);
	const TWeakObjectPtr<APlayerState> PlayerStateKey(PlayerState);
	if (!Character)
	{
		if (FPlayerMinimapMarkerEntry* Entry = PlayerMarkerMap.Find(PlayerStateKey))
		{
			Entry->Character.Reset();
			if (UCanvasPanel* MarkerRoot = Entry->MarkerRoot.Get())
			{
				MarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		return;
	}

	FPlayerMinimapMarkerEntry* Entry = EnsurePlayerMarker(PlayerState);
	if (!Entry)
	{
		return;
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	Entry->Character = Character;
	Entry->bIsLocalPlayer = Character->IsLocallyControlled()
		|| (OwningPlayer && PlayerState == OwningPlayer->PlayerState);
	ApplyPlayerMarkerStyle(*Entry);
}

FPlayerMinimapMarkerEntry* UMinimapUI::EnsurePlayerMarker(APlayerState* PlayerState)
{
	if (!IsValid(PlayerState) || !MinimapCanvas || !WidgetTree)
	{
		return nullptr;
	}

	const TWeakObjectPtr<APlayerState> PlayerStateKey(PlayerState);
	if (FPlayerMinimapMarkerEntry* ExistingEntry = PlayerMarkerMap.Find(PlayerStateKey))
	{
		return ExistingEntry;
	}

	const FVector2D MarkerSize(
		FMath::Max(PlayerMarkerSize.X, 1.0),
		FMath::Max(PlayerMarkerSize.Y, 1.0));

	UCanvasPanel* MarkerRoot = WidgetTree->ConstructWidget<UCanvasPanel>();
	if (!MarkerRoot)
	{
		return nullptr;
	}

	UCanvasPanelSlot* MarkerSlot = MinimapCanvas->AddChildToCanvas(MarkerRoot);
	if (!MarkerSlot)
	{
		return nullptr;
	}

	MarkerSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	MarkerSlot->SetAutoSize(false);
	MarkerSlot->SetSize(MarkerSize);
	MarkerSlot->SetZOrder(PlayerMarkerZOrder);
	MarkerRoot->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	MarkerRoot->SetVisibility(ESlateVisibility::Collapsed);

	UImage* MainImage = WidgetTree->ConstructWidget<UImage>();
	if (!MainImage)
	{
		MarkerRoot->RemoveFromParent();
		return nullptr;
	}

	UCanvasPanelSlot* MainImageSlot = MarkerRoot->AddChildToCanvas(MainImage);
	if (!MainImageSlot)
	{
		MarkerRoot->RemoveFromParent();
		return nullptr;
	}

	UImage* DirectionTipImage = nullptr;
	if (PlayerMarkerTexture)
	{
		MainImage->SetBrushFromTexture(PlayerMarkerTexture, false);
		MainImageSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		MainImageSlot->SetOffsets(FMargin(0.0f));
	}
	else
	{
		const float MinMarkerSize = static_cast<float>(FMath::Min(MarkerSize.X, MarkerSize.Y));
		const FVector2D BodySize = MarkerSize * 0.65;
		const float BodyRadius = static_cast<float>(FMath::Min(BodySize.X, BodySize.Y) * 0.5);
		MainImage->SetBrush(FSlateRoundedBoxBrush(
			FLinearColor::White,
			BodyRadius,
			FVector2f(static_cast<float>(BodySize.X), static_cast<float>(BodySize.Y))));
		MainImageSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		MainImageSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MainImageSlot->SetPosition(MarkerSize * 0.5);
		MainImageSlot->SetSize(BodySize);

		DirectionTipImage = WidgetTree->ConstructWidget<UImage>();
		if (DirectionTipImage)
		{
			const FVector2D TipSize(MinMarkerSize * 0.28f, MinMarkerSize * 0.28f);
			const float TipRadius = static_cast<float>(TipSize.X * 0.5);
			DirectionTipImage->SetBrush(FSlateRoundedBoxBrush(
				FLinearColor::White,
				TipRadius,
				FVector2f(static_cast<float>(TipSize.X), static_cast<float>(TipSize.Y))));

			if (UCanvasPanelSlot* TipSlot = MarkerRoot->AddChildToCanvas(DirectionTipImage))
			{
				TipSlot->SetAnchors(FAnchors(0.0f, 0.0f));
				TipSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				TipSlot->SetPosition(FVector2D(MarkerSize.X * 0.88, MarkerSize.Y * 0.5));
				TipSlot->SetSize(TipSize);
			}
			else
			{
				DirectionTipImage = nullptr;
			}
		}
	}

	FPlayerMinimapMarkerEntry NewEntry;
	NewEntry.MarkerRoot = MarkerRoot;
	NewEntry.MainImage = MainImage;
	NewEntry.DirectionTipImage = DirectionTipImage;
	NewEntry.CanvasSlot = MarkerSlot;
	return &PlayerMarkerMap.Add(PlayerStateKey, MoveTemp(NewEntry));
}

void UMinimapUI::ApplyPlayerMarkerStyle(FPlayerMinimapMarkerEntry& Entry) const
{
	const FLinearColor MarkerColor = Entry.bIsLocalPlayer
		? LocalPlayerMarkerColor
		: TeammateMarkerColor;

	if (UCanvasPanelSlot* MarkerSlot = Entry.CanvasSlot.Get())
	{
		MarkerSlot->SetZOrder(Entry.bIsLocalPlayer
			? LocalPlayerMarkerZOrder
			: PlayerMarkerZOrder);
	}

	if (UImage* MainImage = Entry.MainImage.Get())
	{
		MainImage->SetColorAndOpacity(MarkerColor);
	}

	if (UImage* DirectionTipImage = Entry.DirectionTipImage.Get())
	{
		DirectionTipImage->SetColorAndOpacity(MarkerColor);
	}
}

void UMinimapUI::RemovePlayerMarker(const TWeakObjectPtr<APlayerState>& PlayerStateKey)
{
	if (FPlayerMinimapMarkerEntry* Entry = PlayerMarkerMap.Find(PlayerStateKey))
	{
		if (UCanvasPanel* MarkerRoot = Entry->MarkerRoot.Get())
		{
			MarkerRoot->RemoveFromParent();
		}
	}

	PlayerMarkerMap.Remove(PlayerStateKey);
}

bool UMinimapUI::UpdatePlayerMarker(FPlayerMinimapMarkerEntry& Entry) const
{
	ADefenseCharacter* Character = Entry.Character.Get();
	UCanvasPanel* MarkerRoot = Entry.MarkerRoot.Get();
	UCanvasPanelSlot* MarkerSlot = Entry.CanvasSlot.Get();
	if (!IsValid(Character) || !MarkerRoot || !MarkerSlot)
	{
		return false;
	}

	FVector2D CanvasPosition;
	if (!WorldToMarkerCanvas(Character->GetActorLocation(), CanvasPosition))
	{
		return false;
	}

	MarkerSlot->SetPosition(CanvasPosition);
	const float TextureRotationOffset = Entry.DirectionTipImage.IsValid()
		? 0.0f
		: PlayerMarkerRotationOffset;
	MarkerRoot->SetRenderTransformAngle(WorldYawToMarkerAngle(
		Character->GetActorRotation().Yaw,
		TextureRotationOffset));
	MarkerRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

bool UMinimapUI::UpdateMarkerPosition(AEnemyBase* Enemy, FEnemyMinimapMarkerEntry& Entry) const
{
	UCanvasPanelSlot* MarkerSlot = Entry.CanvasSlot.Get();
	if (!Enemy || !MarkerSlot)
	{
		return false;
	}

	FVector2D CanvasPosition;
	if (!WorldToMarkerCanvas(Enemy->GetActorLocation(), CanvasPosition))
	{
		return false;
	}

	MarkerSlot->SetPosition(CanvasPosition);
	return true;
}

bool UMinimapUI::WorldToMarkerCanvas(
	const FVector& WorldLocation,
	FVector2D& OutCanvasPosition) const
{
	if (!MapConfigData || !MinimapCanvas || !MapImage)
	{
		return false;
	}

	const FVector2D WorldRange = MapConfigData->MinimapWorldMax - MapConfigData->MinimapWorldMin;
	if (FMath::IsNearlyZero(WorldRange.X) || FMath::IsNearlyZero(WorldRange.Y))
	{
		return false;
	}

	const FVector2D NormalizedPosition(
		(WorldLocation.X - MapConfigData->MinimapWorldMin.X) / WorldRange.X,
		(WorldLocation.Y - MapConfigData->MinimapWorldMin.Y) / WorldRange.Y);

	if (NormalizedPosition.X < 0.0 || NormalizedPosition.X > 1.0
		|| NormalizedPosition.Y < 0.0 || NormalizedPosition.Y > 1.0)
	{
		return false;
	}

	const FGeometry& MapGeometry = MapImage->GetCachedGeometry();
	const FGeometry& CanvasGeometry = MinimapCanvas->GetCachedGeometry();
	const FVector2D MapSize = MapGeometry.GetLocalSize();
	if (MapSize.X <= 0.0 || MapSize.Y <= 0.0)
	{
		return false;
	}

	// 이미지 UV 기준으로 변환한 뒤 맵별 축 교환/반전 옵션을 적용한다.
	FVector2D MinimapUV(NormalizedPosition.X, 1.0 - NormalizedPosition.Y);
	if (MapConfigData->bSwapMinimapAxes)
	{
		Swap(MinimapUV.X, MinimapUV.Y);
	}
	if (MapConfigData->bInvertMinimapX)
	{
		MinimapUV.X = 1.0 - MinimapUV.X;
	}
	if (MapConfigData->bInvertMinimapY)
	{
		MinimapUV.Y = 1.0 - MinimapUV.Y;
	}

	const FVector2D MapLocalPosition(
		MinimapUV.X * MapSize.X,
		MinimapUV.Y * MapSize.Y);

	const FVector2D AbsolutePosition = MapGeometry.LocalToAbsolute(MapLocalPosition);
	OutCanvasPosition = CanvasGeometry.AbsoluteToLocal(AbsolutePosition);
	return true;
}

float UMinimapUI::WorldYawToMarkerAngle(
	const float WorldYaw,
	const float MarkerRotationOffset) const
{
	if (!MapConfigData)
	{
		return MarkerRotationOffset - WorldYaw;
	}

	const float YawRadians = FMath::DegreesToRadians(WorldYaw);
	FVector2D MinimapDirection(FMath::Cos(YawRadians), -FMath::Sin(YawRadians));

	if (MapConfigData->bSwapMinimapAxes)
	{
		Swap(MinimapDirection.X, MinimapDirection.Y);
	}
	if (MapConfigData->bInvertMinimapX)
	{
		MinimapDirection.X *= -1.0;
	}
	if (MapConfigData->bInvertMinimapY)
	{
		MinimapDirection.Y *= -1.0;
	}

	const float DirectionAngle = FMath::RadiansToDegrees(
		FMath::Atan2(MinimapDirection.Y, MinimapDirection.X));
	return MarkerRotationOffset + DirectionAngle;
}
