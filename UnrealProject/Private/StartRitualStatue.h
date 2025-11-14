// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveActor.h"
#include "StartRitualStatue.generated.h"

/**
 * 
 */
UCLASS()
class AStartRitualStatue : public AInteractiveActor
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void RequestToggleReady(bool bReady); // 위젯에서 호출

	// 패널 초기화 훅 (BP에서 바인딩)
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnReadyStateChanged(int32 ReadyCount, int32 TotalPlayers);
	
	
};
