#include "Traps/BuildComponent.h"

#include "Characters/Player/DefensePlayerState.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Equipment/LoadoutComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Traps/Grid/GridManager.h"
#include "Traps/TrapBase.h"
#include "Traps/TrapData.h"

UBuildComponent::UBuildComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UBuildComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* OwnerPawn = GetOwnerPawn();
	if (OwnerPawn && OwnerPawn->IsLocallyControlled() && HasSelectedTrap())
	{
		UpdateTrapPreview();
	}
	else
	{
		DestroyTrapPreview();
	}
}

void UBuildComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyTrapPreview();

	Super::EndPlay(EndPlayReason);
}

void UBuildComponent::BuildTrap()
{
	UTrapData* TrapData = GetSelectedTrapData();
	AGridManager* GridManager = FindGridManager();
	if (!TrapData || !GridManager)
	{
		return;
	}

	FHitResult Hit;
	FTrapCellKey CellKey;
	TArray<FTrapCellKey> FootprintCells;
	if (!TraceBuildTarget(Hit)
		|| !GridManager->TryGetCellKeyForHit(TrapData, Hit, CellKey))
	{
		return;
	}

	GridManager->GetTrapFootprintCells(TrapData, CellKey, FootprintCells);
	if (!GridManager->AreCellsAvailable(FootprintCells))
	{
		return;
	}

	if (TrapPreviewActor)
	{
		TrapPreviewActor->SetActorHiddenInGame(true);
	}

	ServerRPC_RequestBuildTrap(CellKey);
}

void UBuildComponent::SellTrap()
{
	FHitResult Hit;
	ATrapBase* Trap = TraceBuildTarget(Hit) ? Cast<ATrapBase>(Hit.GetActor()) : nullptr;
	if (!Trap)
	{
		return;
	}

	ServerRPC_RequestSellTrap(Trap);
}

UTrapData* UBuildComponent::GetSelectedTrapData() const
{
	APawn* OwnerPawn = GetOwnerPawn();
	const ULoadoutComponent* LoadoutComponent = OwnerPawn ? OwnerPawn->FindComponentByClass<ULoadoutComponent>() : nullptr;
	return LoadoutComponent ? LoadoutComponent->GetCurTrap() : nullptr;
}

APawn* UBuildComponent::GetOwnerPawn() const
{
	return Cast<APawn>(GetOwner());
}

bool UBuildComponent::TraceBuildTarget(FHitResult& OutHit) const
{
	APawn* OwnerPawn = GetOwnerPawn();
	AController* OwningController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	UWorld* World = GetWorld();
	if (!OwnerPawn || !OwningController || !World)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	OwningController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TrapBuildTrace), false, OwnerPawn);
	Params.AddIgnoredActor(OwnerPawn);
	if (TrapPreviewActor)
	{
		Params.AddIgnoredActor(TrapPreviewActor);
	}

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * BuildTraceRange;
	const bool bHit = World->LineTraceSingleByChannel(
		OutHit,
		ViewLocation,
		TraceEnd,
		ECC_Visibility,
		Params
	);

#if ENABLE_DRAW_DEBUG
	// DrawDebugLine(
	// 	World,
	// 	ViewLocation,
	// 	bHit ? OutHit.ImpactPoint : TraceEnd,
	// 	bHit ? FColor::Green : FColor::Red,
	// 	false,
	// 	0.f,
	// 	0,
	// 	0.25f
	// );

	// if (bHit)
	// {
	// 	// Crosshair Trace가 실제로 맞은 위치
	// 	DrawDebugPoint(World, OutHit.ImpactPoint, 6.f, FColor::Red, false, 0.f);
	// }
#endif

	return bHit;
}

AGridManager* UBuildComponent::FindGridManager()
{
	if (IsValid(CachedGridManager))
	{
		return CachedGridManager;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGridManager> It(World); It; ++It)
	{
		CachedGridManager = *It;
		return CachedGridManager;
	}

	return nullptr;
}

void UBuildComponent::UpdateTrapPreview()
{
	UTrapData* TrapData = GetSelectedTrapData();
	UWorld* World = GetWorld();
	APawn* OwnerPawn = GetOwnerPawn();
	if (!TrapData || !TrapData->TrapClass || !World || !OwnerPawn)
	{
		DestroyTrapPreview();
		return;
	}

	// 함정 프리뷰를 재사용 X
	if (TrapPreviewActor && TrapPreviewActor->GetSourceTrapData() != TrapData)
	{
		DestroyTrapPreview();
	}

	if (!TrapPreviewActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerPawn;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		TrapPreviewActor = World->SpawnActor<ATrapBase>(
			TrapData->TrapClass,
			OwnerPawn->GetActorLocation(),
			FRotator::ZeroRotator,
			SpawnParams
		);

		if (TrapPreviewActor)
		{
			TrapPreviewActor->SetReplicates(false);
			TrapPreviewActor->InitializePreviewTrap(TrapData);
			TrapPreviewActor->SetActorHiddenInGame(true);
		}
	}

	if (!TrapPreviewActor)
	{
		return;
	}

	FHitResult Hit;
	FTrapCellKey CellKey;
	AGridManager* GridManager = FindGridManager();
	TArray<FTrapCellKey> FootprintCells;
	if (!GridManager
		|| !TraceBuildTarget(Hit)
		|| !GridManager->TryGetCellKeyForHit(TrapData, Hit, CellKey))
	{
		TrapPreviewActor->SetActorHiddenInGame(true);
		return;
	}

	GridManager->GetTrapFootprintCells(TrapData, CellKey, FootprintCells);
	if (!GridManager->AreCellsAvailable(FootprintCells))
	{
		TrapPreviewActor->SetActorHiddenInGame(true);
		return;
	}

	const FTransform PreviewTransform = GridManager->GetTrapFootprintTransform(TrapData, CellKey);
	TrapPreviewActor->SetActorTransform(PreviewTransform);
	TrapPreviewActor->SetActorHiddenInGame(false);

#if ENABLE_DRAW_DEBUG
	// Grid가 계산한 Footprint 중심
	// DrawDebugPoint(World, PreviewTransform.GetLocation(), 10.f, FColor::Yellow, false, 0.f);
#endif
}

void UBuildComponent::DestroyTrapPreview()
{
	if (TrapPreviewActor)
	{
		TrapPreviewActor->Destroy();
		TrapPreviewActor = nullptr;
	}
}

void UBuildComponent::ServerRPC_RequestBuildTrap_Implementation(const FTrapCellKey& AnchorCell)
{
	UTrapData* TrapData = GetSelectedTrapData();
	APawn* OwnerPawn = GetOwnerPawn();
	AController* OwningController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	ADefensePlayerState* PlayerState = OwnerPawn ? OwnerPawn->GetPlayerState<ADefensePlayerState>() : nullptr;
	AGridManager* GridManager = FindGridManager();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !TrapData || !TrapData->TrapClass
		|| !OwnerPawn || !PlayerState || !GridManager)
	{
		return;
	}

	if (!GridManager->IsTrapSurfaceCompatible(TrapData, AnchorCell))
	{
		return;
	}

	const FVector PlacementCenter = GridManager->GetTrapFootprintCenter(TrapData, AnchorCell);
	if (FVector::DistSquared(OwnerPawn->GetActorLocation(), PlacementCenter) > FMath::Square(BuildTraceRange))
	{
		return;
	}

	TArray<FTrapCellKey> FootprintCells;
	GridManager->GetTrapFootprintCells(TrapData, AnchorCell, FootprintCells);
	if (!GridManager->AreCellsAvailable(FootprintCells))
	{
		return;
	}

	const int32 TrapCost = FMath::Max(0, TrapData->Cost);
	if (!PlayerState->TrySpendCoin(TrapCost))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwningController ? OwningController->GetPawn() : nullptr;
	SpawnParams.Instigator = OwningController ? OwningController->GetPawn() : nullptr;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform SpawnTransform = GridManager->GetTrapFootprintTransform(TrapData, AnchorCell);

	ATrapBase* SpawnedTrap = GetWorld()->SpawnActor<ATrapBase>(
		TrapData->TrapClass,
		SpawnTransform,
		SpawnParams
	);

	if (!SpawnedTrap || !GridManager->TryOccupyCells(FootprintCells, SpawnedTrap))
	{
		if (SpawnedTrap)
		{
			SpawnedTrap->Destroy();
		}
		PlayerState->RefundCoin(TrapCost);
		return;
	}

	SpawnedTrap->InitializePlacedTrap(TrapData, PlayerState, FootprintCells);
}

void UBuildComponent::ServerRPC_RequestSellTrap_Implementation(ATrapBase* Trap)
{
	APawn* OwnerPawn = GetOwnerPawn();
	AGridManager* GridManager = FindGridManager();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !OwnerPawn || !GridManager
		|| !GridManager->IsTrapRegistered(Trap))
	{
		return;
	}

	if (FVector::DistSquared(OwnerPawn->GetActorLocation(), Trap->GetActorLocation()) > FMath::Square(BuildTraceRange))
	{
		return;
	}

	ADefensePlayerState* RefundTarget = Trap->GetOwnerPS();
	const UTrapData* TrapData = Trap->GetSourceTrapData();
	const int32 RefundCoin = TrapData ? FMath::Max(0, TrapData->Cost) : 0;

	GridManager->ReleaseTrap(Trap);
	Trap->Destroy();

	if (RefundTarget)
	{
		RefundTarget->RefundCoin(RefundCoin);
	}
}
