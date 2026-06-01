#pragma once
#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EHarnessViewMode : uint8
{
	TopDown     UMETA(DisplayName = "평면 뷰 (Top-Down)"),
	Isometric   UMETA(DisplayName = "ISO 뷰 (Isometric)"),
	FirstPerson UMETA(DisplayName = "1인칭 뷰 (FPS)")
};