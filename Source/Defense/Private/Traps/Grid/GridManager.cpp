#include "Traps/Grid/GridManager.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Math/RotationMatrix.h"
#include "Misc/Crc.h"
#include "PhysicsEngine/BodySetup.h"
#include "Traps/TrapBase.h"
#include "Traps/TrapData.h"


namespace
{
	constexpr float AxisAlignedThreshold = 0.999f;
	const FName TrapFloorTag(TEXT("TrapSurface.Floor"));
	const FName TrapWallTag(TEXT("TrapSurface.Wall"));
	const FName TrapCeilingTag(TEXT("TrapSurface.Ceiling"));

	struct FRegionBakeSurface
	{
		TObjectPtr<UPrimitiveComponent> Component = nullptr;
		ETrapGridSurface SurfaceType = ETrapGridSurface::Floor;
		FVector AxisU = FVector::ForwardVector;
		FVector AxisV = FVector::RightVector;
		FVector Normal = FVector::UpVector;
		float PlaneDistance = 0.f;
		float MinU = 0.f;
		float MaxU = 0.f;
		float MinV = 0.f;
		float MaxV = 0.f;
		FString StableName;
	};

	FName GetSurfaceTag(ETrapGridSurface SurfaceType)
	{
		switch (SurfaceType)
		{
		case ETrapGridSurface::Wall:
			return TrapWallTag;

		case ETrapGridSurface::Ceiling:
			return TrapCeilingTag;

		case ETrapGridSurface::Floor:
		default:
			return TrapFloorTag;
		}
	}

	bool RegionContainsComponent(
		const FTrapGridRegion& Region,
		const UPrimitiveComponent* Component
	)
	{
		return Component && Region.SourceComponents.ContainsByPredicate(
			[Component](const TObjectPtr<UPrimitiveComponent>& SourceComponent)
			{
				return SourceComponent.Get() == Component;
			}
		);
	}

	void GetTaggedSurfaceTypes(
		const UPrimitiveComponent* Component,
		TArray<ETrapGridSurface, TInlineAllocator<3>>& OutSurfaceTypes
	)
	{
		OutSurfaceTypes.Reset();
		if (!Component)
		{
			return;
		}

		const AActor* Owner = Component->GetOwner();
		const bool bHasComponentSurfaceTag = Component->ComponentHasTag(TrapFloorTag)
			|| Component->ComponentHasTag(TrapWallTag)
			|| Component->ComponentHasTag(TrapCeilingTag);

		auto HasSurfaceTag = [Component, Owner, bHasComponentSurfaceTag](const FName Tag)
		{
			return bHasComponentSurfaceTag
				? Component->ComponentHasTag(Tag)
				: Owner && Owner->ActorHasTag(Tag);
		};

		if (HasSurfaceTag(TrapFloorTag))
		{
			OutSurfaceTypes.Add(ETrapGridSurface::Floor);
		}
		if (HasSurfaceTag(TrapWallTag))
		{
			OutSurfaceTypes.Add(ETrapGridSurface::Wall);
		}
		if (HasSurfaceTag(TrapCeilingTag))
		{
			OutSurfaceTypes.Add(ETrapGridSurface::Ceiling);
		}
	}

	bool IsAxisAligned(const FQuat& Rotation)
	{
		const FVector Axes[] =
		{
			Rotation.GetAxisX(),
			Rotation.GetAxisY(),
			Rotation.GetAxisZ()
		};

		for (const FVector& Axis : Axes)
		{
			const FVector AbsAxis = Axis.GetAbs();
			if (FMath::Max3(AbsAxis.X, AbsAxis.Y, AbsAxis.Z) < AxisAlignedThreshold)
			{
				return false;
			}
		}

		return true;
	}

	bool TryGetSimpleBoxCollisionBounds(
		UPrimitiveComponent* Component,
		FBox& OutBounds
	)
	{
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
		if (!StaticMeshComponent)
		{
			return false;
		}

		const UBodySetup* BodySetup = StaticMeshComponent->GetBodySetup();
		if (!BodySetup
			|| BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple
			|| BodySetup->AggGeom.GetElementCount() != 1
			|| BodySetup->AggGeom.BoxElems.Num() != 1)
		{
			return false;
		}

		FTransform ComponentTransform = StaticMeshComponent->GetComponentTransform();
		const FKBoxElem ScaledBox = BodySetup->AggGeom.BoxElems[0].GetFinalScaled(
			ComponentTransform.GetScale3D(),
			FTransform::Identity
		);
		ComponentTransform.RemoveScaling();

		const FTransform BoxWorldTransform = ScaledBox.GetTransform() * ComponentTransform;
		if (!IsAxisAligned(BoxWorldTransform.GetRotation()))
		{
			return false;
		}

		const FVector BoxHalfExtent(
			ScaledBox.X * 0.5f,
			ScaledBox.Y * 0.5f,
			ScaledBox.Z * 0.5f
		);
		OutBounds = FBox(-BoxHalfExtent, BoxHalfExtent).TransformBy(BoxWorldTransform);
		return OutBounds.IsValid != 0;
	}

	void GetRegionBasis(const FVector& Normal, FVector& OutAxisU, FVector& OutAxisV)
	{
		if (FMath::Abs(Normal.Z) >= AxisAlignedThreshold)
		{
			OutAxisU = FVector::ForwardVector;
			OutAxisV = Normal.Z >= 0.f ? FVector::RightVector : -FVector::RightVector;
			return;
		}

		OutAxisV = FVector::UpVector;
		if (FMath::Abs(Normal.X) >= AxisAlignedThreshold)
		{
			OutAxisU = Normal.X >= 0.f ? FVector::RightVector : -FVector::RightVector;
			return;
		}

		OutAxisU = Normal.Y >= 0.f ? -FVector::ForwardVector : FVector::ForwardVector;
	}

	float ProjectExtent(const FVector& BoxExtent, const FVector& Axis)
	{
		return FMath::Abs(Axis.X) * BoxExtent.X
			+ FMath::Abs(Axis.Y) * BoxExtent.Y
			+ FMath::Abs(Axis.Z) * BoxExtent.Z;
	}

	void AddRegionBakeSurface(
		UPrimitiveComponent* Component,
		ETrapGridSurface SurfaceType,
		const FVector& Normal,
		const FVector& FaceCenter,
		const FVector& BoundsOrigin,
		const FVector& BoundsExtent,
		TArray<FRegionBakeSurface>& OutSurfaces
	)
	{
		FRegionBakeSurface& Surface = OutSurfaces.AddDefaulted_GetRef();
		Surface.Component = Component;
		Surface.SurfaceType = SurfaceType;
		Surface.Normal = Normal;
		GetRegionBasis(Normal, Surface.AxisU, Surface.AxisV);
		Surface.PlaneDistance = FVector::DotProduct(FaceCenter, Normal);

		const float CenterU = FVector::DotProduct(BoundsOrigin, Surface.AxisU);
		const float CenterV = FVector::DotProduct(BoundsOrigin, Surface.AxisV);
		const float ExtentU = ProjectExtent(BoundsExtent, Surface.AxisU);
		const float ExtentV = ProjectExtent(BoundsExtent, Surface.AxisV);
		Surface.MinU = CenterU - ExtentU;
		Surface.MaxU = CenterU + ExtentU;
		Surface.MinV = CenterV - ExtentV;
		Surface.MaxV = CenterV + ExtentV;
		Surface.StableName = FString::Printf(
			TEXT("%s|%d|%.3f,%.3f,%.3f|%.3f"),
			*Component->GetPathName(),
			static_cast<uint8>(SurfaceType),
			Normal.X,
			Normal.Y,
			Normal.Z,
			Surface.PlaneDistance
		);
	}

	bool AreRegionSurfacesConnected(
		const FRegionBakeSurface& A,
		const FRegionBakeSurface& B,
		float PlaneTolerance,
		float ConnectionTolerance,
		float MinimumContactLength
	)
	{
		if (A.SurfaceType != B.SurfaceType
			|| !A.Normal.Equals(B.Normal, KINDA_SMALL_NUMBER)
			|| FMath::Abs(A.PlaneDistance - B.PlaneDistance) > PlaneTolerance)
		{
			return false;
		}

		const float OverlapU = FMath::Min(A.MaxU, B.MaxU) - FMath::Max(A.MinU, B.MinU);
		const float OverlapV = FMath::Min(A.MaxV, B.MaxV) - FMath::Max(A.MinV, B.MinV);
		const float GapU = FMath::Max(-OverlapU, 0.f);
		const float GapV = FMath::Max(-OverlapV, 0.f);

		return GapU <= ConnectionTolerance
			&& GapV <= ConnectionTolerance
			&& (OverlapU >= MinimumContactLength || OverlapV >= MinimumContactLength);
	}

	FGuid MakeStableRegionId(const FString& Signature)
	{
		return FGuid(
			FCrc::StrCrc32(*(Signature + TEXT("|A"))),
			FCrc::StrCrc32(*(Signature + TEXT("|B"))),
			FCrc::StrCrc32(*(Signature + TEXT("|C"))),
			FCrc::StrCrc32(*(Signature + TEXT("|D")))
		);
	}
}

AGridManager::AGridManager()
{
	/*
		* 서버 Manager → 권위 있는 점유 상태
		* 클라이언트 Manager → 프리뷰 계산용 상태
	*/
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AGridManager::BuildRegions()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	TArray<FRegionBakeSurface> Surfaces;
	int32 SkippedComponentCount = 0;

	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor || Actor == this)
		{
			continue;
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents(PrimitiveComponents);

		for (UPrimitiveComponent* Component : PrimitiveComponents)
		{
			TArray<ETrapGridSurface, TInlineAllocator<3>> SurfaceTypes;
			GetTaggedSurfaceTypes(Component, SurfaceTypes);
			if (SurfaceTypes.IsEmpty())
			{
				continue;
			}

			if (!Component->IsQueryCollisionEnabled()
				|| Component->GetCollisionObjectType() != ECC_WorldStatic
				|| Component->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block
				|| !IsAxisAligned(Component->GetComponentQuat()))
			{
				++SkippedComponentCount;
				continue;
			}

			FBox CollisionBounds;
			if (!TryGetSimpleBoxCollisionBounds(Component, CollisionBounds))
			{
				++SkippedComponentCount;
				continue;
			}

			const FVector BoundsOrigin = CollisionBounds.GetCenter();
			const FVector BoundsExtent = CollisionBounds.GetExtent();
			for (const ETrapGridSurface SurfaceType : SurfaceTypes)
			{
				switch (SurfaceType)
				{
				case ETrapGridSurface::Floor:
					AddRegionBakeSurface(
						Component,
						SurfaceType,
						FVector::UpVector,
						BoundsOrigin + FVector::UpVector * BoundsExtent.Z,
						BoundsOrigin,
						BoundsExtent,
						Surfaces
					);
					break;

				case ETrapGridSurface::Ceiling:
					AddRegionBakeSurface(
						Component,
						SurfaceType,
						-FVector::UpVector,
						BoundsOrigin - FVector::UpVector * BoundsExtent.Z,
						BoundsOrigin,
						BoundsExtent,
						Surfaces
					);
					break;

				case ETrapGridSurface::Wall:
				default:
				{
					const FVector WallAxis = BoundsExtent.X <= BoundsExtent.Y
						? FVector::ForwardVector
						: FVector::RightVector;
					const float WallHalfThickness = ProjectExtent(BoundsExtent, WallAxis);

					AddRegionBakeSurface(
						Component,
						SurfaceType,
						WallAxis,
						BoundsOrigin + WallAxis * WallHalfThickness,
						BoundsOrigin,
						BoundsExtent,
						Surfaces
					);
					AddRegionBakeSurface(
						Component,
						SurfaceType,
						-WallAxis,
						BoundsOrigin - WallAxis * WallHalfThickness,
						BoundsOrigin,
						BoundsExtent,
						Surfaces
					);
					break;
				}
				}
			}
		}
	}

	Surfaces.Sort([](const FRegionBakeSurface& A, const FRegionBakeSurface& B)
	{
		return A.StableName < B.StableName;
	});

	TArray<FTrapGridRegion> NewRegions;
	TArray<bool> Assigned;
	Assigned.Init(false, Surfaces.Num());

	for (int32 SurfaceIndex = 0; SurfaceIndex < Surfaces.Num(); ++SurfaceIndex)
	{
		if (Assigned[SurfaceIndex])
		{
			continue;
		}

		TArray<int32> PendingIndices;
		TArray<int32> RegionSurfaceIndices;
		PendingIndices.Add(SurfaceIndex);
		Assigned[SurfaceIndex] = true;

		while (!PendingIndices.IsEmpty())
		{
			const int32 CurrentIndex = PendingIndices.Pop(EAllowShrinking::No);
			RegionSurfaceIndices.Add(CurrentIndex);

			for (int32 CandidateIndex = 0; CandidateIndex < Surfaces.Num(); ++CandidateIndex)
			{
				if (Assigned[CandidateIndex]
					|| !AreRegionSurfacesConnected(
						Surfaces[CurrentIndex],
						Surfaces[CandidateIndex],
						SurfacePlaneTolerance,
						RegionConnectionTolerance,
						RegionMinimumContactLength
					))
				{
					continue;
				}

				Assigned[CandidateIndex] = true;
				PendingIndices.Add(CandidateIndex);
			}
		}

		const FRegionBakeSurface& FirstSurface = Surfaces[RegionSurfaceIndices[0]];
		float MinU = FirstSurface.MinU;
		float MaxU = FirstSurface.MaxU;
		float MinV = FirstSurface.MinV;
		float MaxV = FirstSurface.MaxV;
		float PlaneDistanceSum = 0.f;
		FString RegionSignature;

		FTrapGridRegion& Region = NewRegions.AddDefaulted_GetRef();
		Region.SurfaceType = FirstSurface.SurfaceType;
		Region.AxisU = FirstSurface.AxisU;
		Region.AxisV = FirstSurface.AxisV;
		Region.Normal = FirstSurface.Normal;

		for (const int32 RegionSurfaceIndex : RegionSurfaceIndices)
		{
			const FRegionBakeSurface& Surface = Surfaces[RegionSurfaceIndex];
			MinU = FMath::Min(MinU, Surface.MinU);
			MaxU = FMath::Max(MaxU, Surface.MaxU);
			MinV = FMath::Min(MinV, Surface.MinV);
			MaxV = FMath::Max(MaxV, Surface.MaxV);
			PlaneDistanceSum += Surface.PlaneDistance;
			Region.SourceComponents.AddUnique(Surface.Component);
			RegionSignature += Surface.StableName;
			RegionSignature += TEXT(";");
		}

		const float PlaneDistance = PlaneDistanceSum / RegionSurfaceIndices.Num();
		Region.Origin = Region.Normal * PlaneDistance + Region.AxisU * MinU + Region.AxisV * MinV;
		Region.Size = FVector2D(MaxU - MinU, MaxV - MinV);
		Region.RegionId = MakeStableRegionId(RegionSignature);
	}

	NewRegions.Sort([](const FTrapGridRegion& A, const FTrapGridRegion& B)
	{
		return A.RegionId.ToString() < B.RegionId.ToString();
	});

	Modify();
	BakedRegions = MoveTemp(NewRegions);
	ValidCells.Reset();
	InvalidCells.Reset();
	MarkPackageDirty();

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Trap Grid Region Bake | Regions=%d Surfaces=%d SkippedComponents=%d"),
		BakedRegions.Num(),
		Surfaces.Num(),
		SkippedComponentCount
	);
#endif
}

void AGridManager::ValidateRegions()
{
#if WITH_EDITOR
	TSet<FGuid> RegionIds;
	int32 InvalidRegionCount = 0;

	for (const FTrapGridRegion& Region : BakedRegions)
	{
		bool bValidSources = !Region.SourceComponents.IsEmpty();
		for (UPrimitiveComponent* SourceComponent : Region.SourceComponents)
		{
			if (!IsValid(SourceComponent))
			{
				bValidSources = false;
				break;
			}

			TArray<ETrapGridSurface, TInlineAllocator<3>> SurfaceTypes;
			GetTaggedSurfaceTypes(SourceComponent, SurfaceTypes);
			FBox CollisionBounds;
			if (!SourceComponent->IsQueryCollisionEnabled()
				|| SourceComponent->GetCollisionObjectType() != ECC_WorldStatic
				|| SourceComponent->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block
				|| !IsAxisAligned(SourceComponent->GetComponentQuat())
				|| !TryGetSimpleBoxCollisionBounds(SourceComponent, CollisionBounds)
				|| !SurfaceTypes.Contains(Region.SurfaceType))
			{
				bValidSources = false;
				break;
			}
		}

		const bool bValidBasis = Region.AxisU.IsNormalized()
			&& Region.AxisV.IsNormalized()
			&& Region.Normal.IsNormalized()
			&& FMath::Abs(FVector::DotProduct(Region.AxisU, Region.AxisV)) <= KINDA_SMALL_NUMBER
			&& FVector::DotProduct(FVector::CrossProduct(Region.AxisU, Region.AxisV), Region.Normal) >= AxisAlignedThreshold;
		const bool bValidRegion = Region.RegionId.IsValid()
			&& !RegionIds.Contains(Region.RegionId)
			&& bValidBasis
			&& Region.Size.X > KINDA_SMALL_NUMBER
			&& Region.Size.Y > KINDA_SMALL_NUMBER
			&& bValidSources;

		if (!bValidRegion)
		{
			++InvalidRegionCount;
		}

		RegionIds.Add(Region.RegionId);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Trap Grid Region Validate | Regions=%d Invalid=%d"),
		BakedRegions.Num(),
		InvalidRegionCount
	);
#endif
}

void AGridManager::ClearRegions()
{
#if WITH_EDITOR
	Modify();
	BakedRegions.Reset();
	ValidCells.Reset();
	InvalidCells.Reset();
	MarkPackageDirty();

	UE_LOG(LogTemp, Display, TEXT("Trap Grid Region Clear"));
#endif
}

void AGridManager::ShowRegions()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	const float SafeCellSize = FMath::Max(CellSize, 1.f);
	const float DebugOffset = 2.f;

	for (const FTrapGridRegion& Region : BakedRegions)
	{
		const FVector Outer00 = RegionLocalToWorld(Region, FVector2D(0.f, 0.f), DebugOffset);
		const FVector Outer10 = RegionLocalToWorld(Region, FVector2D(Region.Size.X, 0.f), DebugOffset);
		const FVector Outer01 = RegionLocalToWorld(Region, FVector2D(0.f, Region.Size.Y), DebugOffset);
		const FVector Outer11 = RegionLocalToWorld(Region, Region.Size, DebugOffset);

		DrawDebugLine(World, Outer00, Outer10, FColor::Cyan, false, RegionDebugDuration, 0, 2.f);
		DrawDebugLine(World, Outer10, Outer11, FColor::Cyan, false, RegionDebugDuration, 0, 2.f);
		DrawDebugLine(World, Outer11, Outer01, FColor::Cyan, false, RegionDebugDuration, 0, 2.f);
		DrawDebugLine(World, Outer01, Outer00, FColor::Cyan, false, RegionDebugDuration, 0, 2.f);

		const int32 CellCountU = FMath::Max(
			0,
			FMath::FloorToInt((Region.Size.X + CellProbeInset) / SafeCellSize)
		);
		const int32 CellCountV = FMath::Max(
			0,
			FMath::FloorToInt((Region.Size.Y + CellProbeInset) / SafeCellSize)
		);
		const float GridSizeU = CellCountU * SafeCellSize;
		const float GridSizeV = CellCountV * SafeCellSize;

		for (int32 UIndex = 0; UIndex <= CellCountU; ++UIndex)
		{
			const float U = UIndex * SafeCellSize;
			DrawDebugLine(
				World,
				RegionLocalToWorld(Region, FVector2D(U, 0.f), DebugOffset),
				RegionLocalToWorld(Region, FVector2D(U, GridSizeV), DebugOffset),
				FColor::Green,
				false,
				RegionDebugDuration,
				0,
				0.5f
			);
		}

		for (int32 VIndex = 0; VIndex <= CellCountV; ++VIndex)
		{
			const float V = VIndex * SafeCellSize;
			DrawDebugLine(
				World,
				RegionLocalToWorld(Region, FVector2D(0.f, V), DebugOffset),
				RegionLocalToWorld(Region, FVector2D(GridSizeU, V), DebugOffset),
				FColor::Green,
				false,
				RegionDebugDuration,
				0,
				0.5f
			);
		}

		const FVector RegionCenter = RegionLocalToWorld(Region, Region.Size * 0.5f, DebugOffset);
		DrawDebugDirectionalArrow(
			World,
			RegionCenter,
			RegionCenter + Region.Normal * 50.f,
			12.f,
			FColor::Yellow,
			false,
			RegionDebugDuration,
			0,
			1.5f
		);
	}
#endif
}

const FTrapGridRegion* AGridManager::FindRegionById(const FGuid& RegionId) const
{
	if (!RegionId.IsValid())
	{
		return nullptr;
	}

	return BakedRegions.FindByPredicate([&RegionId](const FTrapGridRegion& Region)
	{
		return Region.RegionId == RegionId;
	});
}

bool AGridManager::TryGetRegionForHit(
	const FHitResult& Hit,
	const FTrapGridRegion*& OutRegion
) const
{
	OutRegion = nullptr;

	const UPrimitiveComponent* HitComponent = Hit.GetComponent();
	if (!Hit.bBlockingHit || !HitComponent)
	{
		return false;
	}

	const FVector HitNormal = Hit.ImpactNormal.GetSafeNormal();
	for (const FTrapGridRegion& Region : BakedRegions)
	{
		if (!RegionContainsComponent(Region, HitComponent)
			|| FVector::DotProduct(HitNormal, Region.Normal) < AxisAlignedThreshold
			|| !IsPlaceableHit(Hit, Region.SurfaceType))
		{
			continue;
		}

		const FVector RelativeLocation = Hit.ImpactPoint - Region.Origin;
		const float PlaneDistance = FMath::Abs(FVector::DotProduct(RelativeLocation, Region.Normal));
		const FVector2D LocalLocation = WorldToRegionLocal(Region, Hit.ImpactPoint);
		if (PlaneDistance > SurfacePlaneTolerance
			|| LocalLocation.X < -RegionConnectionTolerance
			|| LocalLocation.Y < -RegionConnectionTolerance
			|| LocalLocation.X > Region.Size.X + RegionConnectionTolerance
			|| LocalLocation.Y > Region.Size.Y + RegionConnectionTolerance)
		{
			continue;
		}

		OutRegion = &Region;
		return true;
	}

	return false;
}

FVector2D AGridManager::WorldToRegionLocal(
	const FTrapGridRegion& Region,
	const FVector& WorldLocation
) const
{
	const FVector RelativeLocation = WorldLocation - Region.Origin;
	return FVector2D(
		FVector::DotProduct(RelativeLocation, Region.AxisU),
		FVector::DotProduct(RelativeLocation, Region.AxisV)
	);
}

FVector AGridManager::RegionLocalToWorld(
	const FTrapGridRegion& Region,
	const FVector2D& LocalLocation,
	float NormalOffset
) const
{
	return Region.Origin
		+ Region.AxisU * LocalLocation.X
		+ Region.AxisV * LocalLocation.Y
		+ Region.Normal * NormalOffset;
}

FIntPoint AGridManager::WorldToRegionCell(
	const FTrapGridRegion& Region,
	const FVector& WorldLocation
) const
{
	const float SafeCellSize = FMath::Max(CellSize, 1.f);
	const FVector2D LocalLocation = WorldToRegionLocal(Region, WorldLocation);

	return FIntPoint(
		FMath::FloorToInt(LocalLocation.X / SafeCellSize),
		FMath::FloorToInt(LocalLocation.Y / SafeCellSize)
	);
}

FVector AGridManager::RegionCellToWorldCenter(
	const FTrapGridRegion& Region,
	const FIntPoint& Cell
) const
{
	const float SafeCellSize = FMath::Max(CellSize, 1.f);
	return RegionLocalToWorld(
		Region,
		FVector2D(
			(Cell.X + 0.5f) * SafeCellSize,
			(Cell.Y + 0.5f) * SafeCellSize
		)
	);
}

FIntPoint AGridManager::WorldToRegionAnchorCell(
	const FTrapGridRegion& Region,
	const FVector& WorldLocation,
	const FIntPoint& FootprintCells
) const
{
	const float SafeCellSize = FMath::Max(CellSize, 1.f);
	const FVector2D LocalLocation = WorldToRegionLocal(Region, WorldLocation);

	return FIntPoint(
		FMath::RoundToInt(LocalLocation.X / SafeCellSize - static_cast<float>(FootprintCells.X) * 0.5f),
		FMath::RoundToInt(LocalLocation.Y / SafeCellSize - static_cast<float>(FootprintCells.Y) * 0.5f)
	);
}

FVector AGridManager::GetRegionFootprintCenter(
	const FTrapGridRegion& Region,
	const FIntPoint& AnchorCell,
	const FIntPoint& FootprintCells
) const
{
	const float SafeCellSize = FMath::Max(CellSize, 1.f);
	return RegionLocalToWorld(
		Region,
		FVector2D(
			(AnchorCell.X + static_cast<float>(FootprintCells.X) * 0.5f) * SafeCellSize,
			(AnchorCell.Y + static_cast<float>(FootprintCells.Y) * 0.5f) * SafeCellSize
		)
	);
}

FTransform AGridManager::GetRegionFootprintTransform(
	const FTrapGridRegion& Region,
	const FIntPoint& AnchorCell,
	const FIntPoint& FootprintCells
) const
{
	const FQuat Rotation = FRotationMatrix::MakeFromXZ(
		Region.AxisU,
		Region.Normal
	).ToQuat();

	return FTransform(
		Rotation,
		GetRegionFootprintCenter(Region, AnchorCell, FootprintCells),
		FVector::OneVector
	);
}

bool AGridManager::IsPlaceableHit(const FHitResult& Hit, ETrapGridSurface SurfaceType) const
{
	const UPrimitiveComponent* HitComponent = Hit.GetComponent();
	const AActor* HitActor = Hit.GetActor();
	if (!Hit.bBlockingHit || !HitComponent)
	{
		return false;
	}

	const FName RequiredTag = GetSurfaceTag(SurfaceType);
	return HitComponent->ComponentHasTag(RequiredTag)
		|| (HitActor && HitActor->ActorHasTag(RequiredTag));
}

bool AGridManager::IsTrapSurfaceCompatible(
	const UTrapData* TrapData,
	const FTrapCellKey& CellKey
) const
{
	const FTrapGridRegion* Region = FindRegionById(CellKey.RegionId);
	return TrapData && Region && TrapData->GridSurface == Region->SurfaceType;
}

// Hit 위치를 Trap Footprint의 시작 Cell(Anchor)로 변환
// 홀수/짝수 크기 모두 Footprint 중심이 조준 위치에 가장 가깝게 Snap
bool AGridManager::FindClosestValidAnchor(
	const UTrapData* TrapData,
	const FVector& HitLocation,
	const FTrapGridRegion& Region,
	FTrapCellKey& OutCellKey
)
{
	if (!TrapData)
	{
		return false;
	}

	const FIntPoint Footprint = TrapData->FootprintCells;
	if (Footprint.X <= 0 || Footprint.Y <= 0)
	{
		return false;
	}

	const FTrapCellKey RequestedAnchor = WorldToTrapAnchorCellKey(
		HitLocation,
		Region,
		Footprint
	);

	struct FAnchorCandidate
	{
		FTrapCellKey CellKey;
		float DistanceSquared = 0.f;
	};

	TArray<FAnchorCandidate> Candidates;
	const int32 SearchRadiusU = FMath::Max(1, (Footprint.X + 1) / 2);
	const int32 SearchRadiusV = FMath::Max(1, (Footprint.Y + 1) / 2);
	Candidates.Reserve((SearchRadiusU * 2 + 1) * (SearchRadiusV * 2 + 1));

	for (int32 UOffset = -SearchRadiusU; UOffset <= SearchRadiusU; ++UOffset)
	{
		for (int32 VOffset = -SearchRadiusV; VOffset <= SearchRadiusV; ++VOffset)
		{
			FAnchorCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.CellKey = RequestedAnchor;
			Candidate.CellKey.Cell += FIntPoint(UOffset, VOffset);
			Candidate.DistanceSquared = FVector::DistSquared(
				GetTrapFootprintCenter(TrapData, Candidate.CellKey),
				HitLocation
			);
		}
	}

	Candidates.Sort([](const FAnchorCandidate& A, const FAnchorCandidate& B)
	{
		return A.DistanceSquared < B.DistanceSquared;
	});

	for (const FAnchorCandidate& Candidate : Candidates)
	{
		TArray<FTrapCellKey> CandidateFootprint;
		GetTrapFootprintCells(TrapData, Candidate.CellKey, CandidateFootprint);
		if (AreCellsValid(CandidateFootprint))
		{
			OutCellKey = Candidate.CellKey;
			return true;
		}
	}

	return false;
}

bool AGridManager::TryGetCellKeyForHit(
	const UTrapData* TrapData,
	const FHitResult& Hit,
	FTrapCellKey& OutCellKey
)
{
	if (!TrapData || !Hit.GetComponent())
	{
		return false;
	}

	const FTrapGridRegion* Region = nullptr;
	if (!TryGetRegionForHit(Hit, Region)
		|| !Region
		|| TrapData->GridSurface != Region->SurfaceType)
	{
		return false;
	}

	return FindClosestValidAnchor(
		TrapData,
		Hit.ImpactPoint,
		*Region,
		OutCellKey
	);
}

bool AGridManager::EvaluateCellStaticValidity(const FTrapCellKey& CellKey) const
{
	UWorld* World = GetWorld();
	const FTrapGridRegion* Region = FindRegionById(CellKey.RegionId);
	if (!World || !Region)
	{
		return false;
	}

	const float SafeCellSize = FMath::Max(CellSize, 1.f);
	const float ProbeHalfSpan = FMath::Max(SafeCellSize * 0.5f - CellProbeInset, 0.f);
	const FVector CellCenter = RegionCellToWorldCenter(*Region, CellKey.Cell);

	TArray<FVector2D, TInlineAllocator<5>> ProbeOffsets;
	ProbeOffsets.Add(FVector2D::ZeroVector);
	if (ProbeHalfSpan > KINDA_SMALL_NUMBER)
	{
		ProbeOffsets.Add(FVector2D(-ProbeHalfSpan, -ProbeHalfSpan));
		ProbeOffsets.Add(FVector2D(-ProbeHalfSpan, ProbeHalfSpan));
		ProbeOffsets.Add(FVector2D(ProbeHalfSpan, -ProbeHalfSpan));
		ProbeOffsets.Add(FVector2D(ProbeHalfSpan, ProbeHalfSpan));
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TrapGridCellProbe), false, this);

	for (const FVector2D& ProbeOffset : ProbeOffsets)
	{
		const FVector ProbeLocation = CellCenter
			+ Region->AxisU * ProbeOffset.X
			+ Region->AxisV * ProbeOffset.Y;
		const FVector TraceStart = ProbeLocation + Region->Normal * SurfaceProbeDistance;
		const FVector TraceEnd = ProbeLocation - Region->Normal * SurfaceProbeDistance;

		FHitResult SurfaceHit;
		if (!World->LineTraceSingleByObjectType(
			SurfaceHit,
			TraceStart,
			TraceEnd,
			ObjectQueryParams,
			QueryParams
		))
		{
			return false;
		}

		if (!RegionContainsComponent(*Region, SurfaceHit.GetComponent())
			|| FVector::DotProduct(SurfaceHit.ImpactNormal.GetSafeNormal(), Region->Normal) < AxisAlignedThreshold
			|| !IsPlaceableHit(SurfaceHit, Region->SurfaceType))
		{
			return false;
		}

		const float PlaneDistance = FMath::Abs(FVector::DotProduct(
			SurfaceHit.ImpactPoint - Region->Origin,
			Region->Normal
		));
		if (PlaneDistance > SurfacePlaneTolerance)
		{
			return false;
		}
	}

	return true;
}

bool AGridManager::IsCellValid(const FTrapCellKey& CellKey)
{
	if (ValidCells.Contains(CellKey))
	{
		return true;
	}
	if (InvalidCells.Contains(CellKey))
	{
		return false;
	}

	if (EvaluateCellStaticValidity(CellKey))
	{
		ValidCells.Add(CellKey);
		return true;
	}

	InvalidCells.Add(CellKey);
	return false;
}

bool AGridManager::IsCellOccupied(const FTrapCellKey& CellKey) const
{
	const TWeakObjectPtr<ATrapBase>* FoundTrap = CellToTrap.Find(CellKey);
	return FoundTrap && FoundTrap->IsValid();
}

bool AGridManager::AreCellsValid(const TArray<FTrapCellKey>& CellKeys)
{
	if (CellKeys.IsEmpty())
	{
		return false;
	}

	for (const FTrapCellKey& CellKey : CellKeys)
	{
		if (!IsCellValid(CellKey))
		{
			return false;
		}
	}

	return true;
}

bool AGridManager::AreCellsAvailable(const TArray<FTrapCellKey>& CellKeys)
{
	if (!AreCellsValid(CellKeys))
	{
		return false;
	}

	for (const FTrapCellKey& CellKey : CellKeys)
	{
		if (IsCellOccupied(CellKey))
		{
			return false;
		}
	}

	return true;
}

FTrapCellKey AGridManager::WorldToTrapAnchorCellKey(
	const FVector& WorldLocation,
	const FTrapGridRegion& Region,
	const FIntPoint& FootprintCells
) const
{
	FTrapCellKey AnchorCell;
	AnchorCell.RegionId = Region.RegionId;
	AnchorCell.Cell = WorldToRegionAnchorCell(Region, WorldLocation, FootprintCells);

	return AnchorCell;
}

void AGridManager::GetTrapFootprintCells(
	const UTrapData* TrapData,
	const FTrapCellKey& AnchorCell,
	TArray<FTrapCellKey>& OutCellKeys
) const
{
	OutCellKeys.Reset();

	if (!TrapData)
	{
		return;
	}

	const FIntPoint Footprint = TrapData->FootprintCells;
	if (Footprint.X <= 0 || Footprint.Y <= 0)
	{
		return;
	}

	OutCellKeys.Reserve(Footprint.X * Footprint.Y);

	for (int32 UIndex = 0; UIndex < Footprint.X; ++UIndex)
	{
		for (int32 VIndex = 0; VIndex < Footprint.Y; ++VIndex)
		{
			FTrapCellKey CellKey = AnchorCell;
			CellKey.Cell += FIntPoint(UIndex, VIndex);
			OutCellKeys.Add(CellKey);
		}
	}
}

FVector AGridManager::GetTrapFootprintCenter(
	const UTrapData* TrapData,
	const FTrapCellKey& AnchorCell
) const
{
	const FTrapGridRegion* Region = FindRegionById(AnchorCell.RegionId);
	return Region
		? GetRegionFootprintCenter(
			*Region,
			AnchorCell.Cell,
			TrapData ? TrapData->FootprintCells : FIntPoint(1, 1)
		)
		: FVector::ZeroVector;
}

FTransform AGridManager::GetTrapFootprintTransform(
	const UTrapData* TrapData,
	const FTrapCellKey& AnchorCell
) const
{
	const FTrapGridRegion* Region = FindRegionById(AnchorCell.RegionId);
	return Region
		? GetRegionFootprintTransform(
			*Region,
			AnchorCell.Cell,
			TrapData ? TrapData->FootprintCells : FIntPoint(1, 1)
		)
		: FTransform::Identity;
}

bool AGridManager::TryOccupyCells(const TArray<FTrapCellKey>& CellKeys, ATrapBase* Trap)
{
	// Server
	
	if (!HasAuthority() || !IsValid(Trap)) { return false; }

	const FObjectKey TrapKey(Trap);

	if (TrapToCells.Contains(TrapKey)) { return false; } // 이미 등록된 Trap인가?
	if (!AreCellsAvailable(CellKeys)) { return false; }

	
	// 중복 제거 → Footprint 계산 실수로 중복 Cell이 전달돼도 내부 점유 기록에는 한 번만 저장
	TSet<FTrapCellKey> UniqueCells;
	
	for (const FTrapCellKey& CellKey : CellKeys)
	{
		UniqueCells.Add(CellKey);
	}
	
	TArray<FTrapCellKey> OccupiedCellList = UniqueCells.Array();
	
	for (const FTrapCellKey& CellKey : OccupiedCellList)
	{
		CellToTrap.Add(CellKey, Trap); // CellToTrap 등록
	}

	TrapToCells.Add(TrapKey, OccupiedCellList); // TrapToCells 등록

	// OnDestroyed 연결
	Trap->OnDestroyed.AddUniqueDynamic(
		this,
		&AGridManager::HandleTrapDestroyed
	);

	return true;
}

void AGridManager::ReleaseTrap(ATrapBase* Trap)
{
	// Server
	
	if (!HasAuthority() || !Trap) { return; }

	const FObjectKey TrapKey(Trap);

	TArray<FTrapCellKey> OccupiedCellList;
	if (!TrapToCells.RemoveAndCopyValue(TrapKey,OccupiedCellList)) { return; }

	for (const FTrapCellKey& CellKey : OccupiedCellList)
	{
		const TWeakObjectPtr<ATrapBase>* FoundTrap = CellToTrap.Find(CellKey);

		if (FoundTrap && FoundTrap->Get() == Trap)
		{
			CellToTrap.Remove(CellKey);
		}
	}

	Trap->OnDestroyed.RemoveDynamic(
		this,
		&AGridManager::HandleTrapDestroyed
	);
}

void AGridManager::RegisterClientOccupiedCells(ATrapBase* Trap, const TArray<FTrapCellKey>& CellKeys)
{
	if (HasAuthority() || !Trap)
	{
		return;
	}

	for (const FTrapCellKey& CellKey : CellKeys)
	{
		CellToTrap.Add(CellKey, Trap);
	}
}

void AGridManager::ReleaseClientTrap(ATrapBase* Trap)
{
	if (HasAuthority() || !Trap)
	{
		return;
	}

	for (auto It = CellToTrap.CreateIterator(); It; ++It)
	{
		if (It.Value().Get() == Trap)
		{
			It.RemoveCurrent();
		}
	}
}

bool AGridManager::IsTrapRegistered(const ATrapBase* Trap) const
{
	return Trap && TrapToCells.Contains(FObjectKey(Trap));
}

void AGridManager::HandleTrapDestroyed(AActor* DestroyedActor)
{
	if (ATrapBase* DestroyedTrap = Cast<ATrapBase>(DestroyedActor))
	{
		ReleaseTrap(DestroyedTrap);
	}
}
