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
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	UBoxComponent* CollisionBoxComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	EPlacementState PlacementState;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	int32 FurnitureID = 0;

	FVector2D PlacedGridAnchor = FVector2D::ZeroVector;
	FVector2D PlacedDimensions = FVector2D::ZeroVector;

	EPlacementState GetPlacementState() const { return PlacementState; }

	// 기즈모 포함 전체 바운드가 아닌 콜리전 박스만 반환 — 배치 겹침 판정용
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
