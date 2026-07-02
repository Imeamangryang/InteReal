#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "FLightsDataRow.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FLightsDataRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lights")
	int32 ID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lights")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lights")
	TObjectPtr<UTexture2D> DisplayImage = nullptr;

	// 색상/밝기/범위/조명 종류(Point/Spot/Rect) 등 실제 조명 설정값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lights")
	FLightAttributes LightAttributes;

	FFurnitureDataRow ToFurnitureDataRow() const
	{
		FFurnitureDataRow Row;
		Row.ID = ID;
		Row.DisplayName = DisplayName;
		Row.DisplayImage = DisplayImage;
		Row.Category = EFurnitureAssetCategory::Lighting;
		Row.LightAttributes = LightAttributes;
		Row.AllowedPlacementTypes = static_cast<uint8>(EPlacementSurfaceType::Floor)
			| static_cast<uint8>(EPlacementSurfaceType::Surface)
			| static_cast<uint8>(EPlacementSurfaceType::Wall)
			| static_cast<uint8>(EPlacementSurfaceType::Ceiling);
		return Row;
	}
};
