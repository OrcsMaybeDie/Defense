#include "Traps/BuildGridSurface.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Characters/Player/DefensePlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Traps/TrapBase.h"
#include "Traps/TrapData.h"

ABuildGridSurface::ABuildGridSurface()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BuildArea = CreateDefaultSubobject<UBoxComponent>(TEXT("BuildArea"));
	BuildArea->SetupAttachment(SceneRoot);
	BuildArea->SetBoxExtent(FVector(500.f, 500.f, 20.f));
	BuildArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BuildArea->SetCollisionObjectType(ECC_WorldStatic);
	BuildArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	BuildArea->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ABuildGridSurface::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABuildGridSurface, OccupiedGridCoords);
}

bool ABuildGridSurface::CanPlaceTrapAt(const UTrapData* TrapData, const FVector& HitLocation, FIntPoint* OutGridCoord,
	FVector* OutPlaceLocation) const
{
	if (!TrapData) return false;
	if (TrapData->GridSurface != SurfaceType) return false;
	
	const FIntPoint GridCoord = WorldToGrid(HitLocation);
	if (OutGridCoord)
	{
		*OutGridCoord = GridCoord;
	}

	if (OutPlaceLocation)
	{
		*OutPlaceLocation = GridToWorldCenter(GridCoord);
	}

	return !OccupiedGridCoords.Contains(GridCoord);
}

bool ABuildGridSurface::TryPlaceTrap(UTrapData* TrapData, const FVector& HitLocation, AController* InstigatorController, ADefensePlayerState* InstalledByPlayerState)
{
	if (!HasAuthority() || !TrapData || !TrapData->TrapClass || !InstalledByPlayerState) return false;

	FIntPoint GridCoord;
	FVector PlaceLocation;
	if (!CanPlaceTrapAt(TrapData, HitLocation, &GridCoord, &PlaceLocation))
	{
		return false;
	}

	OccupiedGridCoords.AddUnique(GridCoord);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstigatorController ? InstigatorController->GetPawn() : nullptr;
	SpawnParams.Instigator = InstigatorController ? InstigatorController->GetPawn() : nullptr;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATrapBase* SpawnedTrap = GetWorld()->SpawnActor<ATrapBase>(
		TrapData->TrapClass,
		PlaceLocation,
		GetActorRotation(),
		SpawnParams
	);

	if (!SpawnedTrap)
	{
		OccupiedGridCoords.Remove(GridCoord);
		return false;
	}

	SpawnedTrap->InitializePlacedTrap(TrapData, InstalledByPlayerState);
	OccupiedSlots.Add(GridCoord, SpawnedTrap);
	ForceNetUpdate();
	return true;
}

bool ABuildGridSurface::TryRemoveTrap(const FVector& HitLocation, ADefensePlayerState** OutRefundTarget, int32* OutRefundCoin)
{
	if (!HasAuthority()) return false;

	if (OutRefundTarget)
	{
		*OutRefundTarget = nullptr;
	}

	if (OutRefundCoin)
	{
		*OutRefundCoin = 0;
	}

	const FIntPoint GridCoord = WorldToGrid(HitLocation);
	TObjectPtr<ATrapBase>* ExistingTrap = OccupiedSlots.Find(GridCoord);
	if (!ExistingTrap || !IsValid(ExistingTrap->Get()))
	{
		OccupiedSlots.Remove(GridCoord);
		OccupiedGridCoords.Remove(GridCoord);
		ForceNetUpdate();
		return false;
	}

	ATrapBase* Trap = ExistingTrap->Get();
	if (OutRefundTarget)
	{
		*OutRefundTarget = Trap->GetOwnerPS();
	}

	if (OutRefundCoin)
	{
		const UTrapData* TrapData = Trap->GetSourceTrapData();
		*OutRefundCoin = TrapData ? FMath::Max(0, TrapData->Cost) : 0;
	}

	Trap->Destroy();
	OccupiedSlots.Remove(GridCoord);
	OccupiedGridCoords.Remove(GridCoord);
	ForceNetUpdate();
	return true;
}

void ABuildGridSurface::MarkSlotOccupiedLocally(const FVector& HitLocation)
{
	if (HasAuthority()) return;

	const FIntPoint GridCoord = WorldToGrid(HitLocation);
	OccupiedGridCoords.AddUnique(GridCoord);
}

void ABuildGridSurface::MarkSlotFreeLocally(const FVector& HitLocation)
{
	if (HasAuthority()) return;

	const FIntPoint GridCoord = WorldToGrid(HitLocation);
	OccupiedGridCoords.Remove(GridCoord);
}

FIntPoint ABuildGridSurface::WorldToGrid(const FVector& WorldLocation) const
{
	const FVector LocalLocation = GetActorRotation().UnrotateVector(WorldLocation - GetActorLocation());
	return FIntPoint(
		FMath::FloorToInt(LocalLocation.X / CellSize),
		FMath::FloorToInt(LocalLocation.Y / CellSize)
	);
}

FVector ABuildGridSurface::GridToWorldCenter(const FIntPoint& GridCoord) const
{
	const float SurfaceHeight = BuildArea ? BuildArea->GetScaledBoxExtent().Z : 0.f;
	const FVector LocalCenter(
		(GridCoord.X + 0.5f) * CellSize,
		(GridCoord.Y + 0.5f) * CellSize,
		SurfaceHeight
	);

	return GetActorLocation() + GetActorRotation().RotateVector(LocalCenter);
}
