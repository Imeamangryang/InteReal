#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "InteReal/EditMode/Gizmo/InteRealGizmoTypes.h"
#include "InteReal/EditMode/Placement/IPlacementHandler.h"
#include "InteRealGizmoComponent.generated.h"

class AFurniture;
class APlayerController;
class UInteriorPlacementSubsystem;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FInteRealGizmoVisualPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gizmo")
	FName AxisTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gizmo")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gizmo")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gizmo")
	FTransform RelativeTransform = FTransform::Identity;
};

UCLASS(ClassGroup=(InteReal), meta=(BlueprintSpawnableComponent))
class INTEREAL_API UInteRealGizmoComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UInteRealGizmoComponent();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	void SetSelectedActive(bool bActive);
	bool IsSelectedActive() const { return bSelectedActive; }

	void SetGizmoHidden(bool bHidden);

	void SetDisplayMode(EInteRealGizmoDisplayMode NewMode);
	EInteRealGizmoDisplayMode GetDisplayMode() const { return DisplayMode; }
	void SetShowMove(bool bShow);
	void SetShowRotate(bool bShow);
	bool IsShowingMove() const;
	bool IsShowingRotate() const;

	void UpdateAnchorFromOwner();
	void UpdateHover(bool bIsHitting, const FHitResult& CursorHit);
	bool BeginDrag(const FString& Axis, const FVector& WorldOrigin, const FVector& WorldDir, const FVector2D& MousePos);
	bool UpdateDrag(const FVector& WorldOrigin, const FVector& WorldDir,
		UInteriorPlacementSubsystem* PlacementSubsystem, bool bSnapRotationToGrid, const FVector2D& MousePos);
	void EndDrag();

	void UpdateConstantScreenSize(APlayerController* PlayerController, float ScaleMultiplier = 1.0f);
	FBox GetVisibleGizmoBounds() const;

	static FString GetAxisTagFromComponent(const UPrimitiveComponent* Component);
	FString GetAxisTagFromHit(const FHitResult& CursorHit) const;
	bool OwnsGizmoComponent(const UPrimitiveComponent* Component) const;

	bool IsDragging() const { return bIsDragging; }
	EGizmoTransformAxis GetCurrentAxis() const { return CurrentDraggingAxis; }
	const FString& GetCurrentAxisTag() const { return CurrentDraggingAxisTag; }
	float GetCurrentRotationDeltaDegrees() const { return CurrentRotationDeltaDegrees; }
	FVector GetAnchorLocation() const;

	UPROPERTY(EditAnywhere, Category = "Gizmo|Visual")
	TArray<FInteRealGizmoVisualPart> VisualParts;

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float ReferenceDistance = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float MinScreenScale = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float MaxScreenScale = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float TargetScreenDiameterPixels = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Gizmo")
	float ScreenSizeScale = 0.5f;

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
	void BuildGeneratedVisuals();
	void DestroyGeneratedVisuals();
	void ConfigureGizmoCollision(UPrimitiveComponent* Component, bool bEnabled) const;
	void InitAxisMaterials();
	void SetAxisOutline(const FString& Axis, bool bEnable);
	void SetAxisColorHighlight(const FString& Axis, bool bEnable);
	FLinearColor GetBaseAxisColor(const FString& Axis) const;
	void SetAxisRotationVisuals(const FString& Axis, float DeltaAngle, bool bSnapped);
	void ResetRotationVisuals();
	EGizmoTransformAxis ParseAxisTag(const FString& AxisTag) const;
	float ApplyCardinalSnap(float AngleDegrees) const;
	bool ComputeRotationPlaneAngle(const FVector& WorldOrigin, const FVector& WorldDir, float& OutAngleDeg) const;
	AFurniture* GetOwnerFurniture() const;
	bool IsMoveAxisVisible(const FString& Axis) const;
	bool IsRotationAxisVisible(const FString& Axis) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> GeneratedMeshes;

	TMap<FString, TArray<TObjectPtr<UMaterialInstanceDynamic>>> AxisMaterials;
	TMap<FString, TArray<TObjectPtr<UMeshComponent>>> AxisMeshes;
	TMap<FString, FLinearColor> OriginalAxisColors;

	FString HoveredAxis;
	EInteRealGizmoDisplayMode DisplayMode = EInteRealGizmoDisplayMode::All;
	bool bSelectedActive = false;
	bool bIsGizmoHidden = false;
	bool bVisualsBuilt = false;

	bool bIsDragging = false;
	EGizmoTransformAxis CurrentDraggingAxis = EGizmoTransformAxis::None;
	FString CurrentDraggingAxisTag;
	float DragStartAngleDeg = 0.0f;
	float LastRotationMouseAngleDeg = 0.0f;
	float AccumulatedRotationDeltaDegrees = 0.0f;
	FVector RotationPivotWorld = FVector::ZeroVector;
	FVector RotationAxisWorld = FVector::UpVector;
	FVector RotationBasisU = FVector::ForwardVector;
	FVector RotationBasisV = FVector::RightVector;
	FRotator DragStartFurnitureRot = FRotator::ZeroRotator;
	float CurrentRotationDeltaDegrees = 0.0f;
	FVector DragStartLocation = FVector::ZeroVector;
	FVector DragCursorOffset = FVector::ZeroVector;
	FVector2D DragStartMousePos = FVector2D::ZeroVector;
};

