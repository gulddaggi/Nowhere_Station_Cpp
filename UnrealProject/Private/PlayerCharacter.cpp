// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/WidgetInteractionComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "InteractiveActor.h"
#include "StartRitualStatue.h"
#include "NSPlayerController.h"
#include "MopTarget.h"
#include "MemoryShardInteract.h"
#include "Kismet/KismetSystemLibrary.h"    
#include "Kismet/GameplayStatics.h"
#include "NSGameModeBase.h"


const FName APlayerCharacter::EquipSocketName(TEXT("ItemSocket"));
static const FName NAME_StartSuction(TEXT("StartSuction"));
static const FName NAME_StopSuction(TEXT("StopSuction"));
static const FName NAME_Collect(TEXT("Collect"));

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // 캐릭터의 Tick 함수 호출 여부 설정

	// 스프링암 컴포넌트 생성 및 설정
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 300.0f;
	SpringArm->bUsePawnControlRotation = true; // 컨트롤러 회전에 따라 스프링암 회전
	SpringArm->SetRelativeLocation(FVector(0, 50, 80));

	// 카메라 컴포넌트 생성 및 설정
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false; // 카메라는 스프링암을 따라 회전

	// 블루프린트가 기대하는 네이티브 컴포넌트 "WidgetInteractionComponent"
	UIWidgetInteraction = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("UIWidgetInteraction"));
	UIWidgetInteraction->SetupAttachment(Camera);
	UIWidgetInteraction->InteractionDistance = WidgetInteractionDistance;
	UIWidgetInteraction->TraceChannel = ECollisionChannel::ECC_Visibility; // 필요 시 변경
	UIWidgetInteraction->bShowDebug = false;

	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(RootComponent);
	InteractionCollision->InitSphereRadius(120.f);   // 필요 시 100~160 조절
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	InteractionCollision->SetGenerateOverlapEvents(true);

	InteractionCollision->OnComponentBeginOverlap.AddDynamic(this, &APlayerCharacter::OnInteractOverlapBegin);
	InteractionCollision->OnComponentEndOverlap.AddDynamic(this, &APlayerCharacter::OnInteractOverlapEnd);

	// 서버에서는 상호작용 처리 비활성화(타입은 유지)
	if (IsRunningDedicatedServer())
	{
		UIWidgetInteraction->Deactivate();
	}

	// 캐릭터가 컨트롤러 회전에 따라 회전하도록 설정
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true; // 이동 방향으로 회전 비활성화

	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);

	// 기본 걷기 속도 설정
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	// 기본적으로 이동 가능하도록 설정
	bIsMovementInputEnabled = true;

	// 네트워크 관련 기본값
	bReplicates = true;
	SetNetUpdateFrequency(100.f);
	SetMinNetUpdateFrequency(30.f);

	if (auto* Move = GetCharacterMovement())
	{
		Move->SetIsReplicated(true);                   // 이동 컴포넌트 복제 명시
		Move->NetworkSmoothingMode = ENetworkSmoothingMode::Linear;
		Move->MaxWalkSpeed = WalkSpeed;                // 시작 속도
	}

	VacuumMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VacuumMeshComp"));
	MopMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MopMeshComp"));

	VacuumMesh->SetupAttachment(GetRootComponent());
	MopMesh->SetupAttachment(GetRootComponent());

	if (VacuumMesh) { VacuumMesh->SetVisibility(false, false); VacuumMesh->SetHiddenInGame(true, false); VacuumMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	if (MopMesh) { MopMesh->SetVisibility(false, false);    MopMesh->SetHiddenInGame(true, false);    MopMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); }

	VacuumCollision = CreateDefaultSubobject<USphereComponent>(TEXT("VacuumCollision"));
	VacuumCollision->SetupAttachment(GetRootComponent());
	VacuumCollision->InitSphereRadius(VacuumOverlapRadius);
	VacuumCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 기본은 꺼둠
	VacuumCollision->SetCollisionObjectType(ECC_WorldDynamic);
	VacuumCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	VacuumCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	VacuumCollision->OnComponentBeginOverlap.AddDynamic(this, &APlayerCharacter::OnVacuumOverlapBegin);
	VacuumCollision->OnComponentEndOverlap.AddDynamic(this, &APlayerCharacter::OnVacuumOverlapEnd);
	RefreshVacuumFieldTransform();
}

void APlayerCharacter::RefreshVacuumFieldTransform()
{
	if (VacuumCollision)
	{
		// 소켓 로컬 전방(+X)으로 오프셋
		VacuumCollision->SetRelativeLocation(FVector(VacuumForwardOffset, 0.f, 0.f));
		VacuumCollision->SetSphereRadius(VacuumSphereRadius);
	}
}

bool APlayerCharacter::IsVacuumEquipped() const
{
	return CurrentEquip == EEquipmentType::Vacuum;
}

bool APlayerCharacter::IsVacuumActive() const
{
	return bActionLocked && IsVacuumEquipped();
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (IsRunningDedicatedServer() && UIWidgetInteraction)
	{
		UIWidgetInteraction->Deactivate();
	}
	else if (UIWidgetInteraction)
	{
		UIWidgetInteraction->InteractionDistance = WidgetInteractionDistance;
	}

	// Enhanced Input Subsystem에 매핑 컨텍스트 추가
	if (APlayerController* PlayerContoller = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem
			= ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerContoller->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(PlayerMappingContext, 0);
		}
	}

	// 본체는 항상 보이도록 강제(사라짐 방지)
	USkeletalMeshComponent* BodySkel = EquipAttachTarget ? EquipAttachTarget.Get() : GetMesh();
	if (BodySkel)
	{
		BodySkel->SetHiddenInGame(false);
		BodySkel->SetVisibility(true);
		BodySkel->SetOwnerNoSee(false);
		BodySkel->SetOnlyOwnerSee(false);
	}

	AttachEquipMeshesToSocket();
	ApplyEquipVisuals();
	ApplyCurrentEquipOffset();
}

FTransform APlayerCharacter::GetOffsetForEquip(EEquipmentType Type) const
{
	if (const FTransform* T = EquipOffsets.Find(Type)) return *T;
	return FTransform::Identity;
}

void APlayerCharacter::AttachEquipMeshesToSocket()
{
	if (GetNetMode() == NM_DedicatedServer) return;

	// 부착 대상 스켈레탈(Body -> 없으면 GetMesh)
	USkeletalMeshComponent* Skel = EquipAttachTarget ? EquipAttachTarget.Get() : nullptr;
	if (!Skel)
	{
		// 이름이 Body인 스켈레탈 자동 탐색
		TArray<USkeletalMeshComponent*> AllSkels;
		GetComponents(AllSkels);
		for (auto* S : AllSkels)
			if (S && S->GetName().Equals(TEXT("Body"))) { Skel = S; break; }
	}
	if (!Skel) Skel = GetMesh();
	if (!Skel || !Skel->GetSkeletalMeshAsset()) return;

	// 두 메시를 소켓에 직접 부착
	auto Attach = [&](UStaticMeshComponent* C)
		{
			if (!C) return;
			C->AttachToComponent(
				Skel,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				EquipSocketName // "ItemSocket"
			);
		};
	Attach(VacuumMesh);
	Attach(MopMesh);

	if (VacuumCollision && Skel)
	{
		// 안전하게 재부착
		VacuumCollision->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		const FAttachmentTransformRules Rules(
			EAttachmentRule::SnapToTarget,   // 위치
			EAttachmentRule::SnapToTarget,   // 회전
			EAttachmentRule::SnapToTarget,   // 스케일
			true
		);
		VacuumCollision->AttachToComponent(Skel, Rules, EquipSocketName); // "ItemSocket"
		RefreshVacuumFieldTransform(); // 반경/오프셋 재적용
	}
}

void APlayerCharacter::ApplyCurrentEquipOffset()
{
	if (GetNetMode() == NM_DedicatedServer) return;

	// 기본값(아이덴티티)로 초기화
	if (VacuumMesh) VacuumMesh->SetRelativeTransform(FTransform::Identity, false, nullptr, ETeleportType::TeleportPhysics);
	if (MopMesh)    MopMesh->SetRelativeTransform(FTransform::Identity, false, nullptr, ETeleportType::TeleportPhysics);

	// 현재 장비만 오프셋 적용
	const FTransform Off = GetOffsetForEquip(CurrentEquip);
	switch (CurrentEquip)
	{
	case EEquipmentType::Vacuum: if (VacuumMesh) VacuumMesh->SetRelativeTransform(Off, false, nullptr, ETeleportType::TeleportPhysics); break;
	case EEquipmentType::Mop:    if (MopMesh)    MopMesh->SetRelativeTransform(Off, false, nullptr, ETeleportType::TeleportPhysics); break;
	default: break; // None
	}
}

void APlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 전용서버에서 렌더/소켓 작업 불필요
	if (GetNetMode() == NM_DedicatedServer) return;
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input Component로 캐스팅하여 입력 액션 바인딩
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &APlayerCharacter::StartSprinting);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopSprinting);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &APlayerCharacter::InteractPressed);

		EnhancedInputComponent->BindAction(EquipSlot1Action, ETriggerEvent::Started, this, &APlayerCharacter::Input_EquipSlot1);
		EnhancedInputComponent->BindAction(EquipSlot2Action, ETriggerEvent::Started, this, &APlayerCharacter::Input_EquipSlot2);
		EnhancedInputComponent->BindAction(CleanAction, ETriggerEvent::Started, this, &APlayerCharacter::Input_CleanStarted);
		EnhancedInputComponent->BindAction(CleanAction, ETriggerEvent::Completed, this, &APlayerCharacter::Input_CleanEnded);
		EnhancedInputComponent->BindAction(CleanAction, ETriggerEvent::Canceled, this, &APlayerCharacter::Input_CleanEnded);
		EnhancedInputComponent->BindAction(MenuAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Input_Menu);
	}

	if (!EquipSlot1Action || !EquipSlot2Action) {
		UE_LOG(LogTemp, Warning, TEXT("Equip actions are not assigned!"));
	}

}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
	if (!IsLocallyControlled()) return;
	if (!bIsMovementInputEnabled) return; // 이동이 비활성화 상태면 함수를 즉시 종료

	const FVector2D MoveVector = Value.Get<FVector2D>();

	if (GetController())
	{
		const FRotator ControlRotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, ControlRotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MoveVector.Y);
		AddMovementInput(RightDirection, MoveVector.X);
	}
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
	if (!IsLocallyControlled()) return;
	if (!bIsMovementInputEnabled) return; // 카메라 조작이 비활성화 상태면 함수를 즉시 종료

	const FVector2D LookVector = Value.Get<FVector2D>();
	if (GetController())
	{
		AddControllerYawInput(LookVector.X);
		AddControllerPitchInput(LookVector.Y);
	}
}

void APlayerCharacter::StartSprinting()
{
	if (!IsLocallyControlled()) return;

	bIsSprinting = true;
	UpdateMovementSpeedFromState();

	if (!HasAuthority())
	{
		ServerSetSprinting(true);
	}
}

void APlayerCharacter::StopSprinting()
{
	if (!IsLocallyControlled()) return;

	bIsSprinting = false;
	UpdateMovementSpeedFromState();

	if (!HasAuthority())
	{
		ServerSetSprinting(false);
	}
}

void APlayerCharacter::InteractPressed()
{
	if (!IsLocallyControlled()) return;

	FHitResult Hit;
	AActor* Target = Client_FindBestInteractable(&Hit);

	if (!Target)
	{
		CurrentInteractable = nullptr;
		return;
	}

	LastInteractTarget = Target;

	// 여신상: 준비/취소 토글 → PC.Server RPC 경유 (이미 구현)
	if (AStartRitualStatue* Statue = Cast<AStartRitualStatue>(Target))
	{
		if (ANSPlayerController* PC = Cast<ANSPlayerController>(GetController()))
		{
			static bool bLocalReady = false;
			bLocalReady = !bLocalReady;
			PC->Server_ToggleReady(bLocalReady); // 서버 집계로 전달
		}
		return;
	}

	// 수리 가능 오브젝트: 서버에 "수리 시작" 요청
	if (AInteractiveActor* IA = Cast<AInteractiveActor>(Target))
	{
		CurrentInteractable = IA;

		// 조건: 고장 상태 & QTE 모드 아님
		if (!IA->IsInQTEMode() && IA->IsBroken())
		{
			Server_TryStartRepair(IA);          
			SetMovementInputEnabled(false);
			PlayRepairAnimation(true);
		}
	}
}

AActor* APlayerCharacter::Client_FindBestInteractable(FHitResult* OutHit) const
{
	if (!IsLocallyControlled()) return nullptr;

	// 카메라 기준 시작/방향/끝점
	const FVector Start = Camera ? Camera->GetComponentLocation() : GetActorLocation();
	const FVector Fwd = Camera ? Camera->GetForwardVector() : GetActorForwardVector();
	const FVector End = Start + Fwd * InteractDistance;

	// 구(스피어) 스윕 멀티
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractSweep), false, this);
	FCollisionShape Shape = FCollisionShape::MakeSphere(InteractSphereRadius);

	GetWorld()->SweepMultiByChannel(
		Hits, Start, End, FQuat::Identity,
		ECC_Visibility, Shape, Params
	);

	// 후보 수집 + 필터(클래스/인터페이스)
	struct FCandidate { AActor* A = nullptr; FVector P; float Dist = 0, Cos = 0, Score = 0; FHitResult Hit; };
	TArray<FCandidate> Cands;

	auto Consider = [&](const FHitResult& H)
		{
			AActor* A = H.GetActor();
			if (!A) return;

			// 여신상/수리물만 후보로. 인터페이스(UInteractable)가 있다면 그걸로 체크해도 좋음.
			const bool bOk = A->IsA(AStartRitualStatue::StaticClass()) || A->IsA(AInteractiveActor::StaticClass());
			if (!bOk) return;

			const FVector ImpactPt = FVector(H.ImpactPoint);
			FVector Pt = A->GetActorLocation();
			if (H.bBlockingHit)
			{
				Pt = ImpactPt;
			}

			const FVector Dir = (Pt - Start).GetSafeNormal();
			const float   Cos = FVector::DotProduct(Fwd, Dir);
			const float   Ang = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cos, -1.f, 1.f)));

			if (Ang > InteractConeHalfAngleDeg) return; // 콘 밖이면 제외


			FCandidate C;
			C.A = A;
			C.P = Pt;
			C.Dist = (Pt - Start).Size();
			C.Cos = Cos;

			// 점수: 중앙 정렬 + 가까움 + 스티키 보정
			const float DistNorm = 1.f - (C.Dist / FMath::Max(50.f, InteractDistance)); // 가까울수록 큼
			const float Sticky = (A == LastInteractTarget.Get()) ? 0.15f : 0.f;
		 C.Score = C.Cos * 0.70f + DistNorm * 0.30f + Sticky;   // 가중치는 프로젝트 감으로 조절
			C.Hit = H;
			Cands.Add(C);
		};

	for (const FHitResult& H : Hits) Consider(H);

	if (Cands.Num() == 0)
	{
		// 스윕이 비었으면, 얇은 라인트레이스로 마지막 시도(하위 호환)
		FHitResult H;
		if (GetWorld()->LineTraceSingleByChannel(H, Start, End, ECC_Visibility, Params))
			Consider(H);
	}

	// 최종 선택
	FCandidate* Best = nullptr;
	for (FCandidate& C : Cands)
	{
		if (!Best || C.Score > Best->Score) Best = &C;
	}

	if (Best)
	{
		if (OutHit) *OutHit = Best->Hit;
		return Best->A;
	}
	return nullptr;
}

void APlayerCharacter::InteractReleased()
{
}

// 서버에서 직접 대상 액터 호출
void APlayerCharacter::Server_TryStartRepair_Implementation(AInteractiveActor* Target)
{
	UE_LOG(LogTemp, Log, TEXT("Server_TryStartRepair: Target=%s"), *GetNameSafe(Target));
	if (Target && Target->IsBroken() && !Target->IsInQTEMode())
	{
		Target->Server_RequestStartRepair(this);
	}
}


void APlayerCharacter::Server_TryStopRepair_Implementation(AInteractiveActor* Target)
{
	if (Target) Target->Server_StopRepair(this);
}

void APlayerCharacter::PlayRepairAnimation(bool bIsPlaying)
{
	if (!MainMeshComponent) return;

	UAnimInstance* AnimInstance = MainMeshComponent->GetAnimInstance();

	if (AnimInstance && RepairMontage)
	{
		if (bIsPlaying)
		{
			if (!AnimInstance->Montage_IsPlaying(RepairMontage))
			{
				AnimInstance->Montage_Play(RepairMontage);
			}
		}
		else
		{
			AnimInstance->Montage_Stop(0.25f, RepairMontage);
		}
	}
}

void APlayerCharacter::SetMovementInputEnabled(bool bIsEnabled)
{
	bIsMovementInputEnabled = bIsEnabled;
}

// 복제 등록
void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APlayerCharacter, bIsSprinting);
	DOREPLIFETIME(APlayerCharacter, CurrentEquip);
	DOREPLIFETIME(APlayerCharacter, CleanState);
	DOREPLIFETIME(APlayerCharacter, bActionLocked);
	DOREPLIFETIME(APlayerCharacter, CleaningTarget);
}

// 상태->속도 적용(서버/클라 공용)
void APlayerCharacter::UpdateMovementSpeedFromState()
{
	if (auto* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = bIsSprinting ? RunSpeed : WalkSpeed;
	}
}

// RepNotify: 서버에서 bIsSprinting 바뀌면 클라가 여기서 속도 반영
void APlayerCharacter::OnRep_IsSprinting()
{
	UpdateMovementSpeedFromState();
}

// 서버 RPC: 클라 입력을 서버에 요청
void APlayerCharacter::ServerSetSprinting_Implementation(bool bNewIsSprinting)
{
	bIsSprinting = bNewIsSprinting;
	UpdateMovementSpeedFromState();
}

void APlayerCharacter::Client_PlayRepairMontage_Implementation(bool bPlay)
{
	USkeletalMeshComponent* MeshComp =
		(MainMeshComponent && MainMeshComponent.Get())
		? MainMeshComponent.Get()
		: GetMesh();

	UAnimInstance* Anim = MeshComp ? MeshComp->GetAnimInstance() : nullptr;

	if (Anim && RepairMontage)
	{
		if (bPlay) Anim->Montage_Play(RepairMontage);
		else       Anim->Montage_Stop(0.2f, RepairMontage);
	}
}

void APlayerCharacter::Multicast_PlayRepairMontage_Implementation(bool bPlay)
{
	// 이미 쓰는 메쉬로 통일(MainMeshComponent)
	UAnimInstance* Anim = (MainMeshComponent ? MainMeshComponent->GetAnimInstance() : nullptr);
	if (!Anim || !RepairMontage) return;

	if (bPlay)  Anim->Montage_Play(RepairMontage);
	else        Anim->Montage_Stop(0.25f, RepairMontage);
}

void APlayerCharacter::Input_EquipSlot1()
{
	if (!IsLocallyControlled()) return;
	if (bActionLocked) return; // 청소 중 등 액션 잠금이면 무시

	// 현재 Vacuum이면 해제(None), 아니면 Vacuum 장착
	const EEquipmentType Next = (CurrentEquip == EEquipmentType::Vacuum)
		? EEquipmentType::None
		: EEquipmentType::Vacuum;

	Server_SetEquip(Next);
}

void APlayerCharacter::Input_EquipSlot2()
{
	if (!IsLocallyControlled()) return;
	if (bActionLocked) return;

	const EEquipmentType Next = (CurrentEquip == EEquipmentType::Mop)
		? EEquipmentType::None
		: EEquipmentType::Mop;

	Server_SetEquip(Next);
}

void APlayerCharacter::Server_SetEquip_Implementation(EEquipmentType NewEquip)
{
	if (bActionLocked) return;
	if (NewEquip == CurrentEquip) return; 

	EEquipmentType Prev = CurrentEquip;
	CurrentEquip = NewEquip;

	ApplyEquipVisuals();
	ApplyCurrentEquipOffset();
	BP_OnEquipChanged(CurrentEquip, Prev);
	ForceNetUpdate();
}

void APlayerCharacter::OnRep_CurrentEquip()
{
	ApplyEquipVisuals();          
	ApplyCurrentEquipOffset();    
	BP_OnEquipChanged(CurrentEquip, EEquipmentType::None);
}

void APlayerCharacter::ApplyEquipVisuals()
{
	const bool bVac = (CurrentEquip == EEquipmentType::Vacuum);
	const bool bMop = (CurrentEquip == EEquipmentType::Mop);

	auto Show = [](UStaticMeshComponent* C, bool bEnable)
		{
			if (!C) return;
			C->SetVisibility(bEnable, false);
			C->SetHiddenInGame(!bEnable, false);
			C->SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			C->SetOnlyOwnerSee(false);
			C->SetOwnerNoSee(false);
			C->MarkRenderStateDirty();
		};

	Show(VacuumMesh, bVac);
	Show(MopMesh, bMop);
}

// 청소 입력 -> 서버 요청
void APlayerCharacter::Input_CleanStarted()
{
	// 장비가 Vacuum이면 Vacuum 시작
	if (CurrentEquip == EEquipmentType::Vacuum)
	{
		Server_BeginVacuum();
		return;
	}

	// Mop 타깃 우선 분기
	if (ClientMopCandidate.IsValid())
	{
		Server_BeginCleanWithTarget(ClientMopCandidate.Get());
	}
	else
	{
		Server_BeginClean();
	}
}

void APlayerCharacter::Input_Menu()
{
	if (ANSPlayerController* PlayerContoller = Cast<ANSPlayerController>(GetController()))
	{
		PlayerContoller->TogglePauseMenu();
	}
}

void APlayerCharacter::Input_CleanEnded()
{
	// Vacuum 중이면 Vacuum 종료
	if (CleanState == ECleanState::Vacuuming)
	{
		Server_EndVacuum();
		return;
	}

	Server_EndClean();
}

void APlayerCharacter::Server_BeginClean_Implementation()
{
	if (CleanState != ECleanState::None || bActionLocked) return;
	if (CurrentEquip != EEquipmentType::Mop) return;

	AActor* Target = Server_FindMopTarget();
	if (!Target) return;



	MopTarget = Target;
	if (Target && Target->GetClass()->ImplementsInterface(UMopTarget::StaticClass()))
	{
		IMopTarget::Execute_Server_BeginMop(Target, this);
	}

	CleanState = ECleanState::Mopping;
	bActionLocked = true;

	// 이동/몽타주는 RepNotify/BP에서 처리해도 되고 여기서 해도 됨
	OnRep_CleanState();

	// 연출(애니/사운드)을 위해 StainType 전달
	EStainType T = EStainType::None; // 권장: None 추가해서 미세팅 상태를 식별
	if (Target && Target->GetClass()->ImplementsInterface(UMopTarget::StaticClass()))
	{
		T = IMopTarget::Execute_GetStainType(Target);
	}

	Multicast_MopStart(T);

	UE_LOG(LogTemp, Log, TEXT("[MOP] Begin target=%s Type=%s"),
		*GetNameSafe(Target), *UEnum::GetValueAsString(T));
	UE_LOG(LogTemp, Log, TEXT("[MOP] Target=%s Type=%s"),
		*GetNameSafe(MopTarget), *UEnum::GetValueAsString(IMopTarget::Execute_GetStainType(MopTarget)));
	GetWorldTimerManager().SetTimer(MopTickHandle, this, &APlayerCharacter::Server_MopTick, 0.05f, true);
	ForceNetUpdate();
}

void APlayerCharacter::Server_EndClean_Implementation()
{
	if (CleanState != ECleanState::Mopping) return;

	UE_LOG(LogTemp, Log, TEXT("[MOP] End"));

	if (MopTarget && MopTarget->GetClass()->ImplementsInterface(UMopTarget::StaticClass()))
	{
		IMopTarget::Execute_Server_EndMop(MopTarget);
	}
	MopTarget = nullptr;

	GetWorldTimerManager().ClearTimer(MopTickHandle);

	CleanState = ECleanState::None;
	bActionLocked = false;

	OnRep_CleanState();
	Multicast_MopStop();
	ForceNetUpdate();
}

void APlayerCharacter::Server_MopTick()
{
	if (CleanState != ECleanState::Mopping || !MopTarget)
	{
		UE_LOG(LogTemp, Log, TEXT("[MOP] Early end"));
		Server_EndClean();
		return;
	}

	bool bDone = false;
	const bool bIntf = MopTarget->GetClass()->ImplementsInterface(UMopTarget::StaticClass());
	if (bIntf) { bDone = IMopTarget::Execute_Server_MopAdvance(MopTarget, 0.05f); }
	UE_LOG(LogTemp, Log, TEXT("[MOP] Tick target=%s intf=%d done=%d"), *GetNameSafe(MopTarget), bIntf, bDone);

	if (bDone)
	{
		if (ANSGameModeBase* GM = GetWorld()->GetAuthGameMode<ANSGameModeBase>())
		{
			GM->AddScore_Stain(1);
		}
		Server_EndClean();
	}
}

void APlayerCharacter::Multicast_MopStart_Implementation(EStainType StainType)
{
	// 코스메틱(모든 클라) ⇒ 타입별 몽타주 + SFX
	PlayMopCosmetics(StainType);

	// BP에서 StainType으로 몽타주 분기(벽/바닥/오브젝트) – 기존 그래프 그대로
	BP_OnMopStarted(StainType); // 없으면 생략
}

void APlayerCharacter::Multicast_MopStop_Implementation()
{
	StopMopCosmetics();
}

void APlayerCharacter::OnRep_CleanState()
{
	// 이동 잠금 같은 로컬 적용이 있으면 여기서
	SetMovementInputEnabled(CleanState == ECleanState::None);

	// BP 연출(Montage/SFX 로직을 이 이벤트에 연결)
	BP_OnCleanStateChanged(CleanState);
}

AActor* APlayerCharacter::Server_FindMopTarget() const
{
	const FVector Center = GetActorLocation() + GetActorForwardVector() * 120.f;

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjTypes;
	ObjTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	TArray<AActor*> Ignore; Ignore.Add(const_cast<APlayerCharacter*>(this));
	TArray<AActor*> OutActors;

	UKismetSystemLibrary::SphereOverlapActors(
		this, Center, 80.f, ObjTypes, /*ClassFilter=*/nullptr, Ignore, OutActors);

	AActor* Best = nullptr; float BestD2 = TNumericLimits<float>::Max();
	for (AActor* A : OutActors)
	{
		if (!A || !A->GetClass()->ImplementsInterface(UMopTarget::StaticClass())) continue;
		const float D2 = FVector::DistSquared(A->GetActorLocation(), Center);
		if (D2 < BestD2) { Best = A; BestD2 = D2; }
	}
	return Best;
}

void APlayerCharacter::PlayMopCosmetics(EStainType T)
{
	if (GetNetMode() == NM_DedicatedServer) return;

	USkeletalMeshComponent* PMesh = MainMeshComponent ? MainMeshComponent.Get() : GetMesh();
	if (PMesh)
	{
		// 타입별 몽타주
		if (UAnimInstance* Anim = PMesh->GetAnimInstance())
		{
			if (UAnimMontage* M = MopMontages.FindRef(T))
			{
				Anim->Montage_Play(M);
			}
		}

		// 시작 사운드(루프면 FadeOut으로 정리)
		if (MopStartSFX)
		{
			MopAudio = UGameplayStatics::SpawnSoundAttached(
				MopStartSFX, PMesh, NAME_None, FVector::ZeroVector, EAttachLocation::SnapToTarget);
		}
	}
}

void APlayerCharacter::StopMopCosmetics()
{
	if (GetNetMode() == NM_DedicatedServer) return;

	// 오디오 정리
	if (MopAudio)
	{
		MopAudio->FadeOut(0.3f, 0.f);
		MopAudio = nullptr;
	}

	// 몽타주 정리
	if (USkeletalMeshComponent* PMesh = (MainMeshComponent ? MainMeshComponent.Get() : GetMesh()))
	{
		if (UAnimInstance* Anim = PMesh->GetAnimInstance())
		{
			Anim->StopAllMontages(0.25f);
		}
	}
}

void APlayerCharacter::OnInteractOverlapBegin(
	UPrimitiveComponent* /*OverlappedComp*/, AActor* Other, UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*Sweep*/)
{
	if (!Other) return;
	if (!Other->GetClass()->ImplementsInterface(UMopTarget::StaticClass())) return;

	OverlappingMopTargets.AddUnique(Other);
	ClientMopCandidate = PickNearestMopCandidate();
}

void APlayerCharacter::OnInteractOverlapEnd(
	UPrimitiveComponent* /*OverlappedComp*/, AActor* Other, UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/)
{
	if (!Other) return;
	OverlappingMopTargets.Remove(Other);
	ClientMopCandidate = PickNearestMopCandidate();
}

AActor* APlayerCharacter::PickNearestMopCandidate() const
{
	const FVector P = GetActorLocation();
	AActor* Best = nullptr; float BestD2 = TNumericLimits<float>::Max();

	for (const TWeakObjectPtr<AActor>& W : OverlappingMopTargets)
	{
		AActor* A = W.Get();
		if (!A) continue;
		const float D2 = FVector::DistSquared(P, A->GetActorLocation());
		if (D2 < BestD2) { Best = A; BestD2 = D2; }
	}
	return Best;
}

void APlayerCharacter::Server_BeginCleanWithTarget_Implementation(AActor* InTarget)
{
	if (CleanState != ECleanState::None || bActionLocked) return;
	if (CurrentEquip != EEquipmentType::Mop) return;

	AActor* Target = nullptr;

	// 빠른 재검증: 인터페이스 + 실제 오버랩(or 근접) 확인
	if (InTarget && InTarget->GetClass()->ImplementsInterface(UMopTarget::StaticClass()))
	{
		const bool bOverlapOK =
			(InteractionCollision && InteractionCollision->IsOverlappingActor(InTarget)) ||
			(FVector::DistSquared(InTarget->GetActorLocation(), GetActorLocation()) <= FMath::Square(150.f));
		if (bOverlapOK)
		{
			Target = InTarget;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[MOP] Target=%s Type=%s"),
		*GetNameSafe(Target), *UEnum::GetValueAsString(IMopTarget::Execute_GetStainType(Target)));

	// 실패 시 서버가 다시 찾기 (안전장치)
	if (!Target) Target = Server_FindMopTarget();
	if (!Target) return;

	MopTarget = Target;

	// 타깃에게 걸레질 시작 알림
	IMopTarget::Execute_Server_BeginMop(Target, this);

	CleanState = ECleanState::Mopping;
	bActionLocked = true;
	OnRep_CleanState(); // 로컬 이동잠금 등 반영

	// 타입별 연출 브로드캐스트
	EStainType T = IMopTarget::Execute_GetStainType(Target);
	Multicast_MopStart(T);

	// 서버 틱
	const float Rate = 0.05f;
	GetWorldTimerManager().SetTimer(MopTickHandle, this, &APlayerCharacter::Server_MopTick, Rate, true);

	ForceNetUpdate();
}

void APlayerCharacter::Server_BeginVacuum_Implementation()
{
	if (CleanState != ECleanState::None || bActionLocked) return;
	if (CurrentEquip != EEquipmentType::Vacuum) return;

	CleanState = ECleanState::Vacuuming;
	bActionLocked = true;

	Multicast_VacuumStart();     

	if (VacuumCollision)
	{
		VacuumCollision->SetSphereRadius(VacuumOverlapRadius);
		VacuumCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		VacuumCollision->SetRelativeLocation(FVector(VacuumForwardOffset, 0.f, 0.f));
	}

	// 20Hz 정도로 주기 오버랩
	GetWorldTimerManager().SetTimer(
		VacuumTickHandle, this, &APlayerCharacter::Server_VacuumTick, 0.05f, true);
}

void APlayerCharacter::Server_EndVacuum_Implementation()
{
	if (CleanState != ECleanState::Vacuuming) return;

	CleanState = ECleanState::None;
	bActionLocked = false;

	GetWorldTimerManager().ClearTimer(VacuumTickHandle);

	// 흡수 중 표시된 샤드 모두 StopSuction
	for (auto W : ActiveVacuumSet)
		if (AActor* A = W.Get())
		{
			if (A->GetClass()->ImplementsInterface(UMemoryShardInteract::StaticClass()))
			{
				IMemoryShardInteract::Execute_StopSuction(A);
				Multicast_StopSuction(A);
			}
		}
	ActiveVacuumSet.Empty();

	if (VacuumCollision)
		VacuumCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Multicast_VacuumStop();
	ForceNetUpdate();
}

void APlayerCharacter::Server_VacuumTick()
{
	if (!HasAuthority() || CleanState != ECleanState::Vacuuming) return;

	TArray<TWeakObjectPtr<AActor>> ToRemove;

	for (auto W : ActiveVacuumSet)
	{
		AActor* A = W.Get();
		if (!A) { ToRemove.Add(W); continue; }

		const float D2 = FVector::DistSquared(A->GetActorLocation(), GetActorLocation());
		if (D2 <= FMath::Square(VacuumCollectDist))
		{
			// 서버에서 실제 Collect 실행
			IMemoryShardInteract::Execute_Collect(A, this);
			Multicast_Collect(A);
			ToRemove.Add(W);
		}
	}

	for (auto& W : ToRemove)
		ActiveVacuumSet.Remove(W);
}

void APlayerCharacter::Multicast_VacuumStart_Implementation()
{
	PlayVacuumCosmetics(true);
}

void APlayerCharacter::Multicast_VacuumStop_Implementation()
{
	PlayVacuumCosmetics(false);
}

void APlayerCharacter::PlayVacuumCosmetics(bool bStart)
{
	if (GetNetMode() == NM_DedicatedServer) return;

	USkeletalMeshComponent* PMesh = MainMeshComponent ? MainMeshComponent.Get() : GetMesh();
	if (!PMesh) return;

	if (UAnimInstance* Anim = PMesh->GetAnimInstance())
	{
		if (bStart && VacuumMontage)
			Anim->Montage_Play(VacuumMontage);
		else
			Anim->StopAllMontages(0.25f);
	}

	if (bStart)
	{
		if (VacuumLoopSFX && !VacuumAudio)
			VacuumAudio = UGameplayStatics::SpawnSoundAttached(VacuumLoopSFX, PMesh);
	}
	else
	{
		if (VacuumAudio)
		{
			VacuumAudio->FadeOut(0.3f, 0.f);
			VacuumAudio = nullptr;
		}
	}
}

void APlayerCharacter::OnVacuumOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit)
{
	if (!HasAuthority()) return;
	if (!IsVacuumActive()) return;
	if (!IsValid(Other) || Other == this) return;

	// 인터페이스 구현 여부만 확인
	if (!Other->GetClass()->ImplementsInterface(UMemoryShardInteract::StaticClass()))
		return;

	if (ActiveVacuumSet.Contains(Other)) return;
	ActiveVacuumSet.Add(Other);

	// 서버 로직 + 코스메틱 동기화
	IMemoryShardInteract::Execute_StartSuction(Other, this);
	Multicast_StartSuction(Other);
}

void APlayerCharacter::OnVacuumOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 BodyIndex)
{
	if (!HasAuthority()) return;
	if (ActiveVacuumSet.Contains(Other)) return;
	if (!IsValid(Other) || Other == this) return;

	if (!Other->GetClass()->ImplementsInterface(UMemoryShardInteract::StaticClass()))
		return;

	if (ActiveVacuumSet.Remove(Other) > 0 &&
		Other->GetClass()->ImplementsInterface(UMemoryShardInteract::StaticClass()))
	{
		IMemoryShardInteract::Execute_StopSuction(Other);
	}
	Multicast_StopSuction(Other);
}

void APlayerCharacter::Multicast_StartSuction_Implementation(AActor* Shard)
{
	if (HasAuthority()) return;
	if (IsValid(Shard) && Shard->GetClass()->ImplementsInterface(UMemoryShardInteract::StaticClass()))
		IMemoryShardInteract::Execute_StartSuction(Shard, this);
}

void APlayerCharacter::Multicast_StopSuction_Implementation(AActor* Shard)
{
	if (HasAuthority()) return;
	if (IsValid(Shard) && Shard->GetClass()->ImplementsInterface(UMemoryShardInteract::StaticClass()))
		IMemoryShardInteract::Execute_StopSuction(Shard);
}

void APlayerCharacter::Multicast_Collect_Implementation(AActor* Shard)
{
	if (HasAuthority()) return;
	if (IsValid(Shard) && Shard->GetClass()->ImplementsInterface(UMemoryShardInteract::StaticClass()))
		IMemoryShardInteract::Execute_Collect(Shard, this);
}