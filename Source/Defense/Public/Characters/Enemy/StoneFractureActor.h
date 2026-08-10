#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StoneFractureActor.generated.h"

class UGeometryCollection;
class UGeometryCollectionComponent;
class URadialFalloff;
class USceneComponent;
class USkeletalMeshComponent;

USTRUCT(BlueprintType)
struct FStoneFracturePart
{
	GENERATED_BODY()

	// Geometry Collection의 피벗을 맞출 Skeletal Mesh 본 또는 소켓.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stone Fracture")
	FName BoneName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stone Fracture")
	TObjectPtr<UGeometryCollection> GeometryCollection = nullptr;

	// Geometry Collection 피벗과 본 사이의 차이를 보정하는 본 기준 Transform.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stone Fracture")
	FTransform BoneOffset = FTransform::Identity;
};

/**
 * 멈춘 Skeletal Mesh 포즈에 신체 부위별 Geometry Collection을 배치하고 파괴하는
 * 로컬 전용 연출 액터. Enemy와 독립적으로 남으며 수명이 끝나면 풀로 돌아간다.
 */
UCLASS(Blueprintable)
class DEFENSE_API AStoneFractureActor : public AActor
{
	GENERATED_BODY()

public:
	AStoneFractureActor();

	virtual void Tick(float DeltaTime) override;

	// SourceMesh의 현재 월드 본 Transform으로 파편을 배치한다.
	bool ActivateFromSkeletalMesh(USkeletalMeshComponent* SourceMesh);
	void DeactivateToPool();
	bool IsFractureActive() const { return bFractureActive; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stone Fracture")
	TObjectPtr<USceneComponent> SceneRoot;

	// 기본 배열에는 손과 발을 포함한 일반적인 UE 본 이름이 들어간다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stone Fracture|Parts")
	TArray<FStoneFracturePart> FractureParts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stone Fracture|Physics", meta=(ClampMin="0.0", Units="s"))
	float FractureDelay = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stone Fracture|Physics", meta=(ClampMin="0.0", Units="cm"))
	float FractureRadius = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stone Fracture|Physics", meta=(ClampMin="0.0"))
	float ExternalStrain = 500000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stone Fracture|Physics", meta=(ClampMin="0.0"))
	float RadialImpulseStrength = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stone Fracture|Pool", meta=(ClampMin="0.1", Units="s"))
	float ActiveLifetime = 8.f;

	// 관절 파편이나 Niagara 같은 추가 로컬 연출을 Blueprint에서 붙일 수 있다.
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Stone Fracture")
	void OnFractureActivated(FVector InFractureOrigin);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Stone Fracture")
	void OnFractureReturnedToPool();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGeometryCollectionComponent>> PartComponents;

	UPROPERTY(Transient)
	TObjectPtr<URadialFalloff> ActiveStrainField = nullptr;

	FVector FractureOrigin = FVector::ZeroVector;
	float ActiveElapsedTime = 0.f;
	float FractureElapsedTime = 0.f;
	bool bFractureActive = false;
	bool bPendingFracture = false;

	void AddDefaultPart(FName BoneName);
	void EnsurePartComponents();
	void ApplyFracture();
	void ReturnSelfToPool();
};
