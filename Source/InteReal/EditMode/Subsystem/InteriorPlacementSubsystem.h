#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/DataTable.h"
#include "InteReal/EditMode/Furniture/Furniture.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "InteReal/EditMode/Placement/IPlacementHandler.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "InteriorPlacementSubsystem.generated.h"

class AGridSpaceManager;
class APlacementVisualizerActor;
class UPlacementHistoryHandler;

UCLASS()
class INTEREAL_API UInteriorPlacementSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	void RegisterVisualizer(APlacementVisualizerActor* InVisualizer);

	// ===== 초기화 =====
	UFUNCTION(BlueprintCallable, Category = "Placement|Init")
	void InitializeFromFloorData(const FHarnessFloorData& FloorData, float Cell = 50.0f);

	UFUNCTION(BlueprintCallable, Category = "Placement|Init")
	void InitializeGrid(int32 Length, int32 Breadth, float Cell);

	// ===== 그리드 =====
	UFUNCTION(BlueprintCallable, Category = "Placement|Grid")
	void SetGridVisible(bool bVisible);

	// ===== 라이트 아이콘 =====
	// 사용자가 토글 버튼으로 고르는 선호값(모드 전환과 무관하게 유지)
	UFUNCTION(BlueprintCallable, Category = "Placement|LightIcons")
	void SetLightFixtureIconsVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Placement|LightIcons")
	bool IsLightFixtureIconsVisible() const { return bLightFixtureIconsPreferred; }

	UFUNCTION(BlueprintCallable, Category = "Placement|LightIcons")
	void ToggleLightFixtureIconsVisible() { SetLightFixtureIconsVisible(!bLightFixtureIconsPreferred); }

	// Edit/View 모드 전환 시 컨트롤러가 호출 — 선호값은 그대로 두고 실제 표시 여부만 갱신
	void SetLightFixtureIconsEditModeActive(bool bActive);

	// 카메라 뷰 전환 시 컨트롤러가 호출. ISO/Top은 기본 OFF로 초기화하고
	// 토글로 다시 표시할 수 있으며, 뷰별 화면 크기를 적용한다.
	void SetLightFixtureIconsViewMode(bool bDefaultVisible, float IconPixelSize);

	// 새로 배치되는 라이트가 따라야 할, 사용자 선호값과 현재 모드를 모두 반영한 실제 표시 여부
	bool AreLightFixtureIconsCurrentlyVisible() const
	{
		return bLightFixtureIconsPreferred && bLightFixtureIconsEditModeActive;
	}

	float GetLightFixtureIconPixelSize() const { return CurrentLightFixtureIconPixelSize; }

	// ===== 프리뷰 =====
	UFUNCTION(BlueprintPure, Category = "Placement|Preview")
	bool HasActivePreview() const { return PreviewFurniture != nullptr; }

	AFurniture* GetPreviewFurniture() const { return PreviewFurniture; }

	UFUNCTION(BlueprintCallable, Category = "Placement|Preview")
	void CreatePreviewFurnitureFromRow(FVector RayPosition, FRotator Rotation, const FFurnitureDataRow& InFurnitureRow);

	UFUNCTION(BlueprintCallable, Category = "Placement|Preview")
	void UpdatePreviewLocation(const FHitResult& CursorHit);
	
	UFUNCTION(BlueprintCallable, Category = "Placement|Preview")
	void SetPreviewHidden(bool bHidden);

	UFUNCTION(BlueprintCallable, Category = "Placement|Preview")
	void RotatePreview(float AngleDeg = 90.0f);

	UFUNCTION(BlueprintCallable, Category = "Placement|Preview")
	void CancelPreview();

	// ===== 배치 확정/제거 =====
	UFUNCTION(BlueprintCallable, Category = "Placement")
	void ConfirmFurniture(bool bContinuePlacement = false);

	UFUNCTION(BlueprintCallable, Category = "Placement")
	void RemoveFurniture(AFurniture* Target);
	void ClearAllFurniture();
	
	UFUNCTION(BlueprintPure, Category = "Placement")
	bool IsPreviewLotEmpty() const;

	const TArray<AFurniture*>& GetPlacedFurnitures() const { return PlacedFurnitures; }

	const FFurnitureDataRow* FindFurnitureRowByID(int32 TargetID) const;
	bool IsOverlappingPlacedFurniture(const AFurniture* Target,
	                                  const AFurniture* IgnoredFurniture = nullptr,
	                                  const AFurniture* RequiredParent = nullptr) const;

	void RevalidatePlacedFurnitureWarnings();
	
	UFUNCTION(BlueprintPure, Category = "Placement|UI")
	EPlacementInvalidReason GetTooltipReasonFor(const AFurniture* SelectedFurniture) const;

	// ===== 기즈모 =====
	UFUNCTION(BlueprintCallable, Category = "Placement|Gizmo")
	void BeginGizmoMove(AFurniture* Target);

	UFUNCTION(BlueprintCallable, Category = "Placement|Gizmo")
	void UpdateGizmoMoveLocation(FVector CursorOnGround, AFurniture* Target, EGizmoTransformAxis Axis);

	UFUNCTION(BlueprintCallable, Category = "Placement|Gizmo")
	void UpdateGizmoMoveFree(FVector TargetWorldLocation, AFurniture* Target);
	void UpdateGizmoRotation(AFurniture* Target);

	UFUNCTION(BlueprintCallable, Category = "Placement|Gizmo")
	void FinalizeGizmoMove(AFurniture* Target);

	UFUNCTION(BlueprintCallable, Category = "Placement|Gizmo")
	void AbortGizmoMove(AFurniture* Target);

	// ===== Undo/Redo =====
	UFUNCTION(BlueprintCallable, Category = "Placement|History")
	void RecordUndoSnapshot();

	UFUNCTION(BlueprintCallable, Category = "Placement|History")
	void Undo();

	UFUNCTION(BlueprintCallable, Category = "Placement|History")
	void Redo();

	UFUNCTION(BlueprintPure, Category = "Placement|History")
	bool CanUndo() const;

	UFUNCTION(BlueprintPure, Category = "Placement|History")
	bool CanRedo() const;

	// ===== JSON =====
	UFUNCTION(BlueprintCallable, Category = "Placement|Serialization")
	FString ExportPlacedFurnituresJson() const;

	UFUNCTION(BlueprintCallable, Category = "Placement|Serialization")
	void ImportPlacedFurnituresJson(const FString& JsonString);

	UFUNCTION(BlueprintCallable, Category = "Placement|Serialization")
	FString ExportEditStateJson() const;

	UFUNCTION(BlueprintCallable, Category = "Placement|Serialization")
	void ImportEditStateJson(const FString& JsonString);

	// ===== 웹 커맨드 (기존 ReceiveWebCommand 지원) =====
	UFUNCTION(BlueprintCallable, Category = "Placement|Web")
	void ReceiveWebCommand(const FString& JsonString);

	// ===== 배치 라인 임시 프리뷰 =====
	UPROPERTY()
	TArray<AFurniture*> LinePreviewFurnitures;

	// ===== 현재 배치 불가 이유 (UI 표시용) =====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	EPlacementInvalidReason InvalidReason = EPlacementInvalidReason::None;
	
	// ===== 자유배치 모드 =====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bFreePlacementMode = false;

	// ===== 핸들러가 접근하는 공유 상태 Getter =====
	AGridSpaceManager* GetGrid() const { return Grid; }
	APlacementVisualizerActor* GetVisualizer() const { return Visualizer; }
	float GetGridCellSize() const { return GridCellSize; }
	float GetWallThickness() const { return WallThickness; }
	float GetFloorZ() const { return FloorZ; }
	float GetCeilingZ() const { return CeilingZ; }
	const TArray<FVector2D>& GetFloorPolygon() const { return FloorPolygon; }
	const TArray<TArray<FVector2D>>& GetFloorRoomPolygons() const { return FloorRoomPolygons; }
	const TArray<TPair<FVector2D, FVector2D>>& GetWallSegments() const { return WallSegments; }
	TArray<AFurniture*>& GetPlacedFurnituresMutable() { return PlacedFurnitures; }
	FVector2D& GetPreviewGridAnchor() { return PreviewGridAnchor; }
	FVector2D& GetCurrentDimensions() { return CurrentDimensions; }
	const FFurnitureDataRow& GetCurrentFurnitureRow() const { return CurrentFurnitureRow; }
	FVector& GetLastRayPosition() { return LastRayPosition; }
	EPlacementSurfaceType GetCurrentSurfaceType() const { return CurrentPreviewSurfaceType; }

	// 핸들러가 공유 상태를 쓸 때 사용하는 Setter
	void SetInvalidReason(EPlacementInvalidReason Reason) { InvalidReason = Reason; }
	void SetCurrentSurfaceType(EPlacementSurfaceType Type) { CurrentPreviewSurfaceType = Type; }
	void SetLastRayPosition(FVector Pos) { LastRayPosition = Pos; }

	void DestroyFurnitureRecursive(AFurniture* Target);

	static bool IsPointInPolygon(FVector2D Point, const TArray<FVector2D>& Polygon);
	bool IsPointInsideFloor(FVector2D Point) const;

	FRotator GetPreviewRotation() const { return PreviewRotation; }
	
	UFUNCTION(BlueprintPure, Category = "Placement|FreePlacement")
	bool IsFreePlacementMode() const { return bFreePlacementMode; }

	UFUNCTION(BlueprintCallable, Category = "Placement|FreePlacement")
	void SetFreePlacementMode(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Placement|FreePlacement")
	void ToggleFreePlacementMode() { SetFreePlacementMode(!bFreePlacementMode); }


private:
	// ===== 핸들러 =====
	UPROPERTY()
	TArray<UObject*> PlacementHandlers;

	UPROPERTY()
	UPlacementHistoryHandler* HistoryHandler = nullptr;

	// ===== 시각 Actor =====
	UPROPERTY()
	APlacementVisualizerActor* Visualizer = nullptr;

	// ===== 그리드 =====
	UPROPERTY()
	AGridSpaceManager* Grid = nullptr;

	// ===== 공유 설정 =====
	float GridCellSize = 50.0f;
	float WallThickness = 20.0f;
	float FloorZ = 0.0f;
	float CeilingZ = 240.0f;

	// ===== 공유 기하 데이터 =====
	TArray<FVector2D> FloorPolygon;
	TArray<TArray<FVector2D>> FloorRoomPolygons;
	TArray<TPair<FVector2D, FVector2D>> WallSegments;

	// ===== 프리뷰 상태 =====
	UPROPERTY()
	AFurniture* PreviewFurniture = nullptr;

	FVector2D PreviewGridAnchor = FVector2D::ZeroVector;
	FVector2D CurrentDimensions = FVector2D::ZeroVector;
	FRotator PreviewRotation = FRotator::ZeroRotator;
	FVector LastRayPosition = FVector::ZeroVector;
	FHitResult LastPlacementHit;
	EPlacementSurfaceType CurrentPreviewSurfaceType = EPlacementSurfaceType::Floor;
	FFurnitureDataRow CurrentFurnitureRow;
	FVector2D LineFillAnchor = FVector2D::ZeroVector;
	
	mutable FFurnitureDataRow CachedLightFurnitureRow;

	UPROPERTY()
	UObject* ActivePlacementHandler = nullptr;

	// ===== 배치된 가구 목록 =====
	UPROPERTY()
	TArray<AFurniture*> PlacedFurnitures;

	// ===== 라이트 아이콘 상태 =====
	bool bLightFixtureIconsPreferred = false;
	bool bLightFixtureIconsEditModeActive = true;
	float CurrentLightFixtureIconPixelSize = 16.0f;

	void ApplyLightFixtureIconsEffectiveState();

	// ===== 내부 헬퍼 =====
	IPlacementHandler* FindHandlerForHit(const FHitResult& Hit) const;
	IPlacementHandler* FindHandlerForFurniture(const AFurniture* Furniture) const;
	IPlacementHandler* GetActiveHandler() const;
	void RebuildCurrentDimensionsFromPreviewRotation();

	void BuildFloorPolygon(const FHarnessFloorData& FloorData);
	void BuildWallSegments(const FHarnessFloorData& FloorData);
	void ApplyWallTraceCollision();
	void MarkOutOfBoundsTiles();
	void PlaceFurnitureCopyAtGridAnchor(FVector2D GridAnchor,
	                                    FVector2D Dims,
	                                    FRotator Rotation,
	                                    const FFurnitureDataRow& Row);

	EPlacementSurfaceType DetermineHitSurfaceType(const FHitResult& Hit) const;
};
