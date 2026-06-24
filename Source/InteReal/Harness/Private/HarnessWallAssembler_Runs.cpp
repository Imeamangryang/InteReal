// =============================================================================
//  HarnessWallAssembler_Runs.cpp
//  콜리니어(collinear) 반에지(half-edge)들을 병합하여 하나의 FWallRun으로 만드는 알고리즘.
//  같은 직선 위에 있고 같은 두께를 가진 에지들을 하나의 Run으로 묶습니다.
// =============================================================================

#include "InteReal/Harness/Public/HarnessWallAssembler.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"

namespace HarnessWall
{

TArray<FWallRun> FHarnessWallAssembler::BuildMergedWallRuns() const
{
    // Flat list of all structural core edges
    TArray<const FTopologyHalfEdge*> SourceEdges;
    SourceEdges.Reserve(Generator->EdgeCache.Num());
    for (const auto& Pair : Generator->EdgeCache)
    {
        SourceEdges.Add(&Pair.Value);
    }

    TArray<bool> bUsed;
    bUsed.Init(false, SourceEdges.Num());

    TArray<FWallRun> Runs;

    for (int32 BaseIndex = 0; BaseIndex < SourceEdges.Num(); ++BaseIndex)
    {
        if (bUsed[BaseIndex])
        {
            continue;
        }

        const FTopologyHalfEdge* BaseEdge = SourceEdges[BaseIndex];
        FVector2D BaseStart;
        FVector2D BaseEnd;
        if (!BaseEdge || !GetEdgePoints(*BaseEdge, BaseStart, BaseEnd))
        {
            bUsed[BaseIndex] = true;
            continue;
        }

        const FVector2D BaseSegment  = BaseEnd - BaseStart;
        const float BaseLength = BaseSegment.Size();
        if (BaseLength <= KINDA_SMALL_NUMBER)
        {
            bUsed[BaseIndex] = true;
            continue;
        }

        const FVector2D BaseDirection = BaseSegment / BaseLength;
        const FVector2D BaseNormal(-BaseDirection.Y, BaseDirection.X);
        const float WallThickness = BaseEdge->wall_thickness > KINDA_SMALL_NUMBER
            ? BaseEdge->wall_thickness
            : HarnessDefaultWallThicknessCm;

        TArray<int32> RunIndices;
        RunIndices.Add(BaseIndex);
        bUsed[BaseIndex] = true;

        float RunMin = 0.0f;
        float RunMax = BaseLength;

        // Greedy merge: keep absorbing collinear edges until no more can be added
        bool bMergedAny = true;
        while (bMergedAny)
        {
            bMergedAny = false;

            for (int32 CandidateIndex = 0; CandidateIndex < SourceEdges.Num(); ++CandidateIndex)
            {
                if (bUsed[CandidateIndex])
                {
                    continue;
                }

                const FTopologyHalfEdge* CandidateEdge = SourceEdges[CandidateIndex];
                FVector2D CandidateStart;
                FVector2D CandidateEnd;
                if (!CandidateEdge || !GetEdgePoints(*CandidateEdge, CandidateStart, CandidateEnd))
                {
                    bUsed[CandidateIndex] = true;
                    continue;
                }

                // Reject if wall thickness differs
                const float CandidateThickness = CandidateEdge->wall_thickness > KINDA_SMALL_NUMBER
                    ? CandidateEdge->wall_thickness
                    : HarnessDefaultWallThicknessCm;
                if (!FMath::IsNearlyEqual(WallThickness, CandidateThickness, 0.1f))
                {
                    continue;
                }

                const FVector2D CandidateSegment  = CandidateEnd - CandidateStart;
                const float CandidateLength = CandidateSegment.Size();
                if (CandidateLength <= KINDA_SMALL_NUMBER)
                {
                    bUsed[CandidateIndex] = true;
                    continue;
                }

                const FVector2D CandidateDirection = CandidateSegment / CandidateLength;

                // Must be collinear with base direction
                if (FMath::Abs(CrossHarness2D(BaseDirection, CandidateDirection)) > HarnessMergeCollinearTolerance)
                {
                    continue;
                }

                // Must lie on the same infinite line (within tolerance)
                const float StartLineDistance = FMath::Abs(FVector2D::DotProduct(CandidateStart - BaseStart, BaseNormal));
                const float EndLineDistance   = FMath::Abs(FVector2D::DotProduct(CandidateEnd   - BaseStart, BaseNormal));
                if (StartLineDistance > HarnessMergeEndpointToleranceCm ||
                    EndLineDistance   > HarnessMergeEndpointToleranceCm)
                {
                    continue;
                }

                float CandidateMin = FVector2D::DotProduct(CandidateStart - BaseStart, BaseDirection);
                float CandidateMax = FVector2D::DotProduct(CandidateEnd   - BaseStart, BaseDirection);
                if (CandidateMin > CandidateMax)
                {
                    Swap(CandidateMin, CandidateMax);
                }

                // Must touch (overlap or abut) the current run extent
                const bool bTouchesRun =
                    CandidateMin <= RunMax + HarnessMergeEndpointToleranceCm &&
                    CandidateMax >= RunMin - HarnessMergeEndpointToleranceCm;
                if (!bTouchesRun)
                {
                    continue;
                }

                RunIndices.Add(CandidateIndex);
                bUsed[CandidateIndex] = true;
                RunMin      = FMath::Min(RunMin, CandidateMin);
                RunMax      = FMath::Max(RunMax, CandidateMax);
                bMergedAny = true;
            }
        }

        // Build the merged run record
        FWallRun Run;
        Run.Start  = BaseStart + (BaseDirection * RunMin);
        Run.End    = BaseStart + (BaseDirection * RunMax);

        const FVector2D RunSegment = Run.End - Run.Start;
        Run.Length = RunSegment.Size();
        if (Run.Length <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        Run.Direction     = RunSegment / Run.Length;
        Run.Center        = (Run.Start + Run.End) * 0.5f;
        Run.Angle         = FMath::RadiansToDegrees(FMath::Atan2(RunSegment.Y, RunSegment.X));
        Run.WallThickness = WallThickness;

        TArray<FString> RunSourceTokens;
        for (int32 RunIndex : RunIndices)
        {
            const FTopologyHalfEdge* SourceEdge = SourceEdges[RunIndex];
            if (!SourceEdge)
            {
                continue;
            }
            Run.Edges.Add(SourceEdge);
            AddOpeningEdgeIds(*SourceEdge, Run.OpeningEdgeIds);
            RunSourceTokens.Add(MakeHarnessSurfaceToken(SourceEdge->id));
        }

        RunSourceTokens.Sort();
        Run.RunId   = MakeMergedSurfaceEdgeId(RunSourceTokens);
        Run.Openings = BuildOpeningsForRun(Run.OpeningEdgeIds, Run.Center, Run.Direction);
        Runs.Add(Run);
    }

    return Runs;
}

} // namespace HarnessWall
