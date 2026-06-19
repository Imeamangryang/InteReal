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
};
