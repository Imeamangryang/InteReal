#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IPlacementHandler.h"
#include "WallPlacementHandler.generated.h"

class UInteriorPlacementSubsystem;
class AFurniture;

UCLASS()
class INTEREAL_API UWallPlacementHandler : public UObject, public IPlacementHandler
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
	
	FVector GetCurrentWallNormal() const { return CurrentWallNormal; }
	void SetCurrentWallNormal(FVector N) { CurrentWallNormal = N; }

private:
	UPROPERTY()
	UInteriorPlacementSubsystem* Subsystem = nullptr;

	FVector CurrentWallNormal = FVector::ZeroVector;
	FVector GizmoDragStartLocation = FVector::ZeroVector;
	FVector2D GizmoWallSegStart = FVector2D::ZeroVector;
	FVector2D GizmoWallSegEnd = FVector2D::ZeroVector;

	bool FindNearestWallSegment(const FVector2D& Point2D, FVector2D& OutSegStart, FVector2D& OutSegEnd) const;
	void ApplyWallAlignedRotation(AFurniture* Target, FVector2D WallNormal) const;
	FVector ComputeWallSnappedLocation(AFurniture* Target,
	                                   const FVector2D& CursorXY,
	                                   const FVector2D& SegStart,
	                                   const FVector2D& SegEnd,
	                                   float Z,
	                                   const FVector2D* FixedWallNormal = nullptr);
	static FVector2D ProjectPointOnSegment(FVector2D Point, FVector2D SegStart, FVector2D SegEnd, float& OutT);
};
