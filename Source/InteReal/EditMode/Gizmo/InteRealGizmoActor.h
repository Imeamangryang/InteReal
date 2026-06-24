#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "InteReal/EditMode/Placement/IPlacementHandler.h"
#include "InteRealGizmoActor.generated.h"

class AFurniture;
class APlayerController;
class UInteriorPlacementSubsystem;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EInteRealGizmoDisplayMode : uint8
{
	All,
	Move,
	Rotation,
	None
};

UCLASS()
class INTEREAL_API AInteRealGizmoActor : public AActor
{
	GENERATED_BODY()

public:
	AInteRealGizmoActor();
	
	void InitAxisMaterials();
	static FString GetAxisTagFromComponent(const UPrimitiveComponent* Component);
	void SetDisplayMode(EInteRealGizmoDisplayMode NewMode);
	EInteRealGizmoDisplayMode GetDisplayMode() const { return DisplayMode; }
	
	void UpdateHover(bool bIsHitting, const FHitResult& CursorHit);
	
	void BeginDrag(const FString& Axis, AFurniture* Target, const FVector& WorldOrigin, const FVector& WorldDir, const FVector2D& MousePos);
	
	void UpdateDrag(AFurniture* Target, const FVector& WorldOrigin, const FVector& WorldDir,
		UInteriorPlacementSubsystem* PlacementSubsystem, bool bSnapRotationToGrid, const FVector2D& MousePos);
	
	void EndDrag();
	
	void UpdateConstantScreenSize(APlayerController* PlayerController, float ScaleMultiplier = 1.0f);
	FBox GetVisibleGizmoBounds() const;
	
	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float ReferenceDistance = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float MinScreenScale = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float MaxScreenScale = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float TargetScreenDiameterPixels = 180.0f;

	bool IsDragging() const { return bIsDragging; }
	EGizmoTransformAxis GetCurrentAxis() const { return CurrentDraggingAxis; }
	const FString& GetCurrentAxisTag() const { return CurrentDraggingAxisTag; }
	float GetCurrentRotationDeltaDegrees() const { return CurrentRotationDeltaDegrees; }

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float RotationSensitivity = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Gizmo|Rotation")
	float CardinalSnapIntervalDegrees = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Gizmo|Rotation")
	float CardinalSnapToleranceDegrees = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Gizmo|Rotation")
	FName RadialWipeParamName = TEXT("RadialWipe");

	UPROPERTY(EditAnywhere, Category = "Gizmo|Rotation")
	FName SnapHighlightParamName = TEXT("SnapHighlight");

	UPROPERTY(EditAnywhere, Category = "Gizmo|Rotation")
	FName RotationDirectionParamName = TEXT("RotationDirection");

	// MoveX/Y/Z 아웃라인 → 흰색
	UPROPERTY(EditAnywhere, Category = "Gizmo|Outline")
	int32 MoveOutlineStencil = 2;
	
	UPROPERTY(EditAnywhere, Category = "Gizmo|Outline")
	int32 RotateOutlineStencil = 3;

	UPROPERTY(EditAnywhere, Category = "Gizmo|Highlight")
	FName GizmoColorParamName = TEXT("GizmoColor");

	UPROPERTY(EditAnywhere, Category = "Gizmo|Highlight")
	FLinearColor ActiveAxisColor = FLinearColor(1.0f, 0.65f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float ZDragSensitivity = 1.0f;

private:
	void SetAxisOutline(const FString& Axis, bool bEnable);
	void SetAxisColorHighlight(const FString& Axis, bool bEnable);
	FLinearColor GetBaseAxisColor(const FString& Axis) const;
	void SetAxisRotationVisuals(const FString& Axis, float DeltaAngle, bool bSnapped);
	void ResetRotationVisuals();
	FString GetAxisTagFromHit(const FHitResult& CursorHit) const;
	EGizmoTransformAxis ParseAxisTag(const FString& AxisTag) const;
	float ApplyCardinalSnap(float AngleDegrees) const;
	bool ComputeRotationPlaneAngle(const FVector& WorldOrigin, const FVector& WorldDir, float& OutAngleDeg) const;

	bool bIsDragging = false;
	EGizmoTransformAxis CurrentDraggingAxis = EGizmoTransformAxis::None;
	FString CurrentDraggingAxisTag;

	float DragStartAngleDeg = 0.0f;
	float LastRotationMouseAngleDeg = 0.0f;
	float AccumulatedRotationDeltaDegrees = 0.0f;

	// 회전 드래그 동안 고정되는 3D 회전 평면 기준점/축/평면 내 2D 기저 벡터.
	// 드래그 중 가구가 실제로 회전해도 이 기준은 BeginDrag 시점 값으로 고정해야
	// 측정 기준 자체가 같이 돌아가며 입력이 왜곡되는 걸 막을 수 있다.
	FVector RotationPivotWorld = FVector::ZeroVector;
	FVector RotationAxisWorld = FVector::UpVector;
	FVector RotationBasisU = FVector::ForwardVector;
	FVector RotationBasisV = FVector::RightVector;
	bool bHasValidRotationFrame = false;
	FRotator DragStartFurnitureRot = FRotator::ZeroRotator;
	float CurrentRotationDeltaDegrees = 0.0f;
	FVector DragStartLocation = FVector::ZeroVector;
	FVector DragCursorOffset = FVector::ZeroVector;
	FVector2D DragStartMousePos = FVector2D::ZeroVector;

	TMap<FString, TArray<TObjectPtr<UMaterialInstanceDynamic>>> AxisMaterials;
	TMap<FString, TArray<TObjectPtr<UMeshComponent>>> AxisMeshes;
	TMap<FString, FLinearColor> OriginalAxisColors;
	FString HoveredAxis;
	EInteRealGizmoDisplayMode DisplayMode = EInteRealGizmoDisplayMode::All;
};
