// Fill out your copyright notice in the Description page of Project Settings.


#include "NSSpawnDirector.h"
#include "PassengerDummy.h"
#include "InteractableDummy.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/TargetPoint.h"
#include "InteractiveActor.h"
#include "MopTarget.h"

static const FName TAG_WALL = TEXT("Stain_Wall");
static const FName TAG_FLOOR = TEXT("Stain_Floor");
static const FName TAG_OBJECT = TEXT("Stain_Object");

ANSSpawnDirector::ANSSpawnDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(false);
}

void ANSSpawnDirector::BeginPlay()
{
	Super::BeginPlay();
	
}

void ANSSpawnDirector::BeginSpawnLoop(float InRoundLengthSec)
{
	if (!HasAuthority()) return;

	bActive = true;
	ElapsedSec = 0.f;
	CurrentStage = ESpawnStage::Early;
	Spawned = FStageQuota{};
	Alive = FStageQuota{};
	NextPassengerTime = NextStainTime = NextRepairTime = 0.f;

	BuildRepairPool();  // 풀 만들기

	// 라운드 길이에 맞춰 세그먼트 수정하고 싶으면 여기서 Early/Peak/CleanupEndSec 조정
	UE_LOG(LogTemp, Log, TEXT("[SPAWN] BeginSpawnLoop"));

	TArray<AActor*> Tmp;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), StainTag, Tmp);
	UE_LOG(LogTemp, Log, TEXT("[SPAWN] Stain points with tag '%s' = %d"), *StainTag.ToString(), Tmp.Num());
}

void ANSSpawnDirector::BuildRepairPool()
{
	RepairPool.Reset();

	TArray<AActor*> Found;
	// BP_RepairableActor(=AInteractiveActor 파생) + 태그로 수집
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AInteractiveActor::StaticClass(), Found);

	for (AActor* A : Found)
	{
		if (!A || (RepairPoolTag != NAME_None && !A->ActorHasTag(RepairPoolTag))) continue;
		AInteractiveActor* R = Cast<AInteractiveActor>(A);
		if (!R) continue;

		// 시작은 고장 아님
		R->SetIsBroken(false);
		// 수리 완료 시점 콜백
		R->OnRepairCompleted.AddDynamic(this, &ANSSpawnDirector::OnRepairCompleted);
		RepairPool.Add(R);
	}

	UE_LOG(LogTemp, Log, TEXT("[SPAWN] RepairPool size=%d"), RepairPool.Num());
}

int32 ANSSpawnDirector::CountActiveRepairs() const
{
	int32 C = 0;
	for (auto& W : RepairPool)
	{
		if (const AInteractiveActor* R = W.Get())
		{
			if (R->IsBroken()) ++C;
		}
	}
	return C;
}

bool ANSSpawnDirector::ActivateRandomRepair()
{
	// 이미 최대치면 패스
	if (CountActiveRepairs() >= MaxSimultaneousRepairs) return false;

	TArray<AInteractiveActor*> Candidates;
	for (auto& W : RepairPool)
	{
		AInteractiveActor* R = W.Get();
		if (R && !R->IsBroken())
		{
			Candidates.Add(R);
		}
	}
	if (Candidates.Num() == 0) return false;

	AInteractiveActor* Pick = Candidates[FMath::RandHelper(Candidates.Num())];
	Pick->SetIsBroken(true);
	Alive.RepairTotal++; Spawned.RepairTotal++;
	UE_LOG(LogTemp, Log, TEXT("[SPAWN] ActivateRepair %s"), *Pick->GetName());
	return true;
}

void ANSSpawnDirector::OnRepairCompleted(AInteractiveActor* Who)
{
	Alive.RepairTotal = FMath::Max(0, Alive.RepairTotal - 1);
	UE_LOG(LogTemp, Log, TEXT("[SPAWN] RepairCompleted %s Alive=%d"),
		Who ? *Who->GetName() : TEXT("Unknown"), Alive.RepairTotal);
}

void ANSSpawnDirector::EndSpawnLoop()
{
	if (!HasAuthority()) return;
	bActive = false;
	ESpawnStage Prev = CurrentStage;
	CurrentStage = ESpawnStage::Inactive;
	UE_LOG(LogTemp, Log, TEXT("[SPAWN] EndSpawnLoop"));
}

void ANSSpawnDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsServerActive()) return;

	ElapsedSec += DeltaSeconds;
	ESpawnStage Before = CurrentStage;
	UpdateStage();
	if (Before != CurrentStage)
	{
		LogStageChange(Before, CurrentStage);
	}

	const float Now = ElapsedSec;
	TrySpawnTick(Now);
}

void ANSSpawnDirector::UpdateStage()
{
	if (ElapsedSec < EarlyEndSec)            CurrentStage = ESpawnStage::Early;
	else if (ElapsedSec < PeakEndSec)        CurrentStage = ESpawnStage::Peak;
	else if (ElapsedSec < CleanupEndSec)     CurrentStage = ESpawnStage::Cleanup;
	else                                     CurrentStage = ESpawnStage::Inactive;
}

void ANSSpawnDirector::TrySpawnTick(float Now)
{
	if (CurrentStage == ESpawnStage::Inactive) return;

	// 타입별 쿨다운 체크 + 해당 스테이지 quota 미달 시 스폰
	if (Now >= NextPassengerTime) if (TrySpawnPassenger()) NextPassengerTime = Now + CooldownSec.Passenger;
	if (Now >= NextStainTime)     if (TrySpawnStain())     NextStainTime = Now + CooldownSec.Stain;
	if (Now >= NextRepairTime)    if (ActivateRandomRepair())    NextRepairTime = Now + CooldownSec.Repair;
	if (Now >= NextShardTime)     if (TrySpawnShard())     NextShardTime = Now + CooldownSec.Shard;
}

static const FStageQuota& GetQuotaFor(ESpawnStage Stage, const FStageQuota& E, const FStageQuota& P, const FStageQuota& C)
{
	switch (Stage) { case ESpawnStage::Early: return E; case ESpawnStage::Peak: return P; default: return C; }
}

bool ANSSpawnDirector::TrySpawnPassenger()
{
	const FStageQuota& Q = GetQuotaFor(CurrentStage, EarlyQuota, PeakQuota, CleanupQuota);
	if (Spawned.PassengerTotal >= Q.PassengerTotal) return false;

	FTransform T; if (!FindRandomPointByTag(PassengerTag, T) || !*PassengerClass) return false;
	AActor* A = GetWorld()->SpawnActorDeferred<AActor>(PassengerClass, T, this);
	if (!A) return false;
	UGameplayStatics::FinishSpawningActor(A, T);

	Spawned.PassengerTotal++; Alive.PassengerTotal++;
	LogBatch(TEXT("Passenger"), Spawned.PassengerTotal);
	return true;
}

bool ANSSpawnDirector::FindRandomPointByTagEx(FName Tag, FTransform& Out, AActor*& OutPoint) const
{
	TArray<AActor*> Points;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, Points);
	if (Points.Num() == 0) { OutPoint = nullptr; return false; }
	OutPoint = Points[FMath::RandHelper(Points.Num())];
	Out = OutPoint->GetActorTransform();
	return true;
}

bool ANSSpawnDirector::TrySpawnStain()
{
	const FStageQuota& Q = GetQuotaFor(CurrentStage, EarlyQuota, PeakQuota, CleanupQuota);
	if (Spawned.StainTotal >= Q.StainTotal) {
		UE_LOG(LogTemp, Verbose, TEXT("[SPAWN][Stain] quota full %d/%d"), Spawned.StainTotal, Q.StainTotal);
		return false;
	}

	FTransform T;
	AActor* SpawnPoint = nullptr;

	if (!FindRandomPointByTagEx(StainTag, T, SpawnPoint)) {
		UE_LOG(LogTemp, Error, TEXT("[SPAWN][Stain] no points with tag '%s'"), *StainTag.ToString());
		return false;
	}

	if (!*MemoryStainClass) {
		UE_LOG(LogTemp, Error, TEXT("[SPAWN][Stain] MemoryStainClass is None (set BP_MemoryStain on Director/BP)"));
		return false;
	}

	if (!FindRandomPointByTagEx(StainTag, T, SpawnPoint) || !*MemoryStainClass) return false;

	// 스폰
	AActor* NewStain = SpawnStainAtPoint(SpawnPoint);
	if (!NewStain) return false;

	// 태그로 타입 고르기 (예시)
	EStainType Type = EStainType::None;
	for (const FName& Tag : SpawnPoint->Tags)
	{
		if (Tag == "Stain_Wall") { Type = EStainType::Wall; break; }
		if (Tag == "Stain_Floor") { Type = EStainType::Floor;   break; }
		if (Tag == "Stain_Object") { Type = EStainType::Object;   break; }
	}

	if (Type == EStainType::None)
	{
		UE_LOG(LogTemp, Error, TEXT("[SPAWN][Stain] no type with tag '%d'"), Type);
	}


	// 타입→머티리얼 매핑 (에디터에서 채운 TMap 등)
	UMaterialInterface* Mat = StainBaseMaterials.FindRef(Type);
	if (!Mat) Mat = StainBaseMaterials[EStainType::Wall]; // 세이프 가드

	// 인터페이스 호출(서버)
	if (NewStain->GetClass()->ImplementsInterface(UMopTarget::StaticClass()))
	{
		IMopTarget::Execute_InitializeStain(NewStain, Type, Mat);
	}

	Spawned.StainTotal++;
	Alive.StainTotal++;
	return true;
}

bool ANSSpawnDirector::TrySpawnRepair()
{
	const FStageQuota& Q = GetQuotaFor(CurrentStage, EarlyQuota, PeakQuota, CleanupQuota);
	if (Spawned.RepairTotal >= Q.RepairTotal) return false;

	FTransform T; if (!FindRandomPointByTag(RepairTag, T) || !*RepairClass) return false;
	AActor* A = GetWorld()->SpawnActorDeferred<AActor>(RepairClass, T, this);
	if (!A) return false;
	if (auto* IA = Cast<AInteractiveActor>(A)) { IA->SetIsBroken(true); }
	UGameplayStatics::FinishSpawningActor(A, T);

	Spawned.RepairTotal++; Alive.RepairTotal++;
	LogBatch(TEXT("Repair"), Spawned.RepairTotal);
	return true;
}

bool ANSSpawnDirector::FindRandomPointByTag(FName Tag, FTransform& Out) const
{
	TArray<AActor*> Points;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, Points);
	if (Points.Num() == 0) return false;
	AActor* Pick = Points[FMath::RandHelper(Points.Num())];
	Out = Pick->GetActorTransform();
	return true;
}

void ANSSpawnDirector::LogStageChange(ESpawnStage From, ESpawnStage To) const
{
	UE_LOG(LogTemp, Log, TEXT("[SPAWN] %s -> %s"),
		*UEnum::GetValueAsString(From),
		*UEnum::GetValueAsString(To));
}

void ANSSpawnDirector::LogBatch(const FString& What, int32 Count) const
{
	if (CurrentStage == ESpawnStage::Peak)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[SPAWN] Peak %s total=%d"), *What, Count);
	}
}

static EStainType GuessStainTypeFromTags(const AActor* P)
{
	if (P->Tags.Contains(TEXT("Stain_Wall")))   return EStainType::Wall;
	if (P->Tags.Contains(TEXT("Stain_Floor")))  return EStainType::Floor;
	if (P->Tags.Contains(TEXT("Stain_Object"))) return EStainType::Object;
	// 태그가 없으면 풀에서 랜덤
	return EStainType::None;
}

void ANSSpawnDirector::GetAllowedStainTypesFromTags(const AActor* SpawnPoint, TArray<EStainType>& OutTypes) const
{
	OutTypes.Reset();
	if (!SpawnPoint) return;

	const TArray<FName>& ArrayTags = SpawnPoint->Tags;

	auto Has = [&](const FName& T) { return ArrayTags.Contains(T); };

	if (Has(TAG_WALL))   OutTypes.Add(EStainType::Wall);
	if (Has(TAG_FLOOR))  OutTypes.Add(EStainType::Floor);
	if (Has(TAG_OBJECT)) OutTypes.Add(EStainType::Object);
}

EStainType ANSSpawnDirector::PickTypeForPoint(const AActor* SpawnPoint) const
{
	TArray<EStainType> Allowed;
	GetAllowedStainTypesFromTags(SpawnPoint, Allowed);

	// 태그가 하나도 없으면 전 타입 허용
	if (Allowed.Num() == 0)
	{
		Allowed = { EStainType::Wall, EStainType::Object, EStainType::Floor };
	}

	const int32 Idx = FMath::RandRange(0, Allowed.Num() - 1);
	return Allowed[Idx];
}

AActor* ANSSpawnDirector::SpawnStainAtPoint(AActor* SpawnPoint)
{
	if (!HasAuthority() || !MemoryStainClass || !SpawnPoint) {
		UE_LOG(LogTemp, Error, TEXT("[SPAWN][Stain] invalid args (Auth=%d, Class=%d, Point=%s)"),
			HasAuthority() ? 1 : 0, *MemoryStainClass ? 1 : 0, *GetNameSafe(SpawnPoint));
		return nullptr;
	}

	const EStainType Type = PickTypeForPoint(SpawnPoint);

	// 스폰 트랜스폼(필요하면 타입별로 정렬)
	FTransform Xform = SpawnPoint->GetActorTransform();

	AActor* Stain = GetWorld()->SpawnActorDeferred<AActor>(MemoryStainClass, Xform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Stain) return nullptr;

	UGameplayStatics::FinishSpawningActor(Stain, Xform);

	// 타입에 맞는 기본 머티리얼
	UMaterialInterface* BaseMat = StainBaseMaterials.FindRef(Type);

	IMopTarget::Execute_InitializeStain(Stain, Type, BaseMat);

	Stain->OnDestroyed.AddDynamic(this, &ANSSpawnDirector::HandleStainDestroyed);

	return Stain;
}

int32 ANSSpawnDirector::GetAliveWorkCount() const
{
	return Alive.RepairTotal + Alive.StainTotal + Alive.ShardTotal;
}

void ANSSpawnDirector::DeactivateAllWork()
{
	if (!HasAuthority()) return;

	// 수리: 모두 정상 상태로 되돌리기
	for (auto& W : RepairPool)
	{
		if (AInteractiveActor* R = W.Get())
		{
			if (R->IsBroken())
			{
				R->SetIsBroken(false);
			}
		}
	}

	// 얼룩: 현재 존재하는 얼룩 액터 전부 제거
	if (*MemoryStainClass)
	{
		TArray<AActor*> Stains;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), MemoryStainClass, Stains);
		for (AActor* A : Stains)
		{
			if (A) { A->Destroy(); } // Destroy될 때 HandleStainDestroyed에서 Alive.StainTotal--
		}
	}

	// 샤드 제거 추가
	if (*MemoryShardClass)
	{
		TArray<AActor*> Shards;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), MemoryShardClass, Shards);
		for (AActor* A : Shards)
		{
			if (A) { A->Destroy(); } // Destroy 시 HandleShardDestroyed에서 Alive.ShardTotal--
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SPAWN] DeactivateAllWork: repairs reset, stains & shards destroyed"));
}

// 얼룩 파괴 시점에 Alive 카운트 조정
void ANSSpawnDirector::HandleStainDestroyed(AActor* DestroyedActor)
{
	Alive.StainTotal = FMath::Max(0, Alive.StainTotal - 1);
}

bool ANSSpawnDirector::TrySpawnShard()
{
	const FStageQuota& Q = GetQuotaFor(CurrentStage, EarlyQuota, PeakQuota, CleanupQuota);
	if (Spawned.ShardTotal >= Q.ShardTotal) return false;

	FTransform T;
	if (!FindRandomPointByTag(ShardTag, T) || !*MemoryShardClass) return false;

	AActor* A = GetWorld()->SpawnActorDeferred<AActor>(MemoryShardClass, T, this);
	if (!A) return false;
	UGameplayStatics::FinishSpawningActor(A, T);

	A->OnDestroyed.AddDynamic(this, &ANSSpawnDirector::HandleShardDestroyed);

	Spawned.ShardTotal++;
	Alive.ShardTotal++; 
	LogBatch(TEXT("Shard"), Spawned.ShardTotal);
	return true;
}

void ANSSpawnDirector::HandleShardDestroyed(AActor* DestroyedActor)
{
	Alive.ShardTotal = FMath::Max(0, Alive.ShardTotal - 1);
	UE_LOG(LogTemp, Verbose, TEXT("[SPAWN] ShardDestroyed %s Alive=%d"),
		*GetNameSafe(DestroyedActor), Alive.ShardTotal);
}