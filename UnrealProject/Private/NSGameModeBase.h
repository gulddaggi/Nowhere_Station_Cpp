// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/DateTime.h"

enum class EGamePhase : uint8;
class ANSGameState;
class ANSSpawnDirector;
class ANSPlayerController;

#include "NSGameModeBase.generated.h"

/**
 * 
 */
UCLASS()
class ANSGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ANSGameModeBase();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void NotifyPlayerReadyState(class APlayerController* Who, bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Day")
	void AddScore_Stain(int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Day")
	void AddScore_Repair(int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Day")
	void ForceEvaluateAndNextDay();    // 테스트용 강제 평가

	UPROPERTY(EditDefaultsOnly, Category = "Session")
	int32 MaxPlayers = 4;

	// 진행 중에는 신규 접속 금지
	UPROPERTY(VisibleInstanceOnly, Category = "Session")
	bool bLockJoins = false;

	// 진행 상태에 따라 OnDayStarted/OnDayEnded 같은 지점에서 토글
	void SetJoinLocked(bool bLocked);
private:
	UFUNCTION() void OnGSDKServerActive();                 // ALLOCATE 신호 수신
	UFUNCTION() void OnGSDKShutdown();                     // 종료 콜백
	UFUNCTION() bool OnGSDKHealthCheck();                  // 헬스체크 콜백
	UFUNCTION() void OnGSDKMaintenance(const FDateTime& When);  // 점검 통지

	FString GetPlayerIdForGsdk(class APlayerState* PS) const;
	void BindGsdkCallbacks();

	// SpawnDirector 인스턴스 (레벨에 배치했으면 Find해서 씀)
	UPROPERTY()
	ANSSpawnDirector* SpawnDirector = nullptr;

	// 스폰용 클래스 지정
	UPROPERTY(EditDefaultsOnly, Category = "Spawn")
	TSubclassOf<ANSSpawnDirector> SpawnDirectorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Score")
	int32 ScorePerStain = 5;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Score")
	int32 ScorePerRepair = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Score")
	int32 PenaltyPerLeftover = 0;

	static constexpr int32 MaxDays = 7;

#if UE_SERVER
	// 디버그 로그: public IP / 할당 포트 / 로그 폴더 출력
	void LogGSDKConnectionInfo() const;

	// 접속 중 플레이어 ID를 우리가 관리해서 매번 전체 목록을 밀어넣는 방식
	TSet<FString> ConnectedIds;
	void PushConnectedPlayersToGsdk();
#endif

	// 준비 중인 플레이어 집합
	TSet<TWeakObjectPtr<APlayerController>> ReadySet;

	FTimerHandle FadeHandle;

	void EvaluateDay();            // 점수→평판/날짜 갱신
	void ResetForNextDay();        // 대기 상태로 초기화
	void FadeOutThenTeleport();    // 암전→텔레포트→해제
	void TeleportAllToStatue();

	// 라운드 타이머
	FTimerHandle WorkTickHandle;
	void StartWorkPhase();
	void TickWorkTimer();
	void StartEndingPhase();

	FTimerHandle StartCountdownHandle;

	void BeginStartCountdown();     // 전원 Ready 시 시작 (5초)
	void TickStartCountdown();      // 1초마다 감소
	void CancelStartCountdown();    // 누군가 취소하면 중단

	// 내부 헬퍼
	void SetPhase(class ANSGameState* GS, EGamePhase NewPhase);

protected:
	// 인원/진행중 여부 체크해서 거절
	virtual void PreLogin(
		const FString& Options,
		const FString& Address,
		const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;

	UPROPERTY(EditDefaultsOnly, Category = "Server")
	bool bShutdownWhenEmpty = true;

	UPROPERTY(EditDefaultsOnly, Category = "Server", meta = (ClampMin = "0.0"))
	float EmptyShutdownDelay = 10.f;

private:
	FTimerHandle EmptyShutdownHandle;
	void MaybeScheduleEmptyShutdown();
	void DoEmptyShutdown();
};
