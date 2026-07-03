// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Player/WeaponComponent.h"

#include "Animation/AnimInstance.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Characters/Player/StatusComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Equipment/LoadoutComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}


void UWeaponComponent::Attack(EWeaponAttackType AttackType)
{
	UWeaponData* WeaponData = GetCurWeaponData();
	if (!WeaponData) return;

	ServerRPC_RequestAttack(AttackType);
}


void UWeaponComponent::ServerRPC_RequestAttack_Implementation(EWeaponAttackType AttackType)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()) return;

	UWeaponData* WeaponData = GetCurWeaponData();
	if (!WeaponData) return;

	const FAttackData* AttackData = GetAttackData(WeaponData, AttackType);
	float* LastAttackTime = nullptr;

	switch (AttackType)
	{
	case EWeaponAttackType::Attack:
		LastAttackTime = &LastAttackServerTime;
		break;

	case EWeaponAttackType::AltAttack:
		LastAttackTime = &LastAltAttackServerTime;
		break;

	default:
		ensureMsgf(false, TEXT("Unhandled WeaponAttackType"));
		return;
	}

	if (!AttackData || !LastAttackTime) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - *LastAttackTime < AttackData->Cooldown)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Attack rejected by server cooldown. Remaining: %.2f"),
			AttackData->Cooldown - (CurrentTime - *LastAttackTime));
		return;
	}

	UStatusComponent* StatusComp = OwnerActor->FindComponentByClass<UStatusComponent>();
	if (StatusComp && !StatusComp->TrySpendMana(AttackData->ManaCost))
	{
		UE_LOG(LogTemp, Verbose, TEXT("Attack rejected by server mana. Cost: %.1f / Mana: %.1f"),
			AttackData->ManaCost,
			StatusComp->Mana);
		return;
	}

	*LastAttackTime = CurrentTime;
	MulticastRPC_PlayAttack(AttackType);
	HitscanAttack(*AttackData);
}

void UWeaponComponent::MulticastRPC_PlayAttack_Implementation(EWeaponAttackType AttackType)
{
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	UWeaponData* WeaponData = GetCurWeaponData();
	const FAttackData* AttackData = GetAttackData(WeaponData, AttackType);

	if (AttackData && AttackData->Animation && OwnerCharacter && OwnerCharacter->GetMesh())
	{
		if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(
				AttackData->Animation,
				AttackData->AnimationSlotName
			);
		}
	}

	if (OwnerCharacter)
	{
		OwnerCharacter->OnAttackAccepted(AttackType);
	}
}

ADefenseCharacter* UWeaponComponent::GetOwnerCharacter() const
{
	return Cast<ADefenseCharacter>(GetOwner());
}

UWeaponData* UWeaponComponent::GetCurWeaponData() const
{
	AActor* OwnerActor = GetOwner();
	const ULoadoutComponent* LoadoutComp = OwnerActor ? OwnerActor->FindComponentByClass<ULoadoutComponent>() : nullptr;
	return LoadoutComp ? LoadoutComp->GetCurWeapon() : nullptr;
}

const FAttackData* UWeaponComponent::GetAttackData(UWeaponData* WeaponData, EWeaponAttackType AttackType) const
{
	if (!WeaponData) return nullptr;

	switch (AttackType)
	{
	case EWeaponAttackType::Attack:
		return &WeaponData->Attack;

	case EWeaponAttackType::AltAttack:
		return &WeaponData->AltAttack;

	default:
		ensureMsgf(false, TEXT("Unhandled WeaponAttackType"));
		return nullptr;
	}
}

void UWeaponComponent::HitscanAttack(const FAttackData& AttackData)
{
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter) return;

	AController* OwningController = OwnerCharacter->GetController();
	if (!OwningController) return;

	UWorld* World = GetWorld();
	if (!World) return;

	FVector ViewLocation;
	FRotator ViewRotation;
	OwningController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector Start = ViewLocation;
	const FVector End = Start + ViewRotation.Vector() * AttackData.Range;
	const float TraceRadius = FMath::Max(AttackData.Radius, 1.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponHitscanAttack), false, OwnerCharacter);
	Params.AddIgnoredActor(OwnerCharacter);

	const bool bHit = World->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(TraceRadius),
		Params
	);

#if ENABLE_DRAW_DEBUG
	const FColor DebugColor = bHit ? FColor::Red : FColor::Green;
	DrawDebugLine(World, Start, End, DebugColor, false, 1.0f, 0, 1.0f);
	DrawDebugSphere(World, bHit ? Hit.ImpactPoint : End, TraceRadius, 16, DebugColor, false, 1.0f);
#endif

	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();

		UE_LOG(LogTemp, Warning, TEXT("SphereTrace Hit: %s / Damage: %.1f"),
			*GetNameSafe(HitActor),
			AttackData.Damage);

		if (Cast<ADefenseCharacter>(HitActor)) return;

		UGameplayStatics::ApplyDamage(
			HitActor,
			AttackData.Damage,
			OwningController,
			OwnerCharacter,
			UDamageType::StaticClass()
		);
	}
}

