#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "InteReal/EditMode/Placement/IPlacementHandler.h"
#include "InteRealGizmoActor.generated.h"

class AFurniture;
class UInteriorPlacementSubsystem;
class UMaterialInstanceDynamic;

UCLASS()
class INTEREAL_API AInteRealGizmoActor : public AActor
{
	GENERATED_BODY()

public:
	AInteRealGizmoActor();
	
	void InitAxisMaterials();
	
	void UpdateHover(bool bIsHitting, const FHitResult& CursorHit);
	
	void BeginDrag(const FString& Axis, AFurniture* Target, const FVector& WorldOrigin, const FVector& WorldDir, const FVector2D& MousePos);
	
	void UpdateDrag(AFurniture* Target, const FVector& WorldOrigin, const FVector& WorldDir,
		UInteriorPlacementSubsystem* PlacementSubsystem, bool bSnapRotationToGrid, const FVector2D& MousePos);
	
	void EndDrag();
	
	void UpdateConstantScreenSize(const FVector& CameraLocation, float CameraFOVDegrees);
	
	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float ReferenceDistance = 1000.0f;

	bool IsDragging() const { return bIsDragging; }
	EGizmoTransformAxis GetCurrentAxis() const { return CurrentDraggingAxis; }
	const FString& GetCurrentAxisTag() const { return CurrentDraggingAxisTag; }

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float RotationSensitivity = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	FName OpacityParamName = TEXT("Opacity");

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float DefaultOpacity = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float HighlightOpacity = 1.0f;
	
	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float ZDragSensitivity = 1.0f;

private:
	void SetAxisOpacity(const FString& Axis, float Opacity);
	FString GetAxisTagFromHit(const FHitResult& CursorHit) const;
	EGizmoTransformAxis ParseAxisTag(const FString& AxisTag) const;

	bool bIsDragging = false;
	EGizmoTransformAxis CurrentDraggingAxis = EGizmoTransformAxis::None;
	FString CurrentDraggingAxisTag;

	float DragStartAngleDeg = 0.0f;
	FRotator DragStartFurnitureRot = FRotator::ZeroRotator;
	FVector DragStartLocation = FVector::ZeroVector;
	FVector DragCursorOffset = FVector::ZeroVector;
	FVector2D DragStartMousePos = FVector2D::ZeroVector;

	TMap<FString, TArray<TObjectPtr<UMaterialInstanceDynamic>>> AxisMaterials;
	FString HoveredAxis;
};
