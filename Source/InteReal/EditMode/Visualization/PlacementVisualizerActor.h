#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/DecalComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/DataTable.h"
#include "InteReal/EditMode/Furnitures/Furniture.h"
#include "PlacementVisualizerActor.generated.h"

// 레벨에 배치되는 시각/설정 전담 Actor.
// 로직은 UInteriorPlacementSubsystem이 담당하고, 이 Actor는 렌더링과 에디터 설정값만 소유한다.
UCLASS()
class INTEREAL_API APlacementVisualizerActor : public AActor
{
	GENERATED_BODY()

public:
	APlacementVisualizerActor();

protected:
	virtual void BeginPlay() override;

public:
	// ===== 설정 (에디터에서 지정) =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TSubclassOf<AFurniture> FurnitureClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	UDataTable* FurnitureDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Wall")
	float WallThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grid")
	int32 GridLength = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grid")
	int32 GridBreadth = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grid")
	float GridCellSize = 10.0f;

	// ===== 시각 컴포넌트 =====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual|Grid")
	UDynamicMeshComponent* GridMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual|Grid")
	UDecalComponent* GridDecal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|Grid")
	UMaterialInterface* GridMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual|Placement")
	UDynamicMeshComponent* PlacementVizValid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual|Placement")
	UDynamicMeshComponent* PlacementVizInvalid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|Placement")
	UMaterialInterface* ValidCellMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual|Placement")
	UMaterialInterface* InvalidCellMaterial;

	// ===== Subsystem이 호출하는 시각 갱신 API =====
	void SetGridVisible(bool bVisible);
	void RebuildGridMesh(const TArray<FVector2D>& FloorPolygon, float InGridCellSize, float FloorZ);
	void RefreshPlacementCellViz(const FBox& FurnitureBounds, bool bInvalid, float ManagerZ);
	void ClearPlacementCellViz();
	void SetFloorZ(float Z);

private:
	UPROPERTY()
	UMaterialInstanceDynamic* GridDynMat = nullptr;
};
