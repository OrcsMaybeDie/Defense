#include "Characters/Player/WeaponComponent.h"

#include "Animation/AnimInstance.h"
#include "Characters/Enemy/EnemyBase.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Defense.h"
#include "Equipment/DefenseWeaponActor.h"
#include "Equipment/LoadoutComponent.h"
#include "Effects/StormTornadoVFXActor.h"
#include "Effects/WeaponProjectileVFXActor.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	constexpr float Stage3MinimumMana = 90.f;
}

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
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

		if (UStatusComponent* StatusComp = OwnerCharacter->FindComponentByClass<UStatusComponent>())
		{
			StatusComp->OnLifeStateChanged.AddUniqueDynamic(this, &UWeaponComponent::HandleLifeStateChanged);
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
	ClearWeaponMovementLock();

	if (ADefenseCharacter* OwnerCharacter = GetOwnerCharacter())
	{
		if (ULoadoutComponent* LoadoutComp = OwnerCharacter->FindComponentByClass<ULoadoutComponent>())
		{
			LoadoutComp->OnSelectedEquipChanged.RemoveDynamic(this, &UWeaponComponent::HandleSelectedEquipmentChanged);
		}

		if (UStatusComponent* StatusComp = OwnerCharacter->FindComponentByClass<UStatusComponent>())
		{
			StatusComp->OnLifeStateChanged.RemoveDynamic(this, &UWeaponComponent::HandleLifeStateChanged);
		}
	}

	ResetLocalChargeState();
	ResetServerChargeState();
	NextFireServerTimes.Reset();
	NextChargedFireServerTimes.Reset();
	NextFireRequestLocalTimes.Reset();
	NextChargedFireLocalTimes.Reset();

	if (EquippedWeaponActor)
	{
		EquippedWeaponActor->Destroy();
		EquippedWeaponActor = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UWeaponComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bChargeInputHeld)
	{
		const EWeaponChargeStage CurrentStage = GetChargeStage();
		const int32 PreviousStageIndex = static_cast<int32>(LastLocalChargeStage);
		const int32 CurrentStageIndex = static_cast<int32>(CurrentStage);
		if (CurrentStageIndex > PreviousStageIndex && LocalChargeWeaponData)
		{
			for (int32 StageIndex = PreviousStageIndex + 1; StageIndex <= CurrentStageIndex; ++StageIndex)
			{
				PlayChargeStageReachedVFX(LocalChargeWeaponData->ChargedFire);
			}
		}
		LastLocalChargeStage = CurrentStage;
		BroadcastChargePreview();
	}
}

void UWeaponComponent::SetCinematicVisualHidden(bool bHidden)
{
	if (bCinematicVisualHidden == bHidden)
	{
		return;
	}

	bCinematicVisualHidden = bHidden;
	if (!EquippedWeaponActor)
	{
		return;
	}

	if (bHidden)
	{
		bWeaponWasHiddenBeforeCinematic = EquippedWeaponActor->IsHidden();
		EquippedWeaponActor->SetActorHiddenInGame(true);
	}
	else
	{
		EquippedWeaponActor->SetActorHiddenInGame(bWeaponWasHiddenBeforeCinematic);
	}
}

void UWeaponComponent::RequestFire()
{
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	UWeaponData* WeaponData = GetCurWeaponData();
	const FWeaponShotData* FireData = GetShotData(WeaponData, EWeaponActionType::Fire);
	UWorld* World = GetWorld();

	if (!OwnerCharacter || !WeaponData || !FireData || !World) return;
	if (!OwnerCharacter->IsLocallyControlled() && !OwnerCharacter->HasAuthority()) return;

	// 차지 입력을 유지하는 동안 일반 발사는 로컬과 서버 양쪽에서 거절한다.
	if (bChargeInputHeld || bServerChargeActive) return;
	if (!CanLocallyFire()) return;

	const float CurrentTime = World->GetTimeSeconds();
	if (const float* NextReadyTime = NextFireRequestLocalTimes.Find(WeaponData))
	{
		if (CurrentTime < *NextReadyTime) return;
	}

	NextFireRequestLocalTimes.Add(WeaponData, CurrentTime + FMath::Max(0.f, FireData->Cooldown));

	// 소유 클라이언트는 서버 왕복을 기다리지 않고 즉시 표현을 재생한다.
	if (OwnerCharacter->IsLocallyControlled())
	{
		PlayWeaponActionCosmetics(
			EWeaponActionType::Fire,
			EWeaponActionPhase::Executed,
			0.f
		);
	}

	ServerRPC_RequestFire();
}

void UWeaponComponent::BeginCharge()
{
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	UWeaponData* WeaponData = GetCurWeaponData();
	UWorld* World = GetWorld();

	if (!OwnerCharacter || !WeaponData || !World)
	{
		return;
	}
	if (!OwnerCharacter->IsLocallyControlled() && !OwnerCharacter->HasAuthority())
	{
		return;
	}
	if (bChargeInputHeld || bServerChargeActive)
	{
		return;
	}
	if (!WeaponData->ChargedFire.bEnabled)
	{
		return;
	}
	if (!CanLocallyBeginCharge(WeaponData->ChargedFire))
	{
		return;
	}

	if (const float* NextReadyTime = NextChargedFireLocalTimes.Find(WeaponData))
	{
		if (World->GetTimeSeconds() < *NextReadyTime)
		{
			return;
		}
	}

	if (OwnerCharacter->IsLocallyControlled())
	{
		bChargeInputHeld = true;
		LocalChargeStartTime = World->GetTimeSeconds();
		LocalChargeWeaponData = WeaponData;
		LastLocalChargeStage = EWeaponChargeStage::None;
		SetComponentTickEnabled(true);
		BroadcastChargePreview();
	}

	ServerRPC_BeginCharge();
}

void UWeaponComponent::ReleaseCharge()
{
	UWorld* World = GetWorld();
	UWeaponData* WeaponData = LocalChargeWeaponData.Get();
	if (!bChargeInputHeld || !World || !WeaponData)
	{
		return;
	}

	const float HeldTime = FMath::Max(0.f, World->GetTimeSeconds() - LocalChargeStartTime);
	const FWeaponChargeData ChargeData = WeaponData->ChargedFire;
	const UStatusComponent* StatusComp = GetOwnerCharacter()
		? GetOwnerCharacter()->FindComponentByClass<UStatusComponent>()
		: nullptr;
	const float UnclampedChargeRatio = CalculateChargeRatio(ChargeData, HeldTime);
	const bool bRequestedStage3 = CalculateChargeStage(ChargeData, UnclampedChargeRatio) == EWeaponChargeStage::Stage3;
	const bool bHasStage3Mana = StatusComp && StatusComp->Mana >= Stage3MinimumMana;
	const float ChargeRatio = ClampChargeRatioToAvailableMana(
		ChargeData,
		UnclampedChargeRatio,
		StatusComp ? StatusComp->Mana : 0.f
	);
	const EWeaponChargeStage ChargeStage = CalculateChargeStage(ChargeData, ChargeRatio);
	const FWeaponChargeStageData* StageData = GetChargeStageData(ChargeData, ChargeStage);
	ResetLocalChargeState();

	if (!ChargeData.bEnabled
		|| !StageData
		|| WeaponData != GetCurWeaponData()
		|| (bRequestedStage3 && !bHasStage3Mana))
	{
		PlayWeaponActionCosmetics(
			EWeaponActionType::ChargedFire,
			EWeaponActionPhase::Cancelled,
			0.f
		);
		ServerRPC_CancelCharge();
		return;
	}

	NextChargedFireLocalTimes.Add(
		WeaponData,
		World->GetTimeSeconds() + FMath::Max(0.f, StageData->ReleaseFire.Cooldown)
	);
	PlayWeaponActionCosmetics(
		EWeaponActionType::ChargedFire,
		EWeaponActionPhase::Executed,
		ChargeRatio
	);
	ApplyWeaponMovementLock(StageData->MovementLockDuration);
	ServerRPC_ReleaseCharge();
}

void UWeaponComponent::CancelCharge()
{
	const bool bHadLocalCharge = bChargeInputHeld;
	const bool bHadServerCharge = bServerChargeActive;

	if (bHadLocalCharge)
	{
		PlayWeaponActionCosmetics(
			EWeaponActionType::ChargedFire,
			EWeaponActionPhase::Cancelled,
			0.f
		);
		ResetLocalChargeState();
	}

	if (bHadLocalCharge || bHadServerCharge)
	{
		ServerRPC_CancelCharge();
	}
}

float UWeaponComponent::GetChargeRatio() const
{
	const UWorld* World = GetWorld();
	if (!World) return 0.f;
	const ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	const UStatusComponent* StatusComp = OwnerCharacter
		? OwnerCharacter->FindComponentByClass<UStatusComponent>()
		: nullptr;
	const float AvailableMana = StatusComp ? StatusComp->Mana : 0.f;

	if (bChargeInputHeld && LocalChargeWeaponData)
	{
		const FWeaponChargeData& ChargeData = LocalChargeWeaponData->ChargedFire;
		return ClampChargeRatioToAvailableMana(
			ChargeData,
			CalculateChargeRatio(
				ChargeData,
				FMath::Max(0.f, World->GetTimeSeconds() - LocalChargeStartTime)
			),
			AvailableMana
		);
	}

	if (bServerChargeActive && ServerChargeWeaponData)
	{
		const FWeaponChargeData& ChargeData = ServerChargeWeaponData->ChargedFire;
		return ClampChargeRatioToAvailableMana(
			ChargeData,
			CalculateChargeRatio(
				ChargeData,
				FMath::Max(0.f, World->GetTimeSeconds() - ServerChargeStartTime)
			),
			AvailableMana
		);
	}

	return 0.f;
}

EWeaponChargeStage UWeaponComponent::GetChargeStage() const
{
	const UWeaponData* WeaponData = bChargeInputHeld
		? LocalChargeWeaponData.Get()
		: ServerChargeWeaponData.Get();
	if (!WeaponData) return EWeaponChargeStage::None;

	return CalculateChargeStage(WeaponData->ChargedFire, GetChargeRatio());
}

float UWeaponComponent::GetChargePreviewManaCost() const
{
	const UWeaponData* WeaponData = bChargeInputHeld
		? LocalChargeWeaponData.Get()
		: ServerChargeWeaponData.Get();
	if (!WeaponData) return 0.f;

	return CalculateChargePreviewManaCost(WeaponData->ChargedFire, GetChargeRatio());
}

bool UWeaponComponent::HasEquippedWeapon() const
{
	return GetCurWeaponData() != nullptr;
}

float UWeaponComponent::GetFireRange() const
{
	const UWeaponData* WeaponData = GetCurWeaponData();
	return WeaponData ? FMath::Max(0.f, WeaponData->Fire.Range) : 0.f;
}

float UWeaponComponent::GetFireAimAssistRadius() const
{
	const UWeaponData* WeaponData = GetCurWeaponData();
	return WeaponData ? FMath::Max(0.f, WeaponData->Fire.AimAssistRadius) : 0.f;
}

void UWeaponComponent::ServerRPC_RequestFire_Implementation()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()) return;
	if (const ADefenseCharacter* OwnerCharacter = GetOwnerCharacter())
	{
		if (OwnerCharacter->IsWeaponMovementLocked()) return;
	}

	if (bServerChargeActive)
	{
		return;
	}

	UWeaponData* WeaponData = GetCurWeaponData();
	const FWeaponShotData* FireData = GetShotData(WeaponData, EWeaponActionType::Fire);
	if (!WeaponData || !FireData) return;

	TryExecuteServerFire(WeaponData, EWeaponActionType::Fire, *FireData, 0.f, 0.f);
}

void UWeaponComponent::ServerRPC_BeginCharge_Implementation()
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	UWeaponData* WeaponData = GetCurWeaponData();
	UStatusComponent* StatusComp = OwnerActor ? OwnerActor->FindComponentByClass<UStatusComponent>() : nullptr;

	if (!OwnerActor || !OwnerActor->HasAuthority() || !World || !WeaponData || !StatusComp)
	{
		ClientRPC_RejectCharge();
		return;
	}

	const FWeaponChargeData& ChargeData = WeaponData->ChargedFire;
	const FWeaponChargeStageData* Stage1Data = GetChargeStageData(ChargeData, EWeaponChargeStage::Stage1);
	const float CurrentTime = World->GetTimeSeconds();
	const float* NextReadyTime = NextChargedFireServerTimes.Find(WeaponData);
	const bool bOnCooldown = NextReadyTime && CurrentTime < *NextReadyTime;

	if (bServerChargeActive
		|| !StatusComp->IsAlive()
		|| (GetOwnerCharacter() && GetOwnerCharacter()->IsWeaponMovementLocked())
		|| !ChargeData.bEnabled
		|| !Stage1Data
		|| bOnCooldown
		|| !StatusComp->CanSpendMana(Stage1Data->ManaCost))
	{
		ClientRPC_RejectCharge();
		return;
	}

	bServerChargeActive = true;
	ServerChargeStartTime = CurrentTime;
	ServerChargeWeaponData = WeaponData;

}

void UWeaponComponent::ServerRPC_ReleaseCharge_Implementation()
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !World || !bServerChargeActive)
	{
		ClientRPC_RejectCharge();
		return;
	}

	UWeaponData* WeaponData = ServerChargeWeaponData.Get();
	UStatusComponent* StatusComp = OwnerActor->FindComponentByClass<UStatusComponent>();
	const float HeldTime = FMath::Max(0.f, World->GetTimeSeconds() - ServerChargeStartTime);
	const float UnclampedChargeRatio = WeaponData
		? CalculateChargeRatio(WeaponData->ChargedFire, HeldTime)
		: 0.f;
	const bool bRequestedStage3 = WeaponData
		&& CalculateChargeStage(WeaponData->ChargedFire, UnclampedChargeRatio) == EWeaponChargeStage::Stage3;
	const bool bHasStage3Mana = StatusComp && StatusComp->Mana >= Stage3MinimumMana;
	const float ChargeRatio = WeaponData
		? ClampChargeRatioToAvailableMana(
			WeaponData->ChargedFire,
			UnclampedChargeRatio,
			StatusComp ? StatusComp->Mana : 0.f
		)
		: 0.f;
	const EWeaponChargeStage ChargeStage = WeaponData
		? CalculateChargeStage(WeaponData->ChargedFire, ChargeRatio)
		: EWeaponChargeStage::None;
	const FWeaponChargeStageData* StageData = WeaponData
		? GetChargeStageData(WeaponData->ChargedFire, ChargeStage)
		: nullptr;
	if (!WeaponData
		|| WeaponData != GetCurWeaponData()
		|| !WeaponData->ChargedFire.bEnabled
		|| !StageData
		|| (bRequestedStage3 && !bHasStage3Mana))
	{
		RejectServerCharge();
		return;
	}

	ResetServerChargeState();

	if (!TryExecuteServerFire(
		WeaponData,
		EWeaponActionType::ChargedFire,
		StageData->ReleaseFire,
		ChargeRatio,
		StageData->ManaCost
	))
	{
		MulticastRPC_PlayWeaponAction(
			EWeaponActionType::ChargedFire,
			EWeaponActionPhase::Cancelled,
			0.f
		);
		ClientRPC_RejectCharge();
	}
}

void UWeaponComponent::ServerRPC_CancelCharge_Implementation()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !bServerChargeActive) return;

	MulticastRPC_PlayWeaponAction(
		EWeaponActionType::ChargedFire,
		EWeaponActionPhase::Cancelled,
		0.f
	);
	ResetServerChargeState();
}

void UWeaponComponent::ClientRPC_RejectCharge_Implementation()
{
	ClearWeaponMovementLock();
	PlayWeaponActionCosmetics(
		EWeaponActionType::ChargedFire,
		EWeaponActionPhase::Cancelled,
		0.f
	);
	ResetLocalChargeState();
}

void UWeaponComponent::MulticastRPC_PlayWeaponAction_Implementation(
	EWeaponActionType ActionType,
	EWeaponActionPhase Phase,
	float ChargeRatio
)
{
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter || OwnerCharacter->IsLocallyControlled()) return;

	PlayWeaponActionCosmetics(ActionType, Phase, ChargeRatio);
}

void UWeaponComponent::MulticastRPC_PlayChargedFireVFX_Implementation(
	EWeaponChargeStage ChargeStage,
	FVector_NetQuantize ShotTargetLocation
)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	PlayChargedFireVFX(ChargeStage, ShotTargetLocation);
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

const FWeaponShotData* UWeaponComponent::GetShotData(
	const UWeaponData* WeaponData,
	EWeaponActionType ActionType,
	float ChargeRatio
) const
{
	if (!WeaponData) return nullptr;

	switch (ActionType)
	{
	case EWeaponActionType::Fire:
		return &WeaponData->Fire;

	case EWeaponActionType::ChargedFire:
	{
		if (!WeaponData->ChargedFire.bEnabled) return nullptr;

		const EWeaponChargeStage ChargeStage = CalculateChargeStage(
			WeaponData->ChargedFire,
			ChargeRatio
		);
		const FWeaponChargeStageData* StageData = GetChargeStageData(
			WeaponData->ChargedFire,
			ChargeStage
		);
		return StageData ? &StageData->ReleaseFire : nullptr;
	}

	default:
		ensureMsgf(false, TEXT("Unhandled EWeaponActionType"));
		return nullptr;
	}
}

const FWeaponChargeStageData* UWeaponComponent::GetChargeStageData(
	const FWeaponChargeData& ChargeData,
	EWeaponChargeStage ChargeStage
) const
{
	switch (ChargeStage)
	{
	case EWeaponChargeStage::Stage1:
		return &ChargeData.Stage1;

	case EWeaponChargeStage::Stage2:
		return &ChargeData.Stage2;

	case EWeaponChargeStage::Stage3:
		return &ChargeData.Stage3;

	case EWeaponChargeStage::None:
	default:
		return nullptr;
	}
}

bool UWeaponComponent::CanLocallyFire() const
{
	const ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	const UStatusComponent* StatusComp = OwnerCharacter
		? OwnerCharacter->FindComponentByClass<UStatusComponent>()
		: nullptr;

	return StatusComp
		&& StatusComp->IsAlive()
		&& OwnerCharacter
		&& !OwnerCharacter->IsWeaponMovementLocked();
}

bool UWeaponComponent::CanLocallyBeginCharge(const FWeaponChargeData& ChargeData) const
{
	const ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	const UStatusComponent* StatusComp = OwnerCharacter
		? OwnerCharacter->FindComponentByClass<UStatusComponent>()
		: nullptr;

	return StatusComp
		&& StatusComp->IsAlive()
		&& OwnerCharacter
		&& !OwnerCharacter->IsWeaponMovementLocked()
		&& StatusComp->CanSpendMana(ChargeData.Stage1.ManaCost);
}

bool UWeaponComponent::TryExecuteServerFire(
	UWeaponData* WeaponData,
	EWeaponActionType ActionType,
	const FWeaponShotData& ShotData,
	float ChargeRatio,
	float ManaCost
)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	UStatusComponent* StatusComp = OwnerActor ? OwnerActor->FindComponentByClass<UStatusComponent>() : nullptr;
	if (!OwnerActor || !OwnerActor->HasAuthority() || !World || !WeaponData || !StatusComp || !StatusComp->IsAlive())
	{
		return false;
	}

	TMap<const UWeaponData*, float>& CooldownMap = ActionType == EWeaponActionType::Fire
		? NextFireServerTimes
		: NextChargedFireServerTimes;

	const float CurrentTime = World->GetTimeSeconds();
	if (const float* NextReadyTime = CooldownMap.Find(WeaponData))
	{
		if (CurrentTime < *NextReadyTime)
		{
			return false;
		}
	}

	EWeaponChargeStage ExecutedChargeStage = EWeaponChargeStage::None;
	if (ActionType == EWeaponActionType::ChargedFire)
	{
		ExecutedChargeStage = CalculateChargeStage(
			WeaponData->ChargedFire,
			ChargeRatio
		);
	}

	FHitResult ShotHit;
	const bool bIsStage3 = ExecutedChargeStage == EWeaponChargeStage::Stage3;
	const FVector ShotTargetLocation = PerformHitscan(ShotData, &ShotHit, !bIsStage3);
	AEnemyBase* HitEnemy = bIsStage3 ? Cast<AEnemyBase>(ShotHit.GetActor()) : nullptr;
	if (HitEnemy
		&& (HitEnemy->EnemyMode != EEnemyMode::Combat
			|| HitEnemy->EnemyState == EEnemyState::Die
			|| HitEnemy->EnemyState == EEnemyState::StoneDie))
	{
		HitEnemy = nullptr;
	}
	const float ActualManaCost = bIsStage3
		? (HitEnemy ? StatusComp->Mana : StatusComp->MaxMana * 0.5f)
		: ManaCost;
	if (ActionType == EWeaponActionType::ChargedFire && !StatusComp->TrySpendMana(ActualManaCost))
	{
		return false;
	}

	// Stage 3은 적을 직접 맞힌 경우에만 공격으로 성립한다. 실패 시 마나 절반을
	// 패널티로 소비하고 VFX, 피해, 쿨다운, 발사 표현은 실행하지 않는다.
	if (bIsStage3 && !HitEnemy)
	{
		return false;
	}
	if (ActionType == EWeaponActionType::ChargedFire)
	{
		if (const FWeaponChargeStageData* StageData = GetChargeStageData(
			WeaponData->ChargedFire,
			ExecutedChargeStage
		))
		{
			ApplyWeaponMovementLock(StageData->MovementLockDuration);
		}
	}

	CooldownMap.Add(WeaponData, CurrentTime + FMath::Max(0.f, ShotData.Cooldown));

	MulticastRPC_PlayWeaponAction(ActionType, EWeaponActionPhase::Executed, ChargeRatio);
	if (ExecutedChargeStage == EWeaponChargeStage::Stage3)
	{
		SpawnStage3StormTornado(HitEnemy);
	}
	else if (ExecutedChargeStage != EWeaponChargeStage::None)
	{
		MulticastRPC_PlayChargedFireVFX(ExecutedChargeStage, ShotTargetLocation);
	}
	return true;
}

float UWeaponComponent::CalculateChargeRatio(const FWeaponChargeData& ChargeData, float HeldTime) const
{
	const float MaxTime = FMath::Max(KINDA_SMALL_NUMBER, ChargeData.MaxChargeTime);
	return FMath::Clamp(HeldTime / MaxTime, 0.f, 1.f);
}

float UWeaponComponent::ClampChargeRatioToAvailableMana(
	const FWeaponChargeData& ChargeData,
	float ChargeRatio,
	float AvailableMana
) const
{
	float MaxAffordableRatio = 0.f;
	if (AvailableMana >= ChargeData.Stage1.ManaCost)
	{
		MaxAffordableRatio = 1.f / 3.f;
	}
	if (MaxAffordableRatio > 0.f && AvailableMana >= ChargeData.Stage2.ManaCost)
	{
		MaxAffordableRatio = 2.f / 3.f;
	}
	if (MaxAffordableRatio >= 2.f / 3.f && AvailableMana >= ChargeData.Stage3.ManaCost)
	{
		MaxAffordableRatio = 1.f;
	}

	return FMath::Min(FMath::Clamp(ChargeRatio, 0.f, 1.f), MaxAffordableRatio);
}

EWeaponChargeStage UWeaponComponent::CalculateChargeStage(
	const FWeaponChargeData& ChargeData,
	float ChargeRatio
) const
{
	if (!ChargeData.bEnabled) return EWeaponChargeStage::None;

	const float ClampedRatio = FMath::Clamp(ChargeRatio, 0.f, 1.f);
	if (ClampedRatio >= 1.f - KINDA_SMALL_NUMBER) return EWeaponChargeStage::Stage3;
	if (ClampedRatio >= 2.f / 3.f) return EWeaponChargeStage::Stage2;
	if (ClampedRatio >= 1.f / 3.f) return EWeaponChargeStage::Stage1;
	return EWeaponChargeStage::None;
}

float UWeaponComponent::CalculateChargePreviewManaCost(
	const FWeaponChargeData& ChargeData,
	float ChargeRatio
) const
{
	const float ScaledRatio = FMath::Clamp(ChargeRatio, 0.f, 1.f) * 3.f;
	if (ScaledRatio < 1.f)
	{
		return FMath::Lerp(0.f, ChargeData.Stage1.ManaCost, ScaledRatio);
	}

	if (ScaledRatio < 2.f)
	{
		return FMath::Lerp(ChargeData.Stage1.ManaCost, ChargeData.Stage2.ManaCost, ScaledRatio - 1.f);
	}

	return FMath::Lerp(ChargeData.Stage2.ManaCost, ChargeData.Stage3.ManaCost, ScaledRatio - 2.f);
}

void UWeaponComponent::BroadcastChargePreview()
{
	OnChargePreviewChanged.Broadcast(
		GetChargeStage(),
		GetChargeRatio(),
		GetChargePreviewManaCost()
	);
}

void UWeaponComponent::PlayWeaponActionCosmetics(
	EWeaponActionType ActionType,
	EWeaponActionPhase Phase,
	float ChargeRatio
)
{
	UWorld* World = GetWorld();
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !OwnerCharacter) return;

	const float ClampedChargeRatio = FMath::Clamp(ChargeRatio, 0.f, 1.f);
	const UWeaponData* WeaponData = GetCurWeaponData();
	const EWeaponChargeStage ChargeStage = ActionType == EWeaponActionType::ChargedFire && WeaponData
		? CalculateChargeStage(WeaponData->ChargedFire, ClampedChargeRatio)
		: EWeaponChargeStage::None;
	if (Phase == EWeaponActionPhase::Executed)
	{
		if (GetShotData(WeaponData, ActionType, ClampedChargeRatio))
		{
			OwnerCharacter->NotifyWeaponFired();
			// 모든 일반/마나 공격은 DA의 Fire 애니메이션 하나만 공유한다.
			PlayFireAnimation(OwnerCharacter, WeaponData->Fire);
		}
	}

	if (EquippedWeaponActor)
	{
		EquippedWeaponActor->OnWeaponAction(ActionType, Phase, ChargeStage, ClampedChargeRatio);
	}

	OwnerCharacter->OnWeaponAction(ActionType, Phase, ChargeStage, ClampedChargeRatio);
}

void UWeaponComponent::ResetLocalChargeState()
{
	const bool bHadLocalCharge = bChargeInputHeld;
	bChargeInputHeld = false;
	LocalChargeStartTime = 0.f;
	LocalChargeWeaponData = nullptr;
	LastLocalChargeStage = EWeaponChargeStage::None;

	if (bHadLocalCharge)
	{
		SetComponentTickEnabled(false);
		OnChargePreviewChanged.Broadcast(EWeaponChargeStage::None, 0.f, 0.f);
	}
}

void UWeaponComponent::ResetServerChargeState()
{
	bServerChargeActive = false;
	ServerChargeStartTime = 0.f;
	ServerChargeWeaponData = nullptr;
}

void UWeaponComponent::RejectServerCharge()
{
	if (bServerChargeActive)
	{
		MulticastRPC_PlayWeaponAction(
			EWeaponActionType::ChargedFire,
			EWeaponActionPhase::Cancelled,
			0.f
		);
	}

	ResetServerChargeState();
	ClientRPC_RejectCharge();
}

void UWeaponComponent::RefreshEquippedWeapon()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer) return;

	UWeaponData* WeaponData = GetCurWeaponData();
	SpawnAndAttachWeaponActor(WeaponData);
	ApplyWeaponAnimLayer(WeaponData);
}

void UWeaponComponent::SpawnAndAttachWeaponActor(UWeaponData* WeaponData)
{
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter || !OwnerCharacter->GetMesh() || !GetWorld()) return;

	const TSubclassOf<ADefenseWeaponActor> WeaponActorClass = WeaponData ? WeaponData->WeaponActorClass : nullptr;
	if (EquippedWeaponActor && EquippedWeaponActorClass == WeaponActorClass) return;

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

	if (bCinematicVisualHidden)
	{
		bWeaponWasHiddenBeforeCinematic = EquippedWeaponActor->IsHidden();
		EquippedWeaponActor->SetActorHiddenInGame(true);
	}
}

void UWeaponComponent::ApplyWeaponAnimLayer(UWeaponData* WeaponData)
{
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	UAnimInstance* AnimInstance = OwnerCharacter && OwnerCharacter->GetMesh()
		? OwnerCharacter->GetMesh()->GetAnimInstance()
		: nullptr;
	if (!AnimInstance) return;

	const TSubclassOf<UAnimInstance> DesiredAnimLayerClass = WeaponData && WeaponData->EquippedAnimLayerClass
		? WeaponData->EquippedAnimLayerClass
		: DefaultAnimLayerClass;
	if (AppliedAnimLayerClass == DesiredAnimLayerClass) return;

	if (AppliedAnimLayerClass)
	{
		AnimInstance->UnlinkAnimClassLayers(AppliedAnimLayerClass);
	}

	AppliedAnimLayerClass = DesiredAnimLayerClass;
	if (AppliedAnimLayerClass)
	{
		AnimInstance->LinkAnimClassLayers(AppliedAnimLayerClass);
	}
}

void UWeaponComponent::PlayFireAnimation(ADefenseCharacter* OwnerCharacter, const FWeaponFireData& FireData)
{
	if (!OwnerCharacter || !OwnerCharacter->GetMesh()) return;

	if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
	{
		if (FireData.Montage)
		{
			const float PlayResult = AnimInstance->Montage_Play(FireData.Montage);
			if (PlayResult <= 0.f)
			{
				UE_LOG(LogDefense, Warning, TEXT("Weapon montage play failed. Character=%s Montage=%s AnimInstance=%s"),
					*GetNameSafe(OwnerCharacter),
					*GetNameSafe(FireData.Montage),
					*GetNameSafe(AnimInstance));
			}
		}
		else if (FireData.Animation)
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(
				FireData.Animation,
				FireData.AnimationSlotName
			);
		}
	}
}

void UWeaponComponent::ApplyWeaponMovementLock(float Duration)
{
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!OwnerCharacter || !World || Duration <= 0.f) return;

	OwnerCharacter->SetWeaponMovementLocked(true);
	World->GetTimerManager().ClearTimer(WeaponMovementLockTimer);
	World->GetTimerManager().SetTimer(
		WeaponMovementLockTimer,
		this,
		&UWeaponComponent::ClearWeaponMovementLock,
		Duration,
		false
	);
}

void UWeaponComponent::ClearWeaponMovementLock()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WeaponMovementLockTimer);
	}

	if (ADefenseCharacter* OwnerCharacter = GetOwnerCharacter())
	{
		OwnerCharacter->SetWeaponMovementLocked(false);
	}
}

void UWeaponComponent::HandleSelectedEquipmentChanged(int32 SelectedSlotIdx, UEquipmentData* SelectedEquipment)
{
	if (bChargeInputHeld || bServerChargeActive)
	{
		CancelCharge();
	}

	RefreshEquippedWeapon();
}

void UWeaponComponent::HandleLifeStateChanged(EPlayerLifeState NewLifeState)
{
	if (NewLifeState == EPlayerLifeState::Alive) return;
	ClearWeaponMovementLock();

	if (bChargeInputHeld || bServerChargeActive)
	{
		CancelCharge();
	}
}

FVector UWeaponComponent::PerformHitscan(
	const FWeaponShotData& ShotData,
	FHitResult* OutHitResult,
	bool bApplyDirectDamage
)
{
	if (OutHitResult)
	{
		*OutHitResult = FHitResult();
	}
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter) return FVector::ZeroVector;

	AController* OwningController = OwnerCharacter->GetController();
	UWorld* World = GetWorld();
	if (!OwningController || !World) return OwnerCharacter->GetActorLocation();

	FVector ViewLocation;
	FRotator ViewRotation;
	OwningController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector Start = ViewLocation;
	const FVector End = Start + ViewRotation.Vector() * ShotData.Range;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponHitscan), false, OwnerCharacter);
	Params.AddIgnoredActor(OwnerCharacter);

	const float AimAssistRadius = FMath::Max(0.f, ShotData.AimAssistRadius);
	const bool bHit = AimAssistRadius > 0.f
		? World->SweepSingleByChannel(
			Hit,
			Start,
			End,
			FQuat::Identity,
			ECC_Visibility,
			FCollisionShape::MakeSphere(AimAssistRadius),
			Params
		)
		: World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	const FVector ShotTargetLocation = bHit ? Hit.ImpactPoint : End;
	if (!bHit) return ShotTargetLocation;
	if (OutHitResult)
	{
		*OutHitResult = Hit;
	}

	AActor* HitActor = Hit.GetActor();
	if (!bApplyDirectDamage || !HitActor || Cast<ADefenseCharacter>(HitActor)) return ShotTargetLocation;

	const float FinalDamage = FMath::Max(0.f, ShotData.Damage);
	UGameplayStatics::ApplyDamage(
		HitActor,
		FinalDamage,
		OwningController,
		OwnerCharacter,
		UDamageType::StaticClass()
	);

	return ShotTargetLocation;
}

FTransform UWeaponComponent::GetWeaponMuzzleTransform(FName SocketName) const
{
	if (EquippedWeaponActor)
	{
		if (UMeshComponent* ActiveMesh = EquippedWeaponActor->GetActiveWeaponMesh())
		{
			if (ActiveMesh->DoesSocketExist(SocketName))
			{
				return ActiveMesh->GetSocketTransform(SocketName, RTS_World);
			}
			return ActiveMesh->GetComponentTransform();
		}
		return EquippedWeaponActor->GetActorTransform();
	}

	if (const ADefenseCharacter* OwnerCharacter = GetOwnerCharacter())
	{
		return FTransform(OwnerCharacter->GetControlRotation(), OwnerCharacter->GetActorLocation());
	}

	return FTransform::Identity;
}

void UWeaponComponent::PlayChargeStageReachedVFX(const FWeaponChargeData& ChargeData)
{
	if (!ChargeData.StageReachedVFXActorClass || !EquippedWeaponActor || !GetWorld())
	{
		return;
	}

	UMeshComponent* ActiveMesh = EquippedWeaponActor->GetActiveWeaponMesh();
	if (!ActiveMesh)
	{
		return;
	}

	const FName SocketName = ChargeData.StageReachedVFXSocket;
	const bool bHasSocket = ActiveMesh->DoesSocketExist(SocketName);
	const FTransform SpawnTransform = bHasSocket
		? ActiveMesh->GetSocketTransform(SocketName, RTS_World)
		: ActiveMesh->GetComponentTransform();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.Instigator = GetOwnerCharacter();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* StageEffectActor = GetWorld()->SpawnActor<AActor>(
		ChargeData.StageReachedVFXActorClass,
		SpawnTransform.GetLocation(),
		SpawnTransform.Rotator(),
		SpawnParameters
	);
	if (!StageEffectActor)
	{
		return;
	}

	StageEffectActor->SetActorScale3D(FVector(
		FMath::Max(0.01f, ChargeData.StageReachedVFXScale * 0.35f)
	));
	StageEffectActor->AttachToComponent(
		ActiveMesh,
		FAttachmentTransformRules::KeepWorldTransform,
		bHasSocket ? SocketName : NAME_None
	);
	if (StageEffectActor->GetLifeSpan() <= 0.f)
	{
		StageEffectActor->SetLifeSpan(3.f);
	}
}

void UWeaponComponent::PlayChargedFireVFX(
	EWeaponChargeStage ChargeStage,
	const FVector& ShotTargetLocation
)
{
	const UWeaponData* WeaponData = GetCurWeaponData();
	if (!WeaponData)
	{
		return;
	}

	const FWeaponChargeStageData* StageData = GetChargeStageData(
		WeaponData->ChargedFire,
		ChargeStage
	);
	if (!StageData)
	{
		return;
	}

	if (ChargeStage == EWeaponChargeStage::Stage2)
	{
		SpawnStage2ProjectileVFX(ShotTargetLocation, *StageData);
	}
}

void UWeaponComponent::SpawnStage2ProjectileVFX(
	const FVector& ShotTargetLocation,
	const FWeaponChargeStageData& StageData
)
{
	UWorld* World = GetWorld();
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!World || !OwnerCharacter || !StageData.ProjectileVFXActorClass)
	{
		return;
	}

	const FTransform MuzzleTransform = GetWeaponMuzzleTransform();
	AWeaponProjectileVFXActor* Projectile = World->SpawnActorDeferred<AWeaponProjectileVFXActor>(
		AWeaponProjectileVFXActor::StaticClass(),
		MuzzleTransform,
		OwnerCharacter,
		OwnerCharacter,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);
	if (!Projectile)
	{
		return;
	}

	Projectile->Configure(
		ShotTargetLocation,
		StageData.ProjectileVFXActorClass,
		StageData.ImpactVFXActorClass,
		StageData.ProjectileVFXSpeed,
		StageData.ProjectileVFXScale
	);
	UGameplayStatics::FinishSpawningActor(Projectile, MuzzleTransform);
}

void UWeaponComponent::SpawnStage3StormTornado(AActor* HitEnemy)
{
	UWorld* World = GetWorld();
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!World || !OwnerCharacter || !OwnerCharacter->HasAuthority() || !IsValid(HitEnemy))
	{
		return;
	}

	FVector SpawnLocation = HitEnemy->GetActorLocation();
	if (const UCapsuleComponent* EnemyCapsule = HitEnemy->FindComponentByClass<UCapsuleComponent>())
	{
		// 바닥 충돌이 없는 맵에서도 적 캡슐의 바닥 높이에서 생성되도록 보정한다.
		SpawnLocation.Z -= EnemyCapsule->GetScaledCapsuleHalfHeight();
	}
	FHitResult GroundHit;
	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(Stage3StormGround), false, OwnerCharacter);
	GroundParams.AddIgnoredActor(OwnerCharacter);
	GroundParams.AddIgnoredActor(HitEnemy);
	FCollisionObjectQueryParams GroundObjectParams;
	GroundObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	const FVector GroundTraceStart = SpawnLocation + FVector(0.f, 0.f, 300.f);
	const FVector GroundTraceEnd = SpawnLocation - FVector(0.f, 0.f, 2500.f);
	if (World->LineTraceSingleByObjectType(
		GroundHit,
		GroundTraceStart,
		GroundTraceEnd,
		GroundObjectParams,
		GroundParams
	))
	{
		SpawnLocation = GroundHit.ImpactPoint + GroundHit.ImpactNormal * 4.f;
	}

	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
	AStormTornadoVFXActor* StormActor = World->SpawnActorDeferred<AStormTornadoVFXActor>(
		AStormTornadoVFXActor::StaticClass(),
		SpawnTransform,
		OwnerCharacter,
		OwnerCharacter,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);
	if (!StormActor)
	{
		return;
	}

	UGameplayStatics::FinishSpawningActor(StormActor, SpawnTransform);
}
