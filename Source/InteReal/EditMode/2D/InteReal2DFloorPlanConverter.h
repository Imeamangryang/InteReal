#pragma once

#include "CoreMinimal.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "InteReal2DFloorPlanTypes.h"

class INTEREAL_API FInteReal2DFloorPlanConverter
{
public:
	static FInteReal2DFloorPlanDocument ConvertFromHarness(const FHarnessFloorData& FloorData);

private:
	static FVector2D ConvertTopologyVertexToEditorPoint(const FHarnessFloorData& FloorData, const FTopologyVertex& Vertex);
	static void ComputeBounds(FInteReal2DFloorPlanDocument& Document);
};
