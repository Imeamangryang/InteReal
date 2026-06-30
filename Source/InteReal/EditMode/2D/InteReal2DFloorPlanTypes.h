#pragma once

#include "CoreMinimal.h"
#include "InteReal2DFloorPlanTypes.generated.h"

USTRUCT(BlueprintType)
struct FInteReal2DFloorPlanPolygon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D")
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D")
	TArray<FVector2D> Points;
};

USTRUCT(BlueprintType)
struct FInteReal2DFloorPlanOpening
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D")
	FString Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D")
	FVector2D Start = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D")
	FVector2D End = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct FInteReal2DFloorPlanWallSegment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|FloorPlan")
	FString WallId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|FloorPlan")
	FVector2D Start = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|FloorPlan")
	FVector2D End = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|FloorPlan")
	float ThicknessCm = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|FloorPlan")
	FString Type;
};

USTRUCT(BlueprintType)
struct FInteReal2DFloorPlanDocument
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D")
	TArray<FInteReal2DFloorPlanPolygon> Rooms;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D")
	TArray<FInteReal2DFloorPlanOpening> Openings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D")
	FVector2D BoundsMin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D")
	FVector2D BoundsMax = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D")
	bool bIsValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D")
	bool bFlipYForScreenSpace = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|FloorPlan")
	TArray<FInteReal2DFloorPlanWallSegment> Walls;
};

USTRUCT(BlueprintType)
struct FInteReal2DPlacedFurniture
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
	FGuid InstanceGuid;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
	int32 FurnitureID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
	FVector2D CenterDocumentPosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
	FVector2D Size = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
	float RotationDegrees = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector2D> FootprintLocalPoints;
};
