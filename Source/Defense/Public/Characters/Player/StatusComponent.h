// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StatusComponent.generated.h"

	
// 이벤트 타입 생성
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStatChanged, float, CurValue, float, MaxValue);
	
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEFENSE_API UStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UStatusComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UPROPERTY(ReplicatedUsing=OnRep_Health)
	float Health;
	float MaxHealth = 100.f; // 고정
	UFUNCTION()
	void OnRep_Health();
	
	void Heal(float Amount);
		
	UPROPERTY(ReplicatedUsing=OnRep_Mana)
	float Mana;
	float MaxMana = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attribute")
	float ManaRegenPerSec = 10.f;
	UFUNCTION()
	void OnRep_Mana();
	
	bool CanSpendMana(float Amount) const;
	bool TrySpendMana(float Amount);
	void RestoreMana(float Amount);

	// 이벤트
	UPROPERTY(BlueprintAssignable, Category="Stat")
	FOnStatChanged OnHealthChanged;
	
	UPROPERTY(BlueprintAssignable, Category="Stat")
	FOnStatChanged OnManaChanged;
	
	// 데미지 테스트
	float ApplyDamage(float Amount, AActor* DamageCauser);
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};
