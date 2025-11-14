// Fill out your copyright notice in the Description page of Project Settings.


#include "StartRitualStatue.h"
#include "Kismet/GameplayStatics.h"
#include "NSPlayerController.h"
#include "NSGameState.h"

void AStartRitualStatue::RequestToggleReady(bool bReady) {
    // 로컬 플레이어컨트롤러를 가져와서 자신의 Server RPC 호출
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ANSPlayerController* NSPC = Cast<ANSPlayerController>(PC))
        {
            NSPC->Server_ToggleReady(bReady);
        }
    }
    // HUD/패널은 OnRep(ReadyCount/TotalPlayers)에서 갱신
}



