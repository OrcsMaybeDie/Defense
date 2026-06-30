#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "TrapBase.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UTrapData;
class UBoxComponent;

UCLASS()
class DEFENSE_API ATrapBase : public AActor
{
	GENERATED_BODY()

public:
	ATrapBase();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trap|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trap|Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trap|Components")
	TObjectPtr<UBoxComponent> DamageArea;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Trap")
	float Damage = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Trap")
	float DamageInterval = 3.f;

	FTimerHandle DamageTimerHandle;

	bool bPreviewMode = false;

	void SyncDamageAreaToMesh();
	void StartDamageTimer();
	void StopDamageTimer();
	void ApplyPeriodicDamage();

public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Trap")
	void InitializeTrap(const UTrapData* TrapData);

	UFUNCTION(BlueprintCallable, Category="Trap")
	void SetPreviewMode(bool bPreview);

	FORCEINLINE UStaticMeshComponent* GetMesh() const { return Mesh; }
	FORCEINLINE float GetDamage() const { return Damage; }
	FORCEINLINE float GetDamageInterval() const { return DamageInterval; }
};
