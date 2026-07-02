#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IPlacementHandler.h"
#include "SurfacePlacementHandler.generated.h"

class UInteriorPlacementSubsystem;
class AFurniture;

UCLASS()
class INTEREAL_API USurfacePlacementHandler : public UObject, public IPlacementHandler
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
	UInteriorPlacementSubsystem* Subsystem = nullptr;
	
	UPROPERTY()
	AFurniture* CurrentSurfaceParent = nullptr;

	FVector GizmoDragStartLocation = FVector::ZeroVector;
};
