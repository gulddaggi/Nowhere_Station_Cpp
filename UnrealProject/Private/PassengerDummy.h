// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PassengerDummy.generated.h"

UCLASS()
class APassengerDummy : public AActor
{
	GENERATED_BODY()
	
public:	
	APassengerDummy();

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;

	virtual void BeginPlay() override;

public:	
	// 전용서버에서만 움직이게 할 거라 Movement 복제만
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	
};
