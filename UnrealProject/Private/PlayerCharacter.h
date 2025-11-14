// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NSTypes.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "PlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UWidgetInteractionComponent;
class USkeletalMeshComponent;
class USceneComponent;
class USphereComponent;

UENUM(BlueprintType)
enum class EEquipmentType : uint8 { None, Vacuum, Mop };

UENUM(BlueprintType)
enum class ECleanState : uint8 { None, Vacuuming, Mopping };

UCLASS()
class APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

	// 소켓 있는 스켈레탈 메쉬(BP에서 Body를 할당)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equip")
	TObjectPtr<USkeletalMeshComponent> EquipAttachTarget = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
	TObjectPtr<class UInputAction> EquipSlot1Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
	TObjectPtr<class UInputAction> EquipSlot2Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
	TObjectPtr<class UInputAction> CleanAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
	TObjectPtr<class UInputAction> MenuAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equip")
	TObjectPtr<class UStaticMeshComponent> VacuumMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equip")
	TObjectPtr<class UStaticMeshComponent> MopMesh;

	void AttachEquipMeshesToSocket();
	void ApplyCurrentEquipOffset();

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Clean|Mop")
	TObjectPtr<AActor> MopTarget = nullptr;

	FTimerHandle MopTickHandle;

	// 서버 내부 헬퍼
	AActor* Server_FindMopTarget() const;
	void    Server_MopTick();

	UFUNCTION(NetMulticast, Unreliable) void Multicast_MopStart(EStainType StainType);
	UFUNCTION(NetMulticast, Unreliable) void Multicast_MopStop();

	UFUNCTION(BlueprintImplementableEvent, Category = "Clean")
	void BP_OnMopStarted(EStainType StainType);

	UPROPERTY(EditDefaultsOnly, Category = "Clean|Mop")
	TMap<EStainType, UAnimMontage*> MopMontages;

	UPROPERTY(EditDefaultsOnly, Category = "Clean|Mop")
	TObjectPtr<USoundBase> MopStartSFX = nullptr;

	// 재생 중인 오디오 핸들
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MopAudio = nullptr;

	// 코스메틱 전용(서버 전용 모드에서는 아무것도 안 함)
	void PlayMopCosmetics(EStainType T);
	void StopMopCosmetics();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interact")
	TObjectPtr<USphereComponent> InteractionCollision;

	UPROPERTY(EditDefaultsOnly, Category = "Clean|Vacuum")
	TSubclassOf<AActor> MemoryShardClass;

	UPROPERTY(EditDefaultsOnly, Category = "Clean|Vacuum")
	UAnimMontage* VacuumMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Clean|Vacuum")
	USoundBase* VacuumLoopSFX = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Clean|Vacuum")
	float VacuumRange = 350.f;

	UPROPERTY(EditDefaultsOnly, Category = "Clean|Vacuum")
	float VacuumOverlapRadius = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "Clean|Vacuum")
	float VacuumCollectDist = 80.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clean|Vacuum")
	TObjectPtr<class USphereComponent> VacuumCollision = nullptr;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentEquip, VisibleInstanceOnly, Category = "Equip")
	EEquipmentType CurrentEquip = EEquipmentType::None;

	UPROPERTY(ReplicatedUsing = OnRep_CleanState, VisibleInstanceOnly, Category = "Clean")
	ECleanState CleanState = ECleanState::None;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Clean")
	bool bActionLocked = false;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> CleaningTarget = nullptr;

	UFUNCTION(Server, Reliable) void Server_SetEquip(EEquipmentType NewEquip);
	UFUNCTION(Server, Reliable) void Server_BeginClean();
	UFUNCTION(Server, Reliable) void Server_EndClean();

	UFUNCTION() void OnRep_CurrentEquip();
	UFUNCTION() void OnRep_CleanState();

	void ApplyEquipVisuals();

	TSet<TWeakObjectPtr<AActor>> ActiveSuctionShards;

	// 루프 사운드 핸들
	UPROPERTY(Transient, BlueprintReadOnly)
	UAudioComponent* VacuumAudio = nullptr;

	// 틱 핸들
	FTimerHandle VacuumTickHandle;

	// 서버 RPC
	UFUNCTION(Server, Reliable) void Server_BeginVacuum();

	// 서버 RPC
	UFUNCTION(Server, Reliable) void Server_EndVacuum();

	UFUNCTION(NetMulticast, Unreliable) void Multicast_StartSuction(AActor* Shard);
	UFUNCTION(NetMulticast, Unreliable) void Multicast_StopSuction(AActor* Shard);
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Collect(AActor* Shard);

	// 멀티캐스트(코스메틱)
	UFUNCTION(NetMulticast, Unreliable) void Multicast_VacuumStart();
	UFUNCTION(NetMulticast, Unreliable) void Multicast_VacuumStop();

	// 헬퍼
	void PlayVacuumCosmetics(bool bStart);

	UFUNCTION()
	void OnVacuumOverlapBegin(
		UPrimitiveComponent* Overlapped, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnVacuumOverlapEnd(
		UPrimitiveComponent* Overlapped, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void RefreshVacuumFieldTransform();

	void Server_VacuumTick();
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<class USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<class UCameraComponent> Camera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
	TObjectPtr<class UInputMappingContext> PlayerMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
	TObjectPtr<class UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
	TObjectPtr<class UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
	TObjectPtr<class UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
	TObjectPtr<class UInputAction> InteractAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetInteractionComponent> UIWidgetInteraction;

	// 런타임에 재부착
	virtual void PostInitializeComponents() override;

	// 소켓명(한 곳에서 관리)
	static const FName EquipSocketName;

	// 장비별 위치/회전/스케일 오프셋만 유지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equip|Offsets")
	TMap<EEquipmentType, FTransform> EquipOffsets;

	// 헬퍼들
	FTransform GetOffsetForEquip(EEquipmentType Type) const;

	UFUNCTION()
	void OnInteractOverlapBegin(
		UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractOverlapEnd(
		UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	TArray<TWeakObjectPtr<AActor>> OverlappingMopTargets;
	TWeakObjectPtr<AActor> ClientMopCandidate;

	AActor* PickNearestMopCandidate() const;

	UFUNCTION(Server, Reliable) void Server_BeginCleanWithTarget(AActor* InTarget);

	UPROPERTY()                                  
	TSet<TWeakObjectPtr<AActor>> ActiveVacuumSet;

	bool IsVacuumEquipped() const;
	bool IsVacuumActive() const;

public:
	UPROPERTY(EditDefaultsOnly, Category = "Anim") UAnimMontage* RepairMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> MainMeshComponent;

	UFUNCTION(BlueprintCallable, Category = "Animation")
	void PlayRepairAnimation(bool bIsPlaying);

	void Move(const struct FInputActionValue& Value);
	void Look(const struct FInputActionValue& Value);
	void StartSprinting();
	void StopSprinting();
	void InteractPressed();
	void InteractReleased();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float RunSpeed = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	float WidgetInteractionDistance = 1000.f;

	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable) void Server_TryStartRepair(class AInteractiveActor* Target);
	UFUNCTION(Server, Reliable) void Server_TryStopRepair(class AInteractiveActor* Target);

	UFUNCTION(Client, Reliable)
	void Client_PlayRepairMontage(bool bPlay);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayRepairMontage(bool bPlay);

	UPROPERTY(EditAnywhere, Category = "Interact|Scan")
	float InteractSphereRadius = 24.f;

	UPROPERTY(EditAnywhere, Category = "Interact|Scan")
	float InteractConeHalfAngleDeg = 28.f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastInteractTarget;

	// UI용 현재 후보(항상 스캔을 켤 경우)
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ClientInteractCandidate;

	AActor* Client_FindBestInteractable(struct FHitResult* OutHit = nullptr) const;

public:
	bool bIsMovementInputEnabled;

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetMovementInputEnabled(bool bIsEnabled);

	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Equip")
	void BP_OnEquipChanged(EEquipmentType NewEquip, EEquipmentType PrevEquip);

	UFUNCTION(BlueprintImplementableEvent, Category = "Clean")
	void BP_OnCleanStateChanged(ECleanState NewState);

	UPROPERTY(EditDefaultsOnly, Category = "Vacuum")
	float VacuumSphereRadius = 35.f;

	UPROPERTY(EditDefaultsOnly, Category = "Vacuum")
	float VacuumForwardOffset = 30.f;

protected:
	// 서버가 결정한 스프린트 상태를 복제
	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting, VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement|State")
	bool bIsSprinting = false;

	UFUNCTION()
	void OnRep_IsSprinting();

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewIsSprinting);

	// 상태에 따라 실제 이동 속도 적용(서버/클라 공용)
	void UpdateMovementSpeedFromState();

	// 복제 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
	float InteractDistance = 350.f;

	// 입력 핸들러
	void Input_EquipSlot1();
	void Input_EquipSlot2();
	void Input_CleanStarted();
	void Input_CleanEnded();
	void Input_Menu();

	UPROPERTY()
	TWeakObjectPtr<class AInteractiveActor> CurrentInteractable;
};
