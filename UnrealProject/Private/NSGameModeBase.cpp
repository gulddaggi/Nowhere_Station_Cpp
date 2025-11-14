// Fill out your copyright notice in the Description page of Project Settings.


#include "NSGameModeBase.h"
#include "NSGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Async/Async.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "NSSpawnDirector.h"
#include "NSPlayerController.h"
#include "StartRitualStatue.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"


#if UE_SERVER
#include "GSDKUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#endif

ANSGameModeBase::ANSGameModeBase()
{
    bPauseable = false;
    bStartPlayersAsSpectators = true;

    GameStateClass = ANSGameState::StaticClass();

    PlayerControllerClass = ANSPlayerController::StaticClass();
}

void ANSGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    BindGsdkCallbacks(); // 맵 로드 초기에 콜백 바인딩
}

void ANSGameModeBase::BeginPlay()
{
    Super::BeginPlay();

#if UE_SERVER
    UGSDKUtils::ReadyForPlayers();
#endif
}

void ANSGameModeBase::BindGsdkCallbacks()
{
#if UE_SERVER
    // 동적 델리게이트 생성/바인딩
    FOnGSDKServerActive_Dyn Active;
    Active.BindDynamic(this, &ANSGameModeBase::OnGSDKServerActive);
    UGSDKUtils::RegisterGSDKServerActiveDelegate(Active);

    FOnGSDKShutdown_Dyn Shutdown;
    Shutdown.BindDynamic(this, &ANSGameModeBase::OnGSDKShutdown);
    UGSDKUtils::RegisterGSDKShutdownDelegate(Shutdown);

    FOnGSDKHealthCheck_Dyn Health;
    Health.BindDynamic(this, &ANSGameModeBase::OnGSDKHealthCheck);
    UGSDKUtils::RegisterGSDKHealthCheckDelegate(Health);

    FOnGSDKMaintenance_Dyn Maint;
    Maint.BindDynamic(this, &ANSGameModeBase::OnGSDKMaintenance);
    UGSDKUtils::RegisterGSDKMaintenanceDelegate(Maint);
#endif
}

void ANSGameModeBase::OnGSDKServerActive()
{
    AsyncTask(ENamedThreads::GameThread, [this]() {
        UE_LOG(LogTemp, Log, TEXT("[GSDK] Active -> start match"));
        if (auto* GS = GetGameState<ANSGameState>()) {
            GS->TotalPlayers = GameState.Get() ? GameState.Get()->PlayerArray.Num() : 0;
            GS->ReadyCount = 0;
            SetPhase(GS, EGamePhase::Waiting);
        }
    });
}

void ANSGameModeBase::OnGSDKShutdown()
{
    UE_LOG(LogTemp, Warning, TEXT("[GSDK] Shutdown received - schedule quit on GameThread"));

    AsyncTask(ENamedThreads::GameThread, [this]()
        {
            // 타이머/세션/로그 마무리
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().ClearAllTimersForObject(this);
                // 플레이어 킥/세션 저장 등 필요한 정리
            }

            // 서버 프로세스 정상 종료
            FPlatformMisc::RequestExit(/*Force*/ false);
            // 또는 UKismetSystemLibrary::QuitGame(World, nullptr, EQuitPreference::Quit, false);
        });
}

bool ANSGameModeBase::OnGSDKHealthCheck()
{
    // 서버 헬스 체크 로직(지표/타임아웃 등). OK면 true
    return true;
}

void ANSGameModeBase::OnGSDKMaintenance(const FDateTime& When)
{
    UE_LOG(LogTemp, Warning, TEXT("[GSDK] Maintenance scheduled: %s"), *When.ToString());
}

FString ANSGameModeBase::GetPlayerIdForGsdk(APlayerState* PS) const
{
    if (!PS) return TEXT("");
    FString Out = PS->GetPlayerName();
    if (Out.IsEmpty() && PS->GetUniqueId().IsValid())
    {
        Out = PS->GetUniqueId()->ToString();
    }
    return Out;
}

#if UE_SERVER
void ANSGameModeBase::PushConnectedPlayersToGsdk()
{
    TArray<FConnectedPlayer> List;
    List.Reserve(ConnectedIds.Num());
    for (const FString& Id : ConnectedIds)
    {
        FConnectedPlayer P; P.PlayerId = Id;
        List.Add(P);
    }
    UGSDKUtils::UpdateConnectedPlayers(List);
}

void ANSGameModeBase::LogGSDKConnectionInfo() const
{
    const FString PublicIp = UGSDKUtils::GetConfigValue(TEXT("publicIpV4Address"));
    if (!PublicIp.IsEmpty())
    {
        UE_LOG(LogTemp, Display, TEXT("[GSDK] PublicIP: %s"), *PublicIp);
    }

    FString PortsJson = UGSDKUtils::GetConfigValue(TEXT("gamePortsConfiguration"));
    if (PortsJson.IsEmpty())
        PortsJson = UGSDKUtils::GetConfigValue(TEXT("game_ports"));

    if (!PortsJson.IsEmpty())
    {
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PortsJson);
        TArray<TSharedPtr<FJsonValue>> Ports;
        if (FJsonSerializer::Deserialize(Reader, Ports))
        {
            for (const auto& V : Ports)
            {
                const TSharedPtr<FJsonObject>* Obj = nullptr;
                if (V->TryGetObject(Obj))
                {
                    FString Name; int32 Listen = 0, Client = 0;
                    (*Obj)->TryGetStringField(TEXT("name"), Name);
                    (*Obj)->TryGetNumberField(TEXT("serverListeningPort"), Listen);
                    (*Obj)->TryGetNumberField(TEXT("clientConnectionPort"), Client);
                    UE_LOG(LogTemp, Display, TEXT("[GSDK] Port name=%s listen=%d client=%d"),
                        *Name, Listen, Client);
                }
            }
        }
    }

    FString LogsDir = UGSDKUtils::GetConfigValue(TEXT("logFolder"));
    if (LogsDir.IsEmpty())
    {
        LogsDir = UGSDKUtils::GetLogsDirectory();
    }
    if (!LogsDir.IsEmpty())
    {
        UE_LOG(LogTemp, Display, TEXT("[GSDK] LogsDirectory: %s"), *LogsDir);
    }
}
#endif

void ANSGameModeBase::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    const int32 Num = GameState ? GameState->PlayerArray.Num() : GetNumPlayers();
    if (Num > MaxPlayers && NewPlayer)
    {
        NewPlayer->ClientTravel(TEXT("disconnect"), TRAVEL_Absolute);
        return;
    }
    if (auto* GS = GetGameState<ANSGameState>()) {
        GS->TotalPlayers = GameState.Get()->PlayerArray.Num();
    }
#if UE_SERVER
    if (NewPlayer && NewPlayer->PlayerState)
    {
        const FString PlayerId = GetPlayerIdForGsdk(NewPlayer->PlayerState);
        if (!PlayerId.IsEmpty())
        {
            ConnectedIds.Add(PlayerId);
            PushConnectedPlayersToGsdk();
            UE_LOG(LogTemp, Display, TEXT("[GSDK] +Player %s (total %d)"), *PlayerId, ConnectedIds.Num());
        }
    }
#endif

    constexpr float kStartupDelaySec = 12.0f;

    if (auto* PC = Cast<ANSPlayerController>(NewPlayer))
    {
        PC->Client_ShowStartupLoading(kStartupDelaySec);
    }

    GetWorldTimerManager().ClearTimer(EmptyShutdownHandle);
}

void ANSGameModeBase::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    if (auto* GS = GetGameState<ANSGameState>()) {
        GS->TotalPlayers = GameState.Get()->PlayerArray.Num();

        // 떠난 유저가 Ready였으면 해제
        for (auto It = ReadySet.CreateIterator(); It; ++It) {
            if (It->Get() == Exiting) { It.RemoveCurrent(); break; }
        }
        GS->ReadyCount = ReadySet.Num();
    }
#if UE_SERVER
    const FString PlayerId = GetPlayerIdForGsdk(Exiting ? Exiting->PlayerState : nullptr);
    if (!PlayerId.IsEmpty())
    {
        ConnectedIds.Remove(PlayerId);
        PushConnectedPlayersToGsdk();
        UE_LOG(LogTemp, Display, TEXT("[GSDK] -Player %s (total %d)"), *PlayerId, ConnectedIds.Num());
    }
#endif

    MaybeScheduleEmptyShutdown();
}

void ANSGameModeBase::MaybeScheduleEmptyShutdown()
{
#if UE_SERVER
    if (!bShutdownWhenEmpty) return;

    if (GetNumPlayers() == 0)
    {
        GetWorldTimerManager().SetTimer(
            EmptyShutdownHandle, this, &ANSGameModeBase::DoEmptyShutdown,
            EmptyShutdownDelay, false);
    }
#endif
}

void ANSGameModeBase::DoEmptyShutdown()
{
#if UE_SERVER
    if (GetNumPlayers() > 0) return;
    UE_LOG(LogTemp, Log, TEXT("[Server] All players left. Shutting down..."));
    FPlatformMisc::RequestExit(false);
#endif
}

void ANSGameModeBase::NotifyPlayerReadyState(APlayerController* Who, bool bReady)
{
    if (!Who) return;
    if (auto* GS = GetGameState<ANSGameState>())
    {
        // 준비 ‘잠금’ 이후에는 더 이상 토글 불가
        if (GS->bReadyLocked) return;

        if (bReady) ReadySet.Add(Who); else ReadySet.Remove(Who);
        GS->ReadyCount = ReadySet.Num();
        GS->TotalPlayers = GameState.Get()->PlayerArray.Num();

        const bool bAllReady = (GS->TotalPlayers > 0 && GS->ReadyCount == GS->TotalPlayers);

        if (bAllReady)
        {
            // 이미 카운트다운 중이면 무시
            if (GS->Phase == EGamePhase::Waiting)
                BeginStartCountdown();
        }
        else
        {
            // 누군가 취소 → 카운트다운 중지 & 대기 복귀
            if (GS->Phase == EGamePhase::Starting)
                CancelStartCountdown();
        }
    }
}

void ANSGameModeBase::SetPhase(ANSGameState* GS, EGamePhase NewPhase) {
    if (!GS) return;
    GS->Phase = NewPhase;
    UE_LOG(LogTemp, Log, TEXT("[PHASE] -> %d"), (int)NewPhase);
}

void ANSGameModeBase::StartWorkPhase() {
    if (auto* GS = GetGameState<ANSGameState>()) {
        SetJoinLocked(true);
        GS->DayScore = 0;
        GS->TimeLeftSec = 600; // 진행 시간
        SetPhase(GS, EGamePhase::InProgress);

        GetWorldTimerManager().ClearTimer(WorkTickHandle);
        GetWorld()->GetTimerManager().SetTimer(WorkTickHandle, this, &ANSGameModeBase::TickWorkTimer, 1.0f, true);


        if (!SpawnDirector)
        {
            SpawnDirector = Cast<ANSSpawnDirector>(
                UGameplayStatics::GetActorOfClass(this, ANSSpawnDirector::StaticClass()));
        }
        if (!SpawnDirector && SpawnDirectorClass)
        {
            FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnDirector = GetWorld()->SpawnActor<ANSSpawnDirector>(SpawnDirectorClass, FTransform::Identity, P);
        }

        if (SpawnDirector)
        {
            SpawnDirector->BeginSpawnLoop(GS->TimeLeftSec);
            GS->SpawnStage = ESpawnStage::Early;
            GS->ForceNetUpdate();
        }
    }
}

void ANSGameModeBase::TickWorkTimer() {
    if (auto* GS = GetGameState<ANSGameState>()) {
        GS->TimeLeftSec = FMath::Max(0, GS->TimeLeftSec - 1);

        if (SpawnDirector)
        {
            const ESpawnStage S = SpawnDirector->GetStage();
            if (GS->SpawnStage != S) { GS->SpawnStage = S; GS->ForceNetUpdate(); }
        }

        if (GS->TimeLeftSec <= 0)
        {
            GetWorld()->GetTimerManager().ClearTimer(WorkTickHandle);
            StartEndingPhase();
        }
    }
}

void ANSGameModeBase::BeginStartCountdown()
{
    if (auto* GS = GetGameState<ANSGameState>())
    {
        SetJoinLocked(true);
        GS->StartCountdownSec = 5;
        SetPhase(GS, EGamePhase::Starting);
        GetWorld()->GetTimerManager().SetTimer(
            StartCountdownHandle, this, &ANSGameModeBase::TickStartCountdown, 1.0f, true);
    }
}

void ANSGameModeBase::TickStartCountdown()
{
    if (auto* GS = GetGameState<ANSGameState>())
    {
        GS->StartCountdownSec = FMath::Max(0, GS->StartCountdownSec - 1);
        if (GS->StartCountdownSec <= 0)
        {
            GetWorld()->GetTimerManager().ClearTimer(StartCountdownHandle);
            GS->bReadyLocked = true;                 
            StartWorkPhase();                        
        }
    }
}

void ANSGameModeBase::CancelStartCountdown()
{
    GetWorld()->GetTimerManager().ClearTimer(StartCountdownHandle);
    if (auto* GS = GetGameState<ANSGameState>())
    {
        SetJoinLocked(false);
        GS->StartCountdownSec = 0;
        SetPhase(GS, EGamePhase::Waiting);
    }
}

void ANSGameModeBase::StartEndingPhase() {
    if (auto* GS = GetGameState<ANSGameState>()) {
        SetPhase(GS, EGamePhase::Ending);
        if (SpawnDirector) {
            SpawnDirector->EndSpawnLoop();
            SpawnDirector->DeactivateAllWork();
            GS->SpawnStage = ESpawnStage::Inactive;
            GS->ForceNetUpdate();
        }

        EvaluateDay();
        FadeOutThenTeleport();
    }
}

void ANSGameModeBase::EvaluateDay()
{
    ANSGameState* GS = GetGameState<ANSGameState>();
    if (!GS) return;

    // 남은 작업 패널티를 반영하고 싶으면 SpawnDirector에서 값을 얻어와 뺍니다.
    if (PenaltyPerLeftover > 0 && SpawnDirector)
    {
        const int32 Left = SpawnDirector->GetAliveWorkCount();
        GS->DayScore -= Left * PenaltyPerLeftover;
    }

    // 평판 반영
    if (GS->DayScore >= 0) { GS->Reputation += 1; }
    else { GS->Reputation = FMath::Max(0, GS->Reputation - 1); }

    // 게임오버/클리어 판정은 텔레포트 직후에 처리
}

void ANSGameModeBase::FadeOutThenTeleport()
{
    // 클라 카메라 페이드: PC의 카메라매니저로 처리
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (PC->PlayerCameraManager)
            {
                PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.6f, FLinearColor::Black, false, true);
            }
            // 입력 잠금/커서 등은 필요 시 여기서
        }
    }

    // 0.65초 뒤 텔레포트 & 페이드 해제
    GetWorldTimerManager().SetTimer(FadeHandle, [this]()
        {
            TeleportAllToStatue();

            // 페이드 해제
            for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            {
                if (APlayerController* PC = It->Get())
                {
                    if (PC->PlayerCameraManager)
                    {
                        PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.6f, FLinearColor::Black, false, true);
                    }
                }
            }

            ResetForNextDay();

        }, 0.65f, false);
}

void ANSGameModeBase::TeleportAllToStatue()
{
    // 맵의 여신상(시작 점) 근처로 플레이어를 재배치
    AStartRitualStatue* Statue = nullptr;
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(this, AStartRitualStatue::StaticClass(), Found);
        if (Found.Num() > 0) Statue = Cast<AStartRitualStatue>(Found[0]);
    }
    const FVector Center = Statue ? Statue->GetActorLocation() : FVector::ZeroVector;
    const float BaseRadius = 220.f; // 조각상 반경 + 여유

    // 플레이어 수 집계
    TArray<APlayerController*> PCs;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (APlayerController* PC = It->Get()) PCs.Add(PC);

    const int32 N = FMath::Max(1, PCs.Num());
    const float Step = 2.f * PI / float(N);

    for (int32 i = 0; i < N; ++i)
    {
        if (APlayerController* PC = PCs[i])
            if (APawn* P = PC->GetPawn())
            {
                // 링 상의 목표 위치
                const float A = i * Step;
                FVector Pos = Center + FVector(BaseRadius * FMath::Cos(A), BaseRadius * FMath::Sin(A), 0.f);

                // 바닥으로 스냅(라인트레이스)
                FHitResult Hit;
                FVector Start = Pos + FVector(0, 0, 300);
                FVector End = Pos - FVector(0, 0, 800);
                if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility))
                {
                    float HalfH = 0.f;
                    if (ACharacter* C = Cast<ACharacter>(P))
                        HalfH = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                    Pos.Z = Hit.ImpactPoint.Z + HalfH;
                }

                // 배치 + 조각상 바라보게
                P->SetActorLocation(Pos, false, nullptr, ETeleportType::TeleportPhysics);
                const FRotator Face = (Center - Pos).Rotation();
                P->SetActorRotation(FRotator(0.f, Face.Yaw, 0.f));
            }
    }
}

void ANSGameModeBase::ResetForNextDay()
{
    ANSGameState* GS = GetGameState<ANSGameState>();
    if (!GS) return;

    if (UWorld* W = GetWorld())
    {
        // 안전: 모든 타이머 정리
        W->GetTimerManager().ClearTimer(WorkTickHandle);
        W->GetTimerManager().ClearTimer(StartCountdownHandle);
    }

    if (SpawnDirector)
    {
        // 있으면 호출(없으면 무시)
        SpawnDirector->EndSpawnLoop();
        if (SpawnDirector->HasAuthority())
        {
            // 프로젝트에 구현돼 있으면 사용
            if (SpawnDirector->GetClass()->FindFunctionByName(TEXT("DeactivateAllWork")))
            {
                SpawnDirector->DeactivateAllWork();
            }
        }
    }

    if (GS)
    {
        // 준비 다시 가능하도록 잠금 해제
        GS->bReadyLocked = false;

        // 카운트/타이머/스테이지 초기화
        GS->ReadyCount = 0;
        GS->TimeLeftSec = 0;
        GS->StartCountdownSec = 0;
        GS->SpawnStage = ESpawnStage::Inactive;

        // 서버가 들고 있는 준비 집합 초기화(멤버: TSet<APlayerController*> ReadySet)
        ReadySet.Empty();

        // 상태를 ‘대기’로 복귀
        SetPhase(GS, EGamePhase::Waiting);
        GS->ForceNetUpdate();
    }

    // 플레이어 상태 정리(모핑/입력락 등 풀기)
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            APawn* P = PC->GetPawn();
            if (!P) continue;

            // 이동/시야 잠금이 있었다면 해제
            PC->SetIgnoreMoveInput(false);
            PC->SetIgnoreLookInput(false);
        }
    }

    // 게임오버 / 7일 클리어 체크
    const bool bGameOver = (GS->Reputation <= 0);
    const bool bCleared = (GS->Day >= MaxDays);

    if (!bGameOver && !bCleared)
    {
        GS->Day += 1;                         // 다음 날
        GS->TimeLeftSec = 0;
        GS->DayScore = 0;

        // 준비 상태로 전환 (플레이어들은 여신상에서 다시 "준비"를 누름)
        SetJoinLocked(false);
        SetPhase(GS, EGamePhase::Waiting);
    }
    else
    {
        // 엔딩 상태 유지 – 원하면 타이틀로 이동/메시 표시 등을 여기서
        SetPhase(GS, EGamePhase::Ending);
        UE_LOG(LogTemp, Log, TEXT("[DAY] %s"),
            bGameOver ? TEXT("GameOver") : TEXT("All 7 days cleared"));
    }
}

void ANSGameModeBase::AddScore_Stain(int32 Count)
{
    if (ANSGameState* GS = GetGameState<ANSGameState>())
    {
        GS->AddScore(ScorePerStain * Count);
    }
}

void ANSGameModeBase::AddScore_Repair(int32 Count)
{
    if (ANSGameState* GS = GetGameState<ANSGameState>())
    {
        GS->AddScore(ScorePerRepair * Count);
    }
}

void ANSGameModeBase::ForceEvaluateAndNextDay()
{
    GetWorldTimerManager().ClearTimer(WorkTickHandle);
    StartEndingPhase();
}

void ANSGameModeBase::SetJoinLocked(bool bLocked)
{
    bLockJoins = bLocked;
}

void ANSGameModeBase::PreLogin(
    const FString& Options,
    const FString& Address,
    const FUniqueNetIdRepl& UniqueId,
    FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty())
    {
        return; // 이미 상위에서 거절됨
    }

    // 진행 중/락 상태면 거절
    if (bLockJoins)
    {
        ErrorMessage = TEXT("GAME_IN_PROGRESS");
        return;
    }

    // 4인 제한
    const int32 Num = GameState ? GameState->PlayerArray.Num() : GetNumPlayers();
    if (Num >= MaxPlayers)
    {
        ErrorMessage = TEXT("GAME_FULL");
        return;
    }
}
