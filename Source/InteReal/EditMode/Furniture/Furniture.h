#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "FFurnitureDataRow.h"
#include "Furniture.generated.h"

UENUM(BlueprintType)
enum class EPlacementState : uint8
{
	Preview  UMETA(DisplayName = "Preview"),
	Invalid  UMETA(DisplayName = "Invalid"),
	Placed   UMETA(DisplayName = "Placed")
};

UCLASS()
class INTEREAL_API AFurniture : public AActor
{
	GENERATED_BODY()

public:
	AFurniture();

protected:
	void SetMeshesCustomDepth(bool bEnabled, int32 StencilValue);
	
	void SetMeshesVisibilityCollision(ECollisionResponse Response);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	UBoxComponent* CollisionBoxComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	EPlacementState PlacementState;

	// Full local-space bounds used by placement. Prefer authored simple collision
	// over render bounds so build scale and stray render vertices do not move surfaces.
	FBox PlacementLocalBounds = FBox(EForceInit::ForceInit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Outline")
	UMaterialInterface* ValidOutlineMat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Outline")
	UMaterialInterface* InvalidOutlineMat;

	// OutlineThickness
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Outline")
	float PreviewOutlineThickness = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Outline")
	float PlacedOutlineThickness = 1.0f;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	int32 FurnitureID = 0;

	// 이 가구를 배치할 수 있는 표면 비트마스크
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	uint8 AllowedPlacementTypes = static_cast<uint8>(EPlacementSurfaceType::Floor);

	// 실제로 배치가 확정된 표면
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	EPlacementSurfaceType PlacedSurfaceType = EPlacementSurfaceType::Floor;

	FVector2D PlacedGridAnchor = FVector2D::ZeroVector;
	FVector2D PlacedDimensions = FVector2D::ZeroVector;

	// 벽 배치 시 벽 노멀 저장 (기즈모 이동 시 재사용)
	FVector WallNormalAtPlacement = FVector::ZeroVector;

	UPROPERTY()
	AFurniture* ParentFurniture = nullptr;

	EPlacementState GetPlacementState() const { return PlacementState; }

	bool SupportsPlacementType(EPlacementSurfaceType Type) const
	{
		return (AllowedPlacementTypes & static_cast<uint8>(Type)) != 0;
	}

	EPlacementSurfaceType GetPlacedSurfaceType() const { return PlacedSurfaceType; }
	void SetPlacedSurfaceType(EPlacementSurfaceType Type) { PlacedSurfaceType = Type; }
	
	FBox GetCollisionBounds() const
	{
		if (CollisionBoxComponent)
		{
			return CollisionBoxComponent->Bounds.GetBox();
		}
		return MeshComponent->Bounds.GetBox();
	}

	FBox GetVisualBounds() const
	{
		return GetComponentsBoundingBox(true);
	}

	FBox GetMeshBounds() const
	{
		return MeshComponent ? MeshComponent->Bounds.GetBox() : GetVisualBounds();
	}

	FBox GetPlacementGeometryBounds() const
	{
		if (MeshComponent && PlacementLocalBounds.IsValid)
		{
			return PlacementLocalBounds.TransformBy(MeshComponent->GetComponentTransform());
		}
		return GetMeshBounds();
	}
	
	float GetPivotToBottomOffsetZ() const
	{
		return CollisionBoxComponent ? CollisionBoxComponent->GetRelativeLocation().Z : 0.0f;
	}

	void AlignMeshBottomToZ(float SurfaceZ);
	void AlignMeshBottomCenterTo(const FVector& TargetCenter, float SurfaceZ);
	void AlignPlacementBottomCenterTo(const FVector& TargetCenter, float SurfaceZ);
	void SetRotationPreservingPlacement(const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void SetPlacementState(EPlacementState NewState);

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void ApplyFurnitureRow(const FFurnitureDataRow& InFurnitureRow);

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void SetSelected(bool bSelected);
};
