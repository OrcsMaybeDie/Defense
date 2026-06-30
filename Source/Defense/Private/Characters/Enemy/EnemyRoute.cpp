// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyRoute.h"

#include "Components/SplineComponent.h"


// Sets default values
AEnemyRoute::AEnemyRoute()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SplineComp = CreateDefaultSubobject<USplineComponent>("SplineComp");
	SetRootComponent(SplineComp);
}

// Called when the game starts or when spawned
void AEnemyRoute::BeginPlay()
{
	Super::BeginPlay();
	GenerateWaypoints();
}

void AEnemyRoute::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	GenerateWaypoints();
}

// Called every frame
void AEnemyRoute::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AEnemyRoute::GenerateWaypoints()
{
	Waypoints.Reset();

	if (!SplineComp || SampleInterval <= 0.f)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemyRoute GenerateWaypoints failed | Route=%s Spline=%s SampleInterval=%.1f"),
			//*GetNameSafe(this),
			//*GetNameSafe(SplineComp),
			//SampleInterval);
		return;
	}

	const float SplineLength = SplineComp->GetSplineLength();
	if (SplineLength <= 0.f)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EnemyRoute GenerateWaypoints failed | Route=%s SplineLength=%.1f SampleInterval=%.1f"),
			//*GetNameSafe(this),
			//SplineLength,
			//SampleInterval);
		return;
	}

	for (float Distance = 0.f; Distance < SplineLength; Distance += SampleInterval)
	{
		Waypoints.Add(SplineComp->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
	}

	Waypoints.Add(SplineComp->GetLocationAtDistanceAlongSpline(SplineLength, ESplineCoordinateSpace::World));

	//UE_LOG(LogTemp, Warning, TEXT("EnemyRoute GenerateWaypoints | Route=%s SplineLength=%.1f SampleInterval=%.1f Waypoints=%d"),
		//*GetNameSafe(this),
		//SplineLength,
		//SampleInterval,
		//Waypoints.Num());
}
