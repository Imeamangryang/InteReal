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
	UnsupportedSurface UMETA(DisplayName = "이 위치에는 배치할 수 없습니다"),
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

	// 배치 유효성 시각화 (가구 footprint 영역에 색 표시)
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

	// 벽 두께(cm). InnerWallSegments는 벽 중심선이라 실내측 표면에 붙이려면 이 값의 절반만큼 더 밀어내야 함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	float WallThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	UDataTable* FurnitureDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	TSubclassOf<AFurniture> FurnitureClass;

	// 현재 배치 불가 이유 (UI 툴팁 표시용)
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
	
	const TArray<AFurniture*>& GetPlacedFurnitures() const { return PlacedFurnitures; }

	UFUNCTION(BlueprintPure)
	bool IsPreviewLotEmpty();

	// bContinuePlacement가 true면 배치 확정 후 같은 가구로 새 프리뷰를 즉시 다시 생성 (Shift+클릭 연속 배치용)
	UFUNCTION(BlueprintCallable)
	void ConfirmFurniture(bool bContinuePlacement = false);

	UFUNCTION(BlueprintCallable)
	void CreatePreviewFurnitureFromRow(FVector RayPosition, FRotator Rotation, const FFurnitureDataRow& InFurnitureRow);

	// FurnitureMesh 크기(BoxExtent) 기준으로 Dimensions(그리드 셀 수) 자동 계산
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Furniture|Tools")
	void AutoFillFurnitureDimensions();

	UFUNCTION(BlueprintCallable)
	void UpdatePreviewLocation(const FHitResult& CursorHit);

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

	// 기즈모 이동 드래그 (시작/업데이트/확정/취소)
	UFUNCTION(BlueprintCallable)
	void BeginGizmoMove(AFurniture* Target);

	// 축 제한 이동 (EditModePlayerController 기즈모 화살표용)
	UFUNCTION(BlueprintCallable)
	void UpdateGizmoMoveLocation(FVector CursorOnGround, AFurniture* Target, const FString& Axis);

	// 자유 이동 (InteRealPlayerController 드래그용, X/Y 동시 추적)
	UFUNCTION(BlueprintCallable)
	void UpdateGizmoMoveFree(FVector TargetWorldLocation, AFurniture* Target);

	UFUNCTION(BlueprintCallable)
	void FinalizeGizmoMove(AFurniture* Target);

	UFUNCTION(BlueprintCallable)
	void AbortGizmoMove(AFurniture* Target);
	
	// 임시 가구 배열
	UPROPERTY()
	TArray<AFurniture*> LinePreviewFurnitures; 

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

	// 연속 배치 시 다음 프리뷰를 재생성하기 위해 마지막으로 사용한 가구 데이터를 보관
	FFurnitureDataRow CurrentFurnitureRow;

	// Shift+클릭 라인 채우기 기준점 (이전 배치 또는 프리뷰 생성 위치). 다음 Confirm 때 여기서 현재 프리뷰까지 일렬로 채움
	FVector2D LineFillAnchor = FVector2D::ZeroVector;

	void PlaceFurnitureCopyAtGridAnchor(FVector2D GridAnchor, FVector2D Dimensions, FRotator Rotation, const FFurnitureDataRow& InFurnitureRow);

	// 현재 프리뷰가 어떤 표면 위에 있는지. UpdatePreviewLocation에서 설정하고 ConfirmFurniture가 참조함
	EPlacementSurfaceType CurrentPreviewSurfaceType = EPlacementSurfaceType::Floor;

	EPlacementSurfaceType DetermineHitSurfaceType(const FHitResult& CursorHit) const;
	void UpdatePreviewLocationOnFloor(const FHitResult& CursorHit);
	void UpdatePreviewLocationOnWall(const FHitResult& CursorHit);

	void RefreshPlacementCellViz(AFurniture* Target, bool bInvalid);
	void ClearPlacementCellViz();
	bool IsEditableSurfaceComponent(const UMeshComponent* MeshComp) const;
	void ExportSurfaceMaterials(TArray<TSharedPtr<FJsonValue>>& OutArray) const;
	void ImportSurfaceMaterials(const TArray<TSharedPtr<FJsonValue>>& SurfaceArray);
	void BuildFloorPolygon(const FHarnessFloorData& FloorData);
	void BuildWallSegments(const FHarnessFloorData& FloorData);
	void ApplyWallTraceCollision();
	void MarkOutOfBoundsTiles();
	void RebuildGridMesh();
	static bool IsPointInPolygon(FVector2D Point, const TArray<FVector2D>& Polygon);
	bool IsFurnitureCornersInsideFloor(AFurniture* Target) const;
	bool FurnitureIntersectsWalls(AFurniture* Target) const;

	// 내벽 세그먼트 (문/창 개구부 제외)
	TArray<TPair<FVector2D, FVector2D>> InnerWallSegments;

	// 커서 트레이스로 얻은 현재 벽 노멀, 밀착 오프셋 계산용 (회전해도 유지됨)
	FVector CurrentWallNormal = FVector::ZeroVector;

};
