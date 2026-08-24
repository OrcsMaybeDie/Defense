// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BackgroundMusicActor.generated.h"

class ADefenseGameState;
class UAudioComponent;
class USceneComponent;
class USoundBase;

/**
 * 맵에 배치하여 기본 BGM을 재생하고, 게임 클리어 시 클리어 BGM으로 전환하는 액터입니다.
 * 인트로 맵에서는 ClearMusic을 비워두면 기본 BGM만 재생할 수 있습니다.
 */
UCLASS(Blueprintable)
class DEFENSE_API ABackgroundMusicActor : public AActor
{
	GENERATED_BODY()

public:
	ABackgroundMusicActor();

	UFUNCTION(BlueprintCallable, Category="BGM")
	void PlayDefaultMusic();

	UFUNCTION(BlueprintCallable, Category="BGM")
	void PlayClearMusic();

	UFUNCTION(BlueprintCallable, Category="BGM")
	void StopMusic(float FadeOutDuration = 0.0f);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BGM")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BGM")
	TObjectPtr<UAudioComponent> DefaultMusicComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BGM")
	TObjectPtr<UAudioComponent> ClearMusicComponent;

	/** 반복 재생이 필요하면 지정하는 Sound Wave, Sound Cue 또는 MetaSound에서 루프를 설정해야 합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM|Music")
	TObjectPtr<USoundBase> DefaultMusic;

	/** 인트로 맵처럼 클리어 음악이 필요 없는 경우 비워둘 수 있습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM|Music")
	TObjectPtr<USoundBase> ClearMusic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM|Playback")
	bool bAutoPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM|Playback")
	bool bSwitchOnGameClear = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM|Playback", meta=(ClampMin="0.0", Units="s"))
	float CrossfadeDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM|Playback", meta=(ClampMin="0.0", UIMax="1.0"))
	float DefaultMusicVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM|Playback", meta=(ClampMin="0.0", UIMax="1.0"))
	float ClearMusicVolume = 1.0f;

private:
	UFUNCTION()
	void HandleGameClearChanged(bool bGameClear);

	UPROPERTY()
	TObjectPtr<ADefenseGameState> BoundGameState;

	bool bClearMusicPlayed = false;
};
