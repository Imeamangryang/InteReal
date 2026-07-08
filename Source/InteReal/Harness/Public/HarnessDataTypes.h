#pragma once

#include "CoreMinimal.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "HarnessDataTypes.generated.h"

USTRUCT(BlueprintType)
struct FFurnitureDelta
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FName FurnitureID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
	EFurnitureAssetCategory AssetCategory = EFurnitureAssetCategory::None;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FTransform Transform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
	FLightAttributes LightAttributes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
	uint8 SurfaceType = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
	FVector2D GridAnchor = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
	FVector2D Dimensions = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
	FVector WallNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
	int32 ParentIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FSurfaceMaterialDelta
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FString SurfaceID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FString MaterialPath;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FString BaseColorTexturePath;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    bool bHasMaterialAttributes = false;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    float Metallic = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    float Specular = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    float Roughness = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    float Emissive = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    float TextureTiling = 1.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FString MeshPath;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FVector RelativeScale = FVector::OneVector;
};

USTRUCT(BlueprintType)
struct FInteriorDeltaList
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    TArray<FFurnitureDelta> FurnitureItems;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    TArray<FSurfaceMaterialDelta> SurfaceMaterials;
};
