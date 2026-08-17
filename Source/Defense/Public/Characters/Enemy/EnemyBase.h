// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
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

UENUM(BlueprintType)
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

enum class EEnemyPendingDeathType : uint8
{
	None,
	Normal,
	Stone
};

struct FEnemyActionProgressData
{
	bool bIsValid = false;
	EEnemyState ActionState = EEnemyState::Idle;
	float TotalDuration = 0.f;
	float ElapsedTime = 0.f;
	float RemainingTime = 0.f;
	float TriggerTime = 0.f;
	bool bActionTriggered = false;

	void Reset()
	{
		bIsValid = false;
		ActionState = EEnemyState::Idle;
		TotalDuration = 0.f;
		ElapsedTime = 0.f;
		RemainingTime = 0.f;
		TriggerTime = 0.f;
		bActionTriggered = false;
	}
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<class USceneComponent> RewardPopupAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<class UWidgetComponent> RewardComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Data")
	TObjectPtr<class UEnemyData> EnemyData;

	// State Tree 상태
	UPROPERTY(Replicated,VisibleAnywhere,BlueprintReadOnly)
	EEnemyState EnemyState;

	FEnemyActionProgressData TrackedActionData;
	FEnemyActionProgressData SuspendedActionData;

	void BeginTrackedAction(EEnemyState ActionState, float TotalDuration, float TriggerTime = 0.f);
	void UpdateTrackedAction(float ElapsedTime, bool bActionTriggered);
	void CompleteTrackedAction();
	void ClearSuspendedAction();
	bool TryEnterStone();
	void BeginStoneGameplay();
	void EndStoneGameplay();
	void ResetStoneStateForPool();
	bool TryMarkDeathTaskStarted(EEnemyPendingDeathType DeathType);
	
	// 공격타입에 따라 공격상태일 때 다른 task 수행
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly)
	EEnemyType EnemyType;
	
	// 게임 진행 상태에 따른 상태
	UPROPERTY(ReplicatedUsing=OnRep_UpdateMode)
	EEnemyMode EnemyMode = EEnemyMode::Inactive;

	// 목적지에 도달했을 때 태어난 스포너의 Active 배열에서 제거하기 위함.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Stone")
	TObjectPtr<class UMaterialInterface> StoneMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Stone|Visual")
	bool bApplyDamageOverlayWhileStone = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Stone|Visual",
		meta=(EditCondition="bApplyDamageOverlayWhileStone"))
	bool bShowDamageOutlineWhileStone = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Stone|Fracture")
	TSubclassOf<class AStoneFractureActor> StoneFractureActorClass;
	
	UPROPERTY()
	TObjectPtr<class UEnemyAnim> AnimInst;
	
	// 서버 StateTree가 최종 이동 대상으로 사용한다.
	UPROPERTY()
	TObjectPtr<class APortal> PortalActor;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Portal")
	bool bEnteringPortal = false;

	bool TryBeginPortalEntry(
		class APortal* Portal,
		const FVector& ExitLocation,
		const FVector& PortalPosition,
		const FVector& PortalForward,
		const FVector& PortalRight,
		const FVector& PortalUp,
		float PortalHalfWidth,
		float PortalHalfHeight
	);

	bool FinishPortalEntry(const class APortal* Portal);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_BeginPortalClip(
		FVector PortalPosition,
		FVector PortalForward,
		FVector PortalRight,
		FVector PortalUp,
		float PortalHalfWidth,
		float PortalHalfHeight
	);
	
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

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_EnterStoneVisual();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_ExitStoneVisual(bool bResumeMontage, bool bWaitForMovement);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_StoneDieVisual();

	// UI 업데이트
	UFUNCTION()
	void OnRep_UpdateUI();
	
	UPROPERTY()
	TObjectPtr<class UEnemyHPUI> HPUI;
	
	// 처음엔 HPBar가 안 보이고 맞으면 보이게 함
	bool bHpUIVisible = false;
	bool bDeathHandled = false;
	bool bDeathTaskStarted = false;
	EEnemyPendingDeathType PendingDeathType = EEnemyPendingDeathType::None;
	
	// 플레이어가 한 공격 받기
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// 화염 함정의 단일 진입점. 재호출 시 중첩하지 않고 수치와 지속시간을 갱신한다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Enemy|Burn")
	void ApplyBurnEffect(float Duration, float DamageInterval, float DamagePerTick, AActor* DamageCauser);

	UPROPERTY(ReplicatedUsing=OnRep_IsBurning, VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Burn")
	bool bIsBurning = false;

	// 일반 피격 Outline과 Burn을 함께 구현한 통합 Overlay Material을 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Damage|Visual")
	TObjectPtr<class UMaterialInterface> DamageOverlayMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Damage|Visual", meta=(ClampMin="0.01", Units="s"))
	float DamageOutlineDuration = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Damage|Visual|Electric", meta=(ClampMin="0.01", Units="s"))
	float ElectricHitDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Damage|Visual|Burn")
	FLinearColor BurnColor = FLinearColor(1.f, 0.02f, 0.01f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Damage|Visual|Burn", meta=(ClampMin="0.0"))
	float BurnPulseSpeed = 6.f;
	
	// Data Asset에서 가져옴.
	int32 KillCoinReward = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Reward Popup", meta=(ClampMin="0.01", Units="s"))
	float RewardPopupDuration = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Reward Popup", meta=(ClampMin="0.0", Units="cm"))
	float RewardPopupRiseHeight = 50.f;

	float PreviewMoveSpeed = 200.f;
	float CombatMoveSpeed = 600.f;

	// 지정된 적만 별도의 경로 탐색 규칙을 사용한다. 비어 있으면 AIController 기본 필터를 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UNavigationQueryFilter> NavigationFilterClass;

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
	void RetryPendingDeathTransition();

	// 문을 만든다면 문을 인식해서 부수게 하기 위해 일단 Actor로 지정
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Target")
	TObjectPtr<AActor> Target;

	void SetTarget(AActor* NewTarget);
	void ShowRewardPopup(int32 RewardAmount);

	FORCEINLINE class APortal* GetPortalActor() const { return PortalActor; }
	FORCEINLINE bool IsEnteringPortal() const { return bEnteringPortal; }
	
	virtual void ApplyEnemyData();
	void PrepareForRegularAnimation();
	
	//--------------석화------------------

private:
	UPROPERTY(Transient)
	TObjectPtr<class UAnimMontage> SuspendedMontage = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInterface>> MaterialsBeforeStone;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> PortalMaterialInstances;

	UPROPERTY(Transient)
	TObjectPtr<class APortal> EnteringPortal;

	float SuspendedMontagePosition = 0.f;
	float SuspendedMontagePlayRate = 1.f;
	float LocomotionResumeWaitTime = 0.f;
	TEnumAsByte<EMovementMode> MovementModeBeforeStone = MOVE_Walking;
	bool bStoneGameplayActive = false;
	bool bStoneVisualActive = false;
	bool bPendingLocomotionResume = false;
	bool bPortalCollisionSnapshotValid = false;
	ECollisionResponse CapsulePawnResponseBeforePortal = ECR_Ignore;
	ECollisionResponse CapsuleVisibilityResponseBeforePortal = ECR_Ignore;
	ECollisionResponse CapsuleBarricadeResponseBeforePortal = ECR_Ignore;
	ECollisionResponse CapsuleWorldDynamicResponseBeforePortal = ECR_Ignore;
	ECollisionEnabled::Type MeshCollisionEnabledBeforePortal = ECollisionEnabled::NoCollision;
	float LastDeathRetryTime = -BIG_NUMBER;
	static constexpr float DeathRetryInterval = 0.25f;

	void EnterStoneVisual();
	void ExitStoneVisual(bool bResumeMontage, bool bWaitForMovement);
	void RestoreMaterialsBeforeStone();
	void ResetStoneVisual();
	void ApplyPortalClipVisual(
		const FVector& PortalPosition,
		const FVector& PortalForward,
		const FVector& PortalRight,
		const FVector& PortalUp,
		float PortalHalfWidth,
		float PortalHalfHeight
	);
	void ResetPortalClipVisual();
	void ApplyPortalCollisionState();
	void RestorePortalCollisionState();
	void ResetPortalEntryState();
	void ApplyEnemyCollisionPolicy();
	void UpdateRewardPopup(float DeltaTime);
	void ResetRewardPopup();

	UFUNCTION()
	void OnRep_IsBurning();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPC_BurnReaction();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPC_ShowDamageOutline();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPC_ShowElectricHit();

	void ApplyBurnDamageTick();
	void EndBurnEffect();
	void InitializeDamageOverlay();
	void SetBurnVisualActive(bool bActive);
	void ShowElectricHit();
	void ClearElectricHit();
	void ShowDamageOutline();
	void ClearDamageOutline();
	void UpdateDamageOverlayForStoneState();
	void RestoreDamageOverlay();
	void ClearBurnTimers();

	FTimerHandle BurnDamageTimerHandle;
	FTimerHandle BurnEndTimerHandle;
	FTimerHandle DamageOutlineTimerHandle;
	FTimerHandle ElectricHitTimerHandle;
	float BurnDamagePerTick = 0.f;
	float BurnEndTime = 0.f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> BurnDamageCauser;

	UPROPERTY(Transient)
	TWeakObjectPtr<class AController> BurnEventInstigator;

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> DamageOverlayMID;

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInterface> OverlayMaterialBeforeDamage;

	bool bDamageOutlineActive = false;
	bool bRenderCustomDepthBeforeDamageOutline = false;

	UPROPERTY(Transient)
	TObjectPtr<class URewardUI> RewardUI;

	FVector RewardPopupInitialRelativeLocation = FVector::ZeroVector;
	float RewardPopupElapsedTime = 0.f;
	bool bRewardPopupPlaying = false;

};
