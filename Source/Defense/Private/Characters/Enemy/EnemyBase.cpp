// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Enemy/EnemyBase.h"

#include "StateTreeEvents.h"
#include "Characters/Enemy/EnemyAnim.h"
#include "Characters/Enemy/AI/EnemyController.h"
#include "Characters/Enemy/Data/EnemyData.h"
#include "Characters/Player/DefenseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameManager/DefenseGameMode.h"
#include "GameManager/DestinationActor.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UI/EnemyHPUI.h"

namespace
{
	const TCHAR* LexToString(const EEnemyMode Mode)
	{
		switch (Mode)
		{
		case EEnemyMode::Preview:
			return TEXT("Preview");
		case EEnemyMode::Combat:
			return TEXT("Combat");
		case EEnemyMode::ReturningToPool:
			return TEXT("ReturningToPool");
		case EEnemyMode::Inactive:
			return TEXT("Inactive");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* LexToString(const EEnemyState State)
	{
		switch (State)
		{
		case EEnemyState::Idle:
			return TEXT("Idle");
		case EEnemyState::Patrol:
			return TEXT("Patrol");
		case EEnemyState::Chase:
			return TEXT("Chase");
		case EEnemyState::Damage:
			return TEXT("Damage");
		case EEnemyState::Attack:
			return TEXT("Attack");
		case EEnemyState::Destroy:
			return TEXT("Destroy");
		case EEnemyState::Die:
			return TEXT("Die");
		default:
			return TEXT("Unknown");
		}
	}
}

// Sets default values
AEnemyBase::AEnemyBase()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	HpComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HpComp"));
	HpComp->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyEnemyData();
	HpComp->SetVisibility(false);
	EnemyController = Cast<AEnemyController>(GetController());
	AnimInst = Cast<UEnemyAnim>(GetMesh()->GetAnimInstance());
	EnemyMesh = GetMesh();
	CurHP = MaxHP;
	
	// 서버에서만 AIPerception이 동작하도록 / 클라이언트에서는 비활성화하고 HPUI 정의
	if (!HasAuthority())
	{
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
		HPUI = Cast<UEnemyHPUI>(HpComp->GetWidget());
	}
	else
	{
		GameMode = Cast<ADefenseGameMode>(GetWorld()->GetAuthGameMode());
		DestinationActor = Cast<ADestinationActor>(
	UGameplayStatics::GetActorOfClass(GetWorld(), ADestinationActor::StaticClass())
);
	}
	
}

void AEnemyBase::ApplyEnemyData()
{
	if (!EnemyData)
	{
		return;
	}
	EnemyType = EnemyData->EnemyType;
	MaxHP = EnemyData->MaxHP;
	KillCoinReward = EnemyData->KillCoinReward;
	PreviewMoveSpeed = EnemyData->PreviewMoveSpeed;
	CombatMoveSpeed = EnemyData->CombatMoveSpeed;
}

// Called every frame
void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 서버에서는 UI가 클라이언트 방향으로 회전하는 것 제외
	if (GetNetMode() == NM_DedicatedServer || !HpComp || !bHpUIVisible)
	{
		return;
	}

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	const FVector CamLoc = PlayerController->PlayerCameraManager->GetCameraLocation();
	FVector Dir = CamLoc - HpComp->GetComponentLocation();
	Dir.Z = 0;
	HpComp->SetWorldRotation(Dir.GetSafeNormal().ToOrientationRotator());
}

// Called to bind functionality to input
void AEnemyBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AEnemyBase::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEnemyBase, EnemyState);
	DOREPLIFETIME(AEnemyBase, EnemyMode);
	DOREPLIFETIME(AEnemyBase, CurHP);
	
	
}

void AEnemyBase::OnRep_UpdateMode()
{
	//UE_LOG(LogTemp, Warning, TEXT("Enemy OnRep_UpdateMode | Enemy=%s Mode=%s(%d) NetMode=%d HasAuthority=%d PreviewMats=%s/%s"),
		//*GetNameSafe(this),
		//LexToString(EnemyMode),
		//static_cast<int32>(EnemyMode),
		//static_cast<int32>(GetNetMode()),
		//HasAuthority() ? 1 : 0,
		//*GetNameSafe(PreviewMaterial0),
		//*GetNameSafe(PreviewMaterial1));

	// preview일 때 투명 머티리얼, Combat일 때는 원래 머티리얼로 변경
	if (EnemyMode == EEnemyMode::Preview)
	{
		SetPreview();
		
	}else if (EnemyMode == EEnemyMode::Combat)
	{
		SetCombat();
	}
	else if (EnemyMode == EEnemyMode::Inactive)
	{
		SetInactive();
	}
}

void AEnemyBase::SetPreview()
{
	if (!EnemyMesh)
	{
		EnemyMesh = GetMesh();
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = PreviewMoveSpeed;
	}
	
	//UE_LOG(LogTemp, Warning, TEXT("Enemy SetPreview | Enemy=%s Mode=%s(%d) NetMode=%d HasAuthority=%d Mesh=%s PreviewMats=%s/%s"),
		//*GetNameSafe(this),
		//LexToString(EnemyMode),
		//static_cast<int32>(EnemyMode),
		//static_cast<int32>(GetNetMode()),
		//HasAuthority() ? 1 : 0,
		//*GetNameSafe(EnemyMesh),
		//*GetNameSafe(PreviewMaterial0),
		//*GetNameSafe(PreviewMaterial1));

	// 그리기 처리
	SetActorHiddenInGame(false);

	if(!IsRunningDedicatedServer())
	{
		if (EnemyMesh)
		{
			if (PreviewMaterial)
			{
				EnemyMesh->SetMaterial(0, PreviewMaterial);
				EnemyMesh->SetMaterial(1, PreviewMaterial);
			}
		}
		// 틱 처리
		SetActorTickEnabled(true);
		
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
	}

	
	// 서버에서 처리	
	if (HasAuthority())
	{	if (!EnemyController)
		{
			EnemyController = Cast<AEnemyController>(GetController());
		}
		
		if (EnemyController && EnemyController->StateTreeAIComp)
		{
			EnemyController->StateTreeAIComp->StartLogic();
			EnemyController->StateTreeAIComp->SetComponentTickEnabled(true);
		}
		
		
	}
	
	// 충돌 처리
	SetActorEnableCollision(true);
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	}
	
	if (EnemyMesh)
	{
		EnemyMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	
	
}

void AEnemyBase::SetCombat()
{
	if (!EnemyMesh)
	{
		EnemyMesh = GetMesh();
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = CombatMoveSpeed;
	}

	//UE_LOG(LogTemp, Warning, TEXT("Enemy SetCombat | Enemy=%s Mode=%s(%d) NetMode=%d HasAuthority=%d CombatMats=%s/%s"),
		//*GetNameSafe(this),
		//LexToString(EnemyMode),
		//static_cast<int32>(EnemyMode),
		//static_cast<int32>(GetNetMode()),
		//HasAuthority() ? 1 : 0,
		//*GetNameSafe(CombatMaterial0),
		//*GetNameSafe(CombatMaterial1));
	
	if (!IsRunningDedicatedServer())
	{
		if (EnemyMesh)
		{
			if (CombatMaterial)
			{
				EnemyMesh->SetMaterial(0, CombatMaterial);
				EnemyMesh->SetMaterial(1, CombatMaterial);
			}
		}
		bHpUIVisible = false;
		HpComp->SetVisibility(false);
		SetActorTickEnabled(true); // tick에는 ui 회전만 있어서 데디서버가 아닌 곳에서 켜지게 함.
	}
	
	if (HasAuthority())
	{
		if (EnemyController && EnemyController->StateTreeAIComp)
		{
			EnemyController->StateTreeAIComp->StartLogic();
			EnemyController->StateTreeAIComp->SetComponentTickEnabled(true);
		}
	}
	
	SetActorEnableCollision(true);
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		//CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	
}

void AEnemyBase::SetInactive()
{
	//UE_LOG(LogTemp, Warning, TEXT("Enemy SetInactive | Enemy=%s Mode=%s(%d) NetMode=%d HasAuthority=%d"),
		//*GetNameSafe(this),
		//LexToString(EnemyMode),
		//static_cast<int32>(EnemyMode),
		//static_cast<int32>(GetNetMode()),
		//HasAuthority() ? 1 : 0);

	if (HasAuthority())
	{
		if (!EnemyController)
		{
			EnemyController = Cast<AEnemyController>(GetController());
		}
		
		if (EnemyController && EnemyController->StateTreeAIComp)
		{
			EnemyController->StateTreeAIComp->StopLogic(TEXT("EnemyMode : Inactive"));
		}
	}
	else
	{
		
	}
	
	// 충돌 처리
	SetActorEnableCollision(false);
	
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	}
	
	// 그리기 처리
	SetActorHiddenInGame(true);
	
	if (!IsRunningDedicatedServer())
	{ // 틱 처리
		SetActorTickEnabled(false);
		if (HpComp)
		{
			bHpUIVisible = false;
			HpComp->SetVisibility(false);
		}
	}
}

// Gameplay Tag 이벤트 보내기
void AEnemyBase::SendStateTreeEvent(FName EventTagName) const
{
	if (!Controller)
	{
		return;
	}

	UStateTreeAIComponent* StateTreeAI =
		Controller->FindComponentByClass<UStateTreeAIComponent>();

	if (!StateTreeAI)
	{
		return;
	}

	FStateTreeEvent Event;
	Event.Tag = FGameplayTag::RequestGameplayTag(EventTagName);

	StateTreeAI->SendStateTreeEvent(Event);	
}

void AEnemyBase::MulticastRPC_DamageMotion_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}
	AnimInst->PlayDamageMotion();
}

void AEnemyBase::MulticastRPC_DieMotion_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}
	HpComp->SetVisibility(false);
	bHpUIVisible = false;
	AnimInst->PlayDieMotion();
}

void AEnemyBase::MulticastRPC_StopAllMontages_Implementation()
{
	// 데디 서버에서는 리턴
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (AnimInst)
	{
		AnimInst->Montage_Stop(0.f);
	}
}

// 체력 UI 업데이트
void AEnemyBase::OnRep_UpdateUI()
{
	if (IsRunningDedicatedServer() || nullptr == HPUI)
	{
		return;
	}
	
	if (EnemyMode != EEnemyMode::Combat)
	{
		return;
	}
	
	if (CurHP >= MaxHP)
	{
		return;
	}
	
	if (!bHpUIVisible &&  EnemyState != EEnemyState::Die)
	{
		bHpUIVisible = true;
		HpComp->SetVisibility(true);
	}
	
	HPUI->UpdateHPBar(CurHP, MaxHP);
}

float AEnemyBase::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority())
	{
		return 0.f;
	}

	if (EnemyMode != EEnemyMode::Combat)
	{
		return 0.f;
	}

	if (EnemyState == EEnemyState::Die || EnemyState == EEnemyState::Destroy)
	{
		return 0.f;
	}
	
	const float ActualDamage = Super::TakeDamage(
		DamageAmount,
		DamageEvent,
		EventInstigator,
		DamageCauser
	);

	if (ActualDamage <= 0.0f)
	{
		return 0.0f;
	}

	CurHP -= ActualDamage;

	/*ADefenseCharacter* AttackingCharacter = Cast<ADefenseCharacter>(DamageCauser);
	if (!AttackingCharacter && EventInstigator)
	{
		AttackingCharacter = Cast<ADefenseCharacter>(EventInstigator->GetPawn());
	}

	if (AttackingCharacter)
	{
		Target = AttackingCharacter;
	}*/
	
	if (CurHP <= 0.0f)
	{
		GameMode->AwardEnemyKillCoin(this, DamageCauser, EventInstigator);
		
		// 재화 얻어지나 테스트--------------------
		/*if (APawn* CauserPawn = Cast<APawn>(DamageCauser))
		{
			AController* CauserController = CauserPawn->GetController();

			if (CauserController)
			{
				// 여기서 컨트롤러 사용
				auto* ps = CauserController->GetPlayerState<ADefensePlayerState>();
				UE_LOG(LogTemp, Error, TEXT("Coin : %d"), ps->GetCoin());
			}
		}*/
		//----------------------------------------------
		
		SendStateTreeEvent(FName("AI.Event.Die"));
		bHpUIVisible = false;
	}
	else
	{
		SendStateTreeEvent(FName("AI.Event.Damage"));
	}

	return ActualDamage;
}

