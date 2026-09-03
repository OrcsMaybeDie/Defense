#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StormTornadoVFXActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UParticleSystem;
class UParticleSystemComponent;
class USceneComponent;
class UStaticMesh;

/**
 * FX Variety Pack의 Aqua Storm을 본체로 사용하고 상단 구름과 간헐적인 비를 보강한다.
 * 서버에서는 4초 동안 주변의 가까운 적 최대 5명에게 초당 20 피해를 준다.
 */
UCLASS(NotBlueprintable)
class DEFENSE_API AStormTornadoVFXActor : public AActor
{
	GENERATED_BODY()

public:
	AStormTornadoVFXActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FCloudSprite
	{
		int32 InstanceIndex = INDEX_NONE;
		float StartAngle = 0.f;
		float Radius = 100.f;
		float HeightPhase = 0.f;
		float HeightRange = 100.f;
		float RiseSpeed = 100.f;
		float AngularSpeed = 1.f;
		float BaseScale = 1.f;
		float PulsePhase = 0.f;
	};

	void CreateCloudLayer();
	void UpdateCloudLayer(float Fade);
	void SpawnRainDrop();
	void UpdateRainDrops();
	void ApplyAreaDamage();
	FVector GetCameraLocation() const;

	struct FActiveRainDrop
	{
		TWeakObjectPtr<UParticleSystemComponent> Component;
		float SpawnTime = 0.f;
	};

	UPROPERTY(VisibleAnywhere, Category="Storm Tornado")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category="Storm Tornado")
	TObjectPtr<UParticleSystemComponent> TornadoParticleComponent;

	UPROPERTY(Transient)
	TObjectPtr<UParticleSystem> RainParticleSystem;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SmokeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> CloudInstances;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CloudDynamicMaterial;

	TArray<FCloudSprite> CloudSprites;
	TArray<FActiveRainDrop> ActiveRainDrops;
	FRandomStream RandomStream;
	float ElapsedTime = 0.f;
	float NextRainTime = 0.f;
	float NextDamageTime = 1.f;
	float VisualDuration = 4.f;
};
