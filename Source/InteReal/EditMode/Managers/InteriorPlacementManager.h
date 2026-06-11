// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/DecalComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/DataTable.h"
#include "GridSpaceManager.h"
#include "Intereal/EditMode/Furnitures/Furniture.h"
#include "Intereal/EditMode/Furnitures/FFurnitureDataRow.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "InteriorPlacementManager.generated.h"

UENUM(BlueprintType)
enum class EPlacementInvalidReason : uint8
{
	None UMETA(DisplayName = "없음"),
	Overlapping UMETA(DisplayName = "다른 가구와 겹칩니다"),
	OutOfBounds UMETA(DisplayName = "배치 가능 영역을 벗어났습니다"),
};

class UMeshComponent;

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

	// 도면 모양으로 잘린 그리드 메시 (데칼 대체)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
	UDynamicMeshComponent* GridMeshComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	UMaterialInterface* GridMaterial;



	// 배치 유효성 시각화 — 가구 footprint 아래 통으로 색 표시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlacementViz")
	UDynamicMeshComponent* PlacementVizValid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlacementViz")
	UDynamicMeshComponent* PlacementVizInvalid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlacementViz")
	UMaterialInterface* ValidCellMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlacementViz")
	UMaterialInterface* InvalidCellMaterial;

	// 도면 없이 수동 테스트할 때만 사용. 평소엔 InitializeFromFloorData로 자동 계산
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Manual")
	int GridLength = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Manual")
	int GridBreadth = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Manual")
	float GridCellSize = 10.0f;

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
	FRotator PreviewRotation = FRotator::ZeroRotator;
	FVector LastRayPosition = FVector::ZeroVector;

	TArray<AFurniture*> PlacedFurnitures;

	UPROPERTY()
	UMaterialInstanceDynamic* GridDynMat = nullptr;

public:
	UFUNCTION(BlueprintCallable)
	void InitializeFromFloorData(const FHarnessFloorData& FloorData, float Cell = 50.0f);

	UFUNCTION(BlueprintCallable)
	void InitializeGrid(int Length, int Breadth, float Cell);

	UFUNCTION(BlueprintCallable)
	void SetGridVisible(bool bVisible);

	UFUNCTION(BlueprintPure)
	bool HasActivePreview() const;

	AFurniture* GetPreviewFurniture() const { return PreviewFurniture; }

	UFUNCTION(BlueprintPure)
	bool IsPreviewLotEmpty();

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
	
	UFUNCTION(BlueprintCallable, Category = "Interior | History")
	FString ExportEditStateJson();

	UFUNCTION(BlueprintCallable, Category = "Interior | History")
	void ImportEditStateJson(const FString& JsonString);
	
	UFUNCTION(BlueprintCallable, Category = "Interior | History")
	void RecordUndoSnapshot();

	UFUNCTION(BlueprintCallable, Category = "Interior | History")
	void Undo();

	UFUNCTION(BlueprintCallable, Category = "Interior | History")
	void Redo();

	UFUNCTION(BlueprintPure, Category = "Interior | History")
	bool CanUndo() const { return UndoStack.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "Interior | History")
	bool CanRedo() const { return RedoStack.Num() > 0; }

	const FFurnitureDataRow* FindFurnitureRowByID(int32 TargetID) const;

	// 기즈모 이동 드래그 — 시작/업데이트/확정/취소
	UFUNCTION(BlueprintCallable)
	void BeginGizmoMove(AFurniture* Target);

	// 축 제한 이동 (EditModePlayerController 기즈모 화살표용)
	UFUNCTION(BlueprintCallable)
	void UpdateGizmoMoveLocation(FVector CursorOnGround, AFurniture* Target, const FString& Axis);

	// 자유 이동 (InteRealPlayerController 드래그용 — X/Y 동시 추적)
	UFUNCTION(BlueprintCallable)
	void UpdateGizmoMoveFree(FVector TargetWorldLocation, AFurniture* Target);

	UFUNCTION(BlueprintCallable)
	void FinalizeGizmoMove(AFurniture* Target);

	UFUNCTION(BlueprintCallable)
	void AbortGizmoMove(AFurniture* Target);

private:
	FVector2D GizmoDragOriginalAnchor;
	FVector GizmoDragStartLocation;
	
	static constexpr int32 MaxHistoryCount = 100;

	TArray<FString> UndoStack;
	TArray<FString> RedoStack;

	bool bRestoringHistory = false;

	bool bHasPendingGizmoUndoSnapshot = false;
	FString PendingGizmoUndoSnapshot;

	void PushUndoSnapshot(const FString& Snapshot);

	// 도면 외곽 폴리곤 (WallOuter 정점, 월드 좌표)
	TArray<FVector2D> FloorPolygon;

	void RefreshPlacementCellViz(AFurniture* Target, bool bInvalid);
	void ClearPlacementCellViz();
	bool IsEditableSurfaceComponent(const UMeshComponent* MeshComp) const;
	void ExportSurfaceMaterials(TArray<TSharedPtr<FJsonValue>>& OutArray) const;
	void ImportSurfaceMaterials(const TArray<TSharedPtr<FJsonValue>>& SurfaceArray);
	void BuildFloorPolygon(const FHarnessFloorData& FloorData);
	void BuildWallSegments(const FHarnessFloorData& FloorData);
	void MarkOutOfBoundsTiles();
	void RebuildGridMesh();
	static bool IsPointInPolygon(FVector2D Point, const TArray<FVector2D>& Polygon);
	bool IsFurnitureCornersInsideFloor(AFurniture* Target) const;
	bool FurnitureIntersectsWalls(AFurniture* Target) const;

	// 내벽 세그먼트 (문/창 개구부 제외)
	TArray<TPair<FVector2D, FVector2D>> InnerWallSegments;

};
