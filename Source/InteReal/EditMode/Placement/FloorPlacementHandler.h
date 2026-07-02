#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IPlacementHandler.h"
#include "FloorPlacementHandler.generated.h"

class UInteriorPlacementSubsystem;
class AFurniture;

UCLASS()
class INTEREAL_API UFloorPlacementHandler : public UObject, public IPlacementHandler
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

	// 도면 외곽/내벽 유효성 검사 (다른 핸들러에서도 재사용)
	bool IsCornersInsideFloor(const AFurniture* Target) const;
	bool IntersectsWalls(const AFurniture* Target) const;

private:
	UPROPERTY()
	UInteriorPlacementSubsystem* Subsystem = nullptr;

	// 기즈모 드래그 상태
	FVector2D GizmoDragOriginalAnchor = FVector2D::ZeroVector;
	FVector2D GizmoDragOriginalDimensions = FVector2D::ZeroVector;
	TArray<FIntPoint> GizmoDragOriginalOccupiedCells;
	FVector GizmoDragStartLocation = FVector::ZeroVector;
	FRotator GizmoDragStartRotation = FRotator::ZeroRotator;
};
