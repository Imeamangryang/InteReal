#pragma once

#include "CoreMinimal.h"
#include "InteRealGizmoTypes.generated.h"

UENUM(BlueprintType)
enum class EInteRealGizmoDisplayMode : uint8
{
	All,
	Move,
	Rotation,
	None
};

