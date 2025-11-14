// Fill out your copyright notice in the Description page of Project Settings.


// NSGameState.cpp
#include "NSGameState.h"
#include "Net/UnrealNetwork.h"

void ANSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANSGameState, Phase);
	DOREPLIFETIME(ANSGameState, TimeLeftSec);
	DOREPLIFETIME(ANSGameState, ReadyCount);
	DOREPLIFETIME(ANSGameState, TotalPlayers);
	DOREPLIFETIME(ANSGameState, bReadyLocked);
	DOREPLIFETIME(ANSGameState, StartCountdownSec);
	DOREPLIFETIME(ANSGameState, SpawnStage);
	DOREPLIFETIME(ANSGameState, Day);
	DOREPLIFETIME(ANSGameState, Reputation);
	DOREPLIFETIME(ANSGameState, DayScore);
	DOREPLIFETIME(ANSGameState, MemoryShard);
}

void ANSGameState::OnRep_Phase() {}
void ANSGameState::OnRep_TimeLeft() {}
void ANSGameState::OnRep_ReadyCount() {}
void ANSGameState::OnRep_TotalPlayers() {}

void ANSGameState::OnRep_ReadyLock()
{
	// 클라에서 준비 잠금 반영(패널 숨김/버튼 비활성 등은 위젯에서 이 값 읽어 처리)
	UE_LOG(LogTemp, Verbose, TEXT("[GS] ReadyLocked = %s"), bReadyLocked ? TEXT("true") : TEXT("false"));
}

void ANSGameState::OnRep_StartCountdown()
{
	// 클라에서 X초 뒤 시작 텍스트 갱신
	UE_LOG(LogTemp, Verbose, TEXT("[GS] StartCountdown = %d"), StartCountdownSec);
}

void ANSGameState::OnRep_SpawnStage() { /* HUD 갱신 */ }


void ANSGameState::AddMemoryShard(int32 Delta)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 Old = MemoryShard;
	MemoryShard = FMath::Max(0, Old + Delta);

	if (MemoryShard != Old)
	{
		OnRep_MemoryShard();
	}
}

void ANSGameState::OnRep_MemoryShard()
{
	OnMemoryShardChanged.Broadcast(MemoryShard);
}


