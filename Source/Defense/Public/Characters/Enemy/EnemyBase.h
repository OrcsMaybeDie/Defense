// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

UENUM(BlueprintType)
enum class EEnemyMode  : uint8
{
	Preview UMETA(DisplayName = "Preview"), // state tree 재시작, 플레이어 감지 X, 투명 머티리얼, 스폰지점에서 하나씩
	Combat UMETA(DisplayName = "Combat"),  // state tree O , 스폰지점에서 2~4씩 
	ReturningToPool UMETA(DisplayName = "Returning To Pool"), // 필요없으면 지우기 / 죽었을 경우, 죽는 애니메이션
	Inactive UMETA(DisplayName = "Inactive") // state tree 멈추기, actor hidden, noCollision
};

UENUM()
enum class EEnemyState  : uint8 // State tree의 상태
{
	Idle,
	Patrol,
	Chase,
	Damage,
	Attack,
	Die
};


UCLASS()
class DEFENSE_API AEnemyBase : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemyBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY()
	TObjectPtr<class ADefenseGameMode> GameMode;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Components")
	TObjectPtr<class UWidgetComponent> HpComp;

	// State Tree 상태
	UPROPERTY(Replicated)
	EEnemyState EnemyState;
	
	// 게임 진행 상태에 따른 상태
	UPROPERTY(ReplicatedUsing=OnRep_UpdateMode, EditAnywhere, BlueprintReadWrite)
	EEnemyMode EnemyMode = EEnemyMode::Inactive;

	// DestinationActor에 Overlap시 태어난 스포너에 있는 Active배열에서 제거하기 위함.
	UPROPERTY()
	TObjectPtr<class AEnemySpawner> OwningSpawner;
	
	UFUNCTION()
	void OnRep_UpdateMode();
	
	void SetPreview();
	void SetCombat();
	void SetInactive();
	
	UPROPERTY()
	TObjectPtr<class UMeshComponent> EnemyMesh;
	
	// Quinn 메시로 테스트 중이라 머티리얼 개수 동일하게 함. 추후 수정 예정
	UPROPERTY(editAnywhere, BlueprintReadWrite)
	TObjectPtr<class UMaterialInterface> PreviewMaterial;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UMaterialInterface> CombatMaterial;
	
	UPROPERTY()
	TObjectPtr<class UEnemyAnim> AnimInst;
	
	//---------------피격---------------------------------
	// Enemy HP
	UPROPERTY(ReplicatedUsing=OnRep_UpdateUI)
	float CurHP;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHP = 100.f;
	
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPC_DamageMotion();
	
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPC_DieMotion();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPC_StopAllMontages();
	
	// UI 업데이트
	UFUNCTION()
	void OnRep_UpdateUI();
	
	UPROPERTY()
	TObjectPtr<class UEnemyHPUI> HPUI;
	
	// 처음엔 HPBar가 안 보이고 맞으면 보이게 함
	bool bHpUIVisible = false;
	
	// 플레이어가 한 공격 받기
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 KillCoinReward = 100;
	
	//-----------AI Perception-------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UAIPerceptionComponent> AIComp;
	
	UPROPERTY()
	TObjectPtr<class AEnemyController> EnemyController;
	
	// 시야로 적 감지 (일정 거리 이내)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UAISenseConfig_Sight> SightConfig;
	
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, struct FAIStimulus Stimulus);
	
	void SendStateTreeEvent(FName EventTagName) const;
	
	//-------------타겟 공격----------------------
	// 문을 만든다면 문을 인식해서 부수게 하기 위해 일단 Actor로 지정
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> Target;
	
	// 애니메이션 재생
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPC_AttackMotion();
	
	// 타격 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyVar")
	float AttackDist = 100.f;
	
	// 타겟에 가할 데미지
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category = "MyVar")
	float DamageNum = 10.f;
	
	// Notify_hit에서 실행. 서버에서만 실행. Enemy가 서버에서 스폰되기때문에 RPC지정X
	UFUNCTION(BlueprintCallable)
	void AttackTarget();
	
	//------------------------------------------
	

};
