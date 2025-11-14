// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "NSGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class UNSGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;
	virtual void OnStart() override;

	UFUNCTION() void OnGSDKShutdown();
	UFUNCTION() bool OnGSDKHealthCheck();
	UFUNCTION() void OnGSDKActive();


public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Net|Defaults")
    FString DefaultDSHost = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Net|Defaults")
    int32 DefaultDSPort = 7777;

    // 타이틀에서 로컬 맵 테스트용
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Net|Defaults")
    bool bLocalTest = false;

    // BP에서 바로 부르기 좋은 진입 함수(분기 포함)
    UFUNCTION(BlueprintCallable, Category = "Net")
    void StartFromTitle(bool bForceLocal,
        const FString& HostOverride,
        int32 PortOverride,
        const FString& OptionalPlayerName);

    // DS로 접속 (콘솔 open 과 동일)
    UFUNCTION(BlueprintCallable, Category = "Net")
    void ConnectToDS(const FString& Host, int32 Port, const FString& PlayerName);

    // 로컬 맵 열기(싱글/전용서버 없이)
    UFUNCTION(BlueprintCallable, Category = "Net")
    void OpenLocal(const FString& MapName = TEXT("Map_2F_Ingame"));

    // 임시 닉네임 생성
    UFUNCTION(BlueprintPure, Category = "Net")
    static FString MakeGuestName();

    UFUNCTION(BlueprintCallable)
    void CreateRoomAndJoin(const FString& OptionalPlayerName);

    // 방 만들고 바로 접속 (CloudFunction → MPS → ConnectToDS)
    UFUNCTION(BlueprintCallable, Category = "NS|Title")
    void BP_CreateRoom(const FString& OptionalPlayerName);

    // IP:Port로 바로 접속 (참여하기용)
    UFUNCTION(BlueprintCallable, Category = "NS|Title")
    void BP_JoinDirect(const FString& Host, int32 Port, const FString& PlayerName);

    UPROPERTY(BlueprintReadOnly, Category = "NS|Session")
    FString LastServerIP;

    UPROPERTY(BlueprintReadOnly, Category = "NS|Session")
    int32   LastServerPort = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "NS|PlayFab")
    FString TitleId = TEXT("11D45F");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "NS|PlayFab")
    FString CustomId = TEXT("ns-local-test-001");

    UFUNCTION(BlueprintPure, Category = "NS|PlayFab")
    FString GetTitleIdSafe() const;

    UFUNCTION(BlueprintCallable)
    void LeaveToTitle();

private:
	FTimerHandle HealthPulse;
	double LastHealthTickSec = 0.0;

    void LoginWithCustomId(TFunction<void(const FString& PlayerEntityToken)> OnDone);
    void CallExecuteFunction(const FString& PlayerEntityToken, TFunction<void(const FString& Ip, int32 Port)> OnDone);

    FString CachedPlayerEntityToken;

#if UE_SERVER
private:
	static bool bGSDKBootstrapped;
    static bool bGSDKDelegatesBound;

	void BootstrapGSDK();
#endif
};
