#include "InteReal2DFloorPlanViewTransform.h"

FVector2D FInteReal2DFloorPlanViewTransform::GetDocumentSize() const
{
    return Document.BoundsMax - Document.BoundsMin;
}

FVector2D FInteReal2DFloorPlanViewTransform::GetDrawAreaOffset() const
{
    return bUseDrawSizeOverride
        ? DrawOffset
        : FVector2D::ZeroVector;
}

FVector2D FInteReal2DFloorPlanViewTransform::GetDrawAreaSize() const
{
    return bUseDrawSizeOverride
        ? FVector2D(
            FMath::Max(DrawSizeOverride.X, 1.0f),
            FMath::Max(DrawSizeOverride.Y, 1.0f)
        )
        : LocalSize;
}

bool FInteReal2DFloorPlanViewTransform::IsLocalPointInsideDrawArea(
    const FVector2D& LocalPoint
) const
{
    const FVector2D AreaOffset = GetDrawAreaOffset();
    const FVector2D AreaSize = GetDrawAreaSize();

    return LocalPoint.X >= AreaOffset.X &&
        LocalPoint.Y >= AreaOffset.Y &&
        LocalPoint.X <= AreaOffset.X + AreaSize.X &&
        LocalPoint.Y <= AreaOffset.Y + AreaSize.Y;
}

FVector2D FInteReal2DFloorPlanViewTransform::DocumentToLocal(
    const FVector2D& DocumentPoint
) const
{
    const FVector2D DrawSize = GetDrawAreaSize();
    const FVector2D DocSize = GetDocumentSize();

    const float SafeDocWidth = FMath::Max(DocSize.X, 1.0f);
    const float SafeDocHeight = FMath::Max(DocSize.Y, 1.0f);

    const float RotationRadians = FMath::DegreesToRadians(-DrawRotationDegrees);
    const float CosAngle = FMath::Cos(RotationRadians);
    const float SinAngle = FMath::Sin(RotationRadians);

    const float RotatedBoundsWidth =
        FMath::Abs(CosAngle) * SafeDocWidth +
        FMath::Abs(SinAngle) * SafeDocHeight;

    const float RotatedBoundsHeight =
        FMath::Abs(SinAngle) * SafeDocWidth +
        FMath::Abs(CosAngle) * SafeDocHeight;

    const float AvailableWidth = FMath::Max(1.0f, DrawSize.X - ViewPadding * 2.0f);
    const float AvailableHeight = FMath::Max(1.0f, DrawSize.Y - ViewPadding * 2.0f);

    const float Scale = FMath::Min(
        AvailableWidth / FMath::Max(RotatedBoundsWidth, 1.0f),
        AvailableHeight / FMath::Max(RotatedBoundsHeight, 1.0f)
    );

    const FVector2D ScaledRotatedBounds(
        RotatedBoundsWidth * Scale,
        RotatedBoundsHeight * Scale
    );

    const FVector2D Offset(
        DrawOffset.X + (DrawSize.X - ScaledRotatedBounds.X) * 0.5f,
        DrawOffset.Y + (DrawSize.Y - ScaledRotatedBounds.Y) * 0.5f
    );

    const float UnrotatedX = DocumentPoint.X - Document.BoundsMin.X;

    float UnrotatedY = 0.0f;
    if (bFlipYForScreenSpace)
    {
        UnrotatedY = Document.BoundsMax.Y - DocumentPoint.Y;
    }
    else
    {
        UnrotatedY = DocumentPoint.Y - Document.BoundsMin.Y;
    }

    const FVector2D DocumentCenter(
        SafeDocWidth * 0.5f,
        SafeDocHeight * 0.5f
    );

    const FVector2D CenteredPoint =
        FVector2D(UnrotatedX, UnrotatedY) - DocumentCenter;

    const FVector2D RotatedCenteredPoint(
        CenteredPoint.X * CosAngle - CenteredPoint.Y * SinAngle,
        CenteredPoint.X * SinAngle + CenteredPoint.Y * CosAngle
    );

    const FVector2D RotatedPoint =
        RotatedCenteredPoint +
        FVector2D(RotatedBoundsWidth * 0.5f, RotatedBoundsHeight * 0.5f);

    return Offset + RotatedPoint * Scale;
}

FVector2D FInteReal2DFloorPlanViewTransform::LocalToDocument(
    const FVector2D& LocalPoint
) const
{
    const FVector2D DrawSize = GetDrawAreaSize();
    const FVector2D DocSize = GetDocumentSize();

    const float SafeDocWidth = FMath::Max(DocSize.X, 1.0f);
    const float SafeDocHeight = FMath::Max(DocSize.Y, 1.0f);

    const float RotationRadians = FMath::DegreesToRadians(-DrawRotationDegrees);
    const float CosAngle = FMath::Cos(RotationRadians);
    const float SinAngle = FMath::Sin(RotationRadians);

    const float RotatedBoundsWidth =
        FMath::Abs(CosAngle) * SafeDocWidth +
        FMath::Abs(SinAngle) * SafeDocHeight;

    const float RotatedBoundsHeight =
        FMath::Abs(SinAngle) * SafeDocWidth +
        FMath::Abs(CosAngle) * SafeDocHeight;

    const float AvailableWidth = FMath::Max(1.0f, DrawSize.X - ViewPadding * 2.0f);
    const float AvailableHeight = FMath::Max(1.0f, DrawSize.Y - ViewPadding * 2.0f);

    const float Scale = FMath::Min(
        AvailableWidth / FMath::Max(RotatedBoundsWidth, 1.0f),
        AvailableHeight / FMath::Max(RotatedBoundsHeight, 1.0f)
    );

    const FVector2D ScaledRotatedBounds(
        RotatedBoundsWidth * Scale,
        RotatedBoundsHeight * Scale
    );

    const FVector2D Offset(
        DrawOffset.X + (DrawSize.X - ScaledRotatedBounds.X) * 0.5f,
        DrawOffset.Y + (DrawSize.Y - ScaledRotatedBounds.Y) * 0.5f
    );

    const FVector2D RotatedPoint =
        (LocalPoint - Offset) / FMath::Max(Scale, UE_SMALL_NUMBER);

    const FVector2D RotatedCenteredPoint =
        RotatedPoint - FVector2D(
            RotatedBoundsWidth * 0.5f,
            RotatedBoundsHeight * 0.5f
        );

    const FVector2D CenteredPoint(
        RotatedCenteredPoint.X * CosAngle + RotatedCenteredPoint.Y * SinAngle,
        -RotatedCenteredPoint.X * SinAngle + RotatedCenteredPoint.Y * CosAngle
    );

    const FVector2D DocumentCenter(
        SafeDocWidth * 0.5f,
        SafeDocHeight * 0.5f
    );

    const FVector2D UnrotatedPoint = CenteredPoint + DocumentCenter;

    const float DocX = Document.BoundsMin.X + UnrotatedPoint.X;

    const float DocY = bFlipYForScreenSpace
        ? Document.BoundsMax.Y - UnrotatedPoint.Y
        : Document.BoundsMin.Y + UnrotatedPoint.Y;

    return FVector2D(DocX, DocY);
}