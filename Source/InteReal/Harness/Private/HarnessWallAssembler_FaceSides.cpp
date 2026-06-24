// =============================================================================
//  HarnessWallAssembler_FaceSides.cpp
//  방(Face) 윤곽선을 따라 Wall Surface Side(벽면 측면)를 구성하고 병합합니다.
//
//  주요 단계:
//   1. 각 Face 윤곽 세그먼트에 겹치는 Core Edge를 탐색
//   2. 겹치는 구간(span)을 FWallSurfaceSide 레코드로 변환
//   3. 인접하고 같은 방향/높이를 가진 Side들을 병합
//   4. 각 Side에 속한 개구부를 최종 배정
// =============================================================================

#include "InteReal/Harness/Public/HarnessWallAssembler.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"

namespace HarnessWall
{

// -----------------------------------------------------------------------------
// MakeMergedSurfaceEdgeId
// -----------------------------------------------------------------------------
FString FHarnessWallAssembler::MakeMergedSurfaceEdgeId(const TArray<FString>& SourceIds)
{
    if (SourceIds.Num() == 0)
    {
        return FString(TEXT("Unknown"));
    }
    if (SourceIds.Num() == 1)
    {
        return SourceIds[0];
    }
    return FString::Printf(TEXT("Merge_%s_%s_%d"), *SourceIds[0], *SourceIds.Last(), SourceIds.Num());
}

// -----------------------------------------------------------------------------
// CanMergeSurfaceSides — returns true if two adjacent sides can be joined
// -----------------------------------------------------------------------------
bool FHarnessWallAssembler::CanMergeSurfaceSides(
    const FWallSurfaceSide& A,
    const FWallSurfaceSide& B)
{
    if (A.Direction.IsNearlyZero() || B.Direction.IsNearlyZero())
    {
        return false;
    }

    // Must share the same room geometry properties
    if (!FMath::IsNearlyEqual(A.Height,        B.Height,        0.1f) ||
        !FMath::IsNearlyEqual(A.ZOffset,       B.ZOffset,       0.1f) ||
        !FMath::IsNearlyEqual(A.WallThickness, B.WallThickness, 0.1f))
    {
        return false;
    }

    // Must be collinear and face the same room interior
    if (FVector2D::DotProduct(A.Direction, B.Direction) < 0.999f ||
        FMath::Abs(CrossHarness2D(A.Direction, B.Direction)) > HarnessMergeCollinearTolerance ||
        FVector2D::DotProduct(A.InteriorNormal, B.InteriorNormal) < 0.999f)
    {
        return false;
    }

    // Must abut (endpoints touch)
    const FVector2D AEnd   = A.Center + (A.Direction * (A.Length * 0.5f));
    const FVector2D BStart = B.Center - (B.Direction * (B.Length * 0.5f));
    const FVector2D AStart = A.Center - (A.Direction * (A.Length * 0.5f));
    const FVector2D BEnd   = B.Center + (B.Direction * (B.Length * 0.5f));

    return FVector2D::Distance(AEnd, BStart) <= HarnessMergeEndpointToleranceCm ||
           FVector2D::Distance(AStart, BEnd) <= HarnessMergeEndpointToleranceCm;
}

// -----------------------------------------------------------------------------
// MergeSurfaceSideInto — extends Target to also cover Source
// -----------------------------------------------------------------------------
void FHarnessWallAssembler::MergeSurfaceSideInto(
    FWallSurfaceSide& Target,
    const FWallSurfaceSide& Source)
{
    const FVector2D Start   = Target.Center - (Target.Direction * (Target.Length * 0.5f));
    const FVector2D End     = Source.Center + (Source.Direction * (Source.Length * 0.5f));
    const FVector2D Segment = End - Start;
    const float Length = Segment.Size();
    if (Length <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    Target.Length    = Length;
    Target.Center    = (Start + End) * 0.5f;
    Target.Direction = Segment / Length;
    Target.Angle     = FMath::RadiansToDegrees(FMath::Atan2(Segment.Y, Segment.X));
    Target.InteriorNormal = (Target.InteriorNormal + Source.InteriorNormal).GetSafeNormal();
    if (Target.InteriorNormal.IsNearlyZero())
    {
        Target.InteriorNormal = Source.InteriorNormal;
    }

    for (const FString& Id : Source.SourceSurfaceEdgeIds) { Target.SourceSurfaceEdgeIds.AddUnique(Id); }
    for (const FString& Id : Source.CoreEdgeIds)           { Target.CoreEdgeIds.AddUnique(Id); }
    for (const FString& Id : Source.OpeningEdgeIds)        { Target.OpeningEdgeIds.Add(Id); }

    Target.bHasOuterWall = Target.bHasOuterWall || Source.bHasOuterWall;
}

// -----------------------------------------------------------------------------
// BuildMergedFaceSides — builds all wall-surface sides for one room face
// -----------------------------------------------------------------------------
TArray<FWallSurfaceSide> FHarnessWallAssembler::BuildMergedFaceSides(
    const FTopologyFace& Face) const
{
    const int32 NumVerts = Face.contour_vertex_ids.Num();
    if (NumVerts < 3)
    {
        return {};
    }

    TArray<FVector2D> FacePoints;
    if (!BuildFacePoints(Face, FacePoints))
    {
        return {};
    }

    // --- Step 1: Build raw segments from each face-contour edge ---
    TArray<FWallSurfaceSide> Segments;

    for (int32 i = 0; i < NumVerts; ++i)
    {
        const FString& CurrId = Face.contour_vertex_ids[i];
        const FString& NextId = Face.contour_vertex_ids[(i + 1) % NumVerts];
        const FVector2D* CurrPoint = Generator->VertexCache.Find(CurrId);
        const FVector2D* NextPoint = Generator->VertexCache.Find(NextId);
        if (!CurrPoint || !NextPoint) continue;

        const FVector2D Segment = *NextPoint - *CurrPoint;
        const float SegmentLength = Segment.Size();
        if (SegmentLength <= KINDA_SMALL_NUMBER) continue;
        const FVector2D SegmentDir = Segment / SegmentLength;

        // Use boundary_wall_ids to find the corresponding wall
        const FTopologyHalfEdge* MatchedEdge = nullptr;
        if (i < Face.boundary_wall_ids.Num())
        {
            const FString& WallId = Face.boundary_wall_ids[i];
            // Find edge in EdgeCache by wall_id
            for (const auto& Pair : Generator->EdgeCache)
            {
                if (Pair.Value.wall_id == WallId)
                {
                    MatchedEdge = &Pair.Value;
                    break;
                }
            }
        }

        // If no boundary_wall match, try geometric fallback
        if (!MatchedEdge)
        {
            // Find the closest edge by geometric proximity (increased tolerance for wall offset)
            float BestDist = 50.0f; // max distance threshold in cm
            for (const auto& Pair : Generator->EdgeCache)
            {
                FVector2D EdgeStart, EdgeEnd;
                if (!GetEdgePoints(Pair.Value, EdgeStart, EdgeEnd)) continue;
                
                // Compare edge direction to segment direction
                FVector2D EdgeDir = (EdgeEnd - EdgeStart).GetSafeNormal();
                if (FMath::Abs(CrossHarness2D(SegmentDir, EdgeDir)) > HarnessMergeCollinearTolerance)
                {
                    // Check reversed direction
                    EdgeDir = -EdgeDir;
                    if (FMath::Abs(CrossHarness2D(SegmentDir, EdgeDir)) > HarnessMergeCollinearTolerance)
                        continue;
                }
                
                FVector2D SegCenter = (*CurrPoint + *NextPoint) * 0.5f;
                FVector2D EdgeCenter = (EdgeStart + EdgeEnd) * 0.5f;
                float Dist = FVector2D::Distance(SegCenter, EdgeCenter);
                if (Dist < BestDist)
                {
                    BestDist = Dist;
                    MatchedEdge = &Pair.Value;
                }
            }
        }

        if (!MatchedEdge) continue;

        FWallSurfaceSide Side;
        Side.FaceId = MakeHarnessSurfaceToken(Face.face_id);
        Side.Height = FMath::Max(Face.height_cm, 1.0f);
        Side.ZOffset = Face.z_offset;
        Side.Length = SegmentLength;
        Side.Angle = FMath::RadiansToDegrees(FMath::Atan2(Segment.Y, Segment.X));
        Side.Center = (*CurrPoint + *NextPoint) * 0.5f;
        Side.Direction = SegmentDir;
        Side.InteriorNormal = ComputeHarnessInteriorNormal2D(FacePoints, *CurrPoint, *NextPoint).GetSafeNormal();
        
        Side.SourceSurfaceEdgeIds.AddUnique(MakeHarnessSurfaceToken(MatchedEdge->id));
        Side.bHasOuterWall = MatchedEdge->type.Equals(TEXT("WallOuter"), ESearchCase::IgnoreCase);
        Side.WallThickness = MatchedEdge->wall_thickness > KINDA_SMALL_NUMBER ? MatchedEdge->wall_thickness : HarnessDefaultWallThicknessCm;
        
        AddOpeningEdgeIds(*MatchedEdge, Side.OpeningEdgeIds);
        
        if (const FString* CoreEdgeId = CoreEdgeIdByAnyEdgeId.Find(MatchedEdge->id))
            Side.CoreEdgeIds.AddUnique(*CoreEdgeId);
        else
            Side.CoreEdgeIds.AddUnique(MatchedEdge->id);
            
        Side.SurfaceEdgeId = MakeMergedSurfaceEdgeId(Side.SourceSurfaceEdgeIds);
        Segments.Add(Side);
    }

    // --- Step 3: Merge adjacent collinear sides ---
    TArray<FWallSurfaceSide> MergedSides;
    for (const FWallSurfaceSide& Seg : Segments)
    {
        if (MergedSides.Num() > 0 && CanMergeSurfaceSides(MergedSides.Last(), Seg))
        {
            MergeSurfaceSideInto(MergedSides.Last(), Seg);
        }
        else
        {
            MergedSides.Add(Seg);
        }
    }

    // Try wrapping: merge last into first (polygon wrap-around)
    if (MergedSides.Num() > 1 && CanMergeSurfaceSides(MergedSides.Last(), MergedSides[0]))
    {
        FWallSurfaceSide FirstSide = MergedSides[0];
        MergeSurfaceSideInto(MergedSides.Last(), FirstSide);
        MergedSides.RemoveAt(0);
    }

    // --- Step 4: Assign openings to each merged side ---
    for (FWallSurfaceSide& Side : MergedSides)
    {
        Side.SurfaceEdgeId = MakeMergedSurfaceEdgeId(Side.SourceSurfaceEdgeIds);
        Side.Openings      = BuildOpeningsForRun(Side.OpeningEdgeIds, Side.Center, Side.Direction);
    }

    return MergedSides;
}

} // namespace HarnessWall
