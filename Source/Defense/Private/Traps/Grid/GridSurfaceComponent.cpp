#include "Traps/Grid/GridSurfaceComponent.h"
#include "Components/PrimitiveComponent.h"

UGridSurfaceComponent::UGridSurfaceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const UPrimitiveComponent* UGridSurfaceComponent::GetTargetPrimitive() const
{
	return Cast<UPrimitiveComponent>(GetAttachParent());
}
