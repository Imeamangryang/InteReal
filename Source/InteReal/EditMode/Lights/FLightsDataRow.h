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

	// FFurnitureDataRow로 변환 — Category=Lighting, 나머지 가구 전용 필드는 기본값.
	// 배치 파이프라인(StartFurniturePlacement 등)이 FFurnitureDataRow를 받게 돼 있어서 스폰 직전에만 변환해서 쓴다.
	FFurnitureDataRow ToFurnitureDataRow() const
	{
		FFurnitureDataRow Row;
		Row.ID = ID;
		Row.DisplayName = DisplayName;
		Row.DisplayImage = DisplayImage;
		Row.Category = EFurnitureAssetCategory::Lighting;
		Row.LightAttributes = LightAttributes;
		// 조명은 가구와 달리 바닥에 제한할 이유가 없다 — 커서가 가리키는 표면(바닥/벽/천장)에
		// 그대로 붙도록 전부 허용해서 펜던트/벽조명/바닥조명을 자연스럽게 배치할 수 있게 한다.
		Row.AllowedPlacementTypes = static_cast<uint8>(EPlacementSurfaceType::Floor)
			| static_cast<uint8>(EPlacementSurfaceType::Wall)
			| static_cast<uint8>(EPlacementSurfaceType::Ceiling);
		return Row;
	}
};
