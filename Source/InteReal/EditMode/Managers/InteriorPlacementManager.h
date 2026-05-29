// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/DecalComponent.h"
#include "GridSpaceManager.h"
#include "Intereal/EditMode/Furnitures/Furniture.h"
#include "Intereal/EditMode/Furnitures/FurnitureData.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "InteriorPlacementManager.generated.h"

UCLASS()
class INTEREAL_API AInteriorPlacementManager : public AActor
{
	GENERATED_BODY()

public:
	AInteriorPlacementManager();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
	UDecalComponent* GridDecal;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	UMaterialInterface* GridMaterial;

	// 도면 없이 수동 테스트할 때만 사용. 평소엔 InitializeFromFloorData로 자동 계산
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Manual")
	int GridLength = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Manual")
	int GridBreadth = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Manual")
	float GridCellSize = 50.0f;

	// 배치 가능한 가구 데이터 목록 (FurnitureID = index)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	TArray<UFurnitureData*> FurnitureDataList;

private:
	AGridSpaceManager* Grid;
	float CellSize;

	AFurniture* PreviewFurniture;
	UFurnitureData* PreviewFurnitureData;
	FVector2D PreviewGridAnchor;

	TArray<AFurniture*> PlacedFurnitures;

public:
	// 도면 파싱 후 호출 — vertex 좌표로 바운딩 박스 자동 계산
	UFUNCTION(BlueprintCallable)
	void InitializeFromFloorData(const FHarnessFloorData& FloorData, float Cell = 50.0f);

	UFUNCTION(BlueprintCallable)
	void InitializeGrid(int Length, int Breadth, float Cell);

	UFUNCTION(BlueprintCallable)
	void SetGridVisible(bool bVisible);

	UFUNCTION(BlueprintPure)
	bool HasActivePreview() const;

	UFUNCTION(BlueprintPure)
	bool IsPreviewLotEmpty();

	bool IsPreviewBoundsEmpty() const;
	void DrawBounds() const;

	UFUNCTION(BlueprintCallable)
	void ConfirmFurniture();

	UFUNCTION(BlueprintCallable)
	void CreatePreviewFurnitureFromData(FVector RayPosition, FRotator Rotation, UFurnitureData* InFurnitureData);

	UFUNCTION(BlueprintCallable)
	void UpdatePreviewLocation(FVector RayPosition);

	UFUNCTION(BlueprintCallable)
	void CancelPreview();

	UFUNCTION(BlueprintCallable)
	void RemoveFurniture(AFurniture* Target);
};
