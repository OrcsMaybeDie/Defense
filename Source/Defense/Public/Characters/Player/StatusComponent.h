#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StatusComponent.generated.h"


UENUM(BlueprintType)
enum class EPlayerLifeState : uint8
{
	Alive,
	Dead
};

// 이벤트 타입 생성
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStatChanged, float, CurValue, float, MaxValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLifeStateChanged, EPlayerLifeState, NewLifeState);
	
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEFENSE_API UStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatusComponent();

	UFUNCTION(BlueprintPure, Category="State")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category="State")
	EPlayerLifeState GetLifeState() const;

	UPROPERTY(ReplicatedUsing=OnRep_Health)
	float Health;
	float MaxHealth = 100.f; // 고정
	
	void Heal(float Amount);
	void Revive();

	UPROPERTY(ReplicatedUsing=OnRep_Mana)
	float Mana;
	float MaxMana = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attribute")
	float ManaRegenPerSec = 5.f;
	
	bool CanSpendMana(float Amount) const;
	bool TrySpendMana(float Amount);
	void RestoreMana(float Amount);

	float ApplyDamage(float Amount, AActor* DamageCauser);

	// 이벤트
	UPROPERTY(BlueprintAssignable, Category="Stat")
	FOnStatChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="Stat")
	FOnStatChanged OnManaChanged;

	UPROPERTY(BlueprintAssignable, Category="State")
	FOnLifeStateChanged OnLifeStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(ReplicatedUsing=OnRep_LifeState)
	EPlayerLifeState LifeState = EPlayerLifeState::Alive;

	UFUNCTION()
	void OnRep_LifeState();

	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_Mana();

	void SetLifeState(EPlayerLifeState NewLifeState);

	// Health 대입 & 이벤트 발생 처리 (ApplyDamage, Heal, Revive)
	void SetHealth(float NewHP);
};
