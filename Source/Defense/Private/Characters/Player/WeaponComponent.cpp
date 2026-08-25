#include "Characters/Player/WeaponComponent.h"

#include "Animation/AnimInstance.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Defense.h"
#include "Equipment/DefenseWeaponActor.h"
#include "Equipment/LoadoutComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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
		UE_LOG(LogDefense, Warning, TEXT("Charge start rejected: missing owner, weapon data, or world."));
		return;
	}
	if (!OwnerCharacter->IsLocallyControlled() && !OwnerCharacter->HasAuthority())
	{
		UE_LOG(LogDefense, Warning, TEXT("Charge start rejected: character is neither locally controlled nor authoritative. Character=%s"),
			*GetNameSafe(OwnerCharacter));
		return;
	}
	if (bChargeInputHeld || bServerChargeActive)
	{
		UE_LOG(LogDefense, Warning, TEXT("Charge start rejected: another charge is already active. Character=%s"),
			*GetNameSafe(OwnerCharacter));
		return;
	}
	if (!WeaponData->ChargedFire.bEnabled)
	{
		UE_LOG(LogDefense, Warning, TEXT("Charge start rejected: Charged Fire is disabled. Weapon=%s"),
			*GetNameSafe(WeaponData));
		return;
	}
	if (!CanLocallyBeginCharge(WeaponData->ChargedFire))
	{
		const UStatusComponent* StatusComp = OwnerCharacter->FindComponentByClass<UStatusComponent>();
		UE_LOG(LogDefense, Warning, TEXT("Charge start rejected: invalid state or insufficient Stage1 mana. Character=%s Cost=%.1f Mana=%.1f"),
			*GetNameSafe(OwnerCharacter),
			WeaponData->ChargedFire.Stage1.ManaCost,
			StatusComp ? StatusComp->Mana : -1.f);
		return;
	}

	if (const float* NextReadyTime = NextChargedFireLocalTimes.Find(WeaponData))
	{
		if (World->GetTimeSeconds() < *NextReadyTime)
		{
			UE_LOG(LogDefense, Warning, TEXT("Charge start rejected: charged fire cooldown has %.2f seconds remaining. Weapon=%s"),
				*NextReadyTime - World->GetTimeSeconds(),
				*GetNameSafe(WeaponData));
			return;
		}
	}

	if (OwnerCharacter->IsLocallyControlled())
	{
		bChargeInputHeld = true;
		LocalChargeStartTime = World->GetTimeSeconds();
		LocalChargeWeaponData = WeaponData;
		SetComponentTickEnabled(true);
		BroadcastChargePreview();
		PlayWeaponActionCosmetics(
			EWeaponActionType::ChargedFire,
			EWeaponActionPhase::Started,
			0.f
		);
	}

	ServerRPC_BeginCharge();
}

void UWeaponComponent::ReleaseCharge()
{
	UWorld* World = GetWorld();
	UWeaponData* WeaponData = LocalChargeWeaponData.Get();
	if (!bChargeInputHeld || !World || !WeaponData)
	{
		UE_LOG(LogDefense, Warning, TEXT("Charge release ignored: no local charge is active."));
		return;
	}

	const float HeldTime = FMath::Max(0.f, World->GetTimeSeconds() - LocalChargeStartTime);
	const FWeaponChargeData ChargeData = WeaponData->ChargedFire;
	const UStatusComponent* StatusComp = GetOwnerCharacter()
		? GetOwnerCharacter()->FindComponentByClass<UStatusComponent>()
		: nullptr;
	const float ChargeRatio = ClampChargeRatioToAvailableMana(
		ChargeData,
		CalculateChargeRatio(ChargeData, HeldTime),
		StatusComp ? StatusComp->Mana : 0.f
	);
	const EWeaponChargeStage ChargeStage = CalculateChargeStage(ChargeData, ChargeRatio);
	const FWeaponChargeStageData* StageData = GetChargeStageData(ChargeData, ChargeStage);
	ResetLocalChargeState();

	if (!ChargeData.bEnabled || !StageData || WeaponData != GetCurWeaponData())
	{
		if (!StageData)
		{
			UE_LOG(LogDefense, Warning, TEXT("Charge release cancelled before Stage1. Held=%.2f Max=%.2f Ratio=%.2f"),
				HeldTime,
				ChargeData.MaxChargeTime,
				ChargeRatio);
		}
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
		UE_LOG(LogDefense, Verbose, TEXT("Fire rejected while charge is active. Character=%s"), *GetNameSafe(OwnerActor));
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
		UE_LOG(LogDefense, Warning, TEXT("Server charge start rejected. Character=%s Active=%d Alive=%d Enabled=%d Stage1=%d Cooldown=%d Cost=%.1f Mana=%.1f"),
			*GetNameSafe(OwnerActor),
			bServerChargeActive ? 1 : 0,
			StatusComp->IsAlive() ? 1 : 0,
			ChargeData.bEnabled ? 1 : 0,
			Stage1Data ? 1 : 0,
			bOnCooldown ? 1 : 0,
			Stage1Data ? Stage1Data->ManaCost : -1.f,
			StatusComp->Mana);
		ClientRPC_RejectCharge();
		return;
	}

	bServerChargeActive = true;
	ServerChargeStartTime = CurrentTime;
	ServerChargeWeaponData = WeaponData;

	MulticastRPC_PlayWeaponAction(
		EWeaponActionType::ChargedFire,
		EWeaponActionPhase::Started,
		0.f
	);
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
	const float ChargeRatio = WeaponData
		? ClampChargeRatioToAvailableMana(
			WeaponData->ChargedFire,
			CalculateChargeRatio(WeaponData->ChargedFire, HeldTime),
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
		|| !StageData)
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
			UE_LOG(LogDefense, Verbose, TEXT("Weapon fire rejected by cooldown. Character=%s Action=%d Remaining=%.2f"),
				*GetNameSafe(OwnerActor),
				static_cast<int32>(ActionType),
				*NextReadyTime - CurrentTime);
			return false;
		}
	}

	if (ActionType == EWeaponActionType::ChargedFire && !StatusComp->TrySpendMana(ManaCost))
	{
		UE_LOG(LogDefense, Verbose, TEXT("Weapon fire rejected by mana. Character=%s Cost=%.1f Mana=%.1f"),
			*GetNameSafe(OwnerActor),
			ManaCost,
			StatusComp->Mana);
		return false;
	}

	if (ActionType == EWeaponActionType::ChargedFire)
	{
		const EWeaponChargeStage ChargeStage = CalculateChargeStage(
			WeaponData->ChargedFire,
			ChargeRatio
		);
		if (const FWeaponChargeStageData* StageData = GetChargeStageData(
			WeaponData->ChargedFire,
			ChargeStage
		))
		{
			ApplyWeaponMovementLock(StageData->MovementLockDuration);
		}
	}

	CooldownMap.Add(WeaponData, CurrentTime + FMath::Max(0.f, ShotData.Cooldown));

	MulticastRPC_PlayWeaponAction(ActionType, EWeaponActionPhase::Executed, ChargeRatio);
	PerformHitscan(ShotData);
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

void UWeaponComponent::PerformHitscan(const FWeaponShotData& ShotData)
{
	ADefenseCharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter) return;

	AController* OwningController = OwnerCharacter->GetController();
	UWorld* World = GetWorld();
	if (!OwningController || !World) return;

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

	if (!bHit) return;

	AActor* HitActor = Hit.GetActor();
	if (!HitActor || Cast<ADefenseCharacter>(HitActor)) return;

	const float FinalDamage = FMath::Max(0.f, ShotData.Damage);
	UGameplayStatics::ApplyDamage(
		HitActor,
		FinalDamage,
		OwningController,
		OwnerCharacter,
		UDamageType::StaticClass()
	);
}
