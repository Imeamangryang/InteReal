#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IPlacementHandler.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "DoorWindowPlacementHandler.generated.h"

class AFurniture;
class UInteriorPlacementSubsystem;

UCLASS()
class INTEREAL_API UDoorWindowPlacementHandler : public UObject, public IPlacementHandler
{
	GENERATED_BODY()

public:
	virtual void Initialize(UInteriorPlacementSubsystem* InSubsystem) override;
	virtual bool CanHandle(const FHitResult& Hit) const override;
	virtual bool OwnsFurniture(const AFurniture* Furniture) const override;
	virtual void UpdatePreview(AFurniture* Preview, const FHitResult& Hit) override;
	virtual void OnConfirm(AFurniture* Furniture, bool bIsValid = true) override;
	virtual void OnRemove(AFurniture* Furniture) override;

	virtual void BeginGizmoMove(AFurniture* Target) override;
	virtual void UpdateGizmoMove(AFurniture* Target, FVector Cursor, EGizmoTransformAxis Axis) override;
	virtual void UpdateGizmoMoveFree(AFurniture* Target, FVector TargetLoc) override;
	virtual void FinalizeGizmoMove(AFurniture* Target) override;
	virtual void AbortGizmoMove(AFurniture* Target) override;

private:
	UPROPERTY()
	TObjectPtr<UInteriorPlacementSubsystem> Subsystem = nullptr;

	FVector CurrentWallNormal = FVector::ZeroVector;
	FVector GizmoDragStartLocation = FVector::ZeroVector;
	FVector2D GizmoWallSegStart = FVector2D::ZeroVector;
	FVector2D GizmoWallSegEnd = FVector2D::ZeroVector;

	bool IsOpeningRow(const FFurnitureDataRow& Row) const;
	EPlacementAssetKind ResolveOpeningKind(const FFurnitureDataRow& Row) const;
	bool FindNearestWallSegment(const FVector2D& Point2D, FVector2D& OutSegStart, FVector2D& OutSegEnd) const;
	void ApplyOpeningAlignedRotation(AFurniture* Target, const FVector2D& WallNormal) const;
	void ApplyOpeningFitScale(AFurniture* Target, const FFurnitureDataRow& Row, EPlacementAssetKind Kind) const;
	FVector ComputeOpeningSnappedLocation(AFurniture* Target,
	                                      const FFurnitureDataRow& Row,
	                                      EPlacementAssetKind Kind,
	                                      const FVector2D& CursorXY,
	                                      const FVector2D& SegStart,
	                                      const FVector2D& SegEnd,
	                                      const FVector2D* FixedWallNormal = nullptr);
	bool ValidateOpeningPlacement(AFurniture* Target, const FFurnitureDataRow& Row, const FVector2D& SegStart, const FVector2D& SegEnd) const;

	static FVector GetNativeMeshSize(const AFurniture* Target);
	static float ResolveSlotWidth(const FFurnitureDataRow& Row, EPlacementAssetKind Kind, const FVector& NativeSize);
	static float ResolveSlotHeight(const FFurnitureDataRow& Row, EPlacementAssetKind Kind, const FVector& NativeSize);
	static float ResolveSlotDepth(const FFurnitureDataRow& Row, const UInteriorPlacementSubsystem* Subsystem, const FVector& NativeSize);
	static float ResolveOpeningBottom(const FFurnitureDataRow& Row, EPlacementAssetKind Kind);
	static FVector2D ProjectPointOnSegment(const FVector2D& Point, const FVector2D& SegStart, const FVector2D& SegEnd, float& OutT);
};
