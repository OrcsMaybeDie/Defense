#include "Traps/BuildComponent.h"

#include "Equipment/LoadoutComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Traps/BuildGridSurface.h"
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
	if (OwnerPawn && OwnerPawn->IsLocallyControlled() && bBuildMode)
	{
		UpdateTrapPreview();
	}
}

void UBuildComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyTrapPreview();

	Super::EndPlay(EndPlayReason);
}

void UBuildComponent::ToggleBuildMode()
{
	bBuildMode = !bBuildMode;

	if (!bBuildMode)
	{
		DestroyTrapPreview();
	}
}

void UBuildComponent::BuildTrap()
{
	UTrapData* TrapData = GetSelectedTrapData();
	if (!bBuildMode || !TrapData) return;

	FHitResult Hit;
	ABuildGridSurface* BuildSurface = nullptr;
	if (!TraceBuildTarget(Hit, BuildSurface) || !BuildSurface)
	{
		return;
	}

	const bool bCanPlace = BuildSurface->CanPlaceTrapAt(Hit.ImpactPoint);
	if (!bCanPlace)
	{
		return;
	}

	BuildSurface->MarkSlotOccupiedLocally(Hit.ImpactPoint);
	if (TrapPreviewActor)
	{
		TrapPreviewActor->SetActorHiddenInGame(true);
	}

	ServerRPC_RequestBuildTrap(BuildSurface, Hit.ImpactPoint);
}

void UBuildComponent::SellTrap()
{
	if (!bBuildMode) return;

	FHitResult Hit;
	ABuildGridSurface* BuildSurface = nullptr;
	if (!TraceBuildTarget(Hit, BuildSurface) || !BuildSurface)
	{
		return;
	}

	BuildSurface->MarkSlotFreeLocally(Hit.ImpactPoint);
	ServerRPC_RequestSellTrap(BuildSurface, Hit.ImpactPoint);
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

bool UBuildComponent::TraceBuildTarget(FHitResult& OutHit, ABuildGridSurface*& OutBuildSurface) const
{
	OutBuildSurface = nullptr;

	APawn* OwnerPawn = GetOwnerPawn();
	AController* OwningController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	UWorld* World = GetWorld();
	if (!OwnerPawn || !OwningController || !World) return false;

	FVector ViewLocation;
	FRotator ViewRotation;
	OwningController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector Start = ViewLocation;
	const FVector End = Start + ViewRotation.Vector() * BuildTraceRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TrapBuildTrace), false, OwnerPawn);
	Params.AddIgnoredActor(OwnerPawn);
	if (TrapPreviewActor)
	{
		Params.AddIgnoredActor(TrapPreviewActor);
	}

	const bool bHit = World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
	if (!bHit) return false;

	OutBuildSurface = Cast<ABuildGridSurface>(OutHit.GetActor());
	return true;
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
			TrapPreviewActor->SetPreviewMode(true);
			TrapPreviewActor->SetActorHiddenInGame(true);
		}
	}

	if (!TrapPreviewActor) return;

	FHitResult Hit;
	ABuildGridSurface* BuildSurface = nullptr;
	if (!TraceBuildTarget(Hit, BuildSurface))
	{
		if (!TrapPreviewActor->IsHidden())
		{
			TrapPreviewActor->SetActorHiddenInGame(true);
		}
		return;
	}

	if (!BuildSurface)
	{
		if (!TrapPreviewActor->IsHidden())
		{
			TrapPreviewActor->SetActorHiddenInGame(true);
		}
		return;
	}

	FVector PreviewLocation = Hit.ImpactPoint;
	const bool bCanPlace = BuildSurface->CanPlaceTrapAt(Hit.ImpactPoint, nullptr, &PreviewLocation);
	if (!bCanPlace)
	{
		if (!TrapPreviewActor->IsHidden())
		{
			TrapPreviewActor->SetActorHiddenInGame(true);
		}
		return;
	}

	if (TrapPreviewActor->IsHidden())
	{
		TrapPreviewActor->SetActorHiddenInGame(false);
	}

	TrapPreviewActor->SetActorLocation(PreviewLocation);
	TrapPreviewActor->SetActorRotation(BuildSurface->GetActorRotation());
}

void UBuildComponent::DestroyTrapPreview()
{
	if (TrapPreviewActor)
	{
		TrapPreviewActor->Destroy();
		TrapPreviewActor = nullptr;
	}
}

void UBuildComponent::ServerRPC_RequestBuildTrap_Implementation(ABuildGridSurface* BuildSurface, FVector_NetQuantize HitLocation)
{
	UTrapData* TrapData = GetSelectedTrapData();
	APawn* OwnerPawn = GetOwnerPawn();
	AController* OwningController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !BuildSurface || !TrapData) return;

	BuildSurface->TryPlaceTrap(TrapData, HitLocation, OwningController);
}

void UBuildComponent::ServerRPC_RequestSellTrap_Implementation(ABuildGridSurface* BuildSurface, FVector_NetQuantize HitLocation)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !BuildSurface) return;

	BuildSurface->TryRemoveTrap(HitLocation);
}
