#include "InteReal2DFloorPlanViewportWidget.h"

#include "InteReal2DFloorPlanConverter.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Rendering/DrawElements.h"

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

FVector2D UInteReal2DFloorPlanViewportWidget::GetDocumentSize() const
{
    return Document.BoundsMax - Document.BoundsMin;
}

FVector2D UInteReal2DFloorPlanViewportWidget::TransformDocumentPointToLocal(const FVector2D& DocPoint, const FVector2D& LocalSize) const
{
    const FVector2D DocSize = GetDocumentSize();

    const float SafeDocWidth = FMath::Max(DocSize.X, 1.0f);
    const float SafeDocHeight = FMath::Max(DocSize.Y, 1.0f);

    const float AvailableWidth = FMath::Max(1.0f, LocalSize.X - ViewPadding * 2.0f);
    const float AvailableHeight = FMath::Max(1.0f, LocalSize.Y - ViewPadding * 2.0f);

    const float Scale = FMath::Min(AvailableWidth / SafeDocWidth, AvailableHeight / SafeDocHeight);

    const FVector2D ScaledDocSize(SafeDocWidth * Scale, SafeDocHeight * Scale);
    const FVector2D Offset(
        (LocalSize.X - ScaledDocSize.X) * 0.5f,
        (LocalSize.Y - ScaledDocSize.Y) * 0.5f
    );

    const float LocalX = Offset.X + (DocPoint.X - Document.BoundsMin.X) * Scale;

    float LocalY = 0.0f;
    if (bFlipYForScreenSpace)
    {
        LocalY = Offset.Y + (Document.BoundsMax.Y - DocPoint.Y) * Scale;
    }
    else
    {
        LocalY = Offset.Y + (DocPoint.Y - Document.BoundsMin.Y) * Scale;
    }

    return FVector2D(LocalX, LocalY);
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

    return LayerId + 1;
}