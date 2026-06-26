#include "Characters/Player/DefensePlayerState.h"

#include "Net/UnrealNetwork.h"

// Server
void ADefensePlayerState::SetReady(bool bReady)
{
	// 웨이브 시작 후 모두 ready 해제 | 퇴장/리셋 처리 | UI 버튼에서 처리 등 확인
	if (bIsReady == bReady) { return; }
	
	bIsReady = bReady;
	
	// 서버에서는 OnRep이 자동 호출되지 않음. 메모.
}

void ADefensePlayerState::OnRep_IsReady()
{
	// UI
}

void ADefensePlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ADefensePlayerState, bIsReady);
}




