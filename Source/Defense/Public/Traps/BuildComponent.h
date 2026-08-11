#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Traps/Grid/TrapGridTypes.h"
#include "BuildComponent.generated.h"

class ATrapBase;
class AGridManager;
class UTrapData;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEFENSE_API UBuildComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category="Build")
	bool HasSelectedTrap() const { return GetSelectedTrapData() != nullptr; }

	UFUNCTION(BlueprintCallable, Category="Build")
	void BuildTrap();

	UFUNCTION(BlueprintCallable, Category="Build")
	void SellTrap();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trap|Build", meta=(ClampMin="1"))
	float BuildTraceRange = 5000.f;

	UPROPERTY(Transient)
	TObjectPtr<ATrapBase> TrapPreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<AGridManager> CachedGridManager;

	UTrapData* GetSelectedTrapData() const;
	APawn* GetOwnerPawn() const;

	bool TraceBuildTarget(FHitResult& OutHit) const;
	AGridManager* FindGridManager();
	void UpdateTrapPreview();
	void DestroyTrapPreview();

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestBuildTrap(const FTrapCellKey& AnchorCell);

	UFUNCTION(Server, Reliable)
	void ServerRPC_RequestSellTrap(ATrapBase* Trap);
};
