#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteReal/Harness/Public/HarnessData.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "InteReal2DFloorPlanTypes.h"
#include "InteReal2DFloorPlanViewTransform.h"
#include "InteReal2DFloorPlanViewportWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteReal2DDrawAreaClickedSignature, FVector2D, LocalPosition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteReal2DFurniturePlacementRequestedSignature, FVector2D, DocumentPosition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteReal2DFurniturePreviewMovedSignature, FVector2D, DocumentPosition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInteReal2DPlacedFurnitureSelectedSignature, int32, FurnitureIndex, FInteReal2DPlacedFurniture, Furniture);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInteReal2DPlacedFurnitureMovedSignature, int32, FurnitureIndex, FInteReal2DPlacedFurniture, Furniture);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInteReal2DPlacedFurnitureMoveEndedSignature, int32, FurnitureIndex, FInteReal2DPlacedFurniture, Furniture);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInteReal2DPlacedFurnitureDeletedSignature, int32, FurnitureIndex, FGuid, InstanceGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInteReal2DPlacedFurnituresClearedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInteReal2DPlacedFurnitureSelectionClearedSignature);

UCLASS(BlueprintType, Blueprintable)
class INTEREAL_API UInteReal2DFloorPlanViewportWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="InteReal2D|FloorPlan")
    void SetDocument(const FInteReal2DFloorPlanDocument& InDocument);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|FloorPlan")
    void LoadFromHarnessFloorData(const FHarnessFloorData& InFloorData);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Layout")
    void SetDrawArea(const FVector2D& InDrawOffset, const FVector2D& InDrawSizeOverride);
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void StartFurniturePlacement(const FFurnitureDataRow& FurnitureRow);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void CancelFurniturePlacement();

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void ClearPlacedFurnitures();

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    int32 FindPlacedFurnitureIndexByGuid(const FGuid& InstanceGuid) const;
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool GetPlacedFurnitureByGuid(const FGuid& InstanceGuid, FInteReal2DPlacedFurniture& OutFurniture) const;
    
    FInteReal2DFloorPlanViewTransform BuildViewTransform(const FVector2D& LocalSize) const;

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool SelectPlacedFurnitureByGuid(const FGuid& InstanceGuid);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool RemovePlacedFurnitureByIndex(int32 FurnitureIndex);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool RemovePlacedFurnitureByGuid(const FGuid& InstanceGuid);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool RemoveSelectedFurniture();

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool SelectPlacedFurnitureByIndex(int32 FurnitureIndex);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void ClearSelectedFurniture();

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool UpdatePlacedFurniture(
        int32 FurnitureIndex,
        const FVector2D& CenterDocumentPosition,
        float RotationDegrees
    );

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool UpdatePlacedFurnitureByGuid(
        const FGuid& InstanceGuid,
        const FVector2D& CenterDocumentPosition,
        float RotationDegrees
    );
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool UpdatePlacedFurnitureSize(int32 FurnitureIndex, const FVector2D& NewSize);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool UpdatePlacedFurnitureSizeByGuid(const FGuid& InstanceGuid, const FVector2D& NewSize);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool UpdateSelectedFurniture(
        const FVector2D& CenterDocumentPosition,
        float RotationDegrees
    );
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    FGuid AddPlacedFurnitureAtDocumentPosition(
        const FFurnitureDataRow& FurnitureRow,
        const FVector2D& CenterDocumentPosition,
        float RotationDegrees
    );
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void SetFurniturePreviewAtDocumentPosition(
        const FVector2D& CenterDocumentPosition,
        float RotationDegrees
    );
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void SetFurniturePreviewPlacementValid(bool bPlacementValid);
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Tool")
    void SetSelectToolActive2D(bool bActive);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Tool")
    void SetObjectSnapEnabled2D(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category="InteReal2D|Tool")
    void ToggleObjectSnap2D();

    UFUNCTION(BlueprintPure, Category="InteReal2D|Tool")
    bool IsObjectSnapEnabled2D() const { return bEnableObjectSnap2D; }

    UFUNCTION(BlueprintPure, Category="InteReal2D|Tool")
    bool IsSelectToolActive2D() const { return bSelectToolActive2D; }
    
    UFUNCTION(BlueprintPure, Category="InteReal2D|Tool")
    bool HasSelectedFurniture2D() const { return SelectedFurnitureIndex != INDEX_NONE; }
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    bool UpdatePlacedFurnitureFootprintByGuid(const FGuid& InstanceGuid, const TArray<FVector2D>& FootprintLocalPoints);
    
    UFUNCTION(BlueprintCallable, Category="InteReal2D|Furniture")
    void SetFurniturePreviewFootprint(const TArray<FVector2D>& FootprintLocalPoints);
    
    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Input")
    FInteReal2DDrawAreaClickedSignature OnDrawAreaClicked;
    
    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Furniture")
    FInteReal2DFurniturePlacementRequestedSignature OnFurniturePlacementRequested2D;
    
    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Furniture")
    FInteReal2DFurniturePreviewMovedSignature OnFurniturePreviewMoved2D;
    
    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Furniture")
    FInteReal2DPlacedFurnitureSelectedSignature OnPlacedFurnitureSelected2D;
    
    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Furniture")
    FInteReal2DPlacedFurnitureMovedSignature OnPlacedFurnitureMoved2D;
    
    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Furniture")
    FInteReal2DPlacedFurnitureMoveEndedSignature OnPlacedFurnitureMoveEnded2D;
    
    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Furniture")
    FInteReal2DPlacedFurnitureDeletedSignature OnPlacedFurnitureDeleted2D;

    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Furniture")
    FInteReal2DPlacedFurnituresClearedSignature OnPlacedFurnituresCleared2D;
    
    UPROPERTY(BlueprintAssignable, Category="InteReal2D|Furniture")
    FInteReal2DPlacedFurnitureSelectionClearedSignature OnPlacedFurnitureSelectionCleared2D;
    

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InteReal2D|Input")
    FVector2D LastClickedLocalPosition = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InteReal2D|Input")
    bool bLastClickInsideDrawArea = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Layout", meta=(ToolTip="도면이 위젯 내부에서 그려질 기준 위치 오프셋입니다."))
    FVector2D DrawOffset = FVector2D(30.0f, 110.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Layout", meta=(ToolTip="도면을 특정 크기 영역 안에 맞춰 그릴지 여부입니다."))
    bool bUseDrawSizeOverride = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Layout", meta=(EditCondition="bUseDrawSizeOverride", ToolTip="도면이 맞춰질 가상 그리기 영역 크기입니다. X/Y가 0보다 커야 합니다."))
    FVector2D DrawSizeOverride = FVector2D(800.0f, 600.0f);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Layout", meta=(ClampMin="-360.0", ClampMax="360.0", UIMin="-180.0", UIMax="180.0"))
    float DrawRotationDegrees = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|View", meta=(ClampMin="0.1", UIMin="0.1"))
    float MinViewZoom = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|View", meta=(ClampMin="0.1", UIMin="0.1"))
    float MaxViewZoom = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|View", meta=(ClampMin="0.01", UIMin="0.01"))
    float MouseWheelZoomStep = 1.15f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InteReal2D|View")
    float ViewZoom = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InteReal2D|View")
    FVector2D ViewPanLocal = FVector2D::ZeroVector;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    bool bDrawBackground = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor BackgroundColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style", meta=(ClampMin="0.0", UIMin="0.0"))
    float BackgroundCornerRadius = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    float ViewPadding = 32.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    float RoomLineThickness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    float OpeningLineThickness = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    bool bDrawRooms = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    bool bDrawWallCenterLines = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style", meta=(ClampMin="0.0", UIMin="0.0"))
    float WallLineThickness = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style", meta=(ClampMin="0.0", UIMin="0.0"))
    float OpeningEraseThickness = 8.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor RoomLineColor = FLinearColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor OpeningDoorColor = FLinearColor::Green;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor OpeningWindowColor = FLinearColor::Blue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    FLinearColor OpeningDefaultColor = FLinearColor::Yellow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Style")
    bool bFlipYForScreenSpace = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor FurnitureFillColor = FLinearColor(0.1f, 0.45f, 1.0f, 0.25f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor FurnitureOutlineColor = FLinearColor(0.05f, 0.25f, 0.9f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor SelectedFurnitureFillColor = FLinearColor(1.0f, 0.75f, 0.1f, 0.35f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor HoveredFurnitureFillColor = FLinearColor(0.2f, 0.75f, 1.0f, 0.35f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor HoveredFurnitureOutlineColor = FLinearColor(0.0f, 0.75f, 1.0f, 1.0f);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor SelectedFurnitureOutlineColor = FLinearColor(1.0f, 0.45f, 0.0f, 1.0f);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor FurniturePreviewFillColor = FLinearColor(0.1f, 0.8f, 1.0f, 0.18f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor FurniturePreviewOutlineColor = FLinearColor(0.0f, 0.55f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor InvalidFurniturePreviewFillColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.18f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture")
    FLinearColor InvalidFurniturePreviewOutlineColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Snap")
    bool bEnableObjectSnap2D = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Snap")
    bool bSnapToWallSegments2D = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Snap")
    bool bSnapToInnerWalls2D = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Snap")
    bool bSnapToOuterWalls2D = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Snap")
    bool bSnapToRoomPolygons2D = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Snap")
    bool bUseWallThicknessForSnapGap2D = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Snap", meta=(ClampMin="0.0", UIMin="0.0"))
    float ObjectSnapDistanceDocument = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Snap", meta=(ClampMin="0.0", UIMin="0.0"))
    float ObjectSnapGapDocument = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Snap")
    bool bDrawObjectSnapGuide = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Snap")
    FLinearColor ObjectSnapGuideColor = FLinearColor(0.0f, 0.3f, 0.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Snap", meta=(ClampMin="0.0", UIMin="0.0"))
    float ObjectSnapGuideThickness = 1.5f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Furniture", meta=(ClampMin="0.0", UIMin="0.0"))
    float FurnitureOutlineThickness = 2.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InteReal2D|Furniture")
    bool bIsPlacingFurniture2D = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InteReal2D|Furniture")
    TArray<FInteReal2DPlacedFurniture> PlacedFurnitures2D;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InteReal2D|Furniture")
    int32 SelectedFurnitureIndex = INDEX_NONE;
    
    int32 HoveredFurnitureIndex = INDEX_NONE;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Tool")
    bool bSelectToolActive2D = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Tool")
    FLinearColor ToolButtonActiveTint = FLinearColor(0.68f, 0.60f, 0.52f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Tool")
    FLinearColor ToolButtonInactiveTint = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Tool")
    FLinearColor ToolIconActiveTint = FLinearColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="InteReal2D|Tool")
    FLinearColor ToolIconInactiveTint = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

protected:
    virtual void NativeConstruct() override;
    
    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled
    ) const override;

    virtual FReply NativeOnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent
    ) override;
    
    virtual FReply NativeOnMouseButtonUp(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent
    ) override;
    
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
    
    virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    
private:
    UFUNCTION()
    FEventReply HandleInputCatcherMouseButtonDown(
        FGeometry MyGeometry,
        const FPointerEvent& MouseEvent
    );
    
    UFUNCTION()
    FEventReply HandleInputCatcherMouseButtonUp(
        FGeometry MyGeometry,
        const FPointerEvent& MouseEvent
    );
    
    UFUNCTION()
    FEventReply HandleInputCatcherMouseMove(
        FGeometry MyGeometry,
        const FPointerEvent& MouseEvent
    );
    
    UFUNCTION()
    void HandleSelectToolButtonClicked();

    UFUNCTION()
    void HandleObjectSnapButtonClicked();
    
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<class UBorder> InputCatcherBorder;
    
    UPROPERTY()
    FInteReal2DFloorPlanDocument Document;
    
    FVector2D TransformDocumentPointToLocal(const FVector2D& DocPoint, const FVector2D& LocalSize) const;
    FVector2D TransformLocalPointToDocument(const FVector2D& LocalPoint, const FVector2D& LocalSize) const;
    FVector2D GetViewZoomPivotLocal(const FVector2D& LocalSize) const;
    FVector2D ApplyViewZoomToLocalPoint(const FVector2D& UnzoomedLocalPoint, const FVector2D& LocalSize) const;
    FVector2D RemoveViewZoomFromLocalPoint(const FVector2D& ZoomedLocalPoint, const FVector2D& LocalSize) const;
    void SetViewZoomAtLocalPosition(float NewViewZoom, const FVector2D& ZoomAnchorLocal, const FVector2D& LocalSize);
    FLinearColor ResolveOpeningColor(const FString& OpeningType) const;
    FVector2D GetDrawAreaOffset() const;
    FVector2D GetDrawAreaSize(const FVector2D& LocalSize) const;
    bool IsLocalPointInsideDrawArea(const FVector2D& LocalPoint, const FVector2D& LocalSize) const;
    void ApplyInputCatcherLayout(const FVector2D& LocalSize);
    void DrawFurnitureRect(
        FSlateWindowElementList& OutDrawElements,
        const FGeometry& AllottedGeometry,
        int32 LayerId,
        const FInteReal2DPlacedFurniture& Furniture,
        const FLinearColor& FillColor,
        const FLinearColor& OutlineColor
    ) const;
    
    void DrawFurnitureShape(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32 LayerId, const FInteReal2DPlacedFurniture& Furniture, const FLinearColor& FillColor, const FLinearColor& OutlineColor) const;
    
    bool TryGetPlacedFurnitureIndexAtLocalPosition(
        const FVector2D& LocalPosition,
        const FVector2D& LocalSize,
        int32& OutFurnitureIndex
    ) const;

    bool IsDocumentPointInsideFurniture(
        const FVector2D& DocumentPosition,
        const FInteReal2DPlacedFurniture& Furniture
    ) const;
    
    void GetFurnitureSnapOffsets(const FVector2D& FurnitureSize, float RotationDegrees, TArray<FVector2D>& OutOffsets) const;
    void GetFurnitureEdgeSnapOffsets(const FVector2D& FurnitureSize, float RotationDegrees, TArray<FVector2D>& OutOffsets) const;
    void GetFurnitureSnapPoints(const FInteReal2DPlacedFurniture& Furniture, TArray<FVector2D>& OutPoints) const;
    void GetFurnitureSnapSegments(const FInteReal2DPlacedFurniture& Furniture, TArray<TPair<FVector2D, FVector2D>>& OutSegments) const;
    FVector2D GetClosestPointOnSegment(const FVector2D& Point, const FVector2D& SegmentStart, const FVector2D& SegmentEnd) const;
    bool ResolveObjectSnappedPreviewCenter(const FVector2D& RequestedCenterDocument, FVector2D& OutSnappedCenterDocument, FVector2D& OutSnapSourceDocument, FVector2D& OutSnapTargetDocument) const;
    bool ResolveObjectSnappedFurnitureCenter(int32 IgnoreFurnitureIndex, const FVector2D& FurnitureSize, float RotationDegrees, const FVector2D& RequestedCenterDocument, FVector2D& OutSnappedCenterDocument, FVector2D& OutSnapSourceDocument, FVector2D& OutSnapTargetDocument) const;
    
    UPROPERTY()
    FFurnitureDataRow PendingFurnitureRow;

    FVector2D PendingFurnitureSize = FVector2D::ZeroVector;
    FVector2D PreviewFurnitureCenterDocument = FVector2D::ZeroVector;
    float PreviewFurnitureRotationDegrees = 0.0f;
    bool bHasFurniturePreviewPosition = false;
    bool bIsFurniturePreviewPlacementValid = true;
    
    bool bHasObjectSnapGuide = false;
    FVector2D ObjectSnapGuideSourceDocument = FVector2D::ZeroVector;
    FVector2D ObjectSnapGuideTargetDocument = FVector2D::ZeroVector;
    
    bool bIsPanningView2D = false;
    FVector2D LastPanMouseLocalPosition = FVector2D::ZeroVector;
    
    bool bIsDraggingSelectedFurniture2D = false;
    int32 DraggingFurnitureIndex2D = INDEX_NONE;
    FVector2D FurnitureDragDocumentOffset = FVector2D::ZeroVector;
    
    TArray<FVector2D> PreviewFurnitureFootprintLocalPoints;
};