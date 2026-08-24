// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/BackgroundMusicActor.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameManager/DefenseGameState.h"
#include "Sound/SoundBase.h"

ABackgroundMusicActor::ABackgroundMusicActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DefaultMusicComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("DefaultMusicComponent"));
	DefaultMusicComponent->SetupAttachment(SceneRoot);
	DefaultMusicComponent->bAutoActivate = false;
	DefaultMusicComponent->bAllowSpatialization = false;
	DefaultMusicComponent->bIsUISound = true;

	ClearMusicComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ClearMusicComponent"));
	ClearMusicComponent->SetupAttachment(SceneRoot);
	ClearMusicComponent->bAutoActivate = false;
	ClearMusicComponent->bAllowSpatialization = false;
	ClearMusicComponent->bIsUISound = true;
}

void ABackgroundMusicActor::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (bAutoPlay)
	{
		PlayDefaultMusic();
	}

	if (!bSwitchOnGameClear)
	{
		return;
	}

	BoundGameState = GetWorld() ? GetWorld()->GetGameState<ADefenseGameState>() : nullptr;
	if (BoundGameState)
	{
		BoundGameState->OnGameClearChanged.AddUniqueDynamic(this, &ABackgroundMusicActor::HandleGameClearChanged);

		// 늦게 참가했거나 복제가 먼저 완료된 경우에도 현재 결과를 즉시 반영합니다.
		HandleGameClearChanged(BoundGameState->IsGameClear());
	}
}

void ABackgroundMusicActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundGameState)
	{
		BoundGameState->OnGameClearChanged.RemoveDynamic(this, &ABackgroundMusicActor::HandleGameClearChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void ABackgroundMusicActor::PlayDefaultMusic()
{
	if (!DefaultMusicComponent || !DefaultMusic)
	{
		return;
	}

	bClearMusicPlayed = false;
	ClearMusicComponent->Stop();
	DefaultMusicComponent->SetSound(DefaultMusic);
	DefaultMusicComponent->SetVolumeMultiplier(DefaultMusicVolume);
	DefaultMusicComponent->Play();
}

void ABackgroundMusicActor::PlayClearMusic()
{
	if (bClearMusicPlayed || !ClearMusicComponent || !ClearMusic)
	{
		return;
	}

	bClearMusicPlayed = true;
	ClearMusicComponent->SetSound(ClearMusic);

	if (CrossfadeDuration > 0.0f)
	{
		if (DefaultMusicComponent && DefaultMusicComponent->IsPlaying())
		{
			DefaultMusicComponent->FadeOut(CrossfadeDuration, 0.0f);
		}

		ClearMusicComponent->FadeIn(CrossfadeDuration, ClearMusicVolume);
		return;
	}

	if (DefaultMusicComponent)
	{
		DefaultMusicComponent->Stop();
	}

	ClearMusicComponent->SetVolumeMultiplier(ClearMusicVolume);
	ClearMusicComponent->Play();
}

void ABackgroundMusicActor::StopMusic(float FadeOutDuration)
{
	if (FadeOutDuration > 0.0f)
	{
		if (DefaultMusicComponent && DefaultMusicComponent->IsPlaying())
		{
			DefaultMusicComponent->FadeOut(FadeOutDuration, 0.0f);
		}

		if (ClearMusicComponent && ClearMusicComponent->IsPlaying())
		{
			ClearMusicComponent->FadeOut(FadeOutDuration, 0.0f);
		}
		return;
	}

	if (DefaultMusicComponent)
	{
		DefaultMusicComponent->Stop();
	}

	if (ClearMusicComponent)
	{
		ClearMusicComponent->Stop();
	}
}

void ABackgroundMusicActor::HandleGameClearChanged(bool bGameClear)
{
	if (bGameClear)
	{
		PlayClearMusic();
	}
}
