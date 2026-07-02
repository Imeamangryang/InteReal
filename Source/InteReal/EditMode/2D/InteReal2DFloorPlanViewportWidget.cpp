#include "InteReal2DFloorPlanViewportWidget.h"
#include "InteReal2DFloorPlanConverter.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "Components/Border.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "InteReal2DFloorPlanViewTransform.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"

void UInteReal2DFloorPlanViewportWidget::SetDocument(const FInteReal2DFloorPlanDocument& InDocument)
{
    Document = InDocument;
    InvalidateLayoutAndVolatility();
}

void UInteReal2DFloorPlanViewportWidget::LoadFromHarnessFloorData(const FHarnessFloorData& InFloorData)
{
    Document = FInteReal2DFloorPlanConverter::ConvertFromHarness(InFloorData);
    InvalidateLayoutAndVolatility();
}

void UInteReal2DFloorPlanViewportWidget::SetDrawArea(const FVector2D& InDrawOffset, const FVector2D& InDrawSizeOverride)
{
    DrawOffset = InDrawOffset;
    DrawSizeOverride = InDrawSizeOverride;
    bUseDrawSizeOverride = DrawSizeOverride.X > 0.0f && DrawSizeOverride.Y > 0.0f;

    ApplyInputCatcherLayout(GetCachedGeometry().GetLocalSize());
    InvalidateLayoutAndVolatility();
}

FVector2D UInteReal2DFloorPlanViewportWidget::TransformDocumentPointToLocal(const FVector2D& DocPoint, const FVector2D& LocalSize) const
{
    const FVector2D UnzoomedLocalPoint = BuildViewTransform(LocalSize).DocumentToLocal(DocPoint);
    return ApplyViewZoomToLocalPoint(UnzoomedLocalPoint, LocalSize);
}

FLinearColor UInteReal2DFloorPlanViewportWidget::ResolveOpeningColor(const FString& OpeningType) const
{
    if (OpeningType.Contains(TEXT("Door")))
    {
        return OpeningDoorColor;
    }

    if (OpeningType.Contains(TEXT("Window")))
    {
        return OpeningWindowColor;
    }

    return OpeningDefaultColor;
}

void UInteReal2DFloorPlanViewportWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    SetIsEnabled(true);
    SetVisibility(ESlateVisibility::Visible);
    SetIsFocusable(true);

    if (InputCatcherBorder)
    {
        InputCatcherBorder->SetVisibility(ESlateVisibility::Visible);
        InputCatcherBorder->OnMouseButtonDownEvent.BindDynamic(this, &UInteReal2DFloorPlanViewportWidget::HandleInputCatcherMouseButtonDown);
        InputCatcherBorder->OnMouseButtonUpEvent.BindDynamic(this, &UInteReal2DFloorPlanViewportWidget::HandleInputCatcherMouseButtonUp);
        InputCatcherBorder->OnMouseMoveEvent.BindDynamic(this, &UInteReal2DFloorPlanViewportWidget::HandleInputCatcherMouseMove);

        ApplyInputCatcherLayout(GetCachedGeometry().GetLocalSize());
    }
}

int32 UInteReal2DFloorPlanViewportWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    if (bDrawBackground)
    {
        const FVector2D BackgroundOffset = GetDrawAreaOffset();
        const FVector2D BackgroundSize = GetDrawAreaSize(AllottedGeometry.GetLocalSize());
        const FSlateRoundedBoxBrush BackgroundBrush(BackgroundColor, BackgroundCornerRadius);
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(BackgroundSize, FSlateLayoutTransform(BackgroundOffset)), &BackgroundBrush, ESlateDrawEffect::None, InWidgetStyle.GetColorAndOpacityTint());
        ++LayerId;
    }

    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    if (!Document.bIsValid)
    {
        return LayerId;
    }

    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
    const FVector2D ClipOffset = GetDrawAreaOffset();
    const FVector2D ClipSize = GetDrawAreaSize(LocalSize);
    const FSlateRect ClipRect = AllottedGeometry.GetLayoutBoundingRect().InsetBy(FMargin(ClipOffset.X, ClipOffset.Y, LocalSize.X - ClipOffset.X - ClipSize.X, LocalSize.Y - ClipOffset.Y - ClipSize.Y));

    OutDrawElements.PushClip(FSlateClippingZone(ClipRect));

    if (bDrawRooms)
    {
        for (const FInteReal2DFloorPlanPolygon& Room : Document.Rooms)
        {
            if (Room.Points.Num() < 2)
            {
                continue;
            }

            TArray<FVector2D> DrawPoints;
            DrawPoints.Reserve(Room.Points.Num() + 1);

            for (const FVector2D& DocPoint : Room.Points)
            {
                DrawPoints.Add(TransformDocumentPointToLocal(DocPoint, LocalSize));
            }

            DrawPoints.Add(TransformDocumentPointToLocal(Room.Points[0], LocalSize));
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), DrawPoints, ESlateDrawEffect::None, RoomLineColor, true, RoomLineThickness);
        }

        ++LayerId;
    }

    if (bDrawWallCenterLines)
    {
        for (const FInteReal2DFloorPlanWallSegment& Wall : Document.Walls)
        {
            TArray<FVector2D> WallPoints;
            WallPoints.Add(TransformDocumentPointToLocal(Wall.Start, LocalSize));
            WallPoints.Add(TransformDocumentPointToLocal(Wall.End, LocalSize));
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), WallPoints, ESlateDrawEffect::None, RoomLineColor, true, WallLineThickness);
        }

        ++LayerId;
    }

    for (const FInteReal2DFloorPlanOpening& Opening : Document.Openings)
    {
        TArray<FVector2D> SegmentPoints;
        SegmentPoints.Add(TransformDocumentPointToLocal(Opening.Start, LocalSize));
        SegmentPoints.Add(TransformDocumentPointToLocal(Opening.End, LocalSize));
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), SegmentPoints, ESlateDrawEffect::None, BackgroundColor, true, OpeningEraseThickness);
    }

    ++LayerId;

    for (const FInteReal2DFloorPlanOpening& Opening : Document.Openings)
    {
        TArray<FVector2D> SegmentPoints;
        SegmentPoints.Add(TransformDocumentPointToLocal(Opening.Start, LocalSize));
        SegmentPoints.Add(TransformDocumentPointToLocal(Opening.End, LocalSize));
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), SegmentPoints, ESlateDrawEffect::None, ResolveOpeningColor(Opening.Type), true, OpeningLineThickness);
    }

    ++LayerId;

    for (int32 FurnitureIndex = 0; FurnitureIndex < PlacedFurnitures2D.Num(); ++FurnitureIndex)
    {
        const FInteReal2DPlacedFurniture& Furniture = PlacedFurnitures2D[FurnitureIndex];
        const bool bIsSelected = FurnitureIndex == SelectedFurnitureIndex;
        const bool bIsHovered = FurnitureIndex == HoveredFurnitureIndex;
        const FLinearColor FillColor = bIsSelected ? SelectedFurnitureFillColor : bIsHovered ? HoveredFurnitureFillColor : FurnitureFillColor;
        const FLinearColor OutlineColor = bIsSelected ? SelectedFurnitureOutlineColor : bIsHovered ? HoveredFurnitureOutlineColor : FurnitureOutlineColor;
        DrawFurnitureShape(OutDrawElements, AllottedGeometry, LayerId, Furniture, FillColor, OutlineColor);
    }

    LayerId += 2;

    if (bIsPlacingFurniture2D && bHasFurniturePreviewPosition)
    {
        FInteReal2DPlacedFurniture PreviewFurniture;
        PreviewFurniture.FurnitureID = PendingFurnitureRow.ID;
        PreviewFurniture.DisplayName = PendingFurnitureRow.DisplayName;
        PreviewFurniture.CenterDocumentPosition = PreviewFurnitureCenterDocument;
        PreviewFurniture.Size = PendingFurnitureSize;
        PreviewFurniture.RotationDegrees = PreviewFurnitureRotationDegrees;
        PreviewFurniture.FootprintLocalPoints = PreviewFurnitureFootprintLocalPoints;

        DrawFurnitureShape(OutDrawElements, AllottedGeometry, LayerId, PreviewFurniture, bIsFurniturePreviewPlacementValid ? FurniturePreviewFillColor : InvalidFurniturePreviewFillColor, bIsFurniturePreviewPlacementValid ? FurniturePreviewOutlineColor : InvalidFurniturePreviewOutlineColor);
        
        LayerId += 2;

        if (bDrawObjectSnapGuide && bHasObjectSnapGuide)
        {
            TArray<FVector2D> SnapGuidePoints;
            SnapGuidePoints.Add(TransformDocumentPointToLocal(ObjectSnapGuideSourceDocument, LocalSize));
            SnapGuidePoints.Add(TransformDocumentPointToLocal(ObjectSnapGuideTargetDocument, LocalSize));
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), SnapGuidePoints, ESlateDrawEffect::None, ObjectSnapGuideColor, true, ObjectSnapGuideThickness);
            ++LayerId;
        }
    }

    OutDrawElements.PopClip();

    return LayerId + 1;
}

FVector2D UInteReal2DFloorPlanViewportWidget::GetDrawAreaOffset() const
{
    return BuildViewTransform(GetCachedGeometry().GetLocalSize()).GetDrawAreaOffset();
}

FVector2D UInteReal2DFloorPlanViewportWidget::GetDrawAreaSize(const FVector2D& LocalSize) const
{
    return bUseDrawSizeOverride
        ? FVector2D(
            FMath::Max(DrawSizeOverride.X, 1.0f),
            FMath::Max(DrawSizeOverride.Y, 1.0f)
        )
        : LocalSize;
}

bool UInteReal2DFloorPlanViewportWidget::IsLocalPointInsideDrawArea(const FVector2D& LocalPoint, const FVector2D& LocalSize) const
{
    return BuildViewTransform(LocalSize).IsLocalPointInsideDrawArea(LocalPoint);
}

FReply UInteReal2DFloorPlanViewportWidget::NativeOnMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent
)
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }

    LastClickedLocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    bLastClickInsideDrawArea = IsLocalPointInsideDrawArea(
        LastClickedLocalPosition,
        InGeometry.GetLocalSize()
    );

    if (!bLastClickInsideDrawArea)
    {
        return FReply::Unhandled();
    }

    OnDrawAreaClicked.Broadcast(LastClickedLocalPosition);

    return FReply::Handled();
}

FReply UInteReal2DFloorPlanViewportWidget::NativeOnMouseButtonUp(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent
)
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
    }

    const int32 EndedFurnitureIndex = DraggingFurnitureIndex2D;
    if (bIsDraggingSelectedFurniture2D && PlacedFurnitures2D.IsValidIndex(EndedFurnitureIndex))
    {
        OnPlacedFurnitureMoveEnded2D.Broadcast(
            EndedFurnitureIndex,
            PlacedFurnitures2D[EndedFurnitureIndex]
        );
    }

    bIsDraggingSelectedFurniture2D = false;
    DraggingFurnitureIndex2D = INDEX_NONE;
    FurnitureDragDocumentOffset = FVector2D::ZeroVector;

    return FReply::Handled();
}

void UInteReal2DFloorPlanViewportWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);

    bIsPanningView2D = false;
    LastPanMouseLocalPosition = FVector2D::ZeroVector;

    if (HoveredFurnitureIndex != INDEX_NONE)
    {
        HoveredFurnitureIndex = INDEX_NONE;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

FReply UInteReal2DFloorPlanViewportWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!Document.bIsValid)
    {
        return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
    }

    const float WheelDelta = InMouseEvent.GetWheelDelta();
    if (FMath::IsNearlyZero(WheelDelta))
    {
        return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
    }

    const FVector2D LocalMousePosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const float ZoomMultiplier = WheelDelta > 0.0f ? MouseWheelZoomStep : 1.0f / MouseWheelZoomStep;
    const float NewViewZoom = FMath::Clamp(ViewZoom * ZoomMultiplier, MinViewZoom, MaxViewZoom);

    SetViewZoomAtLocalPosition(NewViewZoom, LocalMousePosition, InGeometry.GetLocalSize());

    return FReply::Handled();
}

FEventReply UInteReal2DFloorPlanViewportWidget::HandleInputCatcherMouseButtonDown(
    FGeometry MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    FEventReply Reply;
    
    if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
    {
        const FGeometry CachedGeometry = GetCachedGeometry();
        bIsPanningView2D = true;
        LastPanMouseLocalPosition = CachedGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        FEventReply HandledReply = UWidgetBlueprintLibrary::Handled();
        return UWidgetBlueprintLibrary::CaptureMouse(HandledReply, InputCatcherBorder);
    }

    if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Reply;
    }

    const FGeometry CachedGeometry = GetCachedGeometry();
    LastClickedLocalPosition = CachedGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    bLastClickInsideDrawArea = IsLocalPointInsideDrawArea(
        LastClickedLocalPosition,
        CachedGeometry.GetLocalSize()
    );

    if (!bLastClickInsideDrawArea)
    {
        return Reply;
    }

    OnDrawAreaClicked.Broadcast(LastClickedLocalPosition);

    if (bIsPlacingFurniture2D && Document.bIsValid)
    {
        FVector2D RequestedDocumentPosition = TransformLocalPointToDocument(LastClickedLocalPosition, CachedGeometry.GetLocalSize());
        FVector2D SnappedDocumentPosition = RequestedDocumentPosition;
        FVector2D SnapSourceDocument = FVector2D::ZeroVector;
        FVector2D SnapTargetDocument = FVector2D::ZeroVector;

        bHasObjectSnapGuide = ResolveObjectSnappedPreviewCenter(RequestedDocumentPosition, SnappedDocumentPosition, SnapSourceDocument, SnapTargetDocument);
        ObjectSnapGuideSourceDocument = SnapSourceDocument;
        ObjectSnapGuideTargetDocument = SnapTargetDocument;

        PreviewFurnitureCenterDocument = SnappedDocumentPosition;
        bHasFurniturePreviewPosition = true;

        OnFurniturePlacementRequested2D.Broadcast(PreviewFurnitureCenterDocument);
    }
    else if (Document.bIsValid)
    {
        if (!bSelectToolActive2D)
        {
            return UWidgetBlueprintLibrary::Handled();
        }

        int32 HitFurnitureIndex = INDEX_NONE;
        if (TryGetPlacedFurnitureIndexAtLocalPosition(LastClickedLocalPosition, CachedGeometry.GetLocalSize(), HitFurnitureIndex))
        {
            SelectPlacedFurnitureByIndex(HitFurnitureIndex);

            const FVector2D ClickDocumentPosition = TransformLocalPointToDocument(LastClickedLocalPosition, CachedGeometry.GetLocalSize());

            bIsDraggingSelectedFurniture2D = true;
            DraggingFurnitureIndex2D = HitFurnitureIndex;
            FurnitureDragDocumentOffset = PlacedFurnitures2D[HitFurnitureIndex].CenterDocumentPosition - ClickDocumentPosition;

            FEventReply HandledReply = UWidgetBlueprintLibrary::Handled();
            return UWidgetBlueprintLibrary::CaptureMouse(HandledReply, InputCatcherBorder);
        }

        ClearSelectedFurniture();
        HoveredFurnitureIndex = INDEX_NONE;
        bIsDraggingSelectedFurniture2D = false;
        DraggingFurnitureIndex2D = INDEX_NONE;
        FurnitureDragDocumentOffset = FVector2D::ZeroVector;
    }

    return UWidgetBlueprintLibrary::Handled();
}

FEventReply UInteReal2DFloorPlanViewportWidget::HandleInputCatcherMouseButtonUp(
    FGeometry MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    FEventReply Reply;
    
    if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
    {
        bIsPanningView2D = false;
        LastPanMouseLocalPosition = FVector2D::ZeroVector;

        FEventReply HandledReply = UWidgetBlueprintLibrary::Handled();
        return UWidgetBlueprintLibrary::ReleaseMouseCapture(HandledReply);
    }

    if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Reply;
    }

    const int32 EndedFurnitureIndex = DraggingFurnitureIndex2D;
    if (bIsDraggingSelectedFurniture2D && PlacedFurnitures2D.IsValidIndex(EndedFurnitureIndex))
    {
        OnPlacedFurnitureMoveEnded2D.Broadcast(
            EndedFurnitureIndex,
            PlacedFurnitures2D[EndedFurnitureIndex]
        );
    }

    bIsDraggingSelectedFurniture2D = false;
    DraggingFurnitureIndex2D = INDEX_NONE;
    FurnitureDragDocumentOffset = FVector2D::ZeroVector;
    bHasObjectSnapGuide = false;
    ObjectSnapGuideSourceDocument = FVector2D::ZeroVector;
    ObjectSnapGuideTargetDocument = FVector2D::ZeroVector;

    FEventReply HandledReply = UWidgetBlueprintLibrary::Handled();
    return UWidgetBlueprintLibrary::ReleaseMouseCapture(HandledReply);
}

FEventReply UInteReal2DFloorPlanViewportWidget::HandleInputCatcherMouseMove(
    FGeometry MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    FEventReply Reply;

    const FGeometry CachedGeometry = GetCachedGeometry();
    const FVector2D LocalMousePosition = CachedGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    if (bIsPanningView2D)
    {
        const FVector2D PanDelta = LocalMousePosition - LastPanMouseLocalPosition;
        ViewPanLocal += PanDelta;
        LastPanMouseLocalPosition = LocalMousePosition;
        Invalidate(EInvalidateWidgetReason::Paint);
        return UWidgetBlueprintLibrary::Handled();
    }
    
    if (bIsDraggingSelectedFurniture2D && PlacedFurnitures2D.IsValidIndex(DraggingFurnitureIndex2D))
    {
        if (!Document.bIsValid)
        {
            return Reply;
        }

        if (!IsLocalPointInsideDrawArea(LocalMousePosition, CachedGeometry.GetLocalSize()))
        {
            return Reply;
        }

        const FVector2D MouseDocumentPosition = TransformLocalPointToDocument(LocalMousePosition, CachedGeometry.GetLocalSize());

        FInteReal2DPlacedFurniture& DraggingFurniture = PlacedFurnitures2D[DraggingFurnitureIndex2D];

        const FVector2D RequestedDocumentPosition = MouseDocumentPosition + FurnitureDragDocumentOffset;
        FVector2D SnappedDocumentPosition = RequestedDocumentPosition;
        FVector2D SnapSourceDocument = FVector2D::ZeroVector;
        FVector2D SnapTargetDocument = FVector2D::ZeroVector;

        bHasObjectSnapGuide = ResolveObjectSnappedFurnitureCenter(DraggingFurnitureIndex2D, DraggingFurniture.Size, DraggingFurniture.RotationDegrees, RequestedDocumentPosition, SnappedDocumentPosition, SnapSourceDocument, SnapTargetDocument);
        ObjectSnapGuideSourceDocument = SnapSourceDocument;
        ObjectSnapGuideTargetDocument = SnapTargetDocument;

        DraggingFurniture.CenterDocumentPosition = SnappedDocumentPosition;

        SelectedFurnitureIndex = DraggingFurnitureIndex2D;

        OnPlacedFurnitureMoved2D.Broadcast(DraggingFurnitureIndex2D, DraggingFurniture);

        Invalidate(EInvalidateWidgetReason::Paint);

        return UWidgetBlueprintLibrary::Handled();
    }

    if (!bIsPlacingFurniture2D && Document.bIsValid)
    {
        int32 NewHoveredFurnitureIndex = INDEX_NONE;
        if (IsLocalPointInsideDrawArea(LocalMousePosition, CachedGeometry.GetLocalSize()))
        {
            TryGetPlacedFurnitureIndexAtLocalPosition(LocalMousePosition, CachedGeometry.GetLocalSize(), NewHoveredFurnitureIndex);
        }

        if (HoveredFurnitureIndex != NewHoveredFurnitureIndex)
        {
            HoveredFurnitureIndex = NewHoveredFurnitureIndex;
            Invalidate(EInvalidateWidgetReason::Paint);
        }

        return Reply;
    }

    if (!bIsPlacingFurniture2D || !Document.bIsValid)
    {
        return Reply;
    }

    if (!IsLocalPointInsideDrawArea(LocalMousePosition, CachedGeometry.GetLocalSize()))
    {
        bHasFurniturePreviewPosition = false;
        bHasObjectSnapGuide = false;
        Invalidate(EInvalidateWidgetReason::Paint);
        return Reply;
    }

    FVector2D RequestedDocumentPosition = TransformLocalPointToDocument(LocalMousePosition, CachedGeometry.GetLocalSize());
    FVector2D SnappedDocumentPosition = RequestedDocumentPosition;
    FVector2D SnapSourceDocument = FVector2D::ZeroVector;
    FVector2D SnapTargetDocument = FVector2D::ZeroVector;

    bHasObjectSnapGuide = ResolveObjectSnappedPreviewCenter(RequestedDocumentPosition, SnappedDocumentPosition, SnapSourceDocument, SnapTargetDocument);
    ObjectSnapGuideSourceDocument = SnapSourceDocument;
    ObjectSnapGuideTargetDocument = SnapTargetDocument;

    PreviewFurnitureCenterDocument = SnappedDocumentPosition;
    bHasFurniturePreviewPosition = true;

    OnFurniturePreviewMoved2D.Broadcast(PreviewFurnitureCenterDocument);

    Invalidate(EInvalidateWidgetReason::Paint);

    return UWidgetBlueprintLibrary::Handled();
}

void UInteReal2DFloorPlanViewportWidget::ApplyInputCatcherLayout(const FVector2D& LocalSize)
{
    if (!InputCatcherBorder)
    {
        return;
    }

    const FVector2D AreaOffset = GetDrawAreaOffset();
    const FVector2D AreaSize = GetDrawAreaSize(LocalSize);

    InputCatcherBorder->SetVisibility(ESlateVisibility::Visible);

    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(InputCatcherBorder->Slot))
    {
        CanvasSlot->SetAutoSize(false);
        CanvasSlot->SetAlignment(FVector2D::ZeroVector);
        CanvasSlot->SetPosition(AreaOffset);
        CanvasSlot->SetSize(AreaSize);
        CanvasSlot->SetZOrder(0);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("InputCatcherBorder is not inside a CanvasPanel. Position/Size cannot be applied with DrawOffset/DrawSizeOverride."));
    }
}


void UInteReal2DFloorPlanViewportWidget::StartFurniturePlacement(const FFurnitureDataRow& FurnitureRow)
{
    PendingFurnitureRow = FurnitureRow;
    PendingFurnitureSize = FVector2D(
        FMath::Max(FurnitureRow.Width, 1.0f),
        FMath::Max(FurnitureRow.Depth, 1.0f)
    );

    bIsPlacingFurniture2D = true;
    bHasFurniturePreviewPosition = false;
    bIsFurniturePreviewPlacementValid = true;
    bHasObjectSnapGuide = false;
    PreviewFurnitureRotationDegrees = 0.0f;
    
    bSelectToolActive2D = false;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void UInteReal2DFloorPlanViewportWidget::CancelFurniturePlacement()
{
    bIsPlacingFurniture2D = false;
    bHasFurniturePreviewPosition = false;
    bIsFurniturePreviewPlacementValid = true;
    bHasObjectSnapGuide = false;
    ObjectSnapGuideSourceDocument = FVector2D::ZeroVector;
    ObjectSnapGuideTargetDocument = FVector2D::ZeroVector;
    PendingFurnitureRow = FFurnitureDataRow();
    PendingFurnitureSize = FVector2D::ZeroVector;
    PreviewFurnitureRotationDegrees = 0.0f;
    
    bSelectToolActive2D = true;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void UInteReal2DFloorPlanViewportWidget::ClearPlacedFurnitures()
{
    PlacedFurnitures2D.Reset();
    SelectedFurnitureIndex = INDEX_NONE;
    HoveredFurnitureIndex = INDEX_NONE;
    bIsDraggingSelectedFurniture2D = false;
    DraggingFurnitureIndex2D = INDEX_NONE;
    FurnitureDragDocumentOffset = FVector2D::ZeroVector;

    OnPlacedFurnituresCleared2D.Broadcast();

    Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UInteReal2DFloorPlanViewportWidget::FindPlacedFurnitureIndexByGuid(const FGuid& InstanceGuid) const
{
    if (!InstanceGuid.IsValid())
    {
        return INDEX_NONE;
    }

    for (int32 FurnitureIndex = 0; FurnitureIndex < PlacedFurnitures2D.Num(); ++FurnitureIndex)
    {
        if (PlacedFurnitures2D[FurnitureIndex].InstanceGuid == InstanceGuid)
        {
            return FurnitureIndex;
        }
    }

    return INDEX_NONE;
}

bool UInteReal2DFloorPlanViewportWidget::GetPlacedFurnitureByGuid(
    const FGuid& InstanceGuid,
    FInteReal2DPlacedFurniture& OutFurniture
) const
{
    const int32 FurnitureIndex = FindPlacedFurnitureIndexByGuid(InstanceGuid);
    if (!PlacedFurnitures2D.IsValidIndex(FurnitureIndex))
    {
        return false;
    }

    OutFurniture = PlacedFurnitures2D[FurnitureIndex];
    return true;
}

FInteReal2DFloorPlanViewTransform UInteReal2DFloorPlanViewportWidget::BuildViewTransform(
    const FVector2D& LocalSize) const
{
    FInteReal2DFloorPlanViewTransform Transform;
    Transform.Document = Document;
    Transform.LocalSize = LocalSize;
    Transform.DrawOffset = DrawOffset;
    Transform.DrawSizeOverride = DrawSizeOverride;
    Transform.bUseDrawSizeOverride = bUseDrawSizeOverride;
    Transform.bFlipYForScreenSpace = bFlipYForScreenSpace;
    Transform.ViewPadding = ViewPadding;
    Transform.DrawRotationDegrees = DrawRotationDegrees;
    return Transform;
}

bool UInteReal2DFloorPlanViewportWidget::SelectPlacedFurnitureByGuid(const FGuid& InstanceGuid)
{
    return SelectPlacedFurnitureByIndex(FindPlacedFurnitureIndexByGuid(InstanceGuid));
}

bool UInteReal2DFloorPlanViewportWidget::RemovePlacedFurnitureByIndex(int32 FurnitureIndex)
{
    if (!PlacedFurnitures2D.IsValidIndex(FurnitureIndex))
    {
        return false;
    }

    const FGuid RemovedInstanceGuid = PlacedFurnitures2D[FurnitureIndex].InstanceGuid;

    PlacedFurnitures2D.RemoveAt(FurnitureIndex);

    if (SelectedFurnitureIndex == FurnitureIndex)
    {
        SelectedFurnitureIndex = INDEX_NONE;
    }
    else if (SelectedFurnitureIndex > FurnitureIndex)
    {
        --SelectedFurnitureIndex;
    }
    
    if (HoveredFurnitureIndex == FurnitureIndex)
    {
        HoveredFurnitureIndex = INDEX_NONE;
    }
    else if (HoveredFurnitureIndex > FurnitureIndex)
    {
        --HoveredFurnitureIndex;
    }

    if (DraggingFurnitureIndex2D == FurnitureIndex)
    {
        bIsDraggingSelectedFurniture2D = false;
        DraggingFurnitureIndex2D = INDEX_NONE;
        FurnitureDragDocumentOffset = FVector2D::ZeroVector;
    }
    else if (DraggingFurnitureIndex2D > FurnitureIndex)
    {
        --DraggingFurnitureIndex2D;
    }

    OnPlacedFurnitureDeleted2D.Broadcast(FurnitureIndex, RemovedInstanceGuid);

    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool UInteReal2DFloorPlanViewportWidget::RemovePlacedFurnitureByGuid(const FGuid& InstanceGuid)
{
    return RemovePlacedFurnitureByIndex(FindPlacedFurnitureIndexByGuid(InstanceGuid));
}

bool UInteReal2DFloorPlanViewportWidget::RemoveSelectedFurniture()
{
    return RemovePlacedFurnitureByIndex(SelectedFurnitureIndex);
}

FVector2D UInteReal2DFloorPlanViewportWidget::TransformLocalPointToDocument(const FVector2D& LocalPoint, const FVector2D& LocalSize) const
{
    const FVector2D UnzoomedLocalPoint = RemoveViewZoomFromLocalPoint(LocalPoint, LocalSize);
    return BuildViewTransform(LocalSize).LocalToDocument(UnzoomedLocalPoint);
}

FVector2D UInteReal2DFloorPlanViewportWidget::GetViewZoomPivotLocal(const FVector2D& LocalSize) const
{
    return GetDrawAreaOffset() + GetDrawAreaSize(LocalSize) * 0.5f;
}

FVector2D UInteReal2DFloorPlanViewportWidget::ApplyViewZoomToLocalPoint(const FVector2D& UnzoomedLocalPoint, const FVector2D& LocalSize) const
{
    const FVector2D PivotLocal = GetViewZoomPivotLocal(LocalSize);
    return PivotLocal + (UnzoomedLocalPoint - PivotLocal) * ViewZoom + ViewPanLocal;
}

FVector2D UInteReal2DFloorPlanViewportWidget::RemoveViewZoomFromLocalPoint(const FVector2D& ZoomedLocalPoint, const FVector2D& LocalSize) const
{
    const FVector2D PivotLocal = GetViewZoomPivotLocal(LocalSize);
    return PivotLocal + (ZoomedLocalPoint - PivotLocal - ViewPanLocal) / FMath::Max(ViewZoom, KINDA_SMALL_NUMBER);
}

void UInteReal2DFloorPlanViewportWidget::SetViewZoomAtLocalPosition(float NewViewZoom, const FVector2D& ZoomAnchorLocal, const FVector2D& LocalSize)
{
    NewViewZoom = FMath::Clamp(NewViewZoom, MinViewZoom, MaxViewZoom);

    if (FMath::IsNearlyEqual(ViewZoom, NewViewZoom))
    {
        return;
    }

    const FVector2D PivotLocal = GetViewZoomPivotLocal(LocalSize);
    const FVector2D AnchorBeforeZoom = PivotLocal + (ZoomAnchorLocal - PivotLocal - ViewPanLocal) / FMath::Max(ViewZoom, KINDA_SMALL_NUMBER);

    ViewZoom = NewViewZoom;

    ViewPanLocal = ZoomAnchorLocal - PivotLocal - (AnchorBeforeZoom - PivotLocal) * ViewZoom;

    Invalidate(EInvalidateWidgetReason::Paint);
}

void UInteReal2DFloorPlanViewportWidget::DrawFurnitureRect(
    FSlateWindowElementList& OutDrawElements,
    const FGeometry& AllottedGeometry,
    int32 LayerId,
    const FInteReal2DPlacedFurniture& Furniture,
    const FLinearColor& FillColor,
    const FLinearColor& OutlineColor
) const
{
    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();

    const FVector2D HalfSize = Furniture.Size * 0.5f;
    const float RotationRadians = FMath::DegreesToRadians(Furniture.RotationDegrees);
    const float CosAngle = FMath::Cos(RotationRadians);
    const float SinAngle = FMath::Sin(RotationRadians);

    const FVector2D LocalCorners[4]
    {
        FVector2D(-HalfSize.X, -HalfSize.Y),
        FVector2D(HalfSize.X, -HalfSize.Y),
        FVector2D(HalfSize.X, HalfSize.Y),
        FVector2D(-HalfSize.X, HalfSize.Y)
    };

    TArray<FVector2D> DrawPoints;
    DrawPoints.Reserve(5);

    for (const FVector2D& Corner : LocalCorners)
    {
        const FVector2D RotatedCorner(
            Corner.X * CosAngle - Corner.Y * SinAngle,
            Corner.X * SinAngle + Corner.Y * CosAngle
        );

        const FVector2D DocPoint = Furniture.CenterDocumentPosition + RotatedCorner;
        DrawPoints.Add(TransformDocumentPointToLocal(DocPoint, LocalSize));
    }

    if (DrawPoints.Num() != 4)
    {
        return;
    }

    const FVector2D ScreenCenter =
        (DrawPoints[0] + DrawPoints[1] + DrawPoints[2] + DrawPoints[3]) * 0.25f;

    const float ScreenWidth = FVector2D::Distance(DrawPoints[0], DrawPoints[1]);
    const float ScreenHeight = FVector2D::Distance(DrawPoints[1], DrawPoints[2]);

    if (ScreenWidth <= UE_SMALL_NUMBER || ScreenHeight <= UE_SMALL_NUMBER)
    {
        return;
    }

    const FVector2D ScreenXAxis = DrawPoints[1] - DrawPoints[0];
    const float ScreenRotationRadians = FMath::Atan2(ScreenXAxis.Y, ScreenXAxis.X);

    const FVector2D PaintSize(ScreenWidth, ScreenHeight);
    const FVector2D PaintPosition = ScreenCenter - PaintSize * 0.5f;

    const FSlateColorBrush FillBrush(FillColor);

    FSlateDrawElement::MakeRotatedBox(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(
            PaintSize,
            FSlateLayoutTransform(PaintPosition)
        ),
        &FillBrush,
        ESlateDrawEffect::None,
        ScreenRotationRadians,
        TOptional<FVector2D>(),
        FSlateDrawElement::RelativeToElement,
        FLinearColor::White
    );

    const FVector2D FirstDrawPoint = DrawPoints[0];
    DrawPoints.Add(FirstDrawPoint);

    FSlateDrawElement::MakeLines(
        OutDrawElements,
        LayerId + 1,
        AllottedGeometry.ToPaintGeometry(),
        DrawPoints,
        ESlateDrawEffect::None,
        OutlineColor,
        true,
        FurnitureOutlineThickness
    );
}

static float Cross2D(const FVector2D& O, const FVector2D& A, const FVector2D& B)
{
    return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X);
}

static void BuildConvexHull2D(const TArray<FVector2D>& InPoints, TArray<FVector2D>& OutHull)
{
    OutHull.Reset();

    TArray<FVector2D> Points;
    Points.Reserve(InPoints.Num());

    for (const FVector2D& Point : InPoints)
    {
        bool bExists = false;
        for (const FVector2D& Existing : Points)
        {
            if (FVector2D::Distance(Point, Existing) <= 0.1f)
            {
                bExists = true;
                break;
            }
        }

        if (!bExists)
        {
            Points.Add(Point);
        }
    }

    if (Points.Num() < 3)
    {
        OutHull = Points;
        return;
    }

    Points.Sort([](const FVector2D& A, const FVector2D& B)
    {
        if (!FMath::IsNearlyEqual(A.X, B.X, 0.01f))
        {
            return A.X < B.X;
        }

        return A.Y < B.Y;
    });

    TArray<FVector2D> Lower;
    for (const FVector2D& Point : Points)
    {
        while (Lower.Num() >= 2 && Cross2D(Lower[Lower.Num() - 2], Lower[Lower.Num() - 1], Point) <= 0.0f)
        {
            Lower.Pop();
        }

        Lower.Add(Point);
    }

    TArray<FVector2D> Upper;
    for (int32 Index = Points.Num() - 1; Index >= 0; --Index)
    {
        const FVector2D& Point = Points[Index];

        while (Upper.Num() >= 2 && Cross2D(Upper[Upper.Num() - 2], Upper[Upper.Num() - 1], Point) <= 0.0f)
        {
            Upper.Pop();
        }

        Upper.Add(Point);
    }

    Lower.Pop();
    Upper.Pop();

    OutHull.Append(Lower);
    OutHull.Append(Upper);
}

void UInteReal2DFloorPlanViewportWidget::DrawFurnitureShape(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32 LayerId, const FInteReal2DPlacedFurniture& Furniture, const FLinearColor& FillColor, const FLinearColor& OutlineColor) const
{
    if (Furniture.FootprintLocalPoints.Num() < 3)
    {
        DrawFurnitureRect(OutDrawElements, AllottedGeometry, LayerId, Furniture, FillColor, OutlineColor);
        return;
    }

    TArray<FVector2D> HullPoints;
    BuildConvexHull2D(Furniture.FootprintLocalPoints, HullPoints);

    if (HullPoints.Num() < 3)
    {
        DrawFurnitureRect(OutDrawElements, AllottedGeometry, LayerId, Furniture, FillColor, OutlineColor);
        return;
    }

    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
    const float RotationRadians = FMath::DegreesToRadians(Furniture.RotationDegrees);
    const float CosAngle = FMath::Cos(RotationRadians);
    const float SinAngle = FMath::Sin(RotationRadians);

    TArray<FVector2D> DrawPoints;
    DrawPoints.Reserve(HullPoints.Num() + 1);

    for (const FVector2D& LocalPoint : HullPoints)
    {
        const FVector2D RotatedPoint(LocalPoint.X * CosAngle - LocalPoint.Y * SinAngle, LocalPoint.X * SinAngle + LocalPoint.Y * CosAngle);
        const FVector2D DocPoint = Furniture.CenterDocumentPosition + RotatedPoint;
        DrawPoints.Add(TransformDocumentPointToLocal(DocPoint, LocalSize));
    }

    if (DrawPoints.Num() < 3)
    {
        DrawFurnitureRect(OutDrawElements, AllottedGeometry, LayerId, Furniture, FillColor, OutlineColor);
        return;
    }

    const FVector2D FirstDrawPoint = DrawPoints[0];
    DrawPoints.Add(FirstDrawPoint);

    FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), DrawPoints, ESlateDrawEffect::None, OutlineColor, true, FurnitureOutlineThickness);
}

FGuid UInteReal2DFloorPlanViewportWidget::AddPlacedFurnitureAtDocumentPosition(
    const FFurnitureDataRow& FurnitureRow,
    const FVector2D& CenterDocumentPosition,
    float RotationDegrees
)
{
    FInteReal2DPlacedFurniture NewFurniture;
    NewFurniture.InstanceGuid = FGuid::NewGuid();
    NewFurniture.FurnitureID = FurnitureRow.ID;
    NewFurniture.DisplayName = FurnitureRow.DisplayName;
    NewFurniture.CenterDocumentPosition = CenterDocumentPosition;
    NewFurniture.Size = FVector2D(
        FMath::Max(FurnitureRow.Width, 1.0f),
        FMath::Max(FurnitureRow.Depth, 1.0f)
    );
    NewFurniture.RotationDegrees = RotationDegrees;

    const FGuid AddedInstanceGuid = NewFurniture.InstanceGuid;

    PlacedFurnitures2D.Add(NewFurniture);
    ClearSelectedFurniture();

    Invalidate(EInvalidateWidgetReason::Paint);

    return AddedInstanceGuid;
}

void UInteReal2DFloorPlanViewportWidget::SetFurniturePreviewAtDocumentPosition(
    const FVector2D& CenterDocumentPosition,
    float RotationDegrees
)
{
    if (!bIsPlacingFurniture2D)
    {
        return;
    }

    PreviewFurnitureCenterDocument = CenterDocumentPosition;
    PreviewFurnitureRotationDegrees = RotationDegrees;
    bHasFurniturePreviewPosition = true;

    Invalidate(EInvalidateWidgetReason::Paint);
}

bool UInteReal2DFloorPlanViewportWidget::SelectPlacedFurnitureByIndex(int32 FurnitureIndex)
{
    if (!PlacedFurnitures2D.IsValidIndex(FurnitureIndex))
    {
        return false;
    }

    SelectedFurnitureIndex = FurnitureIndex;

    OnPlacedFurnitureSelected2D.Broadcast(
        SelectedFurnitureIndex,
        PlacedFurnitures2D[SelectedFurnitureIndex]
    );

    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

void UInteReal2DFloorPlanViewportWidget::ClearSelectedFurniture()
{
    if (SelectedFurnitureIndex == INDEX_NONE)
    {
        return;
    }

    SelectedFurnitureIndex = INDEX_NONE;
    bIsDraggingSelectedFurniture2D = false;
    DraggingFurnitureIndex2D = INDEX_NONE;
    FurnitureDragDocumentOffset = FVector2D::ZeroVector;

    OnPlacedFurnitureSelectionCleared2D.Broadcast();

    Invalidate(EInvalidateWidgetReason::Paint);
}

bool UInteReal2DFloorPlanViewportWidget::UpdatePlacedFurniture(
    int32 FurnitureIndex,
    const FVector2D& CenterDocumentPosition,
    float RotationDegrees
)
{
    if (!PlacedFurnitures2D.IsValidIndex(FurnitureIndex))
    {
        return false;
    }

    FInteReal2DPlacedFurniture& Furniture = PlacedFurnitures2D[FurnitureIndex];
    Furniture.CenterDocumentPosition = CenterDocumentPosition;
    Furniture.RotationDegrees = RotationDegrees;

    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool UInteReal2DFloorPlanViewportWidget::UpdatePlacedFurnitureByGuid(
    const FGuid& InstanceGuid,
    const FVector2D& CenterDocumentPosition,
    float RotationDegrees
)
{
    return UpdatePlacedFurniture(
        FindPlacedFurnitureIndexByGuid(InstanceGuid),
        CenterDocumentPosition,
        RotationDegrees
    );
}

bool UInteReal2DFloorPlanViewportWidget::UpdatePlacedFurnitureSize(int32 FurnitureIndex, const FVector2D& NewSize)
{
    if (!PlacedFurnitures2D.IsValidIndex(FurnitureIndex))
    {
        return false;
    }

    FInteReal2DPlacedFurniture& Furniture = PlacedFurnitures2D[FurnitureIndex];
    Furniture.Size = FVector2D(FMath::Max(NewSize.X, 1.0f), FMath::Max(NewSize.Y, 1.0f));

    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

bool UInteReal2DFloorPlanViewportWidget::UpdatePlacedFurnitureSizeByGuid(const FGuid& InstanceGuid, const FVector2D& NewSize)
{
    return UpdatePlacedFurnitureSize(FindPlacedFurnitureIndexByGuid(InstanceGuid), NewSize);
}

bool UInteReal2DFloorPlanViewportWidget::UpdateSelectedFurniture(
    const FVector2D& CenterDocumentPosition,
    float RotationDegrees
)
{
    return UpdatePlacedFurniture(
        SelectedFurnitureIndex,
        CenterDocumentPosition,
        RotationDegrees
    );
}

bool UInteReal2DFloorPlanViewportWidget::TryGetPlacedFurnitureIndexAtLocalPosition(
    const FVector2D& LocalPosition,
    const FVector2D& LocalSize,
    int32& OutFurnitureIndex
) const
{
    OutFurnitureIndex = INDEX_NONE;

    if (!Document.bIsValid)
    {
        return false;
    }

    const FVector2D DocumentPosition = TransformLocalPointToDocument(LocalPosition, LocalSize);

    for (int32 FurnitureIndex = PlacedFurnitures2D.Num() - 1; FurnitureIndex >= 0; --FurnitureIndex)
    {
        if (IsDocumentPointInsideFurniture(DocumentPosition, PlacedFurnitures2D[FurnitureIndex]))
        {
            OutFurnitureIndex = FurnitureIndex;
            return true;
        }
    }

    return false;
}

bool UInteReal2DFloorPlanViewportWidget::IsDocumentPointInsideFurniture(
    const FVector2D& DocumentPosition,
    const FInteReal2DPlacedFurniture& Furniture
) const
{
    const FVector2D HalfSize = Furniture.Size * 0.5f;
    if (HalfSize.X <= UE_SMALL_NUMBER || HalfSize.Y <= UE_SMALL_NUMBER)
    {
        return false;
    }

    const FVector2D Delta = DocumentPosition - Furniture.CenterDocumentPosition;

    const float RotationRadians = FMath::DegreesToRadians(Furniture.RotationDegrees);
    const float CosAngle = FMath::Cos(RotationRadians);
    const float SinAngle = FMath::Sin(RotationRadians);

    const FVector2D LocalPoint(
        Delta.X * CosAngle + Delta.Y * SinAngle,
        -Delta.X * SinAngle + Delta.Y * CosAngle
    );

    return FMath::Abs(LocalPoint.X) <= HalfSize.X &&
        FMath::Abs(LocalPoint.Y) <= HalfSize.Y;
}

void UInteReal2DFloorPlanViewportWidget::SetFurniturePreviewPlacementValid(bool bPlacementValid)
{
    bIsFurniturePreviewPlacementValid = bPlacementValid;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void UInteReal2DFloorPlanViewportWidget::GetFurnitureSnapOffsets(const FVector2D& FurnitureSize, float RotationDegrees, TArray<FVector2D>& OutOffsets) const
{
    OutOffsets.Reset();

    const FVector2D HalfSize = FurnitureSize * 0.5f;
    const float RotationRadians = FMath::DegreesToRadians(RotationDegrees);
    const float CosAngle = FMath::Cos(RotationRadians);
    const float SinAngle = FMath::Sin(RotationRadians);

    auto AddRotatedOffset = [&](const FVector2D& LocalOffset)
    {
        OutOffsets.Add(FVector2D(LocalOffset.X * CosAngle - LocalOffset.Y * SinAngle, LocalOffset.X * SinAngle + LocalOffset.Y * CosAngle));
    };

    AddRotatedOffset(FVector2D::ZeroVector);
    AddRotatedOffset(FVector2D(-HalfSize.X, -HalfSize.Y));
    AddRotatedOffset(FVector2D(HalfSize.X, -HalfSize.Y));
    AddRotatedOffset(FVector2D(HalfSize.X, HalfSize.Y));
    AddRotatedOffset(FVector2D(-HalfSize.X, HalfSize.Y));
    AddRotatedOffset(FVector2D(0.0f, -HalfSize.Y));
    AddRotatedOffset(FVector2D(HalfSize.X, 0.0f));
    AddRotatedOffset(FVector2D(0.0f, HalfSize.Y));
    AddRotatedOffset(FVector2D(-HalfSize.X, 0.0f));
}

void UInteReal2DFloorPlanViewportWidget::GetFurnitureEdgeSnapOffsets(const FVector2D& FurnitureSize, float RotationDegrees, TArray<FVector2D>& OutOffsets) const
{
    OutOffsets.Reset();

    const FVector2D HalfSize = FurnitureSize * 0.5f;
    const float RotationRadians = FMath::DegreesToRadians(RotationDegrees);
    const float CosAngle = FMath::Cos(RotationRadians);
    const float SinAngle = FMath::Sin(RotationRadians);

    auto AddRotatedOffset = [&](const FVector2D& LocalOffset)
    {
        OutOffsets.Add(FVector2D(LocalOffset.X * CosAngle - LocalOffset.Y * SinAngle, LocalOffset.X * SinAngle + LocalOffset.Y * CosAngle));
    };

    AddRotatedOffset(FVector2D(0.0f, -HalfSize.Y));
    AddRotatedOffset(FVector2D(HalfSize.X, 0.0f));
    AddRotatedOffset(FVector2D(0.0f, HalfSize.Y));
    AddRotatedOffset(FVector2D(-HalfSize.X, 0.0f));
}

void UInteReal2DFloorPlanViewportWidget::GetFurnitureSnapPoints(const FInteReal2DPlacedFurniture& Furniture, TArray<FVector2D>& OutPoints) const
{
    TArray<FVector2D> Offsets;
    GetFurnitureSnapOffsets(Furniture.Size, Furniture.RotationDegrees, Offsets);

    for (const FVector2D& Offset : Offsets)
    {
        OutPoints.Add(Furniture.CenterDocumentPosition + Offset);
    }
}

void UInteReal2DFloorPlanViewportWidget::GetFurnitureSnapSegments(const FInteReal2DPlacedFurniture& Furniture, TArray<TPair<FVector2D, FVector2D>>& OutSegments) const
{
    const FVector2D HalfSize = Furniture.Size * 0.5f;
    const float RotationRadians = FMath::DegreesToRadians(Furniture.RotationDegrees);
    const float CosAngle = FMath::Cos(RotationRadians);
    const float SinAngle = FMath::Sin(RotationRadians);

    auto RotatePoint = [&](const FVector2D& LocalPoint)
    {
        return Furniture.CenterDocumentPosition + FVector2D(LocalPoint.X * CosAngle - LocalPoint.Y * SinAngle, LocalPoint.X * SinAngle + LocalPoint.Y * CosAngle);
    };

    const FVector2D P0 = RotatePoint(FVector2D(-HalfSize.X, -HalfSize.Y));
    const FVector2D P1 = RotatePoint(FVector2D(HalfSize.X, -HalfSize.Y));
    const FVector2D P2 = RotatePoint(FVector2D(HalfSize.X, HalfSize.Y));
    const FVector2D P3 = RotatePoint(FVector2D(-HalfSize.X, HalfSize.Y));

    OutSegments.Add(TPair<FVector2D, FVector2D>(P0, P1));
    OutSegments.Add(TPair<FVector2D, FVector2D>(P1, P2));
    OutSegments.Add(TPair<FVector2D, FVector2D>(P2, P3));
    OutSegments.Add(TPair<FVector2D, FVector2D>(P3, P0));
}

FVector2D UInteReal2DFloorPlanViewportWidget::GetClosestPointOnSegment(const FVector2D& Point, const FVector2D& SegmentStart, const FVector2D& SegmentEnd) const
{
    const FVector2D Segment = SegmentEnd - SegmentStart;
    const float SegmentLengthSquared = Segment.SizeSquared();

    if (SegmentLengthSquared <= UE_SMALL_NUMBER)
    {
        return SegmentStart;
    }

    const float Alpha = FMath::Clamp(FVector2D::DotProduct(Point - SegmentStart, Segment) / SegmentLengthSquared, 0.0f, 1.0f);
    return SegmentStart + Segment * Alpha;
}

bool UInteReal2DFloorPlanViewportWidget::ResolveObjectSnappedPreviewCenter(const FVector2D& RequestedCenterDocument, FVector2D& OutSnappedCenterDocument, FVector2D& OutSnapSourceDocument, FVector2D& OutSnapTargetDocument) const
{
    return ResolveObjectSnappedFurnitureCenter(INDEX_NONE, PendingFurnitureSize, PreviewFurnitureRotationDegrees, RequestedCenterDocument, OutSnappedCenterDocument, OutSnapSourceDocument, OutSnapTargetDocument);
};

bool UInteReal2DFloorPlanViewportWidget::ResolveObjectSnappedFurnitureCenter(int32 IgnoreFurnitureIndex, const FVector2D& FurnitureSize, float RotationDegrees, const FVector2D& RequestedCenterDocument, FVector2D& OutSnappedCenterDocument, FVector2D& OutSnapSourceDocument, FVector2D& OutSnapTargetDocument) const
{
    OutSnappedCenterDocument = RequestedCenterDocument;
    OutSnapSourceDocument = RequestedCenterDocument;
    OutSnapTargetDocument = RequestedCenterDocument;

    if (!bEnableObjectSnap2D || ObjectSnapDistanceDocument <= 0.0f || FurnitureSize.IsNearlyZero())
    {
        return false;
    }

    const float SnapGap = FMath::Max(ObjectSnapGapDocument, 0.0f);

    TArray<FVector2D> PreviewOffsets;
    GetFurnitureSnapOffsets(FurnitureSize, RotationDegrees, PreviewOffsets);

    const float SnapDistanceSquared = ObjectSnapDistanceDocument * ObjectSnapDistanceDocument;
    float BestDistanceSquared = SnapDistanceSquared;
    bool bFoundSnap = false;

    auto TrySnapSourceToTarget = [&](const FVector2D& SourcePoint, const FVector2D& TargetPoint, const FVector2D& PreferredAwayDirection)
    {
        const float DistanceSquared = FVector2D::DistSquared(SourcePoint, TargetPoint);
        if (DistanceSquared < BestDistanceSquared)
        {
            FVector2D AwayDirection = PreferredAwayDirection.GetSafeNormal();
            if (AwayDirection.IsNearlyZero())
            {
                AwayDirection = (RequestedCenterDocument - TargetPoint).GetSafeNormal();
            }

            if (AwayDirection.IsNearlyZero())
            {
                AwayDirection = (SourcePoint - TargetPoint).GetSafeNormal();
            }

            const FVector2D GapOffset = AwayDirection * SnapGap;

            BestDistanceSquared = DistanceSquared;
            OutSnapSourceDocument = SourcePoint;
            OutSnapTargetDocument = TargetPoint;
            OutSnappedCenterDocument = RequestedCenterDocument + (TargetPoint - SourcePoint) + GapOffset;
            bFoundSnap = true;
        }
    };

    auto TrySnapToPointTarget = [&](const FVector2D& TargetPoint)
    {
        for (const FVector2D& PreviewOffset : PreviewOffsets)
        {
            const FVector2D SourcePoint = RequestedCenterDocument + PreviewOffset;
            TrySnapSourceToTarget(SourcePoint, TargetPoint, RequestedCenterDocument - TargetPoint);
        }
    };

    auto TrySnapToSegmentTarget = [&](const FVector2D& SegmentStart, const FVector2D& SegmentEnd)
    {
        const FVector2D Segment = SegmentEnd - SegmentStart;
        if (Segment.SizeSquared() <= UE_SMALL_NUMBER)
        {
            return;
        }

        TArray<FVector2D> EdgePreviewOffsets;
        GetFurnitureEdgeSnapOffsets(FurnitureSize, RotationDegrees, EdgePreviewOffsets);

        const FVector2D SegmentNormalA(-Segment.Y, Segment.X);
        const FVector2D SegmentNormalB(Segment.Y, -Segment.X);

        for (const FVector2D& PreviewOffset : EdgePreviewOffsets)
        {
            const FVector2D SourcePoint = RequestedCenterDocument + PreviewOffset;
            const FVector2D ClosestPoint = GetClosestPointOnSegment(SourcePoint, SegmentStart, SegmentEnd);
            const FVector2D CenterToLine = RequestedCenterDocument - ClosestPoint;
            const FVector2D PreferredNormal = FVector2D::DotProduct(CenterToLine, SegmentNormalA) >= 0.0f ? SegmentNormalA : SegmentNormalB;
            TrySnapSourceToTarget(SourcePoint, ClosestPoint, PreferredNormal);
        }
    };
    
    auto ShouldSnapToWall = [&](const FInteReal2DFloorPlanWallSegment& Wall) -> bool
    {
        if (!bSnapToWallSegments2D)
        {
            return false;
        }

        const bool bIsOuterWall = Wall.Type.Equals(TEXT("WallOuter"), ESearchCase::IgnoreCase);
        const bool bIsInnerWall = Wall.Type.Equals(TEXT("WallInner"), ESearchCase::IgnoreCase);

        if (bIsOuterWall && !bSnapToOuterWalls2D)
        {
            return false;
        }

        if (bIsInnerWall && !bSnapToInnerWalls2D)
        {
            return false;
        }

        return true;
    };

    auto TrySnapToWallSegmentTarget = [&](const FInteReal2DFloorPlanWallSegment& Wall)
    {
        if (!ShouldSnapToWall(Wall))
        {
            return;
        }

        const FVector2D Segment = Wall.End - Wall.Start;
        if (Segment.SizeSquared() <= UE_SMALL_NUMBER)
        {
            return;
        }

        TArray<FVector2D> EdgePreviewOffsets;
        GetFurnitureEdgeSnapOffsets(FurnitureSize, RotationDegrees, EdgePreviewOffsets);

        const float WallGap = SnapGap + (bUseWallThicknessForSnapGap2D ? FMath::Max(Wall.ThicknessCm, 0.0f) * 0.5f : 0.0f);
        const FVector2D SegmentNormalA(-Segment.Y, Segment.X);
        const FVector2D SegmentNormalB(Segment.Y, -Segment.X);

        for (const FVector2D& PreviewOffset : EdgePreviewOffsets)
        {
            const FVector2D SourcePoint = RequestedCenterDocument + PreviewOffset;
            const FVector2D ClosestPoint = GetClosestPointOnSegment(SourcePoint, Wall.Start, Wall.End);
            const FVector2D CenterToLine = RequestedCenterDocument - ClosestPoint;
            FVector2D PreferredNormal = FVector2D::DotProduct(CenterToLine, SegmentNormalA) >= 0.0f ? SegmentNormalA : SegmentNormalB;
            PreferredNormal = PreferredNormal.GetSafeNormal();

            const float DistanceSquared = FVector2D::DistSquared(SourcePoint, ClosestPoint);
            if (DistanceSquared < BestDistanceSquared)
            {
                BestDistanceSquared = DistanceSquared;
                OutSnapSourceDocument = SourcePoint;
                OutSnapTargetDocument = ClosestPoint;
                OutSnappedCenterDocument = RequestedCenterDocument + (ClosestPoint - SourcePoint) + PreferredNormal * WallGap;
                bFoundSnap = true;
            }
        }
    };

    for (int32 FurnitureIndex = 0; FurnitureIndex < PlacedFurnitures2D.Num(); ++FurnitureIndex)
    {
        if (FurnitureIndex == IgnoreFurnitureIndex)
        {
            continue;
        }

        const FInteReal2DPlacedFurniture& PlacedFurniture = PlacedFurnitures2D[FurnitureIndex];

        TArray<FVector2D> FurnitureSnapPoints;
        GetFurnitureSnapPoints(PlacedFurniture, FurnitureSnapPoints);

        for (const FVector2D& FurnitureSnapPoint : FurnitureSnapPoints)
        {
            TrySnapToPointTarget(FurnitureSnapPoint);
        }

        TArray<TPair<FVector2D, FVector2D>> FurnitureSnapSegments;
        GetFurnitureSnapSegments(PlacedFurniture, FurnitureSnapSegments);

        for (const TPair<FVector2D, FVector2D>& SegmentPair : FurnitureSnapSegments)
        {
            TrySnapToSegmentTarget(SegmentPair.Key, SegmentPair.Value);
        }
    }

    for (const FInteReal2DFloorPlanWallSegment& Wall : Document.Walls)
    {
        TrySnapToWallSegmentTarget(Wall);
    }

    if (bSnapToRoomPolygons2D)
    {
        for (const FInteReal2DFloorPlanPolygon& Room : Document.Rooms)
        {
            if (Room.Points.Num() < 2)
            {
                continue;
            }

            for (const FVector2D& RoomPoint : Room.Points)
            {
                TrySnapToPointTarget(RoomPoint);
            }

            for (int32 PointIndex = 0; PointIndex < Room.Points.Num(); ++PointIndex)
            {
                const FVector2D SegmentStart = Room.Points[PointIndex];
                const FVector2D SegmentEnd = Room.Points[(PointIndex + 1) % Room.Points.Num()];
                TrySnapToSegmentTarget(SegmentStart, SegmentEnd);
            }
        }
    }

    return bFoundSnap;
}

void UInteReal2DFloorPlanViewportWidget::HandleSelectToolButtonClicked()
{
    SetSelectToolActive2D(true);
}

void UInteReal2DFloorPlanViewportWidget::HandleObjectSnapButtonClicked()
{
    ToggleObjectSnap2D();
}

void UInteReal2DFloorPlanViewportWidget::SetSelectToolActive2D(bool bActive)
{
    bSelectToolActive2D = bActive;

    if (bSelectToolActive2D && bIsPlacingFurniture2D)
    {
        CancelFurniturePlacement();
    }

    Invalidate(EInvalidateWidgetReason::Paint);
}

void UInteReal2DFloorPlanViewportWidget::SetObjectSnapEnabled2D(bool bEnabled)
{
    bEnableObjectSnap2D = bEnabled;

    if (!bEnableObjectSnap2D)
    {
        bHasObjectSnapGuide = false;
        ObjectSnapGuideSourceDocument = FVector2D::ZeroVector;
        ObjectSnapGuideTargetDocument = FVector2D::ZeroVector;
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

void UInteReal2DFloorPlanViewportWidget::ToggleObjectSnap2D()
{
    SetObjectSnapEnabled2D(!bEnableObjectSnap2D);
}

bool UInteReal2DFloorPlanViewportWidget::UpdatePlacedFurnitureFootprintByGuid(const FGuid& InstanceGuid, const TArray<FVector2D>& FootprintLocalPoints)
{
    const int32 FurnitureIndex = FindPlacedFurnitureIndexByGuid(InstanceGuid);
    if (!PlacedFurnitures2D.IsValidIndex(FurnitureIndex))
    {
        return false;
    }

    PlacedFurnitures2D[FurnitureIndex].FootprintLocalPoints = FootprintLocalPoints;
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}

void UInteReal2DFloorPlanViewportWidget::SetFurniturePreviewFootprint(const TArray<FVector2D>& FootprintLocalPoints)
{
    PreviewFurnitureFootprintLocalPoints = FootprintLocalPoints;
    Invalidate(EInvalidateWidgetReason::Paint);
}