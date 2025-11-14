// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NSPortal.generated.h"

class UBoxComponent;
class UArrowComponent;

UCLASS()
class ANSPortal : public AActor
{
	GENERATED_BODY()
	
public:	
	ANSPortal();

    // 포탈 고유 태그(출발지 식별자)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
    FName SelfTag = NAME_None;

    // 목적지 포탈 태그(연결용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
    FName DestinationTag = NAME_None;

    // 목적지에서의 오프셋(기본 100~200 전방)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
    FVector DestinationOffset = FVector(0, 0, 0);

    // 양방향 판별
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
    bool bTwoWay = false;

    // 중복 트리거 방지(초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
    float CooldownSec = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
    TSoftObjectPtr<AActor> DestinationAnchor;

    // 컴포넌트
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent>  Trigger;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UArrowComponent> Arrow;

protected:
	virtual void BeginPlay() override;

    UFUNCTION()
    void OnTriggerBegin(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp,
        int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);

    // 서버 RPC: 포탈 요청(안전)
    UFUNCTION(Server, Reliable)
    void Server_RequestTeleport(AController* ForController);

    // 실제 텔레포트 수행(서버에서만)
    void TeleportPawn(AController* PC);

    // 목적지 포탈 찾아서 변환 생성
    bool BuildDestinationTransform(FTransform& Out) const;

    // 목적지 포탈 검색
    ANSPortal* FindPortalByTag(FName Tag) const;

    // 쿨다운
    TMap<TWeakObjectPtr<AController>, double> LastUseTime;

    void PreloadAt(const FVector& WorldLoc, float Radius = 5000.f, float HoldSec = 0.25f) const;
};
