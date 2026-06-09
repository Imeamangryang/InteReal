#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "InteReal2DFloorPlanTypes.h"
#include "InteReal2DFloorPlanCreator.generated.h"

UCLASS(BlueprintType)
class INTEREAL_API UInteReal2DFloorPlanCreator : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="InteReal2D|FloorPlan")
	bool BuildFromJson(const FString& JsonData);

	UFUNCTION(BlueprintCallable, Category="InteReal2D|FloorPlan")
	bool BuildFromHarnessData(const FHarnessFloorData& InFloorData);

	UFUNCTION(BlueprintPure, Category="InteReal2D|FloorPlan")
	const FInteReal2DFloorPlanDocument& GetDocument() const { return Document; }

	UFUNCTION(BlueprintPure, Category="InteReal2D|FloorPlan")
	const FHarnessFloorData& GetSourceFloorData() const { return SourceFloorData; }

	UFUNCTION(BlueprintPure, Category="InteReal2D|FloorPlan")
	bool HasValidDocument() const { return Document.bIsValid; }

	UFUNCTION(BlueprintCallable, Category="InteReal2D|FloorPlan")
	void Clear();

private:
	bool ParseHarnessFloorDataFromJson(const FString& JsonData, FHarnessFloorData& OutFloorData) const;

private:
	UPROPERTY(VisibleAnywhere, Category="InteReal2D|FloorPlan")
	FHarnessFloorData SourceFloorData;

	UPROPERTY(VisibleAnywhere, Category="InteReal2D|FloorPlan")
	FInteReal2DFloorPlanDocument Document;
};