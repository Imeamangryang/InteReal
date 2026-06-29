#pragma once

#include "CoreMinimal.h"
#include "HarnessDataTypes.generated.h"

USTRUCT(BlueprintType)
struct FFurnitureDelta
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FName FurnitureID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FTransform Transform;

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
