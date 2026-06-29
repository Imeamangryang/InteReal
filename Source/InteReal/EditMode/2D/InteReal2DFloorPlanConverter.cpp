#include "InteReal2DFloorPlanConverter.h"

#include "Algo/Reverse.h"
#include "Public/HarnessGeneratorGeometry.h"

using namespace InteReal::HarnessGenerator;

namespace
{
    constexpr float FloorPlanBoundaryExpansionCm = 10.2f;
    constexpr float FloorPlanDuplicatePointToleranceCm = 1.0f;
    constexpr float FloorPlanCollinearTolerance = 0.01f;

    TArray<FVector2D> BuildCleanRoomPoints(const TArray<FVector2D>& RawPoints)
    {
        TArray<FVector2D> CleanPoints;
        for (const FVector2D& Point : RawPoints)
        {
            if (CleanPoints.Num() == 0 || FVector2D::Distance(CleanPoints.Last(), Point) > FloorPlanDuplicatePointToleranceCm)
            {
                CleanPoints.Add(Point);
            }
        }

        if (CleanPoints.Num() > 1 && FVector2D::Distance(CleanPoints.Last(), CleanPoints[0]) <= FloorPlanDuplicatePointToleranceCm)
        {
            CleanPoints.Pop();
        }

        TArray<FVector2D> NonCollinearPoints;
        const int32 NumCleanPoints = CleanPoints.Num();
        if (NumCleanPoints >= 3)
        {
            for (int32 Index = 0; Index < NumCleanPoints; ++Index)
            {
                const FVector2D Prev = CleanPoints[(Index - 1 + NumCleanPoints) % NumCleanPoints];
                const FVector2D Curr = CleanPoints[Index];
                const FVector2D Next = CleanPoints[(Index + 1) % NumCleanPoints];

                const FVector2D DirA = (Curr - Prev).GetSafeNormal();
                const FVector2D DirB = (Next - Curr).GetSafeNormal();
                const float Cross = DirA.X * DirB.Y - DirA.Y * DirB.X;

                if (FMath::Abs(Cross) > FloorPlanCollinearTolerance)
                {
                    NonCollinearPoints.Add(Curr);
                }
            }
        }

        if (NonCollinearPoints.Num() < 3)
        {
            NonCollinearPoints = CleanPoints;
        }

        if (NonCollinearPoints.Num() >= 3 && FloorPlanBoundaryExpansionCm > KINDA_SMALL_NUMBER)
        {
            const TArray<FVector2D> ExpandedPoints = OffsetHarnessPolygon2D(NonCollinearPoints, FloorPlanBoundaryExpansionCm);
            if (ExpandedPoints.Num() >= 3)
            {
                NonCollinearPoints = ExpandedPoints;
            }
        }

        if (NonCollinearPoints.Num() >= 3 && ComputeHarnessSignedArea(NonCollinearPoints) < 0.0)
        {
            Algo::Reverse(NonCollinearPoints);
        }

        return NonCollinearPoints;
    }

    float ResolveOpeningWidthCm(const FTopologyOpening& Opening, const FVector2D& OpeningStart, const FVector2D& OpeningEnd)
    {
        const float SpanWidthCm = FVector2D::Distance(OpeningStart, OpeningEnd);

        if (Opening.bHasSpan && SpanWidthCm > UE_SMALL_NUMBER)
        {
            return SpanWidthCm;
        }

        if (Opening.measured_width_cm > UE_SMALL_NUMBER)
        {
            return Opening.measured_width_cm;
        }

        if (Opening.drawn_width_cm > UE_SMALL_NUMBER)
        {
            return Opening.drawn_width_cm;
        }

        return Opening.width_cm;
    }

    bool BuildOpeningFromSpan(const FHarnessFloorData& FloorData, const FTopologyOpening& Opening, FInteReal2DFloorPlanOpening& OutOpening)
    {
        if (!Opening.bHasSpan)
        {
            return false;
        }

        const FVector2D SpanStart = FloorData.ToHarnessPoint(Opening.span_start_cm);
        const FVector2D SpanEnd = FloorData.ToHarnessPoint(Opening.span_end_cm);
        if (FVector2D::Distance(SpanStart, SpanEnd) <= KINDA_SMALL_NUMBER)
        {
            return false;
        }

        OutOpening.Type = Opening.type;
        OutOpening.Start = SpanStart;
        OutOpening.End = SpanEnd;
        return true;
    }

    bool BuildOpeningFromTargetEdge(const TMap<FString, FVector2D>& VertexMap, const TMap<FString, FTopologyHalfEdge>& EdgeMap, const FTopologyOpening& Opening, FInteReal2DFloorPlanOpening& OutOpening)
    {
        const FTopologyHalfEdge* Edge = EdgeMap.Find(Opening.target_edge_id);
        if (!Edge)
        {
            return false;
        }

        const FVector2D* StartPoint = VertexMap.Find(Edge->vertex_start);
        const FVector2D* EndPoint = VertexMap.Find(Edge->vertex_end);
        if (!StartPoint || !EndPoint)
        {
            return false;
        }

        const FVector2D WallSegment = *EndPoint - *StartPoint;
        const float WallLength = WallSegment.Size();
        if (WallLength <= KINDA_SMALL_NUMBER)
        {
            return false;
        }

        const FVector2D WallDirection = WallSegment / WallLength;
        float CenterDistance = FMath::Clamp(Opening.offset_to_center_cm, 0.0f, WallLength);
        if (Opening.offset_from.Equals(TEXT("end"), ESearchCase::IgnoreCase))
        {
            CenterDistance = WallLength - CenterDistance;
        }

        const FVector2D Center = *StartPoint + WallDirection * CenterDistance;
        const float HalfWidth = FMath::Max(Opening.width_cm * 0.5f, 1.0f);

        OutOpening.Type = Opening.type;
        OutOpening.Start = Center - WallDirection * HalfWidth;
        OutOpening.End = Center + WallDirection * HalfWidth;
        return true;
    }
}

FVector2D FInteReal2DFloorPlanConverter::ConvertTopologyVertexToEditorPoint(const FHarnessFloorData& FloorData, const FTopologyVertex& Vertex)
{
    return FloorData.ToHarnessPoint(Vertex);
}

void FInteReal2DFloorPlanConverter::ComputeBounds(FInteReal2DFloorPlanDocument& Document)
{
    FVector2D Min(UE_BIG_NUMBER, UE_BIG_NUMBER);
    FVector2D Max(-UE_BIG_NUMBER, -UE_BIG_NUMBER);
    bool bHasPoint = false;
    
    for (const FInteReal2DFloorPlanWallSegment& Wall : Document.Walls)
    {
        bHasPoint = true;
        Min.X = FMath::Min(Min.X, FMath::Min(Wall.Start.X, Wall.End.X));
        Min.Y = FMath::Min(Min.Y, FMath::Min(Wall.Start.Y, Wall.End.Y));
        Max.X = FMath::Max(Max.X, FMath::Max(Wall.Start.X, Wall.End.X));
        Max.Y = FMath::Max(Max.Y, FMath::Max(Wall.Start.Y, Wall.End.Y));
    }

    for (const FInteReal2DFloorPlanPolygon& Room : Document.Rooms)
    {
        for (const FVector2D& Pt : Room.Points)
        {
            bHasPoint = true;
            Min.X = FMath::Min(Min.X, Pt.X);
            Min.Y = FMath::Min(Min.Y, Pt.Y);
            Max.X = FMath::Max(Max.X, Pt.X);
            Max.Y = FMath::Max(Max.Y, Pt.Y);
        }
    }

    for (const FInteReal2DFloorPlanOpening& Opening : Document.Openings)
    {
        bHasPoint = true;
        Min.X = FMath::Min(Min.X, FMath::Min(Opening.Start.X, Opening.End.X));
        Min.Y = FMath::Min(Min.Y, FMath::Min(Opening.Start.Y, Opening.End.Y));
        Max.X = FMath::Max(Max.X, FMath::Max(Opening.Start.X, Opening.End.X));
        Max.Y = FMath::Max(Max.Y, FMath::Max(Opening.Start.Y, Opening.End.Y));
    }

    if (!bHasPoint)
    {
        Document.BoundsMin = FVector2D::ZeroVector;
        Document.BoundsMax = FVector2D::ZeroVector;
        Document.bIsValid = false;
        return;
    }

    Document.BoundsMin = Min;
    Document.BoundsMax = Max;
    Document.bIsValid = true;
}

FInteReal2DFloorPlanDocument FInteReal2DFloorPlanConverter::ConvertFromHarness(const FHarnessFloorData& FloorData)
{
    FInteReal2DFloorPlanDocument Document;
    Document.bFlipYForScreenSpace = false;

    TMap<FString, FVector2D> VertexMap;
    for (const FTopologyVertex& Vertex : FloorData.vertices)
    {
        VertexMap.Add(Vertex.id, ConvertTopologyVertexToEditorPoint(FloorData, Vertex));
    }

    TSet<FString> AddedWallIds;

    for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
    {
        const FString WallKey = !Edge.wall_id.IsEmpty() ? Edge.wall_id : Edge.id;
        if (AddedWallIds.Contains(WallKey))
        {
            continue;
        }

        const FVector2D* StartPoint = VertexMap.Find(Edge.vertex_start);
        const FVector2D* EndPoint = VertexMap.Find(Edge.vertex_end);
        if (!StartPoint || !EndPoint)
        {
            continue;
        }

        if (FVector2D::Distance(*StartPoint, *EndPoint) <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        FInteReal2DFloorPlanWallSegment Wall;
        Wall.WallId = WallKey;
        Wall.Start = *StartPoint;
        Wall.End = *EndPoint;
        Wall.ThicknessCm = FMath::Max(Edge.wall_thickness, 1.0f);
        Wall.Type = Edge.type;

        Document.Walls.Add(Wall);
        AddedWallIds.Add(WallKey);
    }
    
    for (const FTopologyFace& Face : FloorData.faces)
    {
        if (Face.contour_vertex_ids.Num() < 3)
        {
            continue;
        }

        TArray<FVector2D> RawRoomPoints;
        for (const FString& VertexId : Face.contour_vertex_ids)
        {
            if (const FVector2D* Found = VertexMap.Find(VertexId))
            {
                RawRoomPoints.Add(*Found);
            }
        }

        TArray<FVector2D> RoomPoints = BuildCleanRoomPoints(RawRoomPoints);
        if (RoomPoints.Num() < 3)
        {
            continue;
        }

        FInteReal2DFloorPlanPolygon Room;
        Room.Label = Face.label;
        Room.Points = MoveTemp(RoomPoints);
        Document.Rooms.Add(Room);
    }

    TMap<FString, FTopologyHalfEdge> EdgeMap;
    for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
    {
        EdgeMap.Add(Edge.id, Edge);
    }

    for (const FTopologyOpening& Opening : FloorData.openings)
    {
        FInteReal2DFloorPlanOpening NewOpening;
        if (BuildOpeningFromSpan(FloorData, Opening, NewOpening))
        {
            Document.Openings.Add(NewOpening);
            continue;
        }

        if (BuildOpeningFromTargetEdge(VertexMap, EdgeMap, Opening, NewOpening))
        {
            Document.Openings.Add(NewOpening);
        }
    }

    ComputeBounds(Document);
    return Document;
}