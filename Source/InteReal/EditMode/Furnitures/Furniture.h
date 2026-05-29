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

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void SetPlacementState(EPlacementState NewState);

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void ApplyFurnitureData(UFurnitureData* InFurnitureData);
};