#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProfileSubsystem.generated.h"

class UProfileSaveGame;

UCLASS()
class DEFENSE_API UProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	const UProfileSaveGame* GetCurrentProfile() const { return CurrentProfile; }
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UProfileSaveGame> CurrentProfile;
};
