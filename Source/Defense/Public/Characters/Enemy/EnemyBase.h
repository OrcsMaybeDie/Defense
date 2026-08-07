// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

enum class EEnemyType : uint8;

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
	Waiting,
	Chase,
	Damage,
	Attack,
	Destroy,
	Stone,
	StoneEnd,
	StoneDie,
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Data")
	TObjectPtr<class UEnemyData> EnemyData;

	// State Tree 상태
	UPROPERTY(Replicated,VisibleAnywhere,BlueprintReadOnly)
	EEnemyState EnemyState;
	
	// 공격타입에 따라 공격상태일 때 다른 task 수행
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly)
	EEnemyType EnemyType;
	
	// 게임 진행 상태에 따른 상태
	UPROPERTY(ReplicatedUsing=OnRep_UpdateMode)
	EEnemyMode EnemyMode = EEnemyMode::Inactive;

	// DestinationActor에 Overlap시 태어난 스포너에 있는 Active배열에서 제거하기 위함.
	UPROPERTY()
	TObjectPtr<class AEnemySpawner> OwningSpawner;
	
	UFUNCTION()
	void OnRep_UpdateMode();

	void SetEnemyMode(EEnemyMode NewMode);
	
	virtual void SetPreview();
	virtual void SetCombat();
	virtual void SetInactive();
	virtual void OnEnteredPatrol();
	
	UPROPERTY()
	TObjectPtr<class UMeshComponent> EnemyMesh;
	
	UPROPERTY(editAnywhere, BlueprintReadWrite)
	TObjectPtr<class UMaterialInterface> PreviewMaterial;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UMaterialInterface> CombatMaterial;
	
	UPROPERTY()
	TObjectPtr<class UEnemyAnim> AnimInst;
	
	// 서버(state tree)에서만 씀.
	UPROPERTY()
	TObjectPtr<class ADestinationActor> DestinationActor;
	
	//---------------피격---------------------------------
	// Enemy HP
	UPROPERTY(ReplicatedUsing=OnRep_UpdateUI)
	float CurHP;
	
	UPROPERTY()
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
	
	// Data Asset에서 가져옴.
	int32 KillCoinReward = 100;
	float PreviewMoveSpeed = 200.f;
	float CombatMoveSpeed = 600.f;

	// StateTree 조건과 실제 공격 판정에서 사용할 공격 거리
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Attack")
	float AttackDist = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Attack")
	float BarricadeAttackDist = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Attack")
	float CurrentAttackDist = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Attack")
	float CurrentTargetDistance = MAX_flt;
	
	UPROPERTY()
	TObjectPtr<class AEnemyController> EnemyController;
	
	void SendStateTreeEvent(FName EventTagName) const;

	// 문을 만든다면 문을 인식해서 부수게 하기 위해 일단 Actor로 지정
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Target")
	TObjectPtr<AActor> Target;

	void SetTarget(AActor* NewTarget);

	FORCEINLINE class ADestinationActor* GetDestinationActor() const { return DestinationActor; }
	
	virtual void ApplyEnemyData();
	
	//--------------석화------------------
	

};
