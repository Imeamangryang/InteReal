// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/DecalComponent.h"
#include "Engine/DataTable.h"
#include "GridSpaceManager.h"
#include "Intereal/EditMode/Furnitures/Furniture.h"
#include "Intereal/EditMode/Furnitures/FFurnitureDataRow.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "InteriorPlacementManager.generated.h"

UENUM(BlueprintType)
enum class EPlacementInvalidReason : uint8
{
	None          UMETA(DisplayName = "없음"),
	Overlapping   UMETA(DisplayName = "다른 가구와 겹칩니다"),
	OutOfBounds   UMETA(DisplayName = "배치 가능 영역을 벗어났습니다"),
};

UCLASS()
class INTEREAL_API AInteriorPlacementManager : public AActor
{
	GENERATED_BODY()

public:
	AInteriorPlacementManager();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

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
	float GridCellSize = 10.0f;

	// 프리뷰 이동 보간 속도 (높을수록 빠르게 따라옴, 10~20 권장)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Preview")
	float PreviewInterpSpeed = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	UDataTable* FurnitureDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	TSubclassOf<AFurniture> FurnitureClass;

	// 현재 배치 불가 이유 — UI에서 툴팁으로 표시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	EPlacementInvalidReason InvalidReason = EPlacementInvalidReason::None;

private:
	AGridSpaceManager* Grid;

	AFurniture* PreviewFurniture;
	FVector2D PreviewGridAnchor;

	FVector2D CurrentDimensions = FVector2D::ZeroVector;
	FRotator PreviewRotation    = FRotator::ZeroRotator;
	FVector LastRayPosition     = FVector::ZeroVector;

	// 보간 목표 위치 — Tick에서 lerp
	FVector TargetPreviewLocation = FVector::ZeroVector;
	bool bHasTargetLocation = false;

	TArray<AFurniture*> PlacedFurnitures;

public:
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

	UFUNCTION(BlueprintCallable)
	void ConfirmFurniture();

	UFUNCTION(BlueprintCallable)
	void CreatePreviewFurnitureFromRow(FVector RayPosition, FRotator Rotation, const FFurnitureDataRow& InFurnitureRow);

	UFUNCTION(BlueprintCallable)
	void UpdatePreviewLocation(FVector RayPosition);

	UFUNCTION(BlueprintCallable)
	void CancelPreview();

	UFUNCTION(BlueprintCallable)
	void RemoveFurniture(AFurniture* Target);

	UFUNCTION(BlueprintCallable)
	void RotatePreview(float AngleDeg = 90.0f);

	UFUNCTION(BlueprintCallable, Category = "Interior | WebCommunication")
	FString ExportPlacedFurnituresJson();

	UFUNCTION(BlueprintCallable, Category = "Interior | WebCommunication")
	void ImportPlacedFurnituresJson(const FString& JsonString);

	const FFurnitureDataRow* FindFurnitureRowByID(int32 TargetID) const;
};
