#pragma once

#include "CoreMinimal.h"
#include "TrapData.h"
#include "GameFramework/Actor.h"
#include "BuildGridSurface.generated.h"

class ATrapBase;
class ADefensePlayerState;
class UBoxComponent;
class USceneComponent;
class UTrapData;

UCLASS()
class DEFENSE_API ABuildGridSurface : public AActor
{
	GENERATED_BODY()

public:
	ABuildGridSurface();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Build Grid")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Build Grid")
	TObjectPtr<UBoxComponent> BuildArea;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Build Grid")
	ETrapGridSurface SurfaceType = ETrapGridSurface::Floor;

	static constexpr float CellSize = 150.f;

	UPROPERTY()
	TMap<FIntPoint, TObjectPtr<ATrapBase>> OccupiedSlots;

	UPROPERTY(Replicated)
	TArray<FIntPoint> OccupiedGridCoords;

	bool HasBackingSurface(const FIntPoint& GridCoord) const;

public:
	bool CanPlaceTrapAt(
	const UTrapData* TrapData,
	const FVector& HitLocation,
	FIntPoint* OutGridCoord = nullptr,
	FVector* OutPlaceLocation = nullptr
) const;
	bool TryPlaceTrap(UTrapData* TrapData, const FVector& HitLocation, AController* InstigatorController, ADefensePlayerState* InstalledByPlayerState);
	bool TryRemoveTrap(const FVector& HitLocation, ADefensePlayerState** OutRefundTarget = nullptr, int32* OutRefundCoin = nullptr);
	void MarkSlotOccupiedLocally(const FVector& HitLocation);
	void MarkSlotFreeLocally(const FVector& HitLocation);

	FIntPoint WorldToGrid(const FVector& WorldLocation) const;
	FVector GridToWorldCenter(const FIntPoint& GridCoord) const;

	FORCEINLINE float GetCellSize() const { return CellSize; }
	FORCEINLINE UBoxComponent* GetBuildArea() const { return BuildArea; }
};
