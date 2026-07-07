#include "Characters/Player/DefensePlayerState.h"

#include "Net/UnrealNetwork.h"


void ADefensePlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ADefensePlayerState, bIsReady);
	DOREPLIFETIME(ADefensePlayerState, Coin);
}


/** Ready */
void ADefensePlayerState::SetReady(bool bReady)
{
	// 웨이브 시작 후 모두 ready 해제 | 퇴장/리셋 처리 | UI 버튼에서 처리 등 확인
	if (bIsReady == bReady) { return; }
	
	bIsReady = bReady;
}

void ADefensePlayerState::OnRep_IsReady()
{
	// UI
}


/** 재화 */
void ADefensePlayerState::RefundCoin(int32 Amount)
{
	AddCoin(Amount);
}

void ADefensePlayerState::AddCoin(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0) return;
	SetCoin(Coin + Amount);
}

bool ADefensePlayerState::TrySpendCoin(int32 Amount)
{
	if (!HasAuthority()) return false;
	if (!CanSpendCoin(Amount)) return false;
	
	SetCoin(Coin - Amount);
	return true;
}

bool ADefensePlayerState::CanSpendCoin(int32 Amount) const
{
	return Amount >= 0 && Coin >= Amount;
}

void ADefensePlayerState::SetCoin(int32 NewCoin)
{
	if (!HasAuthority()) return;
	
	const int32 ClampCoin = FMath::Max(0, NewCoin);
	if (Coin == ClampCoin) return;
	
	Coin = ClampCoin;

	// 서버에서는 RepNotify 자동 호출 X
	// 데디서버 UI만 생각하면 없어도 됨.
	// 리슨서버/서버측 로직 이벤트까지 고려하면 호출할 수 있음.
	// OnRep_Coin();
}

void ADefensePlayerState::OnRep_Coin()
{
	OnCoinChanged.Broadcast(Coin);
}
