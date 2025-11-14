// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LoadingScreenLib.generated.h"

/**
 * 
 */
UCLASS()
class ULoadingScreenLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
    UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
    static void PlayTeaserAndOpenLevel(UObject* WorldContextObject,
        FName LevelName,
        const FString& MovieNameNoExt,
        bool bSkippable = true,
        float MinDisplayTime = 0.5f);

    UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
    static void PlayTeaserAndClientTravel(UObject* WorldContextObject,
        const FString& TravelURL,
        const FString& MovieNameNoExt,
        bool bSkippable = true,
        float MinDisplayTime = 0.5f);

    UFUNCTION(BlueprintCallable)
    static void StopTeaser();

private:
    static void StartMovie(const FString& MovieNameNoExt, bool bSkippable, float MinDisplayTime);
    static void OnPostLoadMap(UWorld* LoadedWorld);
    static FDelegateHandle PostLoadHandle;
};
