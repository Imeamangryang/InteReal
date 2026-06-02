#pragma once

#include "CoreMinimal.h"
#include "HarnessDataTypes.generated.h"

USTRUCT(BlueprintType)
struct FFloorPlanInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FString PlanId;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    FString PlanName;
};

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
struct FFurnitureDeltaList
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Harness|Data")
    TArray<FFurnitureDelta> FurnitureItems;
};
