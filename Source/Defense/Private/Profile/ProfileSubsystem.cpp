#include "Profile/ProfileSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Profile/ProfileSaveGame.h"

namespace
{
	const FString ProfileSlotName = TEXT("Profile");
	constexpr int32 ProfileUserIndex = 0;
}

void UProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	if (UGameplayStatics::DoesSaveGameExist(ProfileSlotName, ProfileUserIndex))
	{
		CurrentProfile = Cast<UProfileSaveGame>(
			UGameplayStatics::LoadGameFromSlot(
				ProfileSlotName,
				ProfileUserIndex));
	}
	
	if (!CurrentProfile)
	{
		CurrentProfile = Cast<UProfileSaveGame>(
			UGameplayStatics::CreateSaveGameObject(
				UProfileSaveGame::StaticClass()));
	}
}
