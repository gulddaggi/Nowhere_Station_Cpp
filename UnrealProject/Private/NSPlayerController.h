// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "NSPlayerController.generated.h"

class UInputMappingContext;

/**
 * 
 */
UCLASS()
class ANSPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
    UFUNCTION(BlueprintCallable, Category = "Input")
    void ApplyIngameInput();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext = nullptr;

	UFUNCTION(Server, Reliable)
	void Server_ToggleReady(bool bReady);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* TitleBGM;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* MainBGM;

    UFUNCTION(BlueprintCallable, Category = "Audio")
    void PlayBGM(USoundBase* Sound, float FadeInSec = 0.5f);

    UFUNCTION(BlueprintCallable, Category = "Audio")
    void StopBGM(float FadeOutSec = 0.5f);

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<class UUserWidget> OnboardingWidgetClass;

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowOnboarding();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void CloseOnboarding();

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<class UUserWidget> LoadingOverlayClass;

    UPROPERTY() UUserWidget* LoadingOverlay = nullptr;
    UPROPERTY() bool bStartupReported = false;

    UFUNCTION(Client, Reliable) void Client_ShowStartupLoading(float Duration);
    UFUNCTION(Client, Reliable) void Client_HideStartupLoading();

    UFUNCTION(Server, Reliable) void Server_ReportStartupLoaded();

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UUserWidget> PauseMenuClass;

    UPROPERTY()
    UUserWidget* PauseMenu = nullptr;

    UFUNCTION(BlueprintCallable)
    void TogglePauseMenu();

    void ShowPauseMenu();

    UFUNCTION(BlueprintCallable)
    void HidePauseMenu();

protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;

private:
    void SetGameOnlyInputMode();     
    void SetupEnhancedInput();       
    UPROPERTY() UAudioComponent* MusicAC = nullptr;
    void EnsureAudioComponent();

    UPROPERTY()
    UUserWidget* OnboardingWidget = nullptr;

    bool bOnboardingActive = false;

    void RefreshInputForCurrentUI();
};
