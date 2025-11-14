// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NSTypes.h"
#include "GameFramework/GameStateBase.h"
#include "NSGameState.generated.h"

UENUM(BlueprintType)
enum class EGamePhase : uint8 { Waiting, Starting, InProgress, Ending };

/**
 * 
 */
UCLASS()
class ANSGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly)
	EGamePhase Phase = EGamePhase::Waiting;

	// 남은 업무 시간(초) – 클라 HUD 표시용
	UPROPERTY(ReplicatedUsing = OnRep_TimeLeft, BlueprintReadOnly)
	int32 TimeLeftSec = 0;

	// 시작 의식(여신상) 준비 현황
	UPROPERTY(ReplicatedUsing = OnRep_ReadyCount, BlueprintReadOnly)
	int32 ReadyCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_TotalPlayers, BlueprintReadOnly)
	int32 TotalPlayers = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ReadyLock, BlueprintReadOnly)
	bool bReadyLocked = false;         // 5초 종료 후 true → 더 이상 토글 불가

	UPROPERTY(ReplicatedUsing = OnRep_StartCountdown, BlueprintReadOnly)
	int32 StartCountdownSec = 0;

	UPROPERTY(ReplicatedUsing = OnRep_SpawnStage, BlueprintReadOnly)
	ESpawnStage SpawnStage = ESpawnStage::Inactive;

	UPROPERTY(ReplicatedUsing = OnRep_Day, BlueprintReadOnly, Category = "Day")
	int32 Day = 1;                      // 1일차부터 시작

	UPROPERTY(ReplicatedUsing = OnRep_Reputation, BlueprintReadOnly, Category = "Day")
	int32 Reputation = 1;               // 역 평판 (0이면 게임오버)

	UPROPERTY(ReplicatedUsing = OnRep_DayScore, BlueprintReadOnly, Category = "Day")
	int32 DayScore = 0;                 // 오늘 점수 (업무 중 누적)

	UFUNCTION(BlueprintCallable, Category = "Day")
	void AddScore(int32 Delta) { DayScore += Delta; }

	UPROPERTY(ReplicatedUsing = OnRep_MemoryShard, BlueprintReadOnly, Category = "Shard")
	int32 MemoryShard = 0;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMemoryShardChanged, int32, NewValue);

	UPROPERTY(BlueprintAssignable, Category = "Shard")
	FOnMemoryShardChanged OnMemoryShardChanged;

	UFUNCTION(BlueprintCallable, Category = "Shard")
	void AddMemoryShard(int32 Delta);

	UFUNCTION(BlueprintPure, Category = "Shard")
	int32 GetMemoryShard() const { return MemoryShard; }

	UFUNCTION() void OnRep_MemoryShard();

	UFUNCTION() void OnRep_SpawnStage();
	
	UFUNCTION() void OnRep_Phase();
	UFUNCTION() void OnRep_Day() {}
	UFUNCTION() void OnRep_Reputation() {}
	UFUNCTION() void OnRep_DayScore() {}
	UFUNCTION() void OnRep_TimeLeft();
	UFUNCTION() void OnRep_ReadyCount();
	UFUNCTION() void OnRep_TotalPlayers();
	UFUNCTION() void OnRep_ReadyLock();
	UFUNCTION() void OnRep_StartCountdown();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
};
