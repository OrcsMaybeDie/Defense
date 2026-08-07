#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "MedusaTest.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class DEFENSE_API AMedusaTest : public AActor
{
	GENERATED_BODY()

public:
	AMedusaTest();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Medusa|Components")
	TObjectPtr<UBoxComponent> RootCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Medusa|Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Medusa|Components")
	TObjectPtr<USceneComponent> GazeOrigin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medusa|Detection", meta=(ClampMin="0.01", Units="s"))
	float ScanInterval = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medusa|Detection", meta=(ClampMin="0.0", Units="cm"))
	float PetrifyRange = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medusa|Detection", meta=(ClampMin="0.0", ClampMax="360.0", Units="deg"))
	float ConeAngle = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medusa|Detection", meta=(ClampMin="0.0", Units="cm"))
	float MaxHeightDifference = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medusa|Detection")
	TArray<FName> TargetBoneNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medusa|Collision")
	TEnumAsByte<ECollisionChannel> EnemyObjectChannel = ECC_GameTraceChannel1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medusa|Collision")
	TEnumAsByte<ECollisionChannel> MedusaSightChannel = ECC_GameTraceChannel2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medusa|Debug")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medusa|Debug", meta=(ClampMin="0.0", Units="s"))
	float DebugDuration = 0.f;

private:
	FTimerHandle ScanTimerHandle;

	void ScanForEnemies();
	bool IsPointInsideHorizontalCone(const FVector& Origin, const FVector& Forward, const FVector& Point) const;
	bool HasClearSightToPoint(AActor* TargetActor, const FVector& TargetPoint) const;
	float GetOverlapRadius() const;
	void DrawDetectionDebug(
		const FVector& Origin,
		const FVector& Forward,
		float SphereRadius,
		float InPetrifyRange,
		float InConeAngle,
		float LifeTime
	) const;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDrawDetectionDebug(
		FVector_NetQuantize Origin,
		FVector_NetQuantizeNormal Forward,
		float SphereRadius,
		float InPetrifyRange,
		float InConeAngle,
		float LifeTime
	);
};
