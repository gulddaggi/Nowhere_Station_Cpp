// InteractiveActor.h
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "InteractiveActor.generated.h"

class UWidgetComponent;
class UUserWidget;


USTRUCT(BlueprintType)
struct FQTEConfig
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) float PromptInterval = 1.5f;
    UPROPERTY(EditAnywhere) float PromptTimeout = 1.25f;
    UPROPERTY(EditAnywhere) float SuccessGain = 0.25f; // 진행도 +
    UPROPERTY(EditAnywhere) float FailPenalty = 0.10f; // 진행도 -
    UPROPERTY(EditAnywhere) TArray<FKey> Keys = { EKeys::Q, EKeys::W, EKeys::E, EKeys::R };
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRepairCompleted, AInteractiveActor*, Who);

UCLASS()
class AInteractiveActor : public AActor
{
    GENERATED_BODY()

public:
    AInteractiveActor();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing = OnRep_IsBroken, VisibleAnywhere, BlueprintReadOnly, Category = "State")
    bool bIsBroken = true;

    UPROPERTY(ReplicatedUsing = OnRep_RepairProgress, VisibleAnywhere, BlueprintReadOnly, Category = "State|Repair")
    float RepairProgress = 0.f;

    UFUNCTION(BlueprintPure, Category = "State")
    bool IsBroken() const { return bIsBroken; }

    UFUNCTION(BlueprintPure, Category = "State")
    bool IsInQTEMode() const { return bInQTEMode; }

    UFUNCTION() void OnRep_IsBroken();
    UFUNCTION() void OnRep_RepairProgress();

    UPROPERTY(EditAnywhere, Category = "Repair|QTE")
    FQTEConfig QTE;

    UPROPERTY(VisibleInstanceOnly, Category = "Repair|QTE")
    TWeakObjectPtr<class APlayerController> QTEOwnerPC;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Repair|QTE")
    TWeakObjectPtr<APlayerController> LocalQTEPC;

    UPROPERTY(VisibleInstanceOnly, Category = "Repair|QTE")
    FKey CurrentPromptKey;

    FTimerHandle TimerPrompt, TimerTimeout;

    UFUNCTION(Client, Reliable)
    void Client_UpdateProgress(float NewProgress);

    UFUNCTION(Client, Reliable)
    void Client_ClearPrompt(); // 화면의 QTE 아이콘 제거 요청

    // BP에서 실제 위젯 제거/정리만 수행
    UFUNCTION(BlueprintImplementableEvent)
    void BP_OnQTEClearPrompt();

    UFUNCTION(Server, Reliable, BlueprintCallable) void Server_RequestStartRepair(class APlayerCharacter* By);
    UFUNCTION(Server, Reliable, BlueprintCallable) void Server_SubmitQTEInput(FKey Pressed);

    UFUNCTION(Client, Reliable) void Client_BeginQTE(class APlayerController* ForPC);
    UFUNCTION(Client, Reliable) void Client_ShowPrompt(FKey Key, float Timeout);
    UFUNCTION(Client, Reliable) void Client_EndQTE();

    UFUNCTION(BlueprintImplementableEvent) void BP_OnQTEBegin(APlayerController* ForPC);
    UFUNCTION(BlueprintImplementableEvent) void BP_OnQTEPrompt(FKey Key, float Timeout);
    UFUNCTION(BlueprintImplementableEvent) void BP_OnQTEEnd();
    UFUNCTION(BlueprintImplementableEvent) void BP_OnBrokenChanged(bool bNewBroken);

    UPROPERTY(BlueprintAssignable) FOnRepairCompleted OnRepairCompleted;

    // 기존 인터페이스 유지
    UFUNCTION(BlueprintCallable) void SetIsBroken(bool bNewState);
    UFUNCTION(BlueprintCallable) void StartRepair(); // 클라가 누르면 → Server_RequestStartRepair로
    UFUNCTION(BlueprintCallable) void StopRepair();
protected:
    virtual void BeginPlay() override;

    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    FName OutlineParameterName;

    UPROPERTY(ReplicatedUsing = OnRep_InQTEMode, BlueprintReadOnly, Category = "State|Repair")
    bool bInQTEMode = false;

    UFUNCTION()
    void OnRep_InQTEMode();

    void ScheduleNextPrompt();
    void IssuePrompt();
    void ApplySuccess();
    void ApplyFail();
    void CompleteRepair();

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void OnWidgetInitialized(UObject* WidgetInstance);

    UFUNCTION(BlueprintImplementableEvent, Category = "State|Repair")
    void OnRepairProgressUpdated(float NewProgress);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UWidgetComponent> PanelWidgetComponent;

public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Effects")
    TObjectPtr<class UMaterialInstanceDynamic> OutlineDMI;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class USphereComponent> ProximitySphere;

    UFUNCTION(BlueprintCallable, Category = "Effects")
    void UpdateOutlineVisibility();

    UFUNCTION(Server, Reliable, BlueprintCallable)
    void Server_StopRepair(class APlayerCharacter* By);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> PanelWidgetClass;

    UFUNCTION(BlueprintPure, Category = "UI")
    UUserWidget* GetPanelWidget() const;

};
