// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

USTRUCT()
struct FEnemySpawnPlan
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<class AEnemyBase> EnemyClass;

	UPROPERTY()
	TObjectPtr<class AEnemyRoute> Route;
};

UCLASS()
class DEFENSE_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEnemySpawner();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MyVar")
	TSubclassOf<class AEnemyBase> EnemyFactory;
	
	// 스폰할 적의 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MyVar")
	int32 EnemyCount = 25;

	// 비어 있으면 모든 웨이브에서 스폰. 값이 있으면 지정한 웨이브에서만 스폰.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	TArray<int32> SpawnWaves;

	// 프리뷰 스폰시 시간 간격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MyVar")
	float PreviewSpawnInterval = 5.f;

	// Combat 스폰시 1명당 시간 텀
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MyVar")
	float CombatSpawnInterval = 0.5f;
	
	// Combat 스폰시 2~4명 1조 / 조별 시간 텀
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MyVar")
	float CombatBatchInterval = 2.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MyVar")
	TObjectPtr<class UBoxComponent> BoxComp;
	
	// 에디터에서 맵에 있는 루트 직접 추가, 적을 스폰할 때 해당 적의 컨트롤러에 랜덤하게 루트 지정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MyVar")
	TArray<TObjectPtr<class AEnemyRoute>> EnemyRoutes;
	
	UPROPERTY()
	TObjectPtr<class UEnemyPoolSubsystem> EnemyPool;
	
	// 현재 맵에 나와있는 적 배열
	UPROPERTY()
	TArray<TObjectPtr<class AEnemyBase>> ActiveEnemies;
	
	// 적의 종류, 루트를 저장해 놓은 것. 프리뷰와 Combat에서 동일한 종류의 적이 동일한 루트로 나오게 하기 위함.
	UPROPERTY()
	TArray<FEnemySpawnPlan> CurrentWaveSpawnPlans;
	
	UFUNCTION()
	void SpawnTest();

	void StartPreviewSpawn(int32 WaveNumber);
	void StopPreviewSpawn();
	void ClearPreviewEnemies();
	void StartCombatSpawn(int32 WaveNumber);
	void EndWave();
	int32 PrepareCombatSpawnPlans(int32 WaveNumber);
	int32 GetCurrentWaveSpawnPlanCount() const;
	void SetEnemyPool(class UEnemyPoolSubsystem* InEnemyPool);
	void RemoveActiveEnemy(class AEnemyBase* Enemy);
	bool ShouldSpawnInWave(int32 WaveNumber) const;

	void AssignRandomRouteToEnemy(class AEnemyBase* Enemy) const;
	void RestartEnemyLogic(class AEnemyBase* Enemy) const;

private:
	void SpawnPreviewEnemy();
	void SpawnCombatBatch();
	void ReturnActiveEnemiesToPool();
	void AddActiveEnemy(class AEnemyBase* Enemy);
	void BuildCurrentWaveSpawnPlans();
	class AEnemyRoute* GetRandomRoute() const;
	bool ApplySpawnPlanToEnemy(class AEnemyBase* Enemy, int32 SpawnPlanIndex) const;

	FTimerHandle SpawnTimerHandle;
	int32 PreviewSpawnedCount = 0;
	int32 CombatSpawnedCount = 0;
	int32 CombatSpawnTargetCount = 0;
	int32 CurrentCombatBatchRemaining = 0;
	int32 CombatInitializedCount = 0;
	int32 CombatInitializationFailedCount = 0;
	
public:
	UFUNCTION()
	void OnBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);
	
};
