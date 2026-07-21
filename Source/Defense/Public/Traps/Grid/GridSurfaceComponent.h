#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Traps/TrapData.h"
#include "GridSurfaceComponent.generated.h"

// 레벨 메시 위의 "설치 가능한 직사각형 면"을 선언
UCLASS(ClassGroup=(Trap), meta=(BlueprintSpawnableComponent))
class DEFENSE_API UGridSurfaceComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UGridSurfaceComponent();

	// 이 설치면이 허용하는 Trap 종류
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid")
	ETrapGridSurface SurfaceType = ETrapGridSurface::Floor;

	// Local X/Y 방향의 설치 가능 범위
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid", meta=(ClampMin="1.0"))
	FVector2D SurfaceSizeCm = FVector2D(400.f, 400.f);

	// 특수한 경우 설치면을 끌 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid")
	bool bEnabled = true;

};
