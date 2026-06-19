#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/MeshNormals.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Public/HarnessGeneratorGeometry.h"

using namespace InteReal::HarnessGenerator;

void UHarnessGeneratorComponent::AssembleStructuralWalls(const FHarnessFloorData& FloorData)
{
    struct FMergedOpening
    {
        const FTopologyOpening* Opening = nullptr;
        float CenterX = 0.0f;
        float WidthCm = 0.0f;
    };

    struct FWallSurfaceSide
    {
        FString FaceId;
        FString SurfaceEdgeId;
        TArray<FString> SourceSurfaceEdgeIds;
        TArray<FString> CoreEdgeIds;
        TSet<FString> OpeningEdgeIds;
        TArray<FMergedOpening> Openings;
        bool bHasOuterWall = false;
        bool bTouchesAnotherRoom = false;
        float Height = HarnessDefaultWallHeightCm;
        float ZOffset = 0.0f;
        float Length = 0.0f;
        float Angle = 0.0f;
        float WallThickness = HarnessDefaultWallThicknessCm;
        float StartInset = 0.0f;
        float EndInset = 0.0f;
        FVector2D Center = FVector2D::ZeroVector;
        FVector2D Direction = FVector2D::ZeroVector;
        FVector2D InteriorNormal = FVector2D::ZeroVector;
        FVector2D PrevDirection = FVector2D::ZeroVector;
        FVector2D PrevInteriorNormal = FVector2D::ZeroVector;
        FVector2D NextDirection = FVector2D::ZeroVector;
        FVector2D NextInteriorNormal = FVector2D::ZeroVector;
    };

    struct FWallRun
    {
        FString RunId;
        TArray<const FTopologyHalfEdge*> Edges;
        TSet<FString> OpeningEdgeIds;
        TArray<FMergedOpening> Openings;
        FVector2D Start = FVector2D::ZeroVector;
        FVector2D End = FVector2D::ZeroVector;
        FVector2D Center = FVector2D::ZeroVector;
        FVector2D Direction = FVector2D::ZeroVector;
        float Length = 0.0f;
        float Angle = 0.0f;
        float WallThickness = HarnessDefaultWallThicknessCm;
        float StartInset = 0.0f;
        float EndInset = 0.0f;
    };

    struct FFaceSegmentEdgeMatch
    {
        const FTopologyHalfEdge* Edge = nullptr;
        FString SurfaceEdgeId;
        float StartT = 0.0f;
        float EndT = 0.0f;
        float OverlapLength = 0.0f;
    };

    auto BuildFacePoints = [&](const FTopologyFace& Face, TArray<FVector2D>& OutPoints) -> bool
    {
        OutPoints.Reset();
        for (const FString& VertexId : Face.contour_vertex_ids)
        {
            if (const FVector2D* Point = VertexCache.Find(VertexId))
            {
                OutPoints.Add(*Point);
            }
        }
        return OutPoints.Num() >= 3;
    };

    TMap<FString, const FTopologyHalfEdge*> HalfEdgeById;
    for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
    {
        HalfEdgeById.Add(Edge.id, &Edge);
    }

    TMap<FString, FString> CoreEdgeIdByAnyEdgeId;
    for (const auto& Pair : EdgeCache)
    {
        const FTopologyHalfEdge& Edge = Pair.Value;
        CoreEdgeIdByAnyEdgeId.Add(Edge.id, Edge.id);
        if (!Edge.twin_id.IsEmpty())
        {
            CoreEdgeIdByAnyEdgeId.Add(Edge.twin_id, Edge.id);
        }
    }

    auto GetEdgePoints = [&](const FTopologyHalfEdge& Edge, FVector2D& OutStart, FVector2D& OutEnd) -> bool
    {
        const FVector2D* StartPoint = VertexCache.Find(Edge.vertex_start);
        const FVector2D* EndPoint = VertexCache.Find(Edge.vertex_end);
        if (!StartPoint || !EndPoint)
        {
            return false;
        }

        OutStart = *StartPoint;
        OutEnd = *EndPoint;
        return FVector2D::Distance(OutStart, OutEnd) > KINDA_SMALL_NUMBER;
    };

    auto AddOpeningEdgeIds = [&](const FTopologyHalfEdge& Edge, TSet<FString>& OutEdgeIds)
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
    };

    auto BuildOpeningsForRun = [&](const TSet<FString>& EdgeIds, const FVector2D& RunCenter, const FVector2D& RunDirection) -> TArray<FMergedOpening>
    {
        TArray<FMergedOpening> Result;
        if (RunDirection.IsNearlyZero())
        {
            return Result;
        }

        for (const FTopologyOpening& Opening : FloorData.openings)
        {
            if (!EdgeIds.Contains(Opening.target_edge_id))
            {
                continue;
            }

            FVector2D OpeningCenter = RunCenter;
            float EffectiveOpeningWidth = FMath::Max(Opening.width_cm, 1.0f);
            if (const FTopologyHalfEdge* const* OpeningEdgePtr = HalfEdgeById.Find(Opening.target_edge_id))
            {
                FVector2D OpeningStart;
                FVector2D OpeningEnd;
                if (GetEdgePoints(**OpeningEdgePtr, OpeningStart, OpeningEnd))
                {
                    OpeningCenter = (OpeningStart + OpeningEnd) * 0.5f;
                    EffectiveOpeningWidth = FMath::Max(EffectiveOpeningWidth, FVector2D::Distance(OpeningStart, OpeningEnd));
                }
            }

            FMergedOpening MergedOpening;
            MergedOpening.Opening = &Opening;
            MergedOpening.CenterX = FVector2D::DotProduct(OpeningCenter - RunCenter, RunDirection);
            MergedOpening.WidthCm = EffectiveOpeningWidth;
            Result.Add(MergedOpening);
        }

        return Result;
    };

    auto FindEdgesOverlappingFaceSegment = [&](const FVector2D& SegmentStart, const FVector2D& SegmentEnd, TArray<FFaceSegmentEdgeMatch>& OutMatches) -> bool
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

        for (const auto& Pair : EdgeCache)
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
            if (FMath::Abs(CrossHarness2D(SegmentDirection, EdgeDirection)) > HarnessMergeCollinearTolerance)
            {
                continue;
            }

            const float StartLineDistance = FMath::Abs(FVector2D::DotProduct(EdgeStart - SegmentStart, SegmentNormal));
            const float EndLineDistance = FMath::Abs(FVector2D::DotProduct(EdgeEnd - SegmentStart, SegmentNormal));
            if (StartLineDistance > HarnessMergeEndpointToleranceCm || EndLineDistance > HarnessMergeEndpointToleranceCm)
            {
                continue;
            }

            float EdgeStartT = FVector2D::DotProduct(EdgeStart - SegmentStart, SegmentDirection);
            float EdgeEndT = FVector2D::DotProduct(EdgeEnd - SegmentStart, SegmentDirection);
            if (EdgeStartT > EdgeEndT)
            {
                Swap(EdgeStartT, EdgeEndT);
            }

            const float OverlapStart = FMath::Max(EdgeStartT, 0.0f);
            const float OverlapEnd = FMath::Min(EdgeEndT, SegmentLength);
            const float OverlapLength = OverlapEnd - OverlapStart;
            if (OverlapLength <= HarnessMergeEndpointToleranceCm)
            {
                continue;
            }

            FFaceSegmentEdgeMatch Match;
            Match.Edge = &Edge;
            Match.StartT = OverlapStart;
            Match.EndT = OverlapEnd;
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
    };

    auto FindFaceSide = [&](const FTopologyHalfEdge& Edge, const FTopologyFace& Face, FWallSurfaceSide& OutSide) -> bool
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

        auto ComputeInteriorNormal = [&](const FVector2D& A, const FVector2D& B) -> FVector2D
        {
            return ComputeHarnessInteriorNormal2D(FacePoints, A, B);
        };

        for (int32 i = 0; i < NumVerts; ++i)
        {
            const FString& CurrId = Face.contour_vertex_ids[i];
            const FString& NextId = Face.contour_vertex_ids[(i + 1) % NumVerts];

            const bool bMatchesForward = CurrId == Edge.vertex_start && NextId == Edge.vertex_end;
            const bool bMatchesTwin = CurrId == Edge.vertex_end && NextId == Edge.vertex_start;
            if (!bMatchesForward && !bMatchesTwin)
            {
                continue;
            }

            const FVector2D* CurrPoint = VertexCache.Find(CurrId);
            const FVector2D* NextPoint = VertexCache.Find(NextId);
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

            const FVector2D SegmentDir = Segment / SegmentLength;
            const FVector2D SideCenter = (*CurrPoint + *NextPoint) * 0.5f;
            const FVector2D InteriorNormal = ComputeInteriorNormal(*CurrPoint, *NextPoint);

            const FString& PrevId = Face.contour_vertex_ids[(i - 1 + NumVerts) % NumVerts];
            const FString& AfterId = Face.contour_vertex_ids[(i + 2) % NumVerts];
            const FVector2D* PrevPoint = VertexCache.Find(PrevId);
            const FVector2D* AfterPoint = VertexCache.Find(AfterId);

            OutSide.FaceId = MakeHarnessSurfaceToken(Face.face_id);
            OutSide.SurfaceEdgeId = bMatchesForward || Edge.twin_id.IsEmpty()
                ? MakeHarnessSurfaceToken(Edge.id)
                : MakeHarnessSurfaceToken(Edge.twin_id);
            OutSide.Height = FMath::Max(Face.height_cm, 1.0f);
            OutSide.ZOffset = Face.z_offset;
            OutSide.Length = SegmentLength;
            OutSide.Angle = FMath::RadiansToDegrees(FMath::Atan2(Segment.Y, Segment.X));
            OutSide.Center = SideCenter;
            OutSide.Direction = SegmentDir;
            OutSide.InteriorNormal = InteriorNormal.GetSafeNormal();

            if (PrevPoint)
            {
                OutSide.PrevDirection = (*CurrPoint - *PrevPoint).GetSafeNormal();
                OutSide.PrevInteriorNormal = ComputeInteriorNormal(*PrevPoint, *CurrPoint).GetSafeNormal();
            }

            if (AfterPoint)
            {
                OutSide.NextDirection = (*AfterPoint - *NextPoint).GetSafeNormal();
                OutSide.NextInteriorNormal = ComputeInteriorNormal(*NextPoint, *AfterPoint).GetSafeNormal();
            }

            return true;
        }

        return false;
    };

    auto ApplyOpeningsToWall = [&](UDynamicMesh* DynMesh, const TArray<FMergedOpening>& Openings, float WallDepth, float BottomZ, const FGeometryScriptPrimitiveOptions& PrimOptions)
    {
        for (const FMergedOpening& MergedOpening : Openings)
        {
            const FTopologyOpening* Opening = MergedOpening.Opening;
            if (!Opening)
            {
                continue;
            }

            UDynamicMesh* HoleMesh = NewObject<UDynamicMesh>();
            const float HoleWidth = FMath::Max(MergedOpening.WidthCm + 2.0f, 1.0f);
            float HoleBottom = Opening->z_offset_cm;
            float HoleTop = Opening->z_offset_cm + FMath::Max(Opening->height_cm, 1.0f);

            if (HoleBottom <= 0.1f)
            {
                HoleBottom = BottomZ - 1.0f;
            }
            HoleTop += 0.5f;

            const float HoleHeight = FMath::Max(HoleTop - HoleBottom, 1.0f);
            const float HoleZCenter = (HoleBottom + HoleTop) * 0.5f;
            const float HoleDepth = WallDepth + 6.0f;

            FTransform HoleTransform(FRotator::ZeroRotator, FVector(MergedOpening.CenterX, 0.0f, HoleZCenter), FVector::OneVector);
            FBox HoleBox(
                FVector(-HoleWidth * 0.5f, -HoleDepth * 0.5f, -HoleHeight * 0.5f),
                FVector(HoleWidth * 0.5f, HoleDepth * 0.5f, HoleHeight * 0.5f)
            );

            UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBoundingBox(
                HoleMesh, PrimOptions, HoleTransform, HoleBox, 0, 0, 0
            );

            FGeometryScriptMeshBooleanOptions BoolOptions;
            UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
                DynMesh, FTransform::Identity, HoleMesh, FTransform::Identity,
                EGeometryScriptBooleanOperation::Subtract, BoolOptions
            );
        }
    };

    auto BuildWallBox = [&](const FVector2D& Center2D, float Angle, float Length, float Depth, float BottomZ, float TopZ, const TArray<FMergedOpening>& Openings, const TArray<FName>& Tags, bool bEditable)
    {
        UDynamicMeshComponent* WallComp = NewObject<UDynamicMeshComponent>(GetOwner());
        if (!WallComp)
        {
            return;
        }

        WallComp->SetMobility(EComponentMobility::Movable);
        WallComp->RegisterComponent();
        WallComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        if (bEditable)
        {
            WallComp->ComponentTags.Add(TEXT("EditableWall"));
        }

        for (const FName& Tag : Tags)
        {
            WallComp->ComponentTags.AddUnique(Tag);
        }

        WallComp->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.01f));
        WallComp->SetRelativeLocationAndRotation(FVector(Center2D.X, Center2D.Y, 0.0f), FRotator(0.0f, Angle, 0.0f));

        UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(WallComp);
        WallComp->SetDynamicMesh(DynMesh);

        const float ClampedLength = FMath::Max(Length, 1.0f);
        const float ClampedDepth = FMath::Max(Depth, 0.1f);
        const float Height = FMath::Max(TopZ - BottomZ, 1.0f);
        const float ZCenter = (BottomZ + TopZ) * 0.5f;

        FGeometryScriptPrimitiveOptions PrimOptions;
        FTransform BaseTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, ZCenter), FVector::OneVector);
        FBox WallBox(
            FVector(-ClampedLength * 0.5f, -ClampedDepth * 0.5f, -Height * 0.5f),
            FVector(ClampedLength * 0.5f, ClampedDepth * 0.5f, Height * 0.5f)
        );

        UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBoundingBox(
            DynMesh, PrimOptions, BaseTransform, WallBox, 0, 0, 0
        );

        ApplyOpeningsToWall(DynMesh, Openings, ClampedDepth, BottomZ, PrimOptions);

        if (DefaultFallbackMaterial)
        {
            WallComp->SetMaterial(0, DefaultFallbackMaterial);
        }

        WallComp->SetComplexAsSimpleCollisionEnabled(true, true);
        WallComp->SetCollisionProfileName(TEXT("NoCollision"));
        WallComp->bCastShadowAsTwoSided = true;
        WallComp->NotifyMeshUpdated();

        SpawnedComponents.Add(WallComp);
        AnimatedWalls.Add(WallComp);
    };

    auto BuildWallSurfacePanels = [&](const FWallSurfaceSide& Side, const FVector2D& SurfaceNormal, const FVector2D& PrevSurfaceNormal, const FVector2D& NextSurfaceNormal, float WallThickness, const TArray<FName>& Tags)
    {
        if (Side.Direction.IsNearlyZero() || SurfaceNormal.IsNearlyZero())
        {
            return;
        }

        const float OffsetDistance = (WallThickness * 0.5f) + HarnessSurfaceGapCm;
        const FVector2D SurfaceCenter = Side.Center + (SurfaceNormal * OffsetDistance);
        const FVector2D BaseStart = Side.Center - (Side.Direction * (Side.Length * 0.5f));
        const FVector2D BaseEnd = Side.Center + (Side.Direction * (Side.Length * 0.5f));

        const float HalfLength = Side.Length * 0.5f;
        const float MaxMiterAdjustment = OffsetDistance + HarnessMergeEndpointToleranceCm;
        float XMin = -HalfLength;
        float XMax = HalfLength;

        FVector2D MiterPoint;
        if (!Side.PrevDirection.IsNearlyZero() && !PrevSurfaceNormal.IsNearlyZero() &&
            IntersectHarnessLines2D(BaseStart + (PrevSurfaceNormal * OffsetDistance), Side.PrevDirection, BaseStart + (SurfaceNormal * OffsetDistance), Side.Direction, MiterPoint))
        {
            XMin = FVector2D::DotProduct(MiterPoint - SurfaceCenter, Side.Direction);
            XMin = FMath::Clamp(XMin, -HalfLength - MaxMiterAdjustment, -HalfLength + MaxMiterAdjustment);
        }

        if (!Side.NextDirection.IsNearlyZero() && !NextSurfaceNormal.IsNearlyZero() &&
            IntersectHarnessLines2D(BaseEnd + (SurfaceNormal * OffsetDistance), Side.Direction, BaseEnd + (NextSurfaceNormal * OffsetDistance), Side.NextDirection, MiterPoint))
        {
            XMax = FVector2D::DotProduct(MiterPoint - SurfaceCenter, Side.Direction);
            XMax = FMath::Clamp(XMax, HalfLength - MaxMiterAdjustment, HalfLength + MaxMiterAdjustment);
        }

        if (XMax - XMin <= 1.0f)
        {
            XMin = -HalfLength;
            XMax = HalfLength;
        }

        float StartInset = FMath::Max(Side.StartInset, 0.0f);
        float EndInset = FMath::Max(Side.EndInset, 0.0f);
        const float MaxTotalInset = FMath::Max(Side.Length - 1.0f, 0.0f);
        const float TotalInset = StartInset + EndInset;
        if (TotalInset > MaxTotalInset && TotalInset > KINDA_SMALL_NUMBER)
        {
            const float InsetScale = MaxTotalInset / TotalInset;
            StartInset *= InsetScale;
            EndInset *= InsetScale;
        }

        XMin += StartInset;
        XMax -= EndInset;

        if (XMax - XMin <= 1.0f)
        {
            return;
        }

        const float ZMin = Side.ZOffset + HarnessSurfaceVerticalGapCm;
        const float ZMax = Side.ZOffset + FMath::Max(Side.Height - HarnessSurfaceVerticalGapCm, HarnessSurfaceVerticalGapCm + 1.0f);
        if (ZMax - ZMin <= 1.0f)
        {
            return;
        }

        struct FOpeningRect
        {
            float X0 = 0.0f;
            float X1 = 0.0f;
            float Z0 = 0.0f;
            float Z1 = 0.0f;
        };

        TArray<FOpeningRect> OpeningRects;
        TArray<float> XCutValues;
        TArray<float> ZCutValues;

        auto AddCutValue = [](TArray<float>& Values, float Value)
        {
            for (float Existing : Values)
            {
                if (FMath::IsNearlyEqual(Existing, Value, 0.05f))
                {
                    return;
                }
            }
            Values.Add(Value);
        };

        AddCutValue(XCutValues, XMin);
        AddCutValue(XCutValues, XMax);
        AddCutValue(ZCutValues, ZMin);
        AddCutValue(ZCutValues, ZMax);

        for (const FMergedOpening& MergedOpening : Side.Openings)
        {
            const FTopologyOpening* Opening = MergedOpening.Opening;
            if (!Opening)
            {
                continue;
            }

            const float HalfOpeningWidth = FMath::Max(MergedOpening.WidthCm * 0.5f, 0.5f);
            FOpeningRect Rect;
            Rect.X0 = FMath::Max(MergedOpening.CenterX - HalfOpeningWidth, XMin);
            Rect.X1 = FMath::Min(MergedOpening.CenterX + HalfOpeningWidth, XMax);

            float OpeningBottom = Opening->z_offset_cm;
            float OpeningTop = Opening->z_offset_cm + FMath::Max(Opening->height_cm, 1.0f);
            if (OpeningBottom <= 0.1f)
            {
                OpeningBottom = ZMin - 1.0f;
            }
            OpeningTop += 0.5f;

            Rect.Z0 = FMath::Max(OpeningBottom, ZMin);
            Rect.Z1 = FMath::Min(OpeningTop, ZMax);

            if (Rect.X1 - Rect.X0 <= 0.5f || Rect.Z1 - Rect.Z0 <= 0.5f)
            {
                continue;
            }

            OpeningRects.Add(Rect);
            AddCutValue(XCutValues, Rect.X0);
            AddCutValue(XCutValues, Rect.X1);
            AddCutValue(ZCutValues, Rect.Z0);
            AddCutValue(ZCutValues, Rect.Z1);
        }

        XCutValues.Sort();
        ZCutValues.Sort();

        const float AngleRadians = FMath::DegreesToRadians(Side.Angle);
        const FVector2D LocalPositiveY(-FMath::Sin(AngleRadians), FMath::Cos(AngleRadians));
        const bool bFacePositiveY = FVector2D::DotProduct(SurfaceNormal, LocalPositiveY) >= 0.0f;

        if (bHarnessSurfaceBuildDebug)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[WallSurfaceDebug] BuildSurface Tags=[%s] Face=%s Edge=%s Center=%s SurfaceCenter=%s Dir=%s SurfaceNormal=%s LocalPositiveY=%s Dot=%.3f bFacePositiveY=%d Len=%.2f XMin=%.2f XMax=%.2f InsetStart=%.2f InsetEnd=%.2f Thickness=%.2f"),
                *JoinHarnessTagNames(Tags),
                *Side.FaceId,
                *Side.SurfaceEdgeId,
                *Side.Center.ToString(),
                *SurfaceCenter.ToString(),
                *Side.Direction.ToString(),
                *SurfaceNormal.ToString(),
                *LocalPositiveY.ToString(),
                FVector2D::DotProduct(SurfaceNormal, LocalPositiveY),
                bFacePositiveY ? 1 : 0,
                Side.Length,
                XMin,
                XMax,
                StartInset,
                EndInset,
                WallThickness);
        }

        UE::Geometry::FDynamicMesh3 SurfaceMesh;
        SurfaceMesh.EnableAttributes();
        SurfaceMesh.Attributes()->EnableMaterialID();

        TMap<FIntVector, int32> SurfaceVertexCache;
        auto AppendSharedSurfaceVertex = [&](float X, float Y, float Z) -> int32
        {
            const FIntVector Key(
                FMath::RoundToInt(X * 100.0f),
                FMath::RoundToInt(Y * 100.0f),
                FMath::RoundToInt(Z * 100.0f)
            );

            if (const int32* ExistingVertex = SurfaceVertexCache.Find(Key))
            {
                return *ExistingVertex;
            }

            const int32 NewVertex = SurfaceMesh.AppendVertex(FVector3d(X, Y, Z));
            SurfaceVertexCache.Add(Key, NewVertex);
            return NewVertex;
        };

        for (int32 Xi = 0; Xi < XCutValues.Num() - 1; ++Xi)
        {
            for (int32 Zi = 0; Zi < ZCutValues.Num() - 1; ++Zi)
            {
                const float CellX0 = XCutValues[Xi];
                const float CellX1 = XCutValues[Xi + 1];
                const float CellZ0 = ZCutValues[Zi];
                const float CellZ1 = ZCutValues[Zi + 1];

                if (CellX1 - CellX0 <= 0.1f || CellZ1 - CellZ0 <= 0.1f)
                {
                    continue;
                }

                const float TestX = (CellX0 + CellX1) * 0.5f;
                const float TestZ = (CellZ0 + CellZ1) * 0.5f;
                bool bInsideOpening = false;
                for (const FOpeningRect& Rect : OpeningRects)
                {
                    if (TestX > Rect.X0 && TestX < Rect.X1 && TestZ > Rect.Z0 && TestZ < Rect.Z1)
                    {
                        bInsideOpening = true;
                        break;
                    }
                }

                if (bInsideOpening)
                {
                    continue;
                }

                const int32 V0 = AppendSharedSurfaceVertex(CellX0, 0.0f, CellZ0);
                const int32 V1 = AppendSharedSurfaceVertex(CellX1, 0.0f, CellZ0);
                const int32 V2 = AppendSharedSurfaceVertex(CellX1, 0.0f, CellZ1);
                const int32 V3 = AppendSharedSurfaceVertex(CellX0, 0.0f, CellZ1);

                if (bFacePositiveY)
                {
                    SurfaceMesh.AppendTriangle(V0, V1, V2);
                    SurfaceMesh.AppendTriangle(V0, V2, V3);
                }
                else
                {
                    SurfaceMesh.AppendTriangle(V0, V2, V1);
                    SurfaceMesh.AppendTriangle(V0, V3, V2);
                }
            }
        }

        if (SurfaceMesh.TriangleCount() == 0)
        {
            return;
        }

        UE::Geometry::FDynamicMeshMaterialAttribute* SurfaceMatID = SurfaceMesh.Attributes()->GetMaterialID();
        UE::Geometry::FDynamicMeshUVOverlay* SurfaceUVOverlay = SurfaceMesh.Attributes()->PrimaryUV();
        for (int32 TID : SurfaceMesh.TriangleIndicesItr())
        {
            SurfaceMatID->SetValue(TID, 0);
            const UE::Geometry::FIndex3i Tri = SurfaceMesh.GetTriangle(TID);
            const FVector3d V0 = SurfaceMesh.GetVertex(Tri.A);
            const FVector3d V1 = SurfaceMesh.GetVertex(Tri.B);
            const FVector3d V2 = SurfaceMesh.GetVertex(Tri.C);

            const int32 UV0 = SurfaceUVOverlay->AppendElement(FVector2f(V0.X / 100.0f, V0.Z / 100.0f));
            const int32 UV1 = SurfaceUVOverlay->AppendElement(FVector2f(V1.X / 100.0f, V1.Z / 100.0f));
            const int32 UV2 = SurfaceUVOverlay->AppendElement(FVector2f(V2.X / 100.0f, V2.Z / 100.0f));
            SurfaceUVOverlay->SetTriangle(TID, UE::Geometry::FIndex3i(UV0, UV1, UV2));
        }

        UE::Geometry::FMeshNormals::InitializeOverlayToPerVertexNormals(SurfaceMesh.Attributes()->PrimaryNormals(), false);

        UDynamicMeshComponent* SurfaceComp = NewObject<UDynamicMeshComponent>(GetOwner());
        if (!SurfaceComp)
        {
            return;
        }

        SurfaceComp->SetMobility(EComponentMobility::Movable);
        SurfaceComp->RegisterComponent();
        SurfaceComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        SurfaceComp->ComponentTags.Add(TEXT("EditableWall"));
        SurfaceComp->ComponentTags.Add(TEXT("Wall"));
        for (const FName& Tag : Tags)
        {
            SurfaceComp->ComponentTags.AddUnique(Tag);
        }

        SurfaceComp->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.01f));
        SurfaceComp->SetRelativeLocationAndRotation(FVector(SurfaceCenter.X, SurfaceCenter.Y, 0.0f), FRotator(0.0f, Side.Angle, 0.0f));
        SurfaceComp->SetDynamicMesh(NewObject<UDynamicMesh>(SurfaceComp));
        SurfaceComp->GetDynamicMesh()->SetMesh(MoveTemp(SurfaceMesh));

        if (DefaultFallbackMaterial)
        {
            SurfaceComp->SetMaterial(0, DefaultFallbackMaterial);
        }

        SurfaceComp->SetComplexAsSimpleCollisionEnabled(true, true);
        SurfaceComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        SurfaceComp->SetCollisionObjectType(ECC_WorldStatic);
        SurfaceComp->SetCollisionResponseToAllChannels(ECR_Block);
        SurfaceComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        SurfaceComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
        SurfaceComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        SurfaceComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
        SurfaceComp->bCastShadowAsTwoSided = true;
        SurfaceComp->NotifyMeshUpdated();

        SpawnedComponents.Add(SurfaceComp);
        AnimatedWalls.Add(SurfaceComp);
    };

    auto MakeMergedSurfaceEdgeId = [](const TArray<FString>& SourceIds) -> FString
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
    };

    auto MergeSurfaceSideInto = [](FWallSurfaceSide& Target, const FWallSurfaceSide& Source)
    {
        const FVector2D Start = Target.Center - (Target.Direction * (Target.Length * 0.5f));
        const FVector2D End = Source.Center + (Source.Direction * (Source.Length * 0.5f));
        const FVector2D Segment = End - Start;
        const float Length = Segment.Size();
        if (Length <= KINDA_SMALL_NUMBER)
        {
            return;
        }

        Target.Length = Length;
        Target.Center = (Start + End) * 0.5f;
        Target.Direction = Segment / Length;
        Target.Angle = FMath::RadiansToDegrees(FMath::Atan2(Segment.Y, Segment.X));
        Target.InteriorNormal = (Target.InteriorNormal + Source.InteriorNormal).GetSafeNormal();
        if (Target.InteriorNormal.IsNearlyZero())
        {
            Target.InteriorNormal = Source.InteriorNormal;
        }

        for (const FString& SourceSurfaceEdgeId : Source.SourceSurfaceEdgeIds)
        {
            Target.SourceSurfaceEdgeIds.AddUnique(SourceSurfaceEdgeId);
        }

        for (const FString& CoreEdgeId : Source.CoreEdgeIds)
        {
            Target.CoreEdgeIds.AddUnique(CoreEdgeId);
        }

        for (const FString& OpeningEdgeId : Source.OpeningEdgeIds)
        {
            Target.OpeningEdgeIds.Add(OpeningEdgeId);
        }

        Target.bHasOuterWall = Target.bHasOuterWall || Source.bHasOuterWall;
    };

    auto CanMergeSurfaceSides = [](const FWallSurfaceSide& A, const FWallSurfaceSide& B) -> bool
    {
        if (A.Direction.IsNearlyZero() || B.Direction.IsNearlyZero())
        {
            return false;
        }

        if (!FMath::IsNearlyEqual(A.Height, B.Height, 0.1f) ||
            !FMath::IsNearlyEqual(A.ZOffset, B.ZOffset, 0.1f) ||
            !FMath::IsNearlyEqual(A.WallThickness, B.WallThickness, 0.1f))
        {
            return false;
        }

        if (FVector2D::DotProduct(A.Direction, B.Direction) < 0.999f ||
            FMath::Abs(CrossHarness2D(A.Direction, B.Direction)) > HarnessMergeCollinearTolerance ||
            FVector2D::DotProduct(A.InteriorNormal, B.InteriorNormal) < 0.999f)
        {
            return false;
        }

        const FVector2D AStart = A.Center - (A.Direction * (A.Length * 0.5f));
        const FVector2D AEnd = A.Center + (A.Direction * (A.Length * 0.5f));
        const FVector2D BStart = B.Center - (B.Direction * (B.Length * 0.5f));
        const FVector2D BEnd = B.Center + (B.Direction * (B.Length * 0.5f));
        const bool bEndpointsTouch =
            FVector2D::Distance(AEnd, BStart) <= HarnessMergeEndpointToleranceCm ||
            FVector2D::Distance(AStart, BEnd) <= HarnessMergeEndpointToleranceCm;

        return bEndpointsTouch;
    };

    auto BuildMergedWallRuns = [&]() -> TArray<FWallRun>
    {
        TArray<const FTopologyHalfEdge*> SourceEdges;
        SourceEdges.Reserve(EdgeCache.Num());
        for (const auto& Pair : EdgeCache)
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

            const FVector2D BaseSegment = BaseEnd - BaseStart;
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

                    const float CandidateThickness = CandidateEdge->wall_thickness > KINDA_SMALL_NUMBER
                        ? CandidateEdge->wall_thickness
                        : HarnessDefaultWallThicknessCm;
                    if (!FMath::IsNearlyEqual(WallThickness, CandidateThickness, 0.1f))
                    {
                        continue;
                    }

                    const FVector2D CandidateSegment = CandidateEnd - CandidateStart;
                    const float CandidateLength = CandidateSegment.Size();
                    if (CandidateLength <= KINDA_SMALL_NUMBER)
                    {
                        bUsed[CandidateIndex] = true;
                        continue;
                    }

                    const FVector2D CandidateDirection = CandidateSegment / CandidateLength;
                    if (FMath::Abs(CrossHarness2D(BaseDirection, CandidateDirection)) > HarnessMergeCollinearTolerance)
                    {
                        continue;
                    }

                    const float StartLineDistance = FMath::Abs(FVector2D::DotProduct(CandidateStart - BaseStart, BaseNormal));
                    const float EndLineDistance = FMath::Abs(FVector2D::DotProduct(CandidateEnd - BaseStart, BaseNormal));
                    if (StartLineDistance > HarnessMergeEndpointToleranceCm || EndLineDistance > HarnessMergeEndpointToleranceCm)
                    {
                        continue;
                    }

                    float CandidateMin = FVector2D::DotProduct(CandidateStart - BaseStart, BaseDirection);
                    float CandidateMax = FVector2D::DotProduct(CandidateEnd - BaseStart, BaseDirection);
                    if (CandidateMin > CandidateMax)
                    {
                        Swap(CandidateMin, CandidateMax);
                    }

                    const bool bTouchesRun =
                        CandidateMin <= RunMax + HarnessMergeEndpointToleranceCm &&
                        CandidateMax >= RunMin - HarnessMergeEndpointToleranceCm;
                    if (!bTouchesRun)
                    {
                        continue;
                    }

                    RunIndices.Add(CandidateIndex);
                    bUsed[CandidateIndex] = true;
                    RunMin = FMath::Min(RunMin, CandidateMin);
                    RunMax = FMath::Max(RunMax, CandidateMax);
                    bMergedAny = true;
                }
            }

            FWallRun Run;
            Run.Start = BaseStart + (BaseDirection * RunMin);
            Run.End = BaseStart + (BaseDirection * RunMax);
            const FVector2D RunSegment = Run.End - Run.Start;
            Run.Length = RunSegment.Size();
            if (Run.Length <= KINDA_SMALL_NUMBER)
            {
                continue;
            }

            Run.Direction = RunSegment / Run.Length;
            Run.Center = (Run.Start + Run.End) * 0.5f;
            Run.Angle = FMath::RadiansToDegrees(FMath::Atan2(RunSegment.Y, RunSegment.X));
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
            Run.RunId = MakeMergedSurfaceEdgeId(RunSourceTokens);
            Run.Openings = BuildOpeningsForRun(Run.OpeningEdgeIds, Run.Center, Run.Direction);
            Runs.Add(Run);
        }

        return Runs;
    };

    auto BuildMergedFaceSides = [&](const FTopologyFace& Face) -> TArray<FWallSurfaceSide>
    {
        TArray<FWallSurfaceSide> Segments;
        const int32 NumVerts = Face.contour_vertex_ids.Num();
        if (NumVerts < 3)
        {
            return Segments;
        }

        TArray<FVector2D> FacePoints;
        if (!BuildFacePoints(Face, FacePoints))
        {
            return Segments;
        }

        auto ComputeInteriorNormal = [&](const FVector2D& A, const FVector2D& B) -> FVector2D
        {
            return ComputeHarnessInteriorNormal2D(FacePoints, A, B);
        };

        for (int32 i = 0; i < NumVerts; ++i)
        {
            const FString& CurrId = Face.contour_vertex_ids[i];
            const FString& NextId = Face.contour_vertex_ids[(i + 1) % NumVerts];
            const FVector2D* CurrPoint = VertexCache.Find(CurrId);
            const FVector2D* NextPoint = VertexCache.Find(NextId);
            if (!CurrPoint || !NextPoint)
            {
                continue;
            }

            const FVector2D Segment = *NextPoint - *CurrPoint;
            const float SegmentLength = Segment.Size();
            if (SegmentLength <= KINDA_SMALL_NUMBER)
            {
                continue;
            }
            const FVector2D SegmentDir = Segment / SegmentLength;

            TArray<FFaceSegmentEdgeMatch> EdgeMatches;
            if (!FindEdgesOverlappingFaceSegment(*CurrPoint, *NextPoint, EdgeMatches))
            {
                continue;
            }

            int32 MatchIndex = 0;
            while (MatchIndex < EdgeMatches.Num())
            {
                float SpanStartT = EdgeMatches[MatchIndex].StartT;
                float SpanEndT = EdgeMatches[MatchIndex].EndT;
                TArray<FFaceSegmentEdgeMatch> SpanMatches;
                SpanMatches.Add(EdgeMatches[MatchIndex]);
                ++MatchIndex;

                while (MatchIndex < EdgeMatches.Num() &&
                    EdgeMatches[MatchIndex].StartT <= SpanEndT + HarnessMergeEndpointToleranceCm)
                {
                    SpanEndT = FMath::Max(SpanEndT, EdgeMatches[MatchIndex].EndT);
                    SpanMatches.Add(EdgeMatches[MatchIndex]);
                    ++MatchIndex;
                }

                const float SpanLength = SpanEndT - SpanStartT;
                if (SpanLength <= HarnessMergeEndpointToleranceCm)
                {
                    continue;
                }

                const FVector2D SpanStart = *CurrPoint + (SegmentDir * SpanStartT);
                const FVector2D SpanEnd = *CurrPoint + (SegmentDir * SpanEndT);

                FWallSurfaceSide Side;
                Side.FaceId = MakeHarnessSurfaceToken(Face.face_id);
                Side.Height = FMath::Max(Face.height_cm, 1.0f);
                Side.ZOffset = Face.z_offset;
                Side.Length = SpanLength;
                Side.Angle = FMath::RadiansToDegrees(FMath::Atan2(Segment.Y, Segment.X));
                Side.Center = (SpanStart + SpanEnd) * 0.5f;
                Side.Direction = SegmentDir;
                Side.InteriorNormal = ComputeInteriorNormal(SpanStart, SpanEnd).GetSafeNormal();

                bool bHasThickness = false;
                for (const FFaceSegmentEdgeMatch& Match : SpanMatches)
                {
                    if (!Match.Edge)
                    {
                        continue;
                    }

                    const FString SurfaceEdgeId = MakeHarnessSurfaceToken(Match.SurfaceEdgeId);
                    Side.SourceSurfaceEdgeIds.AddUnique(SurfaceEdgeId);
                    Side.bHasOuterWall = Side.bHasOuterWall || Match.Edge->type.Equals(TEXT("WallOuter"), ESearchCase::IgnoreCase);

                    const float EdgeThickness = Match.Edge->wall_thickness > KINDA_SMALL_NUMBER
                        ? Match.Edge->wall_thickness
                        : HarnessDefaultWallThicknessCm;
                    Side.WallThickness = bHasThickness
                        ? FMath::Max(Side.WallThickness, EdgeThickness)
                        : EdgeThickness;
                    bHasThickness = true;

                    AddOpeningEdgeIds(*Match.Edge, Side.OpeningEdgeIds);
                    if (const FString* CoreEdgeId = CoreEdgeIdByAnyEdgeId.Find(Match.SurfaceEdgeId))
                    {
                        Side.CoreEdgeIds.AddUnique(*CoreEdgeId);
                    }
                    else if (const FString* FallbackCoreEdgeId = CoreEdgeIdByAnyEdgeId.Find(Match.Edge->id))
                    {
                        Side.CoreEdgeIds.AddUnique(*FallbackCoreEdgeId);
                    }
                }

                Side.SurfaceEdgeId = MakeMergedSurfaceEdgeId(Side.SourceSurfaceEdgeIds);
                Segments.Add(Side);
            }
        }

        TArray<FWallSurfaceSide> MergedSides;
        for (const FWallSurfaceSide& SegmentSide : Segments)
        {
            if (MergedSides.Num() > 0 && CanMergeSurfaceSides(MergedSides.Last(), SegmentSide))
            {
                MergeSurfaceSideInto(MergedSides.Last(), SegmentSide);
            }
            else
            {
                MergedSides.Add(SegmentSide);
            }
        }

        if (MergedSides.Num() > 1 && CanMergeSurfaceSides(MergedSides.Last(), MergedSides[0]))
        {
            FWallSurfaceSide FirstSide = MergedSides[0];
            MergeSurfaceSideInto(MergedSides.Last(), FirstSide);
            MergedSides.RemoveAt(0);
        }

        const int32 NumSides = MergedSides.Num();
        for (int32 i = 0; i < NumSides; ++i)
        {
            FWallSurfaceSide& Side = MergedSides[i];
            Side.SurfaceEdgeId = MakeMergedSurfaceEdgeId(Side.SourceSurfaceEdgeIds);
            Side.Openings = BuildOpeningsForRun(Side.OpeningEdgeIds, Side.Center, Side.Direction);

            const FWallSurfaceSide& PrevSide = MergedSides[(i - 1 + NumSides) % NumSides];
            const FWallSurfaceSide& NextSide = MergedSides[(i + 1) % NumSides];

            const FVector2D SideStart = Side.Center - (Side.Direction * (Side.Length * 0.5f));
            const FVector2D SideEnd = Side.Center + (Side.Direction * (Side.Length * 0.5f));
            const FVector2D PrevEnd = PrevSide.Center + (PrevSide.Direction * (PrevSide.Length * 0.5f));
            const FVector2D NextStart = NextSide.Center - (NextSide.Direction * (NextSide.Length * 0.5f));

            if (FVector2D::Distance(PrevEnd, SideStart) <= HarnessMergeEndpointToleranceCm)
            {
                Side.PrevDirection = PrevSide.Direction;
                Side.PrevInteriorNormal = PrevSide.InteriorNormal;
            }

            if (FVector2D::Distance(SideEnd, NextStart) <= HarnessMergeEndpointToleranceCm)
            {
                Side.NextDirection = NextSide.Direction;
                Side.NextInteriorNormal = NextSide.InteriorNormal;
            }
        }

        return MergedSides;
    };

    auto ApplyTJointSurfaceInsets = [](TArray<FWallSurfaceSide>& SurfaceSides, const TArray<FWallRun>& Runs)
    {
        auto BelongsToRun = [](const FWallSurfaceSide& Side, const FWallRun& Run) -> bool
        {
            for (const FTopologyHalfEdge* Edge : Run.Edges)
            {
                if (!Edge)
                {
                    continue;
                }

                if (Side.CoreEdgeIds.Contains(Edge->id) || (!Edge->twin_id.IsEmpty() && Side.CoreEdgeIds.Contains(Edge->twin_id)))
                {
                    return true;
                }
            }

            return false;
        };

        auto ComputeEndpointInset = [&](const FWallSurfaceSide& Side, const FVector2D& Endpoint) -> float
        {
            float Inset = 0.0f;
            if (Side.Direction.IsNearlyZero())
            {
                return Inset;
            }

            for (const FWallRun& Run : Runs)
            {
                if (Run.Direction.IsNearlyZero() || Run.Length <= KINDA_SMALL_NUMBER || BelongsToRun(Side, Run))
                {
                    continue;
                }

                if (FMath::Abs(CrossHarness2D(Side.Direction, Run.Direction)) <= HarnessMergeCollinearTolerance)
                {
                    continue;
                }

                const FVector2D RunStart = Run.Center - (Run.Direction * (Run.Length * 0.5f));
                const float AlongRun = FVector2D::DotProduct(Endpoint - RunStart, Run.Direction);
                if (AlongRun <= HarnessMergeEndpointToleranceCm || AlongRun >= Run.Length - HarnessMergeEndpointToleranceCm)
                {
                    continue;
                }

                const FVector2D RunNormal(-Run.Direction.Y, Run.Direction.X);
                const float DistanceToRun = FMath::Abs(FVector2D::DotProduct(Endpoint - RunStart, RunNormal));
                if (DistanceToRun > HarnessMergeEndpointToleranceCm * 2.0f)
                {
                    continue;
                }

                Inset = FMath::Max(Inset, (Run.WallThickness * 0.5f) + HarnessSurfaceGapCm);
            }

            return Inset;
        };

        for (FWallSurfaceSide& Side : SurfaceSides)
        {
            if (Side.Direction.IsNearlyZero() || Side.Length <= 1.0f)
            {
                continue;
            }

            const FVector2D SideStart = Side.Center - (Side.Direction * (Side.Length * 0.5f));
            const FVector2D SideEnd = Side.Center + (Side.Direction * (Side.Length * 0.5f));
            Side.StartInset = FMath::Max(Side.StartInset, ComputeEndpointInset(Side, SideStart));
            Side.EndInset = FMath::Max(Side.EndInset, ComputeEndpointInset(Side, SideEnd));
        }
    };

    auto ApplyWallRunEndpointInsets = [](TArray<FWallRun>& Runs)
    {
        auto ComputeEndpointInset = [&](const FWallRun& Run, const FVector2D& Endpoint) -> float
        {
            float Inset = 0.0f;
            if (Run.Direction.IsNearlyZero() || Run.Length <= KINDA_SMALL_NUMBER)
            {
                return Inset;
            }

            for (const FWallRun& OtherRun : Runs)
            {
                if (&OtherRun == &Run || OtherRun.Direction.IsNearlyZero() || OtherRun.Length <= KINDA_SMALL_NUMBER)
                {
                    continue;
                }

                if (FMath::Abs(CrossHarness2D(Run.Direction, OtherRun.Direction)) <= HarnessMergeCollinearTolerance)
                {
                    continue;
                }

                const FVector2D OtherStart = OtherRun.Center - (OtherRun.Direction * (OtherRun.Length * 0.5f));
                const float AlongOther = FVector2D::DotProduct(Endpoint - OtherStart, OtherRun.Direction);
                if (AlongOther < -HarnessMergeEndpointToleranceCm || AlongOther > OtherRun.Length + HarnessMergeEndpointToleranceCm)
                {
                    continue;
                }

                const FVector2D OtherNormal(-OtherRun.Direction.Y, OtherRun.Direction.X);
                const float DistanceToOther = FMath::Abs(FVector2D::DotProduct(Endpoint - OtherStart, OtherNormal));
                if (DistanceToOther > HarnessMergeEndpointToleranceCm * 2.0f)
                {
                    continue;
                }

                const bool bTouchesOtherEndpoint =
                    AlongOther <= HarnessMergeEndpointToleranceCm ||
                    AlongOther >= OtherRun.Length - HarnessMergeEndpointToleranceCm;
                if (bTouchesOtherEndpoint && Run.RunId.Compare(OtherRun.RunId) < 0)
                {
                    continue;
                }

                Inset = FMath::Max(Inset, (OtherRun.WallThickness * 0.5f) + HarnessWallZFightSeparationCm);
            }

            return Inset;
        };

        for (FWallRun& Run : Runs)
        {
            if (Run.Direction.IsNearlyZero() || Run.Length <= 1.0f)
            {
                continue;
            }

            const FVector2D RunStart = Run.Center - (Run.Direction * (Run.Length * 0.5f));
            const FVector2D RunEnd = Run.Center + (Run.Direction * (Run.Length * 0.5f));
            Run.StartInset = FMath::Max(Run.StartInset, ComputeEndpointInset(Run, RunStart));
            Run.EndInset = FMath::Max(Run.EndInset, ComputeEndpointInset(Run, RunEnd));

            const float MaxInset = FMath::Max(Run.Length - 1.0f, 0.0f);
            const float TotalInset = Run.StartInset + Run.EndInset;
            if (TotalInset > MaxInset && TotalInset > KINDA_SMALL_NUMBER)
            {
                const float InsetScale = MaxInset / TotalInset;
                Run.StartInset *= InsetScale;
                Run.EndInset *= InsetScale;
            }
        }
    };

    TArray<FWallRun> WallRuns = BuildMergedWallRuns();
    TArray<FWallSurfaceSide> InteriorSurfaceSides;
    for (const FTopologyFace& Face : FloorData.faces)
    {
        InteriorSurfaceSides.Append(BuildMergedFaceSides(Face));
    }

    TMap<FString, int32> CoreEdgeFaceUseCounts;
    TSet<FString> CountedCoreFacePairs;
    for (const FWallSurfaceSide& Side : InteriorSurfaceSides)
    {
        for (const FString& CoreEdgeId : Side.CoreEdgeIds)
        {
            const FString CoreFaceKey = FString::Printf(TEXT("%s|%s"), *CoreEdgeId, *Side.FaceId);
            if (CountedCoreFacePairs.Contains(CoreFaceKey))
            {
                continue;
            }

            CountedCoreFacePairs.Add(CoreFaceKey);
            CoreEdgeFaceUseCounts.FindOrAdd(CoreEdgeId)++;
        }
    }

    for (FWallSurfaceSide& Side : InteriorSurfaceSides)
    {
        for (const FString& CoreEdgeId : Side.CoreEdgeIds)
        {
            if (const int32* UseCount = CoreEdgeFaceUseCounts.Find(CoreEdgeId))
            {
                if (*UseCount > 1)
                {
                    Side.bTouchesAnotherRoom = true;
                    break;
                }
            }
        }
    }

    // ApplyTJointSurfaceInsets(InteriorSurfaceSides, WallRuns);
    // ApplyWallRunEndpointInsets(WallRuns);

    for (const FWallRun& Run : WallRuns)
    {
        if (Run.Edges.Num() == 0 || Run.Length <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        float CoreBottomZ = -HarnessFloorSlabThicknessCm - HarnessCoreZSealCm;
        float CoreTopZ = HarnessDefaultWallHeightCm + HarnessCoreZSealCm;
        bool bHasAdjacentFace = false;

        for (const FTopologyHalfEdge* Edge : Run.Edges)
        {
            if (!Edge)
            {
                continue;
            }

            for (const FTopologyFace& Face : FloorData.faces)
            {
                FWallSurfaceSide Side;
                if (FindFaceSide(*Edge, Face, Side))
                {
                    CoreBottomZ = bHasAdjacentFace
                        ? FMath::Min(CoreBottomZ, Face.z_offset - HarnessFloorSlabThicknessCm - HarnessCoreZSealCm)
                        : Face.z_offset - HarnessFloorSlabThicknessCm - HarnessCoreZSealCm;
                    CoreTopZ = bHasAdjacentFace
                        ? FMath::Max(CoreTopZ, Face.z_offset + Face.height_cm + HarnessCoreZSealCm)
                        : Face.z_offset + Face.height_cm + HarnessCoreZSealCm;
                    bHasAdjacentFace = true;
                }
            }
        }

        const FVector2D CoreStart = Run.Start + (Run.Direction * Run.StartInset);
        const FVector2D CoreEnd = Run.End - (Run.Direction * Run.EndInset);
        const FVector2D CoreSegment = CoreEnd - CoreStart;
        const float CoreLength = CoreSegment.Size();
        if (CoreLength <= 1.0f)
        {
            continue;
        }

        const FVector2D CoreCenter = (CoreStart + CoreEnd) * 0.5f;
        const float OpeningCenterShift = FVector2D::DotProduct(Run.Center - CoreCenter, Run.Direction);
        TArray<FMergedOpening> CoreOpenings = Run.Openings;
        for (FMergedOpening& Opening : CoreOpenings)
        {
            Opening.CenterX += OpeningCenterShift;
        }

        // 💡 [수정] 통짜 뼈대를 세로로 반갈라 왼쪽/오른쪽 2개의 얇은 벽으로 분리 생성합니다.
        const float HalfThickness = Run.WallThickness * 0.5f;
        const float OffsetDist = HalfThickness * 0.5f; // 절반 두께의 중심점을 구하기 위한 오프셋
        const FVector2D CoreNormal(-Run.Direction.Y, Run.Direction.X); // 벽의 수직 방향(Normal)

        const FVector2D LeftCenter = CoreCenter + (CoreNormal * OffsetDist);
        const FVector2D RightCenter = CoreCenter - (CoreNormal * OffsetDist);

        // 1. 왼쪽 뼈대 메쉬
        BuildWallBox(
            LeftCenter,
            Run.Angle,
            CoreLength + Run.WallThickness,
            HalfThickness, // 두께를 절반으로!
            CoreBottomZ,
            CoreTopZ,
            CoreOpenings,
            {
                FName(TEXT("WallCore")),
                FName(TEXT("WallCore_Left")),
                FName(FString::Printf(TEXT("WallCore_%s_L"), *Run.RunId))
            },
            true // 마우스로 선택 가능하도록 bEditable을 true로 설정!
        );

        // 2. 오른쪽 뼈대 메쉬
        BuildWallBox(
            RightCenter,
            Run.Angle,
            CoreLength + Run.WallThickness,
            HalfThickness, // 두께를 절반으로!
            CoreBottomZ,
            CoreTopZ,
            CoreOpenings,
            {
                FName(TEXT("WallCore")),
                FName(TEXT("WallCore_Right")),
                FName(FString::Printf(TEXT("WallCore_%s_R"), *Run.RunId))
            },
            true // 마우스로 선택 가능하도록 bEditable을 true로 설정!
        );
    }

    for (const FWallSurfaceSide& Side : InteriorSurfaceSides)
    {
        const FString SurfaceId = FString::Printf(TEXT("WallSurface_%s_%s"), *Side.FaceId, *Side.SurfaceEdgeId);

        BuildWallSurfacePanels(
            Side,
            Side.InteriorNormal,
            Side.PrevInteriorNormal,
            Side.NextInteriorNormal,
            Side.WallThickness,
            {
                FName(TEXT("WallSurface")),
                FName(FString::Printf(TEXT("RoomFace_%s"), *Side.FaceId)),
                FName(*SurfaceId)
            }
        );

        if (Side.bHasOuterWall && !Side.bTouchesAnotherRoom)
        {
            const FVector2D ExteriorNormal = -Side.InteriorNormal;
            const FString ExteriorSurfaceId = FString::Printf(TEXT("WallExterior_%s"), *Side.SurfaceEdgeId);

            BuildWallSurfacePanels(
                Side,
                ExteriorNormal,
                -Side.PrevInteriorNormal,
                -Side.NextInteriorNormal,
                Side.WallThickness,
                {
                    FName(TEXT("WallExterior")),
                    FName(*ExteriorSurfaceId)
                }
            );
        }
    }
}

