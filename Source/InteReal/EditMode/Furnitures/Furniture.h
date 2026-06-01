#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "FurnitureData.h"
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
	EPlacementState PlacementState;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	UFurnitureData* FurnitureData;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterial;

	// ConfirmFurniture 시점에 매니저가 주입. RemoveFurniture에서 전수조사 없이 해당 타일만 정리하는 데 사용
	FVector2D PlacedGridAnchor = FVector2D::ZeroVector;
	FVector2D PlacedDimensions = FVector2D::ZeroVector;

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void SetPlacementState(EPlacementState NewState);

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void ApplyFurnitureData(UFurnitureData* InFurnitureData);
};
