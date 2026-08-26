#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StormTornadoVFXActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Unity StormTornado 원본의 메시/파티클 레이어를 Unreal 컴포넌트로 재구성한 월드 VFX다.
 * 게임플레이 판정은 담당하지 않으며, 서버가 Stage3 명중 지점에 한 번 스폰한다.
 */
UCLASS(NotBlueprintable)
class DEFENSE_API AStormTornadoVFXActor : public AActor
{
	GENERATED_BODY()

public:
	AStormTornadoVFXActor();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FMaterialState
	{
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		float BaseOpacity = 1.f;
	};

	struct FOrbitingSprite
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Component;
		int32 InstanceIndex = INDEX_NONE;
		float StartAngle = 0.f;
		float Radius = 100.f;
		float MinHeight = 0.f;
		float HeightRange = 100.f;
		float HeightPhase = 0.f;
		float RiseSpeed = 100.f;
		float AngularSpeed = 1.f;
		float BaseScale = 1.f;
		float PulsePhase = 0.f;
	};

	struct FPulsingLightning
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		float Phase = 0.f;
		float Period = 0.8f;
		float VisibleDuration = 0.12f;
		float BaseOpacity = 0.3f;
	};

	UStaticMeshComponent* CreateLayerComponent(
		FName ComponentName,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		float BaseOpacity,
		int32 SortPriority,
		UMaterialInstanceDynamic*& OutDynamicMaterial
	);
	UInstancedStaticMeshComponent* CreateInstancedLayerComponent(
		FName ComponentName,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		float BaseOpacity,
		int32 SortPriority,
		UMaterialInstanceDynamic*& OutDynamicMaterial
	);
	void CreateTornadoMeshes();
	void CreateCloudLayers();
	void CreateLightningLayers();
	void CreateGroundSparks();
	void UpdateMaterialFade(float Fade);
	void UpdateTornadoMeshes(float DeltaSeconds);
	void UpdateOrbitingSprites(const FVector& CameraLocation);
	void UpdateLightning(float Fade);
	FVector GetCameraLocation() const;

	UPROPERTY(VisibleAnywhere, Category="Storm Tornado")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> TornadoMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> LongSlideMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CoreMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OuterWindMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OuterLightningMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LightningMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SmokeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ParticleMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SpawnedComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> SpawnedInstanceComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;

	TArray<FMaterialState> MaterialStates;
	TArray<FOrbitingSprite> OrbitingSprites;
	TArray<FPulsingLightning> PulsingLightning;

	TWeakObjectPtr<UStaticMeshComponent> CoreComponent;
	TWeakObjectPtr<UStaticMeshComponent> OuterWindComponent;
	TWeakObjectPtr<UStaticMeshComponent> OuterLightningComponent;
	TWeakObjectPtr<UInstancedStaticMeshComponent> GroundSparkInstances;

	float ElapsedTime = 0.f;
	float VisualDuration = 8.f;
};
