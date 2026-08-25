#include "Equipment/DefenseWeaponActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

ADefenseWeaponActor::ADefenseWeaponActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);

	StaticWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticWeaponMesh"));
	StaticWeaponMesh->SetupAttachment(WeaponMesh);
	StaticWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticWeaponMesh->SetGenerateOverlapEvents(false);
}

void ADefenseWeaponActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshActiveWeaponMesh();
}

UMeshComponent* ADefenseWeaponActor::GetActiveWeaponMesh() const
{
	if (StaticWeaponMesh && StaticWeaponMesh->GetStaticMesh())
	{
		return StaticWeaponMesh;
	}

	return WeaponMesh && WeaponMesh->GetSkeletalMeshAsset()
		? WeaponMesh.Get()
		: nullptr;
}

void ADefenseWeaponActor::RefreshActiveWeaponMesh()
{
	// 두 에셋이 모두 지정된 경우 새로 추가한 Static Mesh를 명시적인 선택으로 본다.
	const bool bUseStaticMesh = StaticWeaponMesh && StaticWeaponMesh->GetStaticMesh();
	const bool bUseSkeletalMesh = !bUseStaticMesh
		&& WeaponMesh
		&& WeaponMesh->GetSkeletalMeshAsset();

	if (WeaponMesh)
	{
		// WeaponMesh가 루트이므로 자식인 StaticWeaponMesh까지 전파하지 않는다.
		WeaponMesh->SetVisibility(bUseSkeletalMesh, false);
		WeaponMesh->SetHiddenInGame(!bUseSkeletalMesh, false);
		WeaponMesh->SetComponentTickEnabled(bUseSkeletalMesh);
	}

	if (StaticWeaponMesh)
	{
		StaticWeaponMesh->SetVisibility(bUseStaticMesh, false);
		StaticWeaponMesh->SetHiddenInGame(!bUseStaticMesh, false);
	}
}
