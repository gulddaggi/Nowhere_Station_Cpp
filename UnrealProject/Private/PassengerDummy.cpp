// Fill out your copyright notice in the Description page of Project Settings.


#include "PassengerDummy.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"


APassengerDummy::APassengerDummy()
{
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

void APassengerDummy::BeginPlay()
{
	Super::BeginPlay();
	
}

void APassengerDummy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
	Super::GetLifetimeReplicatedProps(Out);
}

