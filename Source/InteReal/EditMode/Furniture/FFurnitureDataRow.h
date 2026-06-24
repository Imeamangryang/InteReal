#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FFurnitureDataRow.generated.h"

class AFurniture;
class UStaticMesh;
class UTexture2D;

// 가구 하나가 여러 표면에 배치 가능할 수 있어 비트마스크 사용
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EPlacementSurfaceType : uint8
{
	None = 0x00 UMETA(Hidden),
	Floor = 0x01 UMETA(DisplayName = "바닥"),
	Wall = 0x02 UMETA(DisplayName = "벽"),
	Surface = 0x04 UMETA(DisplayName = "가구 위 표면"),
	Ceiling = 0x08 UMETA(DisplayName = "천장"),
};

UENUM(BlueprintType)
enum class EPlacementAssetKind : uint8
{
	Generic UMETA(DisplayName = "일반"),
	Door UMETA(DisplayName = "문"),
	Window UMETA(DisplayName = "창문"),
	EntranceDoor UMETA(DisplayName = "현관문"),
	SlidingDoor UMETA(DisplayName = "미닫이문"),
};

UENUM(BlueprintType)
enum class EFurnitureAssetCategory : uint8
{
	None UMETA(DisplayName = "미분류"),
	Bed UMETA(DisplayName = "침대"),
	Seating UMETA(DisplayName = "의자/소파"),
	TableDesk UMETA(DisplayName = "책상"),
	Storage UMETA(DisplayName = "수납"),
	Lighting UMETA(DisplayName = "조명"),
	Electronics UMETA(DisplayName = "전자제품"),
	Kitchen UMETA(DisplayName = "주방"),
	Bathroom UMETA(DisplayName = "욕실"),
	Decor UMETA(DisplayName = "장식/소품"),
	Mirror UMETA(DisplayName = "거울"),
	Plant UMETA(DisplayName = "식물"),
	Rug UMETA(DisplayName = "러그"),
	Shelf UMETA(DisplayName = "선반/책장"),
};

USTRUCT(BlueprintType)
struct FFurnitureDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName = FText::FromString(TEXT("Furniture"));
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture|Search")
	EFurnitureAssetCategory Category = EFurnitureAssetCategory::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> DisplayImage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> FurnitureMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FIntPoint Dimensions = FIntPoint(1, 1);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture|Size", meta = (ClampMin = "0.0", Units = "cm"))
	float Width = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture|Size", meta = (ClampMin = "0.0", Units = "cm"))
	float Depth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture|Size", meta = (ClampMin = "0.0", Units = "cm"))
	float Height = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture|Opening")
	EPlacementAssetKind AssetKind = EPlacementAssetKind::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture|Opening", meta = (ClampMin = "0.0", Units = "cm"))
	float OpeningBottom = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture|Opening")
	bool bFitToOpeningSlot = true;

	// 이 가구를 배치할 수 있는 표면 (복수 선택 가능)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Bitmask, BitmaskEnum = "/Script/InteReal.EPlacementSurfaceType"))
	uint8 AllowedPlacementTypes = static_cast<uint8>(EPlacementSurfaceType::Floor);

	// 벽에 부착 시 벽면에서 띄울 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float WallOffset = 0.0f;
};
