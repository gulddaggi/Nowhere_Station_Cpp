// Fill out your copyright notice in the Description page of Project Settings.


#include "NSPortal.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "EngineUtils.h"

ANSPortal::ANSPortal()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
    Trigger->SetupAttachment(Root);
    Trigger->SetBoxExtent(FVector(80, 80, 140));
    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Trigger->SetCollisionObjectType(ECC_WorldDynamic);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->SetGenerateOverlapEvents(true);

    Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
    Arrow->SetupAttachment(Root);
    Arrow->ArrowSize = 1.2f;
}

void ANSPortal::BeginPlay()
{
	Super::BeginPlay();

    if (Trigger)
    {
        Trigger->OnComponentBeginOverlap.AddDynamic(this, &ANSPortal::OnTriggerBegin);
    }
}

void ANSPortal::OnTriggerBegin(UPrimitiveComponent* /*Comp*/, AActor* Other, UPrimitiveComponent* /*OtherComp*/,
    int32 /*BodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*Hit*/)
{
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Warning, TEXT("Trigger detection"));

    APawn* Pawn = Cast<APawn>(Other);
    if (!Pawn) return;
    AController* PC = Pawn->GetController();
    if (!PC) return;

    // 쿨다운
    const double Now = GetWorld()->TimeSeconds;
    if (double* Prev = LastUseTime.Find(PC))
    {
        if (Now - *Prev < CooldownSec) return;
    }
    LastUseTime.Add(PC, Now);

    Server_RequestTeleport(PC);
}

void ANSPortal::Server_RequestTeleport_Implementation(AController* ForController)
{
    if (!HasAuthority() || !ForController) return;
    TeleportPawn(ForController);
}

bool ANSPortal::BuildDestinationTransform(FTransform& Out) const
{
    // 목적지 포탈
    ANSPortal* Dest = FindPortalByTag(DestinationTag);
    if (!Dest) return false;

    // 기준 변환: 목적지 포탈의 Arrow 또는 Actor 변환
    const FTransform Base = Dest->Arrow ? Dest->Arrow->GetComponentTransform() : Dest->GetActorTransform();

    // 앵커가 지정되었다면 앵커 기준
    FTransform AnchorT = Base;
    if (AActor* Anchor = DestinationAnchor.Get())
    {
        AnchorT = Anchor->GetActorTransform();
    }

    FVector Loc = AnchorT.TransformPosition(DestinationOffset);
    FRotator Rot = AnchorT.Rotator();

    Out = FTransform(Rot, Loc, FVector(1, 1, 1));
    return true;
}

void ANSPortal::TeleportPawn(AController* PC)
{
    APawn* P = PC ? PC->GetPawn() : nullptr;
    if (!P) return;

    FTransform Dest;
    if (!BuildDestinationTransform(Dest)) return;

    ACharacter* C = Cast<ACharacter>(P);
    if (C) C->SetActorEnableCollision(false);

    const bool bTeleported = P->TeleportTo(Dest.GetLocation(), Dest.Rotator(), false, true);

    if (C)
    {
        // 약간의 지연 후 충돌 복구
        FTimerHandle TH;
        GetWorldTimerManager().SetTimer(TH, [C]()
            {
                if (IsValid(C)) C->SetActorEnableCollision(true);
            }, 0.3f, false);
    }
}

ANSPortal* ANSPortal::FindPortalByTag(FName Tag) const
{
    if (Tag.IsNone()) return nullptr;
    UWorld* W = GetWorld();
    if (!W) return nullptr;

    for (TActorIterator<ANSPortal> It(W); It; ++It)
    {
        if (IsValid(*It) && It->SelfTag == Tag)
            return *It;
    }
    return nullptr;
}

void ANSPortal::PreloadAt(const FVector& WorldLoc, float Radius, float HoldSec) const
{

}



