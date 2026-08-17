#pragma once

#include "CoreMinimal.h"
#include "TrapBase.h"
#include "Medusa.generated.h"

class USceneComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraSystem;
class UStaticMeshComponent;

UCLASS()
class DEFENSE_API AMedusa : public ATrapBase
{
	GENERATED_BODY()

public:
	AMedusa();
	virtual void InitializePlacedTrap(
		UTrapData* TrapData,
		ADefensePlayerState* InInstalledByPlayerState,
		const TArray<FTrapCellKey>& InOccupiedCells
	) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Medusa|Components")
	TObjectPtr<USceneComponent> GazeOrigin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Medusa|Components")
	TObjectPtr<UStaticMeshComponent> LeftEyeSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Medusa|Components")
	TObjectPtr<UStaticMeshComponent> RightEyeSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Medusa|Components")
	TObjectPtr<USceneComponent> LeftBeamOrigin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Medusa|Components")
	TObjectPtr<USceneComponent> RightBeamOrigin;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Medusa|Eye")
	TObjectPtr<UMaterialInterface> EyeMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Medusa|Eye")
	FName EyeOpenParameterName = TEXT("EyeOpen");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Medusa|Eye", meta=(ClampMin="0.001", Units="s"))
	float EyeVisualUpdateInterval = 1.f / 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Medusa|VFX")
	TObjectPtr<UNiagaraSystem> BeamVFXSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Medusa|VFX")
	FName BeamStartParameterName = TEXT("User.Beam Start");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Medusa|VFX")
	FName BeamEndParameterName = TEXT("User.Beam End");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Medusa|VFX")
	FName BeamTravelTimeParameterName = TEXT("User.BeamTravelTime");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Medusa|VFX", meta=(ClampMin="0.001", Units="s"))
	float BeamTravelTime = 0.05f;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medusa|Debug")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medusa|Debug", meta=(ClampMin="0.0", Units="s"))
	float DebugDuration = 0.f;

private:
	FTimerHandle ScanTimerHandle;
	FTimerHandle EyeVisualTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LeftEyeMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RightEyeMID;

	double EyeVisualStartTime = 0.0;
	float EyeVisualDuration = 0.f;

	void StartScanTimer();
	void ScanForEnemies();
	void ApplyEyeMaterial();
	void InitializeEyeVisuals();
	void StartEyeVisual(float Duration);
	void UpdateEyeVisual();
	void SetEyeOpenAmount(float Amount);
	void SpawnBeamFromOrigin(const USceneComponent* BeamOrigin, const FVector& TargetPoint) const;
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

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastStartEyeVisual(float Duration);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayPetrifyBeam(FVector_NetQuantize TargetPoint);
};
