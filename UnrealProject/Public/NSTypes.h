#pragma once
#include "CoreMinimal.h"
#include "NSTypes.generated.h"

UENUM(BlueprintType)
enum class EInteractableKind : uint8
{
    Debris,
    Stain
};

UENUM(BlueprintType)
enum class ESpawnStage : uint8
{
    Inactive,
    Early,
    Peak,
    Cleanup
};

UENUM(BlueprintType)
enum class EStainType : uint8
{
    None   UMETA(DisplayName = "None"),
    Wall   UMETA(DisplayName = "Wall"),
    Object UMETA(DisplayName = "Object"),
    Floor  UMETA(DisplayName = "Floor"),
};