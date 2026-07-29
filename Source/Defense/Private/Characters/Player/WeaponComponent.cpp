// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Player/WeaponComponent.h"

#include "Animation/AnimInstance.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Characters/Player/StatusComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Equipment/DefenseWeaponActor.h"
#include "Equipment/LoadoutComponent.h"
#include "Equipment/WeaponData.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ADefenseCharacter* OwnerCharacter = GetOwnerCharacter())
	{
		if (ULoadoutComponent* LoadoutComp = OwnerCharacter->FindComponentByClass<ULoadoutComponent>())
		{
			LoadoutComp->OnSelectedEquipChanged.AddDynamic(this, &UWeaponComponent::HandleSelectedEquipmentChanged);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UWeaponComponent::RefreshEquippedWeapon)
		);
	}
}

void UWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EquippedWeaponActor)
	{
		EquippedWeaponActor->Destroy();
		EquippedWeaponActor = nullptr;
	}

	Super::EndPlay(EndPlayReason);
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

	UStatusComponent* StatusComp = OwnerActor->FindComponentByClass<UStatusComponent>();
	if (!StatusComp || !StatusComp->IsAlive()) return;

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

	if (!StatusComp->TrySpendMana(AttackData->ManaCost))
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

	if (OwnerCharacter)
	{
		OwnerCharacter->NotifyFireWeapon();
		if (EquippedWeaponActor)
		{
			EquippedWeaponActor->OnFireVFX();
		}
		if (AttackData)
		{
			PlayAttackAnimation(OwnerCharacter, *AttackData);
		}
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

void UWeaponComponent::RefreshEquippedWeapon()
{
	UWorld* World = GetWorld();
	
	// dediserver는 충돌 판정과 데미지만 담당
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UWeaponData* WeaponData = GetCurWeaponData();
	
	// Client
	SpawnAndAttachWeaponActor(WeaponData);
	ApplyWeaponAnimLayer(WeaponData);
}

void UWeaponComponent::SpawnAndAttachWeaponActor(UWeaponData* WeaponData)
{
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter || !OwnerCharacter->GetMesh() || !GetWorld()) return;

	const TSubclassOf<ADefenseWeaponActor> WeaponActorClass = WeaponData ? WeaponData->WeaponActorClass : nullptr;

	if (EquippedWeaponActor && EquippedWeaponActorClass == WeaponActorClass)
	{
		return;
	}

	if (EquippedWeaponActor)
	{
		EquippedWeaponActor->Destroy();
		EquippedWeaponActor = nullptr;
		EquippedWeaponActorClass = nullptr;
	}

	if (!WeaponActorClass) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	EquippedWeaponActor = GetWorld()->SpawnActor<ADefenseWeaponActor>(
		WeaponActorClass,
		FTransform::Identity,
		SpawnParams
	);

	if (!EquippedWeaponActor) return;

	EquippedWeaponActorClass = WeaponActorClass;
	EquippedWeaponActor->SetActorEnableCollision(false);
	EquippedWeaponActor->AttachToComponent(
		OwnerCharacter->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		WeaponData->WeaponAttachSocket
	);
	EquippedWeaponActor->SetActorRelativeTransform(WeaponData->WeaponAttachTransform);
}

void UWeaponComponent::ApplyWeaponAnimLayer(UWeaponData* WeaponData)
{
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!WeaponData || !WeaponData->EquippedAnimLayerClass || !OwnerCharacter || !OwnerCharacter->GetMesh()) return;

	if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
	{
		AnimInstance->LinkAnimClassLayers(WeaponData->EquippedAnimLayerClass);
	}
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

void UWeaponComponent::PlayAttackAnimation(ADefenseCharacter* OwnerCharacter, const FAttackData& AttackData)
{
	if (!OwnerCharacter) return;

	if (OwnerCharacter->GetMesh())
	{
		if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
		{
			if (AttackData.Montage)
			{
				const float PlayResult = AnimInstance->Montage_Play(AttackData.Montage);
				if (PlayResult <= 0.f)
				{
					UE_LOG(LogTemp, Warning, TEXT("Attack montage play failed. Character=%s Montage=%s AnimInstance=%s"),
						*GetNameSafe(OwnerCharacter),
						*GetNameSafe(AttackData.Montage),
						*GetNameSafe(AnimInstance));
				}
			}
			else if (AttackData.Animation)
			{
				AnimInstance->PlaySlotAnimationAsDynamicMontage(
					AttackData.Animation,
					AttackData.AnimationSlotName
				);
			}
		}
	}
}

void UWeaponComponent::HandleSelectedEquipmentChanged(int32 SelectedSlotIdx, UEquipmentData* SelectedEquipment)
{
	RefreshEquippedWeapon();
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

