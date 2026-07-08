#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FOpeningAssetDataRow.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EOpeningAssetKind : uint8
{
	Door UMETA(DisplayName = "문"),
	Window UMETA(DisplayName = "창문"),
	EntranceDoor UMETA(DisplayName = "현관문"),
	SlidingDoor UMETA(DisplayName = "미닫이문"),
};

USTRUCT(BlueprintType)
struct FOpeningAssetDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening")
	EOpeningAssetKind OpeningKind = EOpeningAssetKind::Door;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening")
	TObjectPtr<UStaticMesh> OpeningMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening", meta = (Units = "deg"))
	float OpeningMeshYawOffset = 0.0f;
};
