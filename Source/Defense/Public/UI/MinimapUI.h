// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MinimapUI.generated.h"

class AEnemyBase;
class ADefenseCharacter;
class ADefenseGameState;
class APawn;
class APlayerState;
class UCanvasPanel;
class UCanvasPanelSlot;
class UEnemyPoolSubsystem;
class UImage;
class UMapConfigData;
class UTexture2D;

struct FEnemyMinimapMarkerEntry
{
	TWeakObjectPtr<UImage> Image;
	TWeakObjectPtr<UCanvasPanelSlot> CanvasSlot;
	int32 ActiveIndex = INDEX_NONE;
	bool bDesiredActive = false;
	bool bStateChangeQueued = false;
};

struct FPlayerMinimapMarkerEntry
{
	TWeakObjectPtr<UCanvasPanel> MarkerRoot;
	TWeakObjectPtr<UImage> MainImage;
	TWeakObjectPtr<UImage> DirectionTipImage;
	TWeakObjectPtr<UCanvasPanelSlot> CanvasSlot;
	TWeakObjectPtr<ADefenseCharacter> Character;
	bool bIsLocalPlayer = false;
};

UCLASS()
class DEFENSE_API UMinimapUI : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Minimap")
	void SetMapConfigData(UMapConfigData* InMapConfigData);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 지정하지 않으면 GameInstance의 현재 MapConfigData를 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap", meta=(ExposeOnSpawn=true))
	TObjectPtr<UMapConfigData> MapConfigData;

	// Preview 적은 타입과 관계없이 이 색상을 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap|Marker")
	FLinearColor PreviewMarkerColor = FLinearColor(0.45f, 0.45f, 0.45f, 1.0f);

	// 비워두면 텍스처 없이 원형 Slate Brush를 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap|Marker")
	TObjectPtr<UTexture2D> EnemyMarkerTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap|Marker")
	int32 EnemyMarkerZOrder = 10;

	// 위쪽을 향한 화살표 텍스처를 기준으로 회전한다. 비워두면 방향 점이 붙은 원형 마커를 만든다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap|Player")
	TObjectPtr<UTexture2D> PlayerMarkerTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap|Player")
	FVector2D PlayerMarkerSize = FVector2D(18.0f, 18.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap|Player")
	FLinearColor LocalPlayerMarkerColor = FLinearColor(0.0f, 0.65f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap|Player")
	FLinearColor TeammateMarkerColor = FLinearColor(0.15f, 1.0f, 0.25f, 1.0f);

	// 기본값은 위쪽을 향한 텍스처를 월드 +X 방향에 맞춘다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap|Player")
	float PlayerMarkerRotationOffset = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap|Player")
	int32 PlayerMarkerZOrder = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minimap|Player")
	int32 LocalPlayerMarkerZOrder = 30;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCanvasPanel> MinimapCanvas;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> MapImage;

private:
	void ApplyMapConfigData();
	void BindEnemyRegistry();
	void UnbindEnemyRegistry();
	void BindPlayerRegistry();
	void UnbindPlayerRegistry();

	void HandleEnemyRegistered(AEnemyBase* Enemy);
	void HandleEnemyUnregistered(AEnemyBase* Enemy);
	void HandleEnemyModeChanged(AEnemyBase* Enemy);
	void ApplyEnemyMode(AEnemyBase* Enemy);

	FEnemyMinimapMarkerEntry* EnsureMarker(AEnemyBase* Enemy);
	void RemoveMarker(const TWeakObjectPtr<AEnemyBase>& EnemyKey);
	void RequestMarkerActive(AEnemyBase* Enemy, bool bActive);
	void ReconcileMarkerActiveState(const TWeakObjectPtr<AEnemyBase>& EnemyKey);
	void RemoveActiveMarker(const TWeakObjectPtr<AEnemyBase>& EnemyKey, FEnemyMinimapMarkerEntry& Entry);
	void FlushPendingMarkerChanges();
	void HandlePlayerStateAdded(APlayerState* PlayerState);
	void HandlePlayerStateRemoved(APlayerState* PlayerState);

	UFUNCTION()
	void HandlePlayerPawnSet(APlayerState* PlayerState, APawn* NewPawn, APawn* OldPawn);

	FPlayerMinimapMarkerEntry* EnsurePlayerMarker(APlayerState* PlayerState);
	void ApplyPlayerMarkerStyle(FPlayerMinimapMarkerEntry& Entry) const;
	void RemovePlayerMarker(const TWeakObjectPtr<APlayerState>& PlayerStateKey);
	bool UpdatePlayerMarker(FPlayerMinimapMarkerEntry& Entry) const;

	bool UpdateMarkerPosition(AEnemyBase* Enemy, FEnemyMinimapMarkerEntry& Entry) const;
	bool WorldToMarkerCanvas(const FVector& WorldLocation, FVector2D& OutCanvasPosition) const;
	float WorldYawToMarkerAngle(float WorldYaw, float MarkerRotationOffset) const;

	TWeakObjectPtr<UEnemyPoolSubsystem> EnemyPoolSubsystem;
	TMap<TWeakObjectPtr<AEnemyBase>, FEnemyMinimapMarkerEntry> EnemyMarkerMap;
	TArray<TWeakObjectPtr<AEnemyBase>> ActiveMarkers;
	TArray<TWeakObjectPtr<AEnemyBase>> PendingStateChanges;
	TArray<TWeakObjectPtr<AEnemyBase>> PendingMarkerRemovals;
	TWeakObjectPtr<ADefenseGameState> PlayerGameState;
	TSet<TWeakObjectPtr<APlayerState>> BoundPlayerStates;
	TMap<TWeakObjectPtr<APlayerState>, FPlayerMinimapMarkerEntry> PlayerMarkerMap;
	bool bUpdatingMarkers = false;
};
