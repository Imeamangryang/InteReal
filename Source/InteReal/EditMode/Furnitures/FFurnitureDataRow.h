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

USTRUCT(BlueprintType)
struct FFurnitureDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName = FText::FromString(TEXT("Furniture"));

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

	// 이 가구를 배치할 수 있는 표면 (복수 선택 가능)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Bitmask, BitmaskEnum = "/Script/InteReal.EPlacementSurfaceType"))
	uint8 AllowedPlacementTypes = static_cast<uint8>(EPlacementSurfaceType::Floor);

	// 벽에 부착 시 벽면에서 띄울 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float WallOffset = 0.0f;
};
