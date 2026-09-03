// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MapConfigData.generated.h"

class UMissionData;

UCLASS()
class DEFENSE_API UMapConfigData : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TSoftObjectPtr<class UTexture2D> Thumbnail;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	TSoftObjectPtr<class UTexture2D> MinimapImage;

	// 미니맵 이미지의 왼쪽 아래에 대응하는 월드 XY 좌표
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	FVector2D MinimapWorldMin = FVector2D(-5000.0f, -5000.0f);

	// 미니맵 이미지의 오른쪽 위에 대응하는 월드 XY 좌표
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	FVector2D MinimapWorldMax = FVector2D(5000.0f, 5000.0f);

	// 화면에 표시할 미니맵 영역의 크기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap", meta = (ClampMin = "1.0"))
	FVector2D MinimapSize = FVector2D(400.0f, 300.0f);

	// 월드 X/Y가 이미지의 세로/가로에 대응할 때 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap|Orientation")
	bool bSwapMinimapAxes = false;

	// 미니맵 이미지의 가로 방향을 반전
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap|Orientation")
	bool bInvertMinimapX = false;

	// 미니맵 이미지의 세로 방향을 반전
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap|Orientation")
	bool bInvertMinimapY = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	bool bNotReady = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TSoftObjectPtr<class UWorld> GameMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	int32 InitialDestScore = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
	int32 InitCoin = 3000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	TArray<TObjectPtr<UMissionData>> Missions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<class UWaveData> WaveData;
};
