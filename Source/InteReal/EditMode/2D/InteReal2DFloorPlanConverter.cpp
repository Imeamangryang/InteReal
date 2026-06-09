#include "InteReal2DFloorPlanConverter.h"

FVector2D FInteReal2DFloorPlanConverter::ConvertTopologyVertexToEditorPoint(const FTopologyVertex& Vertex)
{
    // 기존 HarnessGeneratorComponent와 좌표계 일치
    return FVector2D(Vertex.y, Vertex.x);
}

void FInteReal2DFloorPlanConverter::ComputeBounds(FInteReal2DFloorPlanDocument& Document)
{
    FVector2D Min(UE_BIG_NUMBER, UE_BIG_NUMBER);
    FVector2D Max(-UE_BIG_NUMBER, -UE_BIG_NUMBER);
    bool bHasPoint = false;

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

    TMap<FString, FVector2D> VertexMap;
    for (const FTopologyVertex& Vertex : FloorData.vertices)
    {
        VertexMap.Add(Vertex.id, ConvertTopologyVertexToEditorPoint(Vertex));
    }

    for (const FTopologyFace& Face : FloorData.faces)
    {
        if (Face.contour_vertex_ids.Num() < 3)
        {
            continue;
        }

        FInteReal2DFloorPlanPolygon Room;
        Room.Label = Face.label;

        for (const FString& VertexId : Face.contour_vertex_ids)
        {
            if (const FVector2D* Found = VertexMap.Find(VertexId))
            {
                Room.Points.Add(*Found);
            }
        }

        if (Room.Points.Num() >= 3)
        {
            Document.Rooms.Add(Room);
        }
    }

    TMap<FString, FTopologyHalfEdge> EdgeMap;
    for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
    {
        EdgeMap.Add(Edge.id, Edge);
    }

    for (const FTopologyOpening& Opening : FloorData.openings)
    {
        const FTopologyHalfEdge* Edge = EdgeMap.Find(Opening.target_edge_id);
        if (!Edge)
        {
            continue;
        }

        const FVector2D* StartPoint = VertexMap.Find(Edge->vertex_start);
        const FVector2D* EndPoint = VertexMap.Find(Edge->vertex_end);
        if (!StartPoint || !EndPoint)
        {
            continue;
        }

        FInteReal2DFloorPlanOpening NewOpening;
        NewOpening.Type = Opening.type;
        NewOpening.Start = *StartPoint;
        NewOpening.End = *EndPoint;

        Document.Openings.Add(NewOpening);
    }

    ComputeBounds(Document);
    return Document;
}