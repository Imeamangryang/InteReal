#include "InteReal2DFloorPlanViewportWidget.h"
#include "InteReal2DFloorPlanConverter.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "Components/Border.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "InteReal2DFloorPlanViewTransform.h"

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
    return BuildViewTransform(LocalSize).DocumentToLocal(DocPoint);
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
        InputCatcherBorder->OnMouseButtonDownEvent.BindDynamic(
            this,
            &UInteReal2DFloorPlanViewportWidget::HandleInputCatcherMouseButtonDown
        );
        InputCatcherBorder->OnMouseButtonUpEvent.BindDynamic(
            this,
            &UInteReal2DFloorPlanViewportWidget::HandleInputCatcherMouseButtonUp
        );
        InputCatcherBorder->OnMouseMoveEvent.BindDynamic(
            this,
            &UInteReal2DFloorPlanViewportWidget::HandleInputCatcherMouseMove
        );

        ApplyInputCatcherLayout(GetCachedGeometry().GetLocalSize());
    }
}

int32 UInteReal2DFloorPlanViewportWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{
    if (bDrawBackground)
    {
        const FVector2D BackgroundOffset = GetDrawAreaOffset();
        const FVector2D BackgroundSize = GetDrawAreaSize(AllottedGeometry.GetLocalSize());

        const FSlateRoundedBoxBrush BackgroundBrush(
            BackgroundColor,
            BackgroundCornerRadius
        );

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(
                BackgroundSize,
                FSlateLayoutTransform(BackgroundOffset)
            ),
            &BackgroundBrush,
            ESlateDrawEffect::None,
            InWidgetStyle.GetColorAndOpacityTint()
        );

        ++LayerId;
    }
    
    LayerId = Super::NativePaint(
        Args,
        AllottedGeometry,
        MyCullingRect,
        OutDrawElements,
        LayerId,
        InWidgetStyle,
        bParentEnabled
    );

    if (!Document.bIsValid)
    {
        return LayerId;
    }

    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();

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

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(),
            DrawPoints,
            ESlateDrawEffect::None,
            RoomLineColor,
            true,
            RoomLineThickness
        );
    }

    ++LayerId;

    for (const FInteReal2DFloorPlanOpening& Opening : Document.Openings)
    {
        TArray<FVector2D> SegmentPoints;
        SegmentPoints.Add(TransformDocumentPointToLocal(Opening.Start, LocalSize));
        SegmentPoints.Add(TransformDocumentPointToLocal(Opening.End, LocalSize));

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(),
            SegmentPoints,
            ESlateDrawEffect::None,
            ResolveOpeningColor(Opening.Type),
            true,
            OpeningLineThickness
        );
    }

    ++LayerId;

    for (int32 FurnitureIndex = 0; FurnitureIndex < PlacedFurnitures2D.Num(); ++FurnitureIndex)
    {
        const FInteReal2DPlacedFurniture& Furniture = PlacedFurnitures2D[FurnitureIndex];
        const bool bIsSelected = FurnitureIndex == SelectedFurnitureIndex;

        DrawFurnitureRect(
            OutDrawElements,
            AllottedGeometry,
            LayerId,
            Furniture,
            bIsSelected ? SelectedFurnitureFillColor : FurnitureFillColor,
            bIsSelected ? SelectedFurnitureOutlineColor : FurnitureOutlineColor
        );
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

        DrawFurnitureRect(
            OutDrawElements,
            AllottedGeometry,
            LayerId,
            PreviewFurniture,
            FurniturePreviewFillColor,
            FurniturePreviewOutlineColor
        );

        LayerId += 2;
    }

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

FEventReply UInteReal2DFloorPlanViewportWidget::HandleInputCatcherMouseButtonDown(
    FGeometry MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    FEventReply Reply;

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
        PreviewFurnitureCenterDocument = TransformLocalPointToDocument(
            LastClickedLocalPosition,
            CachedGeometry.GetLocalSize()
        );
        bHasFurniturePreviewPosition = true;

        OnFurniturePlacementRequested2D.Broadcast(PreviewFurnitureCenterDocument);
    }
    else if (Document.bIsValid)
    {
        int32 HitFurnitureIndex = INDEX_NONE;
        if (TryGetPlacedFurnitureIndexAtLocalPosition(
            LastClickedLocalPosition,
            CachedGeometry.GetLocalSize(),
            HitFurnitureIndex
        ))
        {
            SelectPlacedFurnitureByIndex(HitFurnitureIndex);

            const FVector2D ClickDocumentPosition = TransformLocalPointToDocument(
                LastClickedLocalPosition,
                CachedGeometry.GetLocalSize()
            );

            bIsDraggingSelectedFurniture2D = true;
            DraggingFurnitureIndex2D = HitFurnitureIndex;
            FurnitureDragDocumentOffset =
                PlacedFurnitures2D[HitFurnitureIndex].CenterDocumentPosition - ClickDocumentPosition;

            FEventReply HandledReply = UWidgetBlueprintLibrary::Handled();
            return UWidgetBlueprintLibrary::CaptureMouse(HandledReply, InputCatcherBorder);
        }

        ClearSelectedFurniture();
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
    const FVector2D LocalMousePosition =
        CachedGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

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

        const FVector2D MouseDocumentPosition = TransformLocalPointToDocument(
            LocalMousePosition,
            CachedGeometry.GetLocalSize()
        );

        FInteReal2DPlacedFurniture& DraggingFurniture =
            PlacedFurnitures2D[DraggingFurnitureIndex2D];

        DraggingFurniture.CenterDocumentPosition =
            MouseDocumentPosition + FurnitureDragDocumentOffset;

        SelectedFurnitureIndex = DraggingFurnitureIndex2D;

        OnPlacedFurnitureMoved2D.Broadcast(
            DraggingFurnitureIndex2D,
            DraggingFurniture
        );

        Invalidate(EInvalidateWidgetReason::Paint);

        return UWidgetBlueprintLibrary::Handled();
    }

    if (!bIsPlacingFurniture2D || !Document.bIsValid)
    {
        return Reply;
    }

    if (!IsLocalPointInsideDrawArea(LocalMousePosition, CachedGeometry.GetLocalSize()))
    {
        bHasFurniturePreviewPosition = false;
        Invalidate(EInvalidateWidgetReason::Paint);
        return Reply;
    }

    PreviewFurnitureCenterDocument = TransformLocalPointToDocument(
        LocalMousePosition,
        CachedGeometry.GetLocalSize()
    );
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
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("InputCatcherBorder is not inside a CanvasPanel. Position/Size cannot be applied with DrawOffset/DrawSizeOverride.")
        );
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
    PreviewFurnitureRotationDegrees = 0.0f;

    Invalidate(EInvalidateWidgetReason::Paint);
}

void UInteReal2DFloorPlanViewportWidget::CancelFurniturePlacement()
{
    bIsPlacingFurniture2D = false;
    bHasFurniturePreviewPosition = false;
    PendingFurnitureRow = FFurnitureDataRow();
    PendingFurnitureSize = FVector2D::ZeroVector;
    PreviewFurnitureRotationDegrees = 0.0f;

    Invalidate(EInvalidateWidgetReason::Paint);
}

void UInteReal2DFloorPlanViewportWidget::ClearPlacedFurnitures()
{
    PlacedFurnitures2D.Reset();
    SelectedFurnitureIndex = INDEX_NONE;
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
    return BuildViewTransform(LocalSize).LocalToDocument(LocalPoint);
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