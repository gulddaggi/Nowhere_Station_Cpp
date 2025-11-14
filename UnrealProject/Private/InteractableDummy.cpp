// Fill out your copyright notice in the Description page of Project Settings.


#include "InteractableDummy.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"


// Sets default values
AInteractableDummy::AInteractableDummy()
{
	PrimaryActorTick.bCanEverTick = true;

	SetReplicateMovement(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetIsReplicated(true);

}

void AInteractableDummy::OnRep_Kind()
{

}

void AInteractableDummy::SetKind_Server(EInteractableKind NewKind)
{
	if (HasAuthority())
	{
		Kind = NewKind;
		OnRep_Kind();
	}
}

void AInteractableDummy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const // ✅
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AInteractableDummy, Kind);
}

