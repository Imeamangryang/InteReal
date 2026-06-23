#pragma once

#include "CoreMinimal.h"
#include "InteReal2DFloorPlanTypes.h"

struct FInteReal2DFloorPlanViewTransform
{
	FInteReal2DFloorPlanDocument Document;

	FVector2D LocalSize = FVector2D::ZeroVector;
	FVector2D DrawOffset = FVector2D::ZeroVector;
	FVector2D DrawSizeOverride = FVector2D::ZeroVector;

	bool bUseDrawSizeOverride = false;
	bool bFlipYForScreenSpace = false;

	float ViewPadding = 32.0f;
	float DrawRotationDegrees = 90.0f;

	FVector2D GetDocumentSize() const;
	FVector2D GetDrawAreaOffset() const;
	FVector2D GetDrawAreaSize() const;
	bool IsLocalPointInsideDrawArea(const FVector2D& LocalPoint) const;

	FVector2D DocumentToLocal(const FVector2D& DocumentPoint) const;
	FVector2D LocalToDocument(const FVector2D& LocalPoint) const;
};