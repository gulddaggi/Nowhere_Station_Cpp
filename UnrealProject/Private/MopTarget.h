// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NSTypes.h"

#include "MopTarget.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UMopTarget : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class IMopTarget
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
    // 스폰 직후 1회: 타입/머티리얼 초기화
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void InitializeStain(EStainType Type, UMaterialInterface* BaseMaterial);

    // 타입 조회(플레이어 몽타주 분기용)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    EStainType GetStainType() const;

    // 서버: 시작/중단
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable) void Server_BeginMop(AActor* Instigator);
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable) void Server_EndMop();

    // 서버: 진행 델타 적용(완료 시 true 반환)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool Server_MopAdvance(float DeltaSeconds);
	
};
