// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NSTypes.h"
#include "GameFramework/Actor.h"
#include "NSSpawnDirector.generated.h"

USTRUCT(BlueprintType)
struct FStageQuota
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) int32 PassengerTotal = 0;
	UPROPERTY(EditAnywhere) int32 StainTotal = 0;
	UPROPERTY(EditAnywhere) int32 RepairTotal = 0;
	UPROPERTY(EditAnywhere) int32 ShardTotal = 0;
};

USTRUCT(BlueprintType)
struct FSpawnCooldown
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) float Passenger = 5.f;
	UPROPERTY(EditAnywhere) float Stain = 8.f;
	UPROPERTY(EditAnywhere) float Repair = 10.f;
	UPROPERTY(EditAnywhere) float Shard = 6.f;
};

UCLASS()
class ANSSpawnDirector : public AActor
{
	GENERATED_BODY()
	
public:	
	ANSSpawnDirector();

	// 라운드 시작 시 서버에서 호출
	UFUNCTION() void BeginSpawnLoop(float InRoundLengthSec);
	// 라운드 종료/엔딩 시
	UFUNCTION() void EndSpawnLoop();

	UFUNCTION(BlueprintCallable, Category = "Spawn|Admin")
	int32 GetAliveWorkCount() const;

	// 모든 업무 비활성화: 고장 복구 + 얼룩 제거
	UFUNCTION(BlueprintCallable, Category = "Spawn|Admin")
	void DeactivateAllWork();

	// 현재 스테이지(복제는 GS에서)
	ESpawnStage GetStage() const { return CurrentStage; }

	UPROPERTY(EditAnywhere, Category = "Classes")
	TSubclassOf<AActor> StainClass;
	UPROPERTY(EditAnywhere, Category = "Stain")
	TMap<EStainType, UMaterialInterface*> StainMaterials;
	UPROPERTY(EditAnywhere, Category = "Stain")
	TArray<EStainType> StainTypePool{ EStainType::Wall, EStainType::Object, EStainType::Floor };
	UPROPERTY(EditDefaultsOnly, Category = "Stain")
	TSubclassOf<AActor> MemoryStainClass;
	UPROPERTY(EditDefaultsOnly, Category = "Stain")
	TMap<EStainType, UMaterialInterface*> StainBaseMaterials;
	UPROPERTY(EditAnywhere, Category = "Classes")
	TSubclassOf<AActor> MemoryShardClass;

protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	// 서버 전용
	bool IsServerActive() const { return HasAuthority() && bActive; }

	void GetAllowedStainTypesFromTags(const AActor* SpawnPoint, TArray<EStainType>& OutTypes) const;

	EStainType PickTypeForPoint(const AActor* SpawnPoint) const;

	AActor* SpawnStainAtPoint(AActor* SpawnPoint);

private:
	UPROPERTY(EditAnywhere, Category = "Classes") TSubclassOf<AActor> PassengerClass;
	UPROPERTY(EditAnywhere, Category = "Classes") TSubclassOf<AActor> InteractableClass;
	UPROPERTY(EditAnywhere, Category = "Classes") TSubclassOf<AActor> RepairClass;

	UFUNCTION()
	void HandleStainDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleShardDestroyed(AActor* DestroyedActor);

	// 맵에 배치한 TargetPoint/Empty Actor 등에 Tag로 위치 그룹 지정
	UPROPERTY(EditAnywhere, Category = "Spawn|Tags")
	FName PassengerTag = TEXT("Spawn_Passenger");
	UPROPERTY(EditAnywhere, Category = "Spawn|Tags")
	FName StainTag = TEXT("Spawn_Stain");
	UPROPERTY(EditAnywhere, Category = "Spawn|Tags")
	FName RepairTag = TEXT("Spawn_Repair");
	UPROPERTY(EditAnywhere, Category = "Repair Pool")
	FName RepairPoolTag = TEXT("Repairable");
	UPROPERTY(EditAnywhere, Category = "Spawn|Tags")
	FName ShardTag = TEXT("Spawn_Shard");

	UPROPERTY(EditAnywhere, Category = "Repair Pool")
	int32 MaxSimultaneousRepairs = 2;

	UPROPERTY()
	TArray<TWeakObjectPtr<class AInteractiveActor>> RepairPool;

	UFUNCTION()
	void OnRepairCompleted(class AInteractiveActor* Who);
	void BuildRepairPool();
	bool ActivateRandomRepair();
	int32 CountActiveRepairs() const;

	UPROPERTY(EditAnywhere, Category = "Spawn|Stage")
	float EarlyEndSec = 120.f;
	UPROPERTY(EditAnywhere, Category = "Spawn|Stage")
	float PeakEndSec = 480.f;
	UPROPERTY(EditAnywhere, Category = "Spawn|Stage")
	float CleanupEndSec = 600.f;

	UPROPERTY(EditAnywhere, Category = "Spawn|Stage")
	FStageQuota EarlyQuota{ 12, 6,  6, 2 };
	UPROPERTY(EditAnywhere, Category = "Spawn|Stage")
	FStageQuota PeakQuota{ 18,10, 10, 3 };
	UPROPERTY(EditAnywhere, Category = "Spawn|Stage")
	FStageQuota CleanupQuota{ 8, 6,  6, 2 };

	UPROPERTY(EditAnywhere, Category = "Spawn|Pacing")
	FSpawnCooldown CooldownSec;

	// 내부 상태
	bool  bActive = false;
	float ElapsedSec = 0.f;
	ESpawnStage CurrentStage = ESpawnStage::Inactive;

	FStageQuota Spawned;
	FStageQuota Alive;

	// 각 타입별 다음 스폰 가능 시간
	float NextPassengerTime = 0.f, NextStainTime = 0.f, NextRepairTime = 0.f, NextShardTime = 0.f;

	// 헬퍼
	void UpdateStage();
	void TrySpawnTick(float Now);
	bool TrySpawnPassenger();
	bool TrySpawnStain();
	bool TrySpawnRepair();
	bool TrySpawnShard();


	bool FindRandomPointByTag(FName Tag, FTransform& Out) const;
	bool FindRandomPointByTagEx(FName Tag, FTransform& Out, AActor*& OutPoint) const;
	void LogStageChange(ESpawnStage From, ESpawnStage To) const;
	void LogBatch(const FString& What, int32 Count) const;
};
