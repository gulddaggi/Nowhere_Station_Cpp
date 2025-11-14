// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NSTypes.h"
#include "GameFramework/Actor.h"
#include "InteractableDummy.generated.h"

UCLASS()
class AInteractableDummy : public AActor
{
	GENERATED_BODY()
	
public:	
	AInteractableDummy();

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;

	UPROPERTY(ReplicatedUsing = OnRep_Kind, EditAnywhere, Category = "Interactable")
	EInteractableKind Kind = EInteractableKind::Debris;

	UFUNCTION()
	void OnRep_Kind();

public:	
	UFUNCTION(BlueprintCallable)
	EInteractableKind GetKind() const { return Kind; }

	// 서버에서만 변경
	void SetKind_Server(EInteractableKind NewKind);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
