#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "TrapBase.generated.h"

class UPrimitiveComponent;
class UStaticMeshComponent;
class USceneComponent;
class UTrapData;
class UBoxComponent;
class ADefensePlayerState;

UENUM(BlueprintType)
enum class ETrapRuntimeState : uint8
{
	Preview,
	Placed
};

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
	TObjectPtr<USceneComponent> SceneRoot; // Mesh와 DamageArea를 따로 조정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trap|Components")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trap|Components")
	TObjectPtr<UBoxComponent> DamageArea;

	UPROPERTY(ReplicatedUsing=OnRep_RuntimeState, VisibleInstanceOnly, BlueprintReadOnly, Category="Trap")
	ETrapRuntimeState RuntimeState = ETrapRuntimeState::Preview;
	
	UPROPERTY(BlueprintReadOnly, Category="Trap")
	TObjectPtr<UTrapData> SourceTrapData = nullptr;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Trap")
	TObjectPtr<ADefensePlayerState> OwnerPS = nullptr;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Trap")
	float Damage = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Trap")
	float DamageInterval = 3.f;

	FTimerHandle DamageTimerHandle;

	bool bInitialized = false;
	TSet<TWeakObjectPtr<AActor>> OverlappingEnemies;

	bool IsPlaced() const { return RuntimeState == ETrapRuntimeState::Placed; }
	void ConfigureFromTrapData(UTrapData* TrapData);
	void ApplyTrapMeshScale();
	void ApplyPreviewVisual();
	void SyncDamageAreaToMesh();
	void StartDamageTimer();
	void StopDamageTimer();
	void ApplyPeriodicDamage();
	void CacheCurrentOverlaps();

	UFUNCTION()
	void OnDamageAreaBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void OnDamageAreaEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

	UFUNCTION()
	void OnRep_RuntimeState();

public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializePreviewTrap(UTrapData* TrapData);

	void InitializePlacedTrap(UTrapData* TrapData, ADefensePlayerState* InInstalledByPlayerState);
	
	FORCEINLINE ADefensePlayerState* GetOwnerPS() const { return OwnerPS; }
	FORCEINLINE UTrapData* GetSourceTrapData() const { return SourceTrapData; }
};
