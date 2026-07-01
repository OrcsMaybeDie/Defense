#include "Characters/Player/StatusComponent.h"

#include "Net/UnrealNetwork.h"


UStatusComponent::UStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	
	Health = MaxHealth;
	Mana = MaxMana;
}


void UStatusComponent::BeginPlay()
{
	Super::BeginPlay();
}


void UStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// 마나 회복 계산은 서버만
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	
	RestoreMana(ManaRegenPerSec * DeltaTime);
}


void UStatusComponent::OnRep_Health()
{
	// 클라이언트가 서버에서 복제된 새 Health 값을 받는 타이밍
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UStatusComponent::Heal(float Amount)
{
}


void UStatusComponent::OnRep_Mana()
{
	OnManaChanged.Broadcast(Mana, MaxMana);
}

bool UStatusComponent::CanSpendMana(float Amount) const
{
	return Amount <= 0.f || Mana >= Amount;
}

bool UStatusComponent::TrySpendMana(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	if (Amount <= 0.f) return true;
	if (!CanSpendMana(Amount)) return false;

	const float OldMana = Mana;
	Mana = FMath::Clamp(Mana - Amount, 0.f, MaxMana);

	if (!FMath::IsNearlyEqual(OldMana, Mana))
	{
		// Server-side broadcast keeps listen-server tests and any server-side listeners on the same change path as clients.
		OnManaChanged.Broadcast(Mana, MaxMana);
	}

	return true;
}

void UStatusComponent::RestoreMana(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (Amount <= 0.0f) return;
	
	const float OldMana = Mana;
	Mana = FMath::Clamp(Mana + Amount, 0.f, MaxMana);
	
	if (!FMath::IsNearlyEqual(OldMana, Mana))
	{
		// Server-side broadcast keeps listen-server tests and any server-side listeners on the same change path as clients.
		OnManaChanged.Broadcast(Mana, MaxMana);
	}
}


float UStatusComponent::ApplyDamage(float Amount, AActor* DamageCauser)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return 0.f;
	if (Amount <= 0.f) return 0.f;

	const float OldHealth = Health;
	Health = FMath::Clamp(Health - Amount, 0.f, MaxHealth);

	const float ActualDamage = OldHealth - Health;

	if (!FMath::IsNearlyZero(ActualDamage))
	{
		// Server-side broadcast keeps listen-server tests and any server-side listeners on the same change path as clients.
		OnHealthChanged.Broadcast(Health, MaxHealth);
	}

	return ActualDamage;
}

void UStatusComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UStatusComponent, Health);
	DOREPLIFETIME(UStatusComponent, Mana);
}

