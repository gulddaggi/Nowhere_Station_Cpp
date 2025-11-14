// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "MemoryShardInteract.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UMemoryShardInteract : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class IMemoryShardInteract
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "MemoryShard")
    void StartSuction(class APlayerCharacter* Player);
    virtual void StartSuction_Implementation(class APlayerCharacter* Player) {} 

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "MemoryShard")
    void StopSuction();
    virtual void StopSuction_Implementation() {}

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "MemoryShard")
    void Collect(class APlayerCharacter* Player);
    virtual void Collect_Implementation(class APlayerCharacter* Player) {}
	
};
