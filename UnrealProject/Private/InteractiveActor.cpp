// Fill out your copyright notice in the Description page of Project Settings.


#include "InteractiveActor.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Components/WidgetComponent.h"

#include "TimerManager.h"

#include "NSGameModeBase.h"

AInteractiveActor::AInteractiveActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;                
	SetReplicateMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(Root);

	// 라인트레이스가 맞도록 가시성 채널을 Block
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// 블루프린트가 기대하는 네이티브 컴포넌트 WidgetComponent
	PanelWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PanelWidgetComponent"));
	PanelWidgetComponent->SetupAttachment(RootComponent);
	PanelWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	PanelWidgetComponent->SetDrawAtDesiredSize(true);
	PanelWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	PanelWidgetComponent->SetTwoSided(true);
	PanelWidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);
	PanelWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProximitySphere = CreateDefaultSubobject<USphereComponent>(TEXT("ProximitySphere"));
	ProximitySphere->SetupAttachment(RootComponent);
	ProximitySphere->SetSphereRadius(600.f);

	bIsBroken = true;
	OutlineParameterName = TEXT("OutlineOpacity");
}

void AInteractiveActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if UE_SERVER
	if (PanelWidgetComponent)
	{
		PanelWidgetComponent->SetWidget(nullptr);
		PanelWidgetComponent->SetVisibility(false, true);
		PanelWidgetComponent->SetHiddenInGame(true);
	}
#else
	if (PanelWidgetComponent)
	{
		if (PanelWidgetClass)
		{
			PanelWidgetComponent->SetWidgetClass(PanelWidgetClass);
			PanelWidgetComponent->SetVisibility(true, true);
			PanelWidgetComponent->SetHiddenInGame(false);
		}
		else
		{
			PanelWidgetComponent->SetWidget(nullptr);
		}
	}
#endif
}


void AInteractiveActor::BeginPlay()
{
	Super::BeginPlay();

	// --- 외곽선 DMI 생성 로직 ---
	if (MeshComponent)
	{
		UMaterialInterface* OverlayMaterial = MeshComponent->GetOverlayMaterial();
		if (OverlayMaterial)
		{
			// 오버레이 머티리얼을 기반으로 DMI를 생성
			OutlineDMI = UMaterialInstanceDynamic::Create(OverlayMaterial, this);
			// 생성된 DMI를 오버레이 슬롯에 다시 지정
			MeshComponent->SetOverlayMaterial(OutlineDMI);
		}
	}

	UpdateOutlineVisibility(); // 시작 시 상태에 맞춰 한번 업데이트

#if !UE_SERVER
	if (PanelWidgetComponent)
	{
		UUserWidget* W = PanelWidgetComponent->GetUserWidgetObject();
		if (W)
		{
			OnWidgetInitialized(W);
		}
	}
#endif
}

void AInteractiveActor::UpdateOutlineVisibility()
{
	// BeginPlay에서 이미 생성하고 저장해 둔 DMI의 파라미터 값만 변경.
	if (OutlineDMI)
	{
		const float OpacityValue = bIsBroken ? 1.0f : 0.0f;
		OutlineDMI->SetScalarParameterValue(OutlineParameterName, OpacityValue);
	}
}

void AInteractiveActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AInteractiveActor::StopRepair() { }

UUserWidget* AInteractiveActor::GetPanelWidget() const
{
	return PanelWidgetComponent ? PanelWidgetComponent->GetUserWidgetObject() : nullptr;
}

void AInteractiveActor::Server_StopRepair_Implementation(APlayerCharacter* By)
{
	if (!bInQTEMode) return;

	GetWorldTimerManager().ClearTimer(TimerPrompt);
	GetWorldTimerManager().ClearTimer(TimerTimeout);

	bInQTEMode = false;


	// 모든 클라에서 몽타주 정지
	if (By) By->Multicast_PlayRepairMontage(false);

	if (QTEOwnerPC.IsValid())
		Client_EndQTE();

	OnRep_InQTEMode(); // 패널 복귀
}

void AInteractiveActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AInteractiveActor, bIsBroken);
	DOREPLIFETIME(AInteractiveActor, RepairProgress);
	DOREPLIFETIME(AInteractiveActor, bInQTEMode);
}

void AInteractiveActor::OnRep_IsBroken()
{
	UpdateOutlineVisibility();
	BP_OnBrokenChanged(bIsBroken);
}

void AInteractiveActor::OnRep_RepairProgress()
{
	if (!HasActorBegunPlay())
		return;

	OnRepairProgressUpdated(RepairProgress); // 기존 BP 이벤트 재사용
}

void AInteractiveActor::SetIsBroken(bool bNew)
{
	if (!HasAuthority()) return;
	bIsBroken = bNew;
	if (!bIsBroken) { RepairProgress = 0.f; bInQTEMode = false; }
	OnRep_IsBroken();
}

void AInteractiveActor::StartRepair()
{
	// 클라에서 호출됨 → 서버로 요청
	if (auto* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		Server_RequestStartRepair(Cast<APlayerCharacter>(PC->GetPawn()));
	}
}

void AInteractiveActor::Server_RequestStartRepair_Implementation(APlayerCharacter* By)
{
	if (!HasAuthority() || !bIsBroken || bInQTEMode) return;

	QTEOwnerPC = By ? Cast<APlayerController>(By->GetController()) : nullptr;
	if (!QTEOwnerPC.IsValid()) return;

	SetOwner(QTEOwnerPC.Get());

	bInQTEMode = true;
	RepairProgress = 0.f;

    // 모든 클라이언트에 몽타주 재생
    if (By) By->Multicast_PlayRepairMontage(true);

    // 소유자 입력 잠금/연출
    Client_BeginQTE(QTEOwnerPC.Get());

    // 패널 가리기: OnRep_InQTEMode로 모두 적용
    OnRep_InQTEMode();

    ScheduleNextPrompt();
}

void AInteractiveActor::ScheduleNextPrompt()
{
	GetWorldTimerManager().SetTimer(TimerPrompt, this, &AInteractiveActor::IssuePrompt, QTE.PromptInterval, false);
}

void AInteractiveActor::IssuePrompt()
{
	if (!HasAuthority() || !bInQTEMode) return;
	if (QTE.Keys.Num() == 0) return;

	CurrentPromptKey = QTE.Keys[FMath::RandHelper(QTE.Keys.Num())];

	if (APlayerController* PC = QTEOwnerPC.Get())
	{
		// 남아있을 수 있는 이전 프롬프트 제거
		Client_ClearPrompt();
		// 새 프롬프트 표시
		Client_ShowPrompt(CurrentPromptKey, QTE.PromptTimeout);
	}

	GetWorldTimerManager().SetTimer(TimerTimeout, this, &AInteractiveActor::ApplyFail, QTE.PromptTimeout, false);
}


void AInteractiveActor::Server_SubmitQTEInput_Implementation(FKey Pressed)
{
	UE_LOG(LogTemp, Warning, TEXT("[Server] %s SubmitQTEInput: %s (Owner=%s)"),
		*GetName(), *Pressed.ToString(), *GetNameSafe(GetOwner()));

	if (!HasAuthority() || !bInQTEMode) return;

	GetWorldTimerManager().ClearTimer(TimerTimeout);

	if (Pressed == CurrentPromptKey) ApplySuccess();
	else                             ApplyFail();

	if (bInQTEMode) ScheduleNextPrompt(); // 완료 전이라면 다음 프롬프트 예약
}

void AInteractiveActor::ApplySuccess()
{
	RepairProgress = FMath::Clamp(RepairProgress + QTE.SuccessGain, 0.f, 1.f);

	// 소유 클라 UI 갱신
	Client_UpdateProgress(RepairProgress);

	// 현재 프롬프트 제거
	Client_ClearPrompt();

	// 다른 클라 갱신은 OnRep이 처리
	OnRep_RepairProgress();

	if (RepairProgress >= 1.f) CompleteRepair();
}

void AInteractiveActor::ApplyFail()
{
	if (!HasAuthority() || !bInQTEMode) return;

	RepairProgress = FMath::Clamp(RepairProgress - QTE.FailPenalty, 0.f, 1.f);

	// 소유 클라 UI 갱신
	Client_UpdateProgress(RepairProgress);

	// 현재 프롬프트 제거
	Client_ClearPrompt();

	OnRep_RepairProgress();

	if (bInQTEMode) ScheduleNextPrompt();
}

void AInteractiveActor::CompleteRepair()
{
	GetWorldTimerManager().ClearTimer(TimerPrompt);
	GetWorldTimerManager().ClearTimer(TimerTimeout);

	bInQTEMode = false;
	// 모든 클라에서 몽타주 정지
	if (APlayerController* PC = QTEOwnerPC.Get())
		if (auto* P = Cast<APlayerCharacter>(PC->GetPawn()))
			P->Multicast_PlayRepairMontage(false);

	// 상태 복제(고장 해제) → 외곽선/패널 처리 OnRep에서 공용 반영
	SetIsBroken(false);

	if (auto* GM = GetWorld()->GetAuthGameMode<ANSGameModeBase>())
	{
		GM->AddScore_Repair(1);
	}

	if (QTEOwnerPC.IsValid())
		Client_EndQTE();

	OnRepairCompleted.Broadcast(this);
}

void AInteractiveActor::OnRep_InQTEMode()
{
	// 수리 중엔 모든 클라에서 안내 패널 숨김, 수리 끝났고 여전히 고장 상태면 다시 표시
	if (PanelWidgetComponent)
	{
		const bool bShow = (!bInQTEMode && bIsBroken);
		PanelWidgetComponent->SetVisibility(bShow, /*propagate*/true);
		PanelWidgetComponent->SetHiddenInGame(!bShow, /*propagate*/true);
	}
}

void AInteractiveActor::Client_UpdateProgress_Implementation(float NewProgress)
{
	// BP가 진행도 Bar를 안전하게 갱신(ProgressBar가 아직 없다면 BP에서 IsValid 체크)
	OnRepairProgressUpdated(NewProgress);
}

void AInteractiveActor::Client_ClearPrompt_Implementation()
{
	// BP에서 현재 떠 있는 QTE 아이콘 RemoveFromParent 등으로 정리
	BP_OnQTEClearPrompt();
}


void AInteractiveActor::Client_BeginQTE_Implementation(APlayerController* ForPC) 
{ 
	LocalQTEPC = ForPC;

	// 입력/포커스 안전 설정 (BP에서도 하겠지만 여기서 보강)
	if (ForPC)
	{
		ForPC->SetIgnoreMoveInput(true);
		ForPC->SetIgnoreLookInput(true);

		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(true);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		Mode.SetWidgetToFocus(nullptr);   // 위젯 포커스는 BP에서 CurrentQTEWidget으로
		ForPC->SetInputMode(Mode);
	}

	// BP 연출 실행
	BP_OnQTEBegin(ForPC);

	// UI가 막 생성된 직후, 진행도 0
	OnRepairProgressUpdated(RepairProgress);
}
void AInteractiveActor::Client_ShowPrompt_Implementation(FKey Key, float Timeout) { BP_OnQTEPrompt(Key, Timeout); }
void AInteractiveActor::Client_EndQTE_Implementation()
{
	if (LocalQTEPC.IsValid())
	{
		if (APawn* Pawn = LocalQTEPC->GetPawn())
			if (auto* P = Cast<APlayerCharacter>(Pawn))
			{
				P->SetMovementInputEnabled(true);   // 로컬 플래그 복구
				P->Client_PlayRepairMontage(false); // 같은 메쉬 대상으로 정지
			}

		LocalQTEPC->SetIgnoreMoveInput(false);
		LocalQTEPC->SetIgnoreLookInput(false);
		FInputModeGameOnly Mode; LocalQTEPC->SetInputMode(Mode);
		LocalQTEPC->bShowMouseCursor = false;
	}

	Client_ClearPrompt();
	BP_OnQTEEnd();
	LocalQTEPC = nullptr;
}
