// =============================================================================
//  HarnessWallAssembler_EdgeUtils.cpp
//  기하 쿼리 유틸리티:
//    - Edge 양 끝점 조회
//    - 개구부(Opening) 에지 ID 수집
//    - Face 점 목록 빌드
//    - Face 윤곽선 측면(Side) 검색
//    - Run / FaceSide에 속하는 개구부 목록 구성
//    - Face 세그먼트와 교차하는 에지 탐색
// =============================================================================

#include "InteReal/Harness/Public/HarnessWallAssembler.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"

namespace HarnessWall
{

// -----------------------------------------------------------------------------
// Constructor + context lookup table setup
// -----------------------------------------------------------------------------
FHarnessWallAssembler::FHarnessWallAssembler(
    UHarnessGeneratorComponent* InGenerator,
    const FHarnessFloorData& InFloorData)
    : Generator(InGenerator)
    , FloorData(InFloorData)
{
    check(Generator);

    // Build HalfEdgeById for fast opening-edge lookups
    for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
    {
        HalfEdgeById.Add(Edge.id, &Edge);
    }

    // Build CoreEdgeIdByAnyEdgeId: maps any edge (or its twin) to the canonical core ID
    for (const auto& Pair : Generator->EdgeCache)
    {
        const FTopologyHalfEdge& Edge = Pair.Value;
        CoreEdgeIdByAnyEdgeId.Add(Edge.id, Edge.id);
        if (!Edge.twin_id.IsEmpty())
        {
            CoreEdgeIdByAnyEdgeId.Add(Edge.twin_id, Edge.id);
        }
    }

    // Build wall_id → faces mapping from boundary_wall_ids
    for (const FTopologyFace& Face : FloorData.faces)
    {
        for (const FString& WallId : Face.boundary_wall_ids)
        {
            WallToFacesMap.FindOrAdd(WallId).AddUnique(&Face);
        }
    }
}

// -----------------------------------------------------------------------------
// BuildFacePoints
// -----------------------------------------------------------------------------
bool FHarnessWallAssembler::BuildFacePoints(
    const FTopologyFace& Face,
    TArray<FVector2D>& OutPoints) const
{
    OutPoints.Reset();
    for (const FString& VertexId : Face.contour_vertex_ids)
    {
        if (const FVector2D* Point = Generator->VertexCache.Find(VertexId))
        {
            OutPoints.Add(*Point);
        }
    }
    return OutPoints.Num() >= 3;
}

// -----------------------------------------------------------------------------
// GetEdgePoints
// -----------------------------------------------------------------------------
bool FHarnessWallAssembler::GetEdgePoints(
    const FTopologyHalfEdge& Edge,
    FVector2D& OutStart,
    FVector2D& OutEnd) const
{
    const FVector2D* StartPoint = Generator->VertexCache.Find(Edge.vertex_start);
    const FVector2D* EndPoint   = Generator->VertexCache.Find(Edge.vertex_end);
    if (!StartPoint || !EndPoint)
    {
        return false;
    }
    OutStart = *StartPoint;
    OutEnd   = *EndPoint;
    return FVector2D::Distance(OutStart, OutEnd) > KINDA_SMALL_NUMBER;
}

// -----------------------------------------------------------------------------
// AddOpeningEdgeIds
// -----------------------------------------------------------------------------
void FHarnessWallAssembler::AddOpeningEdgeIds(
    const FTopologyHalfEdge& Edge,
    TSet<FString>& OutEdgeIds) const
{
    OutEdgeIds.Add(Edge.id);
    if (!Edge.twin_id.IsEmpty())
    {
        OutEdgeIds.Add(Edge.twin_id);
    }

    if (const FString* CoreEdgeId = CoreEdgeIdByAnyEdgeId.Find(Edge.id))
    {
        OutEdgeIds.Add(*CoreEdgeId);
    }
    if (!Edge.twin_id.IsEmpty())
    {
        if (const FString* TwinCoreEdgeId = CoreEdgeIdByAnyEdgeId.Find(Edge.twin_id))
        {
            OutEdgeIds.Add(*TwinCoreEdgeId);
        }
    }
}

// -----------------------------------------------------------------------------
// FindFaceSide — finds which face-contour segment corresponds to a given edge
// -----------------------------------------------------------------------------
bool FHarnessWallAssembler::FindFaceSide(
    const FTopologyHalfEdge& Edge,
    const FTopologyFace& Face,
    FWallSurfaceSide& OutSide) const
{
    const int32 NumVerts = Face.contour_vertex_ids.Num();
    if (NumVerts < 3)
    {
        return false;
    }

    TArray<FVector2D> FacePoints;
    if (!BuildFacePoints(Face, FacePoints))
    {
        return false;
    }

    for (int32 i = 0; i < NumVerts; ++i)
    {
        const FString& CurrId = Face.contour_vertex_ids[i];
        const FString& NextId = Face.contour_vertex_ids[(i + 1) % NumVerts];

        const bool bMatchesForward = CurrId == Edge.vertex_start && NextId == Edge.vertex_end;
        const bool bMatchesTwin    = CurrId == Edge.vertex_end   && NextId == Edge.vertex_start;
        if (!bMatchesForward && !bMatchesTwin)
        {
            continue;
        }

        const FVector2D* CurrPoint = Generator->VertexCache.Find(CurrId);
        const FVector2D* NextPoint = Generator->VertexCache.Find(NextId);
        if (!CurrPoint || !NextPoint)
        {
            return false;
        }

        const FVector2D Segment = *NextPoint - *CurrPoint;
        const float SegmentLength = Segment.Size();
        if (SegmentLength <= KINDA_SMALL_NUMBER)
        {
            return false;
        }

        const FVector2D SegmentDir     = Segment / SegmentLength;
        const FVector2D SideCenter     = (*CurrPoint + *NextPoint) * 0.5f;
        const FVector2D InteriorNormal = ComputeHarnessInteriorNormal2D(FacePoints, *CurrPoint, *NextPoint);

        OutSide.FaceId = MakeHarnessSurfaceToken(Face.face_id);
        OutSide.SurfaceEdgeId = (bMatchesForward || Edge.twin_id.IsEmpty())
            ? MakeHarnessSurfaceToken(Edge.id)
            : MakeHarnessSurfaceToken(Edge.twin_id);
        OutSide.Height        = FMath::Max(Face.height_cm, 1.0f);
        OutSide.ZOffset       = Face.z_offset;
        OutSide.Length        = SegmentLength;
        OutSide.Angle         = FMath::RadiansToDegrees(FMath::Atan2(Segment.Y, Segment.X));
        OutSide.Center        = SideCenter;
        OutSide.Direction     = SegmentDir;
        OutSide.InteriorNormal = InteriorNormal.GetSafeNormal();
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// BuildOpeningsForRun — collects and projects openings onto a run / face-side
// -----------------------------------------------------------------------------
TArray<FMergedOpening> FHarnessWallAssembler::BuildOpeningsForRun(
    const TSet<FString>& EdgeIds,
    const FVector2D& RunCenter,
    const FVector2D& RunDirection) const
{
    TArray<FMergedOpening> Result;
    if (RunDirection.IsNearlyZero())
    {
        return Result;
    }

    for (const FTopologyOpening& Opening : FloorData.openings)
    {
        const FTopologyHalfEdge* HostEdge = nullptr;

        // Try direct target-edge match first
        if (!Opening.target_edge_id.IsEmpty() && EdgeIds.Contains(Opening.target_edge_id))
        {
            if (const FTopologyHalfEdge* const* HostEdgePtr = HalfEdgeById.Find(Opening.target_edge_id))
            {
                HostEdge = *HostEdgePtr;
            }
        }

        // Fallback: match by wall_id
        if (!HostEdge)
        {
            for (const FString& EdgeId : EdgeIds)
            {
                if (const FTopologyHalfEdge* const* EdgePtr = HalfEdgeById.Find(EdgeId))
                {
                    if ((*EdgePtr)->wall_id == Opening.host_wall_id)
                    {
                        HostEdge = *EdgePtr;
                        break;
                    }
                }
            }
        }

        if (!HostEdge)
        {
            continue;
        }

        FVector2D OpeningStart;
        FVector2D OpeningEnd;
        if (!GetEdgePoints(*HostEdge, OpeningStart, OpeningEnd))
        {
            continue;
        }

        const FVector2D HostSegment = OpeningEnd - OpeningStart;
        const float HostLength = HostSegment.Size();
        if (HostLength <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const FVector2D HostDirection = HostSegment / HostLength;
        float CenterDistance = FMath::Clamp(Opening.offset_to_center_cm, 0.0f, HostLength);
        if (Opening.offset_from.Equals(TEXT("end"), ESearchCase::IgnoreCase))
        {
            CenterDistance = HostLength - CenterDistance;
        }

        const FVector2D OpeningCenter   = OpeningStart + (HostDirection * CenterDistance);
        const float EffectiveOpeningWidth = FMath::Max(Opening.width_cm, 1.0f);

        FMergedOpening MergedOpening;
        MergedOpening.Opening = &Opening;
        MergedOpening.CenterX = FVector2D::DotProduct(OpeningCenter - RunCenter, RunDirection);
        MergedOpening.WidthCm = EffectiveOpeningWidth;
        Result.Add(MergedOpening);
    }

    return Result;
}

// -----------------------------------------------------------------------------
// FindEdgesOverlappingFaceSegment — returns all core edges that lie along a
// given face-contour segment, sorted by start parameter.
// -----------------------------------------------------------------------------
bool FHarnessWallAssembler::FindEdgesOverlappingFaceSegment(
    const FVector2D& SegmentStart,
    const FVector2D& SegmentEnd,
    TArray<FFaceSegmentEdgeMatch>& OutMatches) const
{
    OutMatches.Reset();

    const FVector2D Segment = SegmentEnd - SegmentStart;
    const float SegmentLength = Segment.Size();
    if (SegmentLength <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const FVector2D SegmentDirection = Segment / SegmentLength;
    const FVector2D SegmentNormal(-SegmentDirection.Y, SegmentDirection.X);

    for (const auto& Pair : Generator->EdgeCache)
    {
        const FTopologyHalfEdge& Edge = Pair.Value;
        FVector2D EdgeStart;
        FVector2D EdgeEnd;
        if (!GetEdgePoints(Edge, EdgeStart, EdgeEnd))
        {
            continue;
        }

        const FVector2D EdgeSegment = EdgeEnd - EdgeStart;
        const float EdgeLength = EdgeSegment.Size();
        if (EdgeLength <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const FVector2D EdgeDirection = EdgeSegment / EdgeLength;

        // Must be (nearly) collinear
        if (FMath::Abs(CrossHarness2D(SegmentDirection, EdgeDirection)) > HarnessMergeCollinearTolerance)
        {
            continue;
        }

        // Must lie on the same line
        const float StartLineDistance = FMath::Abs(FVector2D::DotProduct(EdgeStart - SegmentStart, SegmentNormal));
        const float EndLineDistance   = FMath::Abs(FVector2D::DotProduct(EdgeEnd   - SegmentStart, SegmentNormal));
        if (StartLineDistance > HarnessMergeEndpointToleranceCm ||
            EndLineDistance   > HarnessMergeEndpointToleranceCm)
        {
            continue;
        }

        float EdgeStartT = FVector2D::DotProduct(EdgeStart - SegmentStart, SegmentDirection);
        float EdgeEndT   = FVector2D::DotProduct(EdgeEnd   - SegmentStart, SegmentDirection);
        if (EdgeStartT > EdgeEndT)
        {
            Swap(EdgeStartT, EdgeEndT);
        }

        const float OverlapStart  = FMath::Max(EdgeStartT, 0.0f);
        const float OverlapEnd    = FMath::Min(EdgeEndT, SegmentLength);
        const float OverlapLength = OverlapEnd - OverlapStart;
        if (OverlapLength <= HarnessMergeEndpointToleranceCm)
        {
            continue;
        }

        FFaceSegmentEdgeMatch Match;
        Match.Edge          = &Edge;
        Match.StartT        = OverlapStart;
        Match.EndT          = OverlapEnd;
        Match.OverlapLength = OverlapLength;
        const bool bSameDirection = FVector2D::DotProduct(EdgeDirection, SegmentDirection) >= 0.0f;
        Match.SurfaceEdgeId = bSameDirection || Edge.twin_id.IsEmpty() ? Edge.id : Edge.twin_id;
        OutMatches.Add(Match);
    }

    OutMatches.Sort([](const FFaceSegmentEdgeMatch& A, const FFaceSegmentEdgeMatch& B)
    {
        if (!FMath::IsNearlyEqual(A.StartT, B.StartT, 0.05f))
        {
            return A.StartT < B.StartT;
        }
        return A.SurfaceEdgeId < B.SurfaceEdgeId;
    });

    return OutMatches.Num() > 0;
}

} // namespace HarnessWall
