// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Enemy/EnemyAttackBoss.h"

#include "Characters/Enemy/AI/EnemyController.h"
#include "Components/StateTreeAIComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AIPerceptionComponent.h"

void AEnemyAttackBoss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnemyAttackBoss, bCinematicHold);
}

void AEnemyAttackBoss::SetCombat()
{
	Super::SetCombat();

	if (!bCinematicHold)
	{
		return;
	}

	ApplyCinematicHoldVisualState();
	StopBossLogicForCinematic();
}

void AEnemyAttackBoss::SetInactive()
{
	if (HasAuthority())
	{
		bCinematicHold = false;
	}

	Super::SetInactive();
}

void AEnemyAttackBoss::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (bCinematicHold)
	{
		return;
	}

	Super::OnTargetPerceptionUpdated(Actor, Stimulus);
}

void AEnemyAttackBoss::AttackTarget()
{
	if (bCinematicHold)
	{
		return;
	}

	Super::AttackTarget();
}

bool AEnemyAttackBoss::CanAttack() const
{
	return !bCinematicHold && Super::CanAttack();
}

float AEnemyAttackBoss::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser
)
{
	if (bCinematicHold)
	{
		return 0.0f;
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

bool AEnemyAttackBoss::BeginCinematicHold()
{
	if (!HasAuthority()
		|| bCinematicHold
		|| EnemyMode == EEnemyMode::Inactive
		|| EnemyMode == EEnemyMode::ReturningToPool)
	{
		return false;
	}

	bCinematicHold = true;
	bLockedTarget = false;
	SetTarget(nullptr);

	ApplyCinematicHoldVisualState();
	StopBossLogicForCinematic();
	MulticastRPC_StopAllMontages();
	ForceNetUpdate();

	return true;
}

bool AEnemyAttackBoss::EndCinematicHold(const FTransform& ResumeTransform)
{
	if (!HasAuthority() || !bCinematicHold)
	{
		return false;
	}

	if (AEnemyController* BossController = Cast<AEnemyController>(GetController()))
	{
		BossController->StopMovement();
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}

	FTransform BossResumeTransform = ResumeTransform;
	BossResumeTransform.SetScale3D(GetActorScale3D());
	SetActorTransform(BossResumeTransform, false, nullptr, ETeleportType::TeleportPhysics);
	EnemyState = EEnemyState::Idle;
	bLockedTarget = false;
	SetTarget(nullptr);
	bCinematicHold = false;

	SetActorHiddenInGame(false);
	if (EnemyMode != EEnemyMode::Combat)
	{
		SetEnemyMode(EEnemyMode::Combat);
	}
	else
	{
		SetCombat();
	}

	const bool bLogicRestarted = RestartBossLogicAfterCinematic();
	ForceNetUpdate();
	return bLogicRestarted;
}

void AEnemyAttackBoss::OnRep_CinematicHold()
{
	if (bCinematicHold)
	{
		ApplyCinematicHoldVisualState();
		return;
	}

	if (EnemyMode == EEnemyMode::Combat)
	{
		SetActorHiddenInGame(false);
		SetCombat();
	}
}

void AEnemyAttackBoss::ApplyCinematicHoldVisualState()
{
	if (!bCinematicHold)
	{
		return;
	}

	// Keep the real gameplay boss network-relevant while the cinematic actor is playing.
	// Only its AI and movement are held by StopBossLogicForCinematic().
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

void AEnemyAttackBoss::StopBossLogicForCinematic()
{
	if (!HasAuthority())
	{
		return;
	}

	AEnemyController* BossController = Cast<AEnemyController>(GetController());
	if (BossController)
	{
		BossController->StopMovement();

		if (BossController->StateTreeAIComp)
		{
			BossController->StateTreeAIComp->StopLogic(TEXT("Boss cinematic hold"));
			BossController->StateTreeAIComp->SetComponentTickEnabled(false);
		}
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}

	if (AIComp)
	{
		AIComp->ForgetAll();
		AIComp->Deactivate();
		AIComp->SetComponentTickEnabled(false);
	}
}

bool AEnemyAttackBoss::RestartBossLogicAfterCinematic()
{
	if (!HasAuthority())
	{
		return false;
	}

	AEnemyController* BossController = Cast<AEnemyController>(GetController());
	if (!BossController)
	{
		SpawnDefaultController();
		BossController = Cast<AEnemyController>(GetController());
	}

	if (!BossController || BossController->GetPawn() != this || !BossController->StateTreeAIComp)
	{
		return false;
	}

	if (AIComp)
	{
		AIComp->Activate(true);
		AIComp->SetComponentTickEnabled(true);
	}

	BossController->StateTreeAIComp->SetComponentTickEnabled(true);
	BossController->StateTreeAIComp->RestartLogic();
	return BossController->StateTreeAIComp->IsRunning();
}
