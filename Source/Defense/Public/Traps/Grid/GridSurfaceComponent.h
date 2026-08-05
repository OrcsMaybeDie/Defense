#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Traps/TrapData.h"
#include "GridSurfaceComponent.generated.h"

class UPrimitiveComponent;

// 부모 Primitive를 함정 설치면으로 선언
// 크기와 위치는 부모 Primitive의 World Bounds에서 자동 계산
UCLASS(ClassGroup=(Trap), meta=(BlueprintSpawnableComponent))
class DEFENSE_API UGridSurfaceComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UGridSurfaceComponent();

	// 이 설치면이 허용하는 Trap 종류
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid")
	ETrapGridSurface SurfaceType = ETrapGridSurface::Floor;

	// 특수한 경우 설치면을 끌 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trap Grid")
	bool bEnabled = true;

	// 이 컴포넌트가 직접 붙어 있는 실제 레벨 Primitive
	const UPrimitiveComponent* GetTargetPrimitive() const;
};
