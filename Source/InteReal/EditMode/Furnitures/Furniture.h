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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	UBoxComponent* CollisionBoxComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	EPlacementState PlacementState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Outline")
	UMaterialInterface* ValidOutlineMat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Outline")
	UMaterialInterface* InvalidOutlineMat;

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

	EPlacementState GetPlacementState() const { return PlacementState; }

	bool SupportsPlacementType(EPlacementSurfaceType Type) const
	{
		return (AllowedPlacementTypes & static_cast<uint8>(Type)) != 0;
	}

	EPlacementSurfaceType GetPlacedSurfaceType() const { return PlacedSurfaceType; }
	void SetPlacedSurfaceType(EPlacementSurfaceType Type) { PlacedSurfaceType = Type; }

	// 기즈모 포함 전체 바운드 아니고 콜리전 박스만 반환 (배치 겹침 판정용)
	FBox GetCollisionBounds() const
	{
		if (CollisionBoxComponent)
		{
			return CollisionBoxComponent->Bounds.GetBox();
		}
		return MeshComponent->Bounds.GetBox();
	}

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void SetPlacementState(EPlacementState NewState);

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void ApplyFurnitureRow(const FFurnitureDataRow& InFurnitureRow);

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void SetSelected(bool bSelected);

};
