// Fill out your copyright notice in the Description page of Project Settings.


#include "NSPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NSGameModeBase.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h" 
#include "Blueprint/UserWidget.h"

void ANSPlayerController::Server_ToggleReady_Implementation(bool bReady) {
    if (auto* GM = Cast<ANSGameModeBase>(UGameplayStatics::GetGameMode(this))) {
        GM->NotifyPlayerReadyState(this, bReady); // GameMode 일반 함수 호출
    }
}

void ANSPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;

    // 맵 이름으로 자동 재생(원하면 BP에서 명시 호출)
    const FString MapName = GetWorld()->GetMapName();
    if (MapName.Contains(TEXT("Title")))
        PlayBGM(TitleBGM, 0.5f);
    else
    {
        PlayBGM(MainBGM, 0.5f);

        ShowOnboarding(); // 접속할 때마다 항상 띄움
    }

}

void ANSPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
}

void ANSPlayerController::ApplyIngameInput()
{
    if (!IsLocalController()) return;

    const FString MapName = GetWorld()->GetMapName();
    if (MapName.Contains(TEXT("Title"))) return;

    SetGameOnlyInputMode();
    SetupEnhancedInput();
}

void ANSPlayerController::SetGameOnlyInputMode()
{
    FInputModeGameOnly Mode;
    SetInputMode(Mode);

    bShowMouseCursor = false;
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;
}

void ANSPlayerController::SetupEnhancedInput()
{
#if WITH_EDITOR || 1
    // Enhanced Input을 프로젝트에서 쓰지 않는다면 DefaultMappingContext는 null일 수 있음 → 리턴
#endif
    if (!DefaultMappingContext)
    {
        return; // 매핑 미사용/미지정이면 생략
    }

    if (ULocalPlayer* LP = GetLocalPlayer())
    {
        if (auto* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            // 타이틀에서 쓰던 매핑이 남아있을 수 있으니 깨끗이 비우고 기본 매핑 추가
            Subsys->ClearAllMappings();
            Subsys->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}

void ANSPlayerController::EnsureAudioComponent()
{
    if (!MusicAC) {
        MusicAC = NewObject<UAudioComponent>(this);
        MusicAC->bAutoDestroy = false;
        MusicAC->bIsUISound = true;      // UI/BGM로 취급
        MusicAC->RegisterComponent();
    }
}

void ANSPlayerController::PlayBGM(USoundBase* Sound, float FadeInSec)
{
    if (!IsLocalController() || !Sound) return;
    EnsureAudioComponent();

    if (MusicAC->IsPlaying())
        MusicAC->FadeOut(FadeInSec * 0.8f, 0.f);

    MusicAC->SetSound(Sound);
    MusicAC->FadeIn(FadeInSec, 1.f);
}

void ANSPlayerController::StopBGM(float FadeOutSec)
{
    if (MusicAC && MusicAC->IsPlaying())
        MusicAC->FadeOut(FadeOutSec, 0.f);
}

void ANSPlayerController::ShowOnboarding()
{
    if (bOnboardingActive || !OnboardingWidgetClass) return;

    OnboardingWidget = CreateWidget<UUserWidget>(this, OnboardingWidgetClass);
    if (!OnboardingWidget) return;

    OnboardingWidget->SetIsFocusable(true);
    OnboardingWidget->AddToViewport(2);
    bOnboardingActive = true;

    RefreshInputForCurrentUI();
}

void ANSPlayerController::CloseOnboarding()
{
    if (!bOnboardingActive) return;

    if (OnboardingWidget)
    {
        OnboardingWidget->RemoveFromParent();
        OnboardingWidget = nullptr;
    }
    bOnboardingActive = false;

    RefreshInputForCurrentUI();
}

void ANSPlayerController::Client_ShowStartupLoading_Implementation(float Duration)
{
    // 입력: UIOnly
    FInputModeUIOnly Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    bShowMouseCursor = true;

    if (LoadingOverlayClass && !LoadingOverlay)
    {
        LoadingOverlay = CreateWidget(this, LoadingOverlayClass);
        if (LoadingOverlay) LoadingOverlay->AddToViewport(10000); // 최상단
    }

    RefreshInputForCurrentUI();

    // Duration 후 서버에 준비 보고
    FTimerHandle Th;
    GetWorldTimerManager().SetTimer(Th, [this]()
        {
            if (!bStartupReported)
            {
                bStartupReported = true;
                Server_ReportStartupLoaded(); // 서버: 스폰 트리거
            }
        }, Duration, false);
}

void ANSPlayerController::Client_HideStartupLoading_Implementation()
{
    if (LoadingOverlay)
    {
        LoadingOverlay->RemoveFromParent();
        LoadingOverlay = nullptr;
    }

    RefreshInputForCurrentUI();
}

void ANSPlayerController::Server_ReportStartupLoaded_Implementation()
{
    // 서버에서 이 컨트롤러 스폰 진행 (GameMode에 위임)
    if (HasAuthority())
    {
        if (AGameModeBase* GM = UGameplayStatics::GetGameMode(this))
        {
            // 안전 장치: 이미 폰이 있다면 중복 스폰 방지
            if (GetPawn() == nullptr)
            {
                GM->RestartPlayer(this);
            }
        }

        // 스폰 직후 클라 오버레이 내리기
        Client_HideStartupLoading();
    }
}

void ANSPlayerController::RefreshInputForCurrentUI()
{
    if (!IsLocalController()) return;

    // 로딩 오버레이가 떠 있으면: UI Only + 커서 표시
    if (LoadingOverlay)
    {
        FInputModeUIOnly Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
        bShowMouseCursor = true;

        SetIgnoreMoveInput(true);
        SetIgnoreLookInput(true);
        return;
    }

    // 온보딩(안내) UI가 떠 있으면: UI Only + 커서 표시 + 포커스 지정
    if (bOnboardingActive && OnboardingWidget)
    {
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(OnboardingWidget->TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
        bShowMouseCursor = true;

        SetIgnoreMoveInput(true);
        SetIgnoreLookInput(true);
        return;
    }

    // 그 외: Game Only + 커서 숨김 (평소 인게임)
    FInputModeGameOnly GameMode;
    SetInputMode(GameMode);
    bShowMouseCursor = false;

    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
}

void ANSPlayerController::TogglePauseMenu()
{
    if (!IsLocalController()) return;
    if (PauseMenu && PauseMenu->IsInViewport()) HidePauseMenu();
    else ShowPauseMenu();
}

void ANSPlayerController::ShowPauseMenu()
{
    if (!PauseMenuClass || !IsLocalController()) return;
    
    if (!PauseMenu)
        PauseMenu = CreateWidget<UUserWidget>(this, PauseMenuClass);
    
    if (!PauseMenu) return;

    PauseMenu->AddToViewport(100);

    UWidget* FocusTarget = PauseMenu->GetWidgetFromName(TEXT("Btn_Resume"));
    if (!FocusTarget) FocusTarget = PauseMenu;

    FInputModeUIOnly M;
    M.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    M.SetWidgetToFocus(FocusTarget->GetCachedWidget());
    SetInputMode(M);

    bShowMouseCursor = true;
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
}

void ANSPlayerController::HidePauseMenu()
{
    if (!IsLocalController()) return;
    if (PauseMenu && PauseMenu->IsInViewport())
        PauseMenu->RemoveFromParent();

    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;
    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
}

