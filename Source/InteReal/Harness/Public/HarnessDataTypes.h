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
};

USTRUCT(BlueprintType)
struct FSurfaceMaterialDelta
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FString SurfaceID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FString MaterialPath;
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
