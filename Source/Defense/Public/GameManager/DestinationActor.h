// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestinationActor.generated.h"

UCLASS()
class DEFENSE_API ADestinationActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADestinationActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DestScore = 20;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UStaticMeshComponent> MeshComp;
	
	// 적이 닿으면 DestScore 감소, Enemy 이동 및 비활성화
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class USphereComponent> EnemySensor;
	
	// 플레이어가 닿으면 체력 회복, 범위를 좀 크게 하려고 따로 생성
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class USphereComponent> PlayerSensor;
	
public:
	UPROPERTY()
	TObjectPtr<class UEnemyPoolSubsystem> EnemyPool;
	
	UFUNCTION()
	void OnEnemySensorBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);
	
	UFUNCTION()
	void OnPlayerSensorBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);
};
