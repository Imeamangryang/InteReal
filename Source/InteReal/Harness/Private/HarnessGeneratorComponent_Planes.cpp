#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"


#include "Algo/Reverse.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "InteReal/EditMode/Furniture/FFurnitureDataRow.h"
#include "InteReal/EditMode/Subsystem/InteriorPlacementSubsystem.h"
#include "InteReal/EditMode/Visualization/PlacementVisualizerActor.h"
#include "DynamicMesh/MeshNormals.h"
#include "GeomTools.h"
#include "UDynamicMesh.h"
#include "Public/HarnessGeneratorGeometry.h"

using namespace InteReal::HarnessGenerator;

namespace
{
    constexpr float HarnessPolygonValidationEpsilon = 0.01f;
    constexpr float HarnessRoomFloorInsetCm = 0.5f;

    bool HarnessPointNearlyEquals2D(const FVector2D& A, const FVector2D& B, float ToleranceCm)
    {
        return FVector2D::Distance(A, B) <= ToleranceCm;
    }

    int32 CountHarnessShortEdges2D(const TArray<FVector2D>& Points, float MinEdgeLengthCm)
    {
        if (Points.Num() < 2 || MinEdgeLengthCm <= 0.0f) return 0;
        int32 ShortEdgeCount = 0;
        for (int32 i = 0; i < Points.Num(); ++i)
        {
            const FVector2D& A = Points[i];
            const FVector2D& B = Points[(i + 1) % Points.Num()];
            if (FVector2D::Distance(A, B) < MinEdgeLengthCm) ++ShortEdgeCount;
        }
        return ShortEdgeCount;
    }

    int32 CountHarnessDuplicatePoints2D(const TArray<FVector2D>& Points, float ToleranceCm)
    {
        int32 DuplicateCount = 0;
        for (int32 i = 0; i < Points.Num(); ++i)
        {
            for (int32 j = i + 1; j < Points.Num(); ++j)
            {
                if (HarnessPointNearlyEquals2D(Points[i], Points[j], ToleranceCm)) ++DuplicateCount;
            }
        }
        return DuplicateCount;
    }

    int32 CountHarnessCollinearVertices2D(const TArray<FVector2D>& Points)
    {
        const int32 NumPoints = Points.Num();
        if (NumPoints < 3) return 0;
        int32 CollinearCount = 0;
        for (int32 i = 0; i < NumPoints; ++i)
        {
            const FVector2D Prev = Points[(i - 1 + NumPoints) % NumPoints];
            const FVector2D Curr = Points[i];
            const FVector2D Next = Points[(i + 1) % NumPoints];
            const FVector2D A = (Curr - Prev).GetSafeNormal();
            const FVector2D B = (Next - Curr).GetSafeNormal();
            if (!A.IsNearlyZero() && !B.IsNearlyZero() && FMath::Abs(CrossHarness2D(A, B)) <= 0.01f) ++CollinearCount;
        }
        return CollinearCount;
    }

    int32 HarnessOrientation2D(const FVector2D& A, const FVector2D& B, const FVector2D& C)
    {
        const float Value = CrossHarness2D(B - A, C - A);
        if (FMath::Abs(Value) <= HarnessPolygonValidationEpsilon) return 0;
        return Value > 0.0f ? 1 : -1;
    }

    bool HarnessPointOnSegment2D(const FVector2D& A, const FVector2D& B, const FVector2D& P)
    {
        if (FMath::Abs(CrossHarness2D(B - A, P - A)) > HarnessPolygonValidationEpsilon) return false;
        return P.X >= FMath::Min(A.X, B.X) - HarnessPolygonValidationEpsilon && P.X <= FMath::Max(A.X, B.X) + HarnessPolygonValidationEpsilon && P.Y >= FMath::Min(A.Y, B.Y) - HarnessPolygonValidationEpsilon && P.Y <= FMath::Max(A.Y, B.Y) + HarnessPolygonValidationEpsilon;
    }

    bool HarnessSegmentsIntersect2D(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
    {
        const int32 O1 = HarnessOrientation2D(A, B, C);
        const int32 O2 = HarnessOrientation2D(A, B, D);
        const int32 O3 = HarnessOrientation2D(C, D, A);
        const int32 O4 = HarnessOrientation2D(C, D, B);
        if (O1 != O2 && O3 != O4) return true;
        if (O1 == 0 && HarnessPointOnSegment2D(A, B, C)) return true;
        if (O2 == 0 && HarnessPointOnSegment2D(A, B, D)) return true;
        if (O3 == 0 && HarnessPointOnSegment2D(C, D, A)) return true;
        if (O4 == 0 && HarnessPointOnSegment2D(C, D, B)) return true;
        return false;
    }

    int32 CountHarnessSelfIntersections2D(const TArray<FVector2D>& Points)
    {
        const int32 NumPoints = Points.Num();
        if (NumPoints < 4) return 0;
        int32 IntersectionCount = 0;
        for (int32 i = 0; i < NumPoints; ++i)
        {
            const int32 INext = (i + 1) % NumPoints;
            for (int32 j = i + 1; j < NumPoints; ++j)
            {
                const int32 JNext = (j + 1) % NumPoints;
                if (i == j || INext == j || JNext == i) continue;
                if (i == 0 && JNext == 0) continue;
                if (HarnessSegmentsIntersect2D(Points[i], Points[INext], Points[j], Points[JNext])) ++IntersectionCount;
            }
        }
        return IntersectionCount;
    }
}

void UHarnessGeneratorComponent::FabricateDynamicPlanes(const FHarnessFloorData& FloorData)
{
    const float CommonWallHeightCm = FMath::Max(FloorData.common_wall_height_cm, 1.0f);

    for (const FTopologyFace& Face : FloorData.faces)
    {
        if (Face.contour_vertex_ids.Num() < 3) continue;

        const float DynamicWallHeight = CommonWallHeightCm;

        TArray<FVector2D> RawPoints;
        for (const FString& VId : Face.contour_vertex_ids)
        {
            if (VertexCache.Contains(VId))
            {
                RawPoints.Add(VertexCache[VId]);
            }
        }

        if (bValidateSpacePolygons)
        {
            if (RawPoints.Num() < 3)
            {
                UE_LOG(LogTemp, Warning, TEXT("[HarnessGenerator] Space polygon invalid. SpaceId=%s Reason=RawPointCountTooSmall RawPoints=%d"), *Face.face_id, RawPoints.Num());
            }

            const int32 RawDuplicateCount = CountHarnessDuplicatePoints2D(RawPoints, MinSpacePolygonEdgeLengthCm);
            const int32 RawShortEdgeCount = CountHarnessShortEdges2D(RawPoints, MinSpacePolygonEdgeLengthCm);
            if (RawDuplicateCount > 0 || RawShortEdgeCount > 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("[HarnessGenerator] Space polygon raw cleanup needed. SpaceId=%s RawPoints=%d DuplicatePairs=%d ShortEdges=%d MinEdgeCm=%.2f"), *Face.face_id, RawPoints.Num(), RawDuplicateCount, RawShortEdgeCount, MinSpacePolygonEdgeLengthCm);
            }
        }

        // 以묐났(Degenerate) ?뺤젏 諛?180??吏곸꽑(Collinear) ?몃뱶 ?쒓굅 (?쇨컖遺꾪븷 ?ㅻ쪟 諛⑹?)
        TArray<FVector2D> CleanPoints;
        for (const FVector2D& Pt : RawPoints) {
            if (CleanPoints.Num() == 0 || FVector2D::Distance(CleanPoints.Last(), Pt) > 1.0f) {
                CleanPoints.Add(Pt);
            }
        }
        if (CleanPoints.Num() > 1 && FVector2D::Distance(CleanPoints.Last(), CleanPoints[0]) <= 1.0f) {
            CleanPoints.Pop();
        }

        TArray<FVector2D> TriangulationPoints;
        int32 CNum = CleanPoints.Num();
        if (CNum >= 3) {
            for (int32 i = 0; i < CNum; ++i) {
                FVector2D Prev = CleanPoints[(i - 1 + CNum) % CNum];
                FVector2D Curr = CleanPoints[i];
                FVector2D Next = CleanPoints[(i + 1) % CNum];

                FVector2D Dir1 = (Curr - Prev).GetSafeNormal();
                FVector2D Dir2 = (Next - Curr).GetSafeNormal();

                float Cross = (Dir1.X * Dir2.Y) - (Dir1.Y * Dir2.X);
                if (FMath::Abs(Cross) > 0.01f) {
                    TriangulationPoints.Add(Curr);
                }
            }
        }
        
        if (TriangulationPoints.Num() < 3) TriangulationPoints = CleanPoints;
        if (TriangulationPoints.Num() < 3)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HarnessGenerator] Space polygon skipped. SpaceId=%s Reason=CleanPointCountTooSmall RawPoints=%d CleanPoints=%d TriangulationPoints=%d"), *Face.face_id, RawPoints.Num(), CleanPoints.Num(), TriangulationPoints.Num());
            continue;
        }

        const double BoundaryAreaBeforeExpansion = FMath::Abs(ComputeHarnessSignedArea(TriangulationPoints)) * 0.5;
        if (bValidateSpacePolygons)
        {
            const int32 RemovedDuplicatePoints = RawPoints.Num() - CleanPoints.Num();
            const int32 RemovedCollinearPoints = CleanPoints.Num() - TriangulationPoints.Num();
            const int32 SelfIntersectionCount = CountHarnessSelfIntersections2D(TriangulationPoints);
            const int32 ShortEdgeCount = CountHarnessShortEdges2D(TriangulationPoints, MinSpacePolygonEdgeLengthCm);
            const int32 DuplicatePointCount = CountHarnessDuplicatePoints2D(TriangulationPoints, MinSpacePolygonEdgeLengthCm);
            const bool bTooSmallArea = BoundaryAreaBeforeExpansion < MinSpacePolygonAreaCm2;
            if (RemovedDuplicatePoints > 0 || RemovedCollinearPoints > 0 || SelfIntersectionCount > 0 || ShortEdgeCount > 0 || DuplicatePointCount > 0 || bTooSmallArea)
            {
                UE_LOG(LogTemp, Warning, TEXT("[HarnessGenerator] Space polygon validation. SpaceId=%s RawPoints=%d CleanPoints=%d TriangulationPoints=%d RemovedDuplicate=%d RemovedCollinear=%d ShortEdges=%d DuplicatePairs=%d SelfIntersections=%d Area=%.2f MinArea=%.2f"), *Face.face_id, RawPoints.Num(), CleanPoints.Num(), TriangulationPoints.Num(), RemovedDuplicatePoints, RemovedCollinearPoints, ShortEdgeCount, DuplicatePointCount, SelfIntersectionCount, BoundaryAreaBeforeExpansion, MinSpacePolygonAreaCm2);
            }
            if (SelfIntersectionCount > 0 || bTooSmallArea)
            {
                UE_LOG(LogTemp, Warning, TEXT("[HarnessGenerator] Space polygon may triangulate incorrectly. SpaceId=%s SelfIntersections=%d Area=%.2f"), *Face.face_id, SelfIntersectionCount, BoundaryAreaBeforeExpansion);
            }
        }
        if (bLogSpaceBoundaryDiagnostics)
        {
            UE_LOG(LogTemp, Log, TEXT("[HarnessGenerator] Space floor boundary. SpaceId=%s RawPoints=%d CleanPoints=%d TriangulationPoints=%d BoundaryWalls=%d Area=%.2f Policy=%s ExpansionCm=%.2f"),
                *Face.face_id,
                RawPoints.Num(),
                CleanPoints.Num(),
                TriangulationPoints.Num(),
                Face.boundary_wall_ids.Num(),
                BoundaryAreaBeforeExpansion,
                SpaceBoundaryPolicy == EHarnessSpaceBoundaryPolicy::ExpandUnderWalls ? TEXT("ExpandUnderWalls") : TEXT("AsProvided"),
                FloorBoundaryExpansionCm);
        }

        // Room floors must remain separate so each room can keep an independent floor material.
        // Do not expand room floor polygons under walls here. Expanding every room individually makes
        // adjacent rooms overlap on the same Z plane, which causes floor-to-floor Z-fighting.
        // Instead, apply a tiny inward inset. The wall core/surface meshes cover this small seam.
        if (HarnessRoomFloorInsetCm > KINDA_SMALL_NUMBER)
        {
            const TArray<FVector2D> InsetFloorPoints = OffsetHarnessPolygon2D(TriangulationPoints, -HarnessRoomFloorInsetCm);
            if (InsetFloorPoints.Num() >= 3)
            {
                if (bLogSpaceBoundaryDiagnostics)
                {
                    const double BoundaryAreaAfterInset = FMath::Abs(ComputeHarnessSignedArea(InsetFloorPoints)) * 0.5;
                    UE_LOG(LogTemp, Log, TEXT("[HarnessGenerator] Space floor boundary inset. SpaceId=%s InsetCm=%.2f AreaBefore=%.2f AreaAfter=%.2f"), *Face.face_id, HarnessRoomFloorInsetCm, BoundaryAreaBeforeExpansion, BoundaryAreaAfterInset);
                }
                TriangulationPoints = InsetFloorPoints;
            }
        }

        // 硫댁쟻(Signed Area)???듯빐 諛섏떆怨?諛⑺뼢(Winding Order) ?먮떒 (?꾩슂 ??Reverse)
        double SignedArea = 0.0;
        int32 NumPts = TriangulationPoints.Num();
        for (int32 i = 0; i < NumPts; ++i)
        {
            FVector2D P1 = TriangulationPoints[i];
            FVector2D P2 = TriangulationPoints[(i + 1) % NumPts];
            SignedArea += (P1.X * P2.Y - P2.X * P1.Y);
        }
        
        if (SignedArea < 0.0) 
        {
            Algo::Reverse(TriangulationPoints);
        }
        
        TArray<FVector2D> ExpandedPoints = TriangulationPoints;
        
        FClipSMPolygon InPoly(0); 
        InPoly.FaceNormal.X = 0.0f;
        InPoly.FaceNormal.Y = 0.0f;
        InPoly.FaceNormal.Z = 1.0f;

        for (const FVector2D& Pt : TriangulationPoints)
        {
            FClipSMVertex V;
            V.Pos.X = static_cast<float>(Pt.X);
            V.Pos.Y = static_cast<float>(Pt.Y);
            V.Pos.Z = 0.0f;
            InPoly.Vertices.Add(V);
        }

        // ?ㅺ컖?뺤쓣 ?쇨컖??Triangle) 諛곗뿴濡?遺꾪븷?⑸땲??
        TArray<FClipSMTriangle> OutTris;
        FGeomTools::TriangulatePoly(OutTris, InPoly);
        if (OutTris.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HarnessGenerator] Space polygon triangulation failed. SpaceId=%s TriangulationPoints=%d Area=%.2f"), *Face.face_id, TriangulationPoints.Num(), FMath::Abs(ComputeHarnessSignedArea(TriangulationPoints)) * 0.5);
            continue;
        }

        TArray<int32> TriangleIndices;
        for (const FClipSMTriangle& Tri : OutTris)
        {
            auto FindVertexIndex = [&](const auto& Pos) -> int32 {
                float MinDistSq = UE_BIG_NUMBER;
                int32 BestIndex = 0;
                for (int32 i = 0; i < TriangulationPoints.Num(); ++i) {
                    float DistSq = FMath::Square(TriangulationPoints[i].X - Pos.X) + FMath::Square(TriangulationPoints[i].Y - Pos.Y);
                    if (DistSq < MinDistSq) {
                        MinDistSq = DistSq;
                        BestIndex = i;
                    }
                }
                return BestIndex;
            };

            TriangleIndices.Add(FindVertexIndex(Tri.Vertices[0].Pos));
            TriangleIndices.Add(FindVertexIndex(Tri.Vertices[1].Pos));
            TriangleIndices.Add(FindVertexIndex(Tri.Vertices[2].Pos));
        }

        // --- 1. 諛붾떏(Floor) 硫붿돩 議곕┰ ---
        UE::Geometry::FDynamicMesh3 DynMesh;
        DynMesh.EnableAttributes(); 
        DynMesh.Attributes()->EnableMaterialID();
        UE::Geometry::FDynamicMeshMaterialAttribute* FloorMatID = DynMesh.Attributes()->GetMaterialID();
        
        TArray<int32> TopVIds;
        TArray<int32> BottomVIds;
        double SlabThickness = 20.0; // 諛붾떏 肄섑겕由ы듃 ?щ옒釉??먭퍡 20cm 怨좎젙

        for (const FVector2D& V : TriangulationPoints)
        {
            TopVIds.Add(DynMesh.AppendVertex(FVector3d(V.X, V.Y, Face.z_offset)));
            BottomVIds.Add(DynMesh.AppendVertex(FVector3d(V.X, V.Y, Face.z_offset - SlabThickness)));
        }

        auto AppendTriangleFacing = [&](int32 V0, int32 V1, int32 V2, const FVector3d& ExpectedNormal)
        {
            const FVector3d P0 = DynMesh.GetVertex(V0);
            const FVector3d P1 = DynMesh.GetVertex(V1);
            const FVector3d P2 = DynMesh.GetVertex(V2);
            const FVector3d FaceNormal = FVector3d::CrossProduct(P1 - P0, P2 - P0);

            // ?몄쟻(Cross Product)???듯빐 踰뺤꽑??諛붽묑 諛⑺뼢(ExpectedNormal)??諛붾씪蹂닿쾶 ?뺣젹?⑸땲??
            if (FVector3d::DotProduct(FaceNormal, ExpectedNormal) < 0.0)
            {
                DynMesh.AppendTriangle(V0, V1, V2);
            }
            else
            {
                DynMesh.AppendTriangle(V0, V2, V1);
            }
        };

        for (int32 i = 0; i < TriangleIndices.Num(); i += 3)
        {
            int32 A = TriangleIndices[i];
            int32 B = TriangleIndices[i+1];
            int32 C = TriangleIndices[i+2];

            // ?쀭뙋, ?꾨옯???앹꽦 (Winding Order 二쇱쓽)
            DynMesh.AppendTriangle(TopVIds[C], TopVIds[B], TopVIds[A]);
            DynMesh.AppendTriangle(BottomVIds[A], BottomVIds[B], BottomVIds[C]);
        }

        for (int32 i = 0; i < NumPts; ++i)
        {
            int32 NextI = (i + 1) % NumPts;
            const FVector2D Current = TriangulationPoints[i];
            const FVector2D Next = TriangulationPoints[NextI];

            const int32 SideTopA = DynMesh.AppendVertex(FVector3d(Current.X, Current.Y, Face.z_offset));
            const int32 SideBottomA = DynMesh.AppendVertex(FVector3d(Current.X, Current.Y, Face.z_offset - SlabThickness));
            const int32 SideTopB = DynMesh.AppendVertex(FVector3d(Next.X, Next.Y, Face.z_offset));
            const int32 SideBottomB = DynMesh.AppendVertex(FVector3d(Next.X, Next.Y, Face.z_offset - SlabThickness));

            const FVector2D EdgeVec = Next - Current;
            const FVector3d OutwardNormal(EdgeVec.Y, -EdgeVec.X, 0.0);

            AppendTriangleFacing(SideTopA, SideBottomA, SideTopB, OutwardNormal);
            AppendTriangleFacing(SideTopB, SideBottomA, SideBottomB, OutwardNormal);
        }

        UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = DynMesh.Attributes()->PrimaryUV();
        for (int32 TID : DynMesh.TriangleIndicesItr())
        {
            UE::Geometry::FIndex3i Tri = DynMesh.GetTriangle(TID);
            FVector3d V0 = DynMesh.GetVertex(Tri.A);
            FVector3d V1 = DynMesh.GetVertex(Tri.B);
            FVector3d V2 = DynMesh.GetVertex(Tri.C);
            const bool bIsFloorTop =
                FMath::Abs(V0.Z - Face.z_offset) <= 0.1 &&
                FMath::Abs(V1.Z - Face.z_offset) <= 0.1 &&
                FMath::Abs(V2.Z - Face.z_offset) <= 0.1;

            FloorMatID->SetValue(TID, bIsFloorTop ? 0 : 1);

            // ??쇰쭅 ?ш린 蹂댁젙 (100.0f = 1m ?⑥쐞 留듯븨)
            int32 UV0 = UVOverlay->AppendElement(FVector2f(V0.X / 100.0f, V0.Y / 100.0f));
            int32 UV1 = UVOverlay->AppendElement(FVector2f(V1.X / 100.0f, V1.Y / 100.0f));
            int32 UV2 = UVOverlay->AppendElement(FVector2f(V2.X / 100.0f, V2.Y / 100.0f));
            UVOverlay->SetTriangle(TID, UE::Geometry::FIndex3i(UV0, UV1, UV2));
        }

        UE::Geometry::FMeshNormals::InitializeOverlayToPerVertexNormals(DynMesh.Attributes()->PrimaryNormals(), false);

        UDynamicMeshComponent* DyMeshComp = NewObject<UDynamicMeshComponent>(GetOwner());
        if (!DyMeshComp) continue;

        DyMeshComp->SetMobility(EComponentMobility::Movable);
        DyMeshComp->RegisterComponent();
        DyMeshComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        AddGeneratedComponentTags(DyMeshComp, TEXT("Floor"), Face.face_id, {
            FString::Printf(TEXT("HarnessSpaceId=%s"), *Face.face_id),
            FString::Printf(TEXT("HarnessSpaceKind=%s"), *Face.kind),
            TEXT("HarnessSurfaceRole=Floor")
        });
        DyMeshComp->ComponentTags.AddUnique(FName(TEXT("EditableFloor")));
        DyMeshComp->ComponentTags.AddUnique(FName(FString::Printf(TEXT("FloorFace_%s"), *Face.face_id)));
        DyMeshComp->ComponentTags.AddUnique(FName(TEXT("Floor")));

        if (!DyMeshComp->GetDynamicMesh()) {
            DyMeshComp->SetDynamicMesh(NewObject<UDynamicMesh>(DyMeshComp));
        }

        DyMeshComp->GetDynamicMesh()->SetMesh(MoveTemp(DynMesh));
        DyMeshComp->SetComplexAsSimpleCollisionEnabled(true, true);
        DyMeshComp->SetCollisionProfileName(TEXT("BlockAll"));
        DyMeshComp->SetCastShadow(true);
        DyMeshComp->SetAffectDistanceFieldLighting(true);
        DyMeshComp->SetVisibleInRayTracing(true);
        DyMeshComp->NotifyMeshUpdated();
        
        if (DefaultFallbackMaterial)
        {
            DyMeshComp->SetMaterial(0, DefaultFallbackMaterial);
            DyMeshComp->SetMaterial(1, DefaultFallbackMaterial);
        }

        SpawnedComponents.Add(DyMeshComp);
        
        // --- 2. 泥쒖옣(Ceiling) 硫붿돩 議곕┰ ---
        UE::Geometry::FDynamicMesh3 CeilingMesh;
        CeilingMesh.EnableAttributes();
        CeilingMesh.Attributes()->EnableMaterialID();
        UE::Geometry::FDynamicMeshMaterialAttribute* CeilMatID = CeilingMesh.Attributes()->GetMaterialID();
        
        TArray<int32> CeilTopVIds;
        TArray<int32> CeilBottomVIds;
        
        double CeilingBottomZ = Face.z_offset + DynamicWallHeight; 
        double CeilingTopZ = CeilingBottomZ + SlabThickness; 

        for (const FVector2D& V : TriangulationPoints)
        {
            CeilTopVIds.Add(CeilingMesh.AppendVertex(FVector3d(V.X, V.Y, CeilingTopZ)));
            CeilBottomVIds.Add(CeilingMesh.AppendVertex(FVector3d(V.X, V.Y, CeilingBottomZ)));
        }

        auto AppendCeilingTriangleFacing = [&](int32 V0, int32 V1, int32 V2, const FVector3d& ExpectedNormal)
        {
            const FVector3d P0 = CeilingMesh.GetVertex(V0);
            const FVector3d P1 = CeilingMesh.GetVertex(V1);
            const FVector3d P2 = CeilingMesh.GetVertex(V2);
            const FVector3d FaceNormal = FVector3d::CrossProduct(P1 - P0, P2 - P0);

            if (FVector3d::DotProduct(FaceNormal, ExpectedNormal) < 0.0)
            {
                CeilingMesh.AppendTriangle(V0, V1, V2);
            }
            else
            {
                CeilingMesh.AppendTriangle(V0, V2, V1);
            }
        };

        for (int32 i = 0; i < TriangleIndices.Num(); i += 3)
        {
            int32 A = TriangleIndices[i];
            int32 B = TriangleIndices[i+1];
            int32 C = TriangleIndices[i+2];

            AppendCeilingTriangleFacing(CeilBottomVIds[A], CeilBottomVIds[B], CeilBottomVIds[C], FVector3d(0.0, 0.0, -1.0));
            AppendCeilingTriangleFacing(CeilTopVIds[A], CeilTopVIds[B], CeilTopVIds[C], FVector3d(0.0, 0.0, 1.0));
        }

        for (int32 i = 0; i < NumPts; ++i)
        {
            int32 NextI = (i + 1) % NumPts;
            const FVector2D Current = TriangulationPoints[i];
            const FVector2D Next = TriangulationPoints[NextI];

            const int32 SideTopA = CeilingMesh.AppendVertex(FVector3d(Current.X, Current.Y, CeilingTopZ));
            const int32 SideBottomA = CeilingMesh.AppendVertex(FVector3d(Current.X, Current.Y, CeilingBottomZ));
            const int32 SideTopB = CeilingMesh.AppendVertex(FVector3d(Next.X, Next.Y, CeilingTopZ));
            const int32 SideBottomB = CeilingMesh.AppendVertex(FVector3d(Next.X, Next.Y, CeilingBottomZ));

            const FVector2D EdgeVec = Next - Current;
            const FVector3d OutwardNormal(EdgeVec.Y, -EdgeVec.X, 0.0);

            AppendCeilingTriangleFacing(SideTopA, SideBottomA, SideTopB, OutwardNormal);
            AppendCeilingTriangleFacing(SideTopB, SideBottomA, SideBottomB, OutwardNormal);
        }
        
        UE::Geometry::FDynamicMeshUVOverlay* CeilUVOverlay = CeilingMesh.Attributes()->PrimaryUV();
        for (int32 TID : CeilingMesh.TriangleIndicesItr())
        {
            CeilMatID->SetValue(TID, 0);

            UE::Geometry::FIndex3i Tri = CeilingMesh.GetTriangle(TID);
            FVector3d V0 = CeilingMesh.GetVertex(Tri.A);
            FVector3d V1 = CeilingMesh.GetVertex(Tri.B);
            FVector3d V2 = CeilingMesh.GetVertex(Tri.C);

            int32 UV0 = CeilUVOverlay->AppendElement(FVector2f(V0.X / 100.0f, V0.Y / 100.0f));
            int32 UV1 = CeilUVOverlay->AppendElement(FVector2f(V1.X / 100.0f, V1.Y / 100.0f));
            int32 UV2 = CeilUVOverlay->AppendElement(FVector2f(V2.X / 100.0f, V2.Y / 100.0f));
            CeilUVOverlay->SetTriangle(TID, UE::Geometry::FIndex3i(UV0, UV1, UV2));
        }

        UE::Geometry::FMeshNormals::InitializeOverlayToPerVertexNormals(CeilingMesh.Attributes()->PrimaryNormals(), false);

        UDynamicMeshComponent* CeilComp = NewObject<UDynamicMeshComponent>(GetOwner());
        CeilComp->SetMobility(EComponentMobility::Movable);
        CeilComp->RegisterComponent();
        CeilComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        
        CeilComp->SetDynamicMesh(NewObject<UDynamicMesh>(CeilComp));
        CeilComp->GetDynamicMesh()->SetMesh(MoveTemp(CeilingMesh));
        
        AddGeneratedComponentTags(CeilComp, TEXT("Ceiling"), Face.face_id, {
            FString::Printf(TEXT("HarnessSpaceId=%s"), *Face.face_id),
            FString::Printf(TEXT("HarnessSpaceKind=%s"), *Face.kind),
            TEXT("HarnessSurfaceRole=Ceiling")
        });
        CeilComp->ComponentTags.AddUnique(FName(TEXT("Ceiling")));
        CeilComp->SetComplexAsSimpleCollisionEnabled(true, true);
        CeilComp->SetCollisionProfileName(TEXT("BlockAll"));
        CeilComp->SetCastShadow(true);
        CeilComp->bCastHiddenShadow = true;
        CeilComp->SetAffectDistanceFieldLighting(true);
        CeilComp->SetVisibleInRayTracing(true);
        CeilComp->SetVisibility(true, true);
        CeilComp->SetHiddenInGame(true, true);
        CeilComp->NotifyMeshUpdated();

        if (DefaultFallbackMaterial) CeilComp->SetMaterial(0, DefaultFallbackMaterial);

        SpawnedComponents.Add(CeilComp);

    }
}

// ==============================================================================
// 臾?李쎈Ц ?꾨젅??諛곗튂 (?꾩옱??踰쎌껜 援щ찉 ?앹꽦留?吏??
// ==============================================================================
void UHarnessGeneratorComponent::InstallOpeningComponents(const FHarnessFloorData& FloorData)
{
    if (!bGenerateOpeningAssets || !GetOwner())
    {
        return;
    }

    auto IsWindowOpening = [](const FTopologyOpening& Opening) -> bool
    {
        return Opening.type.Equals(TEXT("Window"), ESearchCase::IgnoreCase) || Opening.kind.Contains(TEXT("window"), ESearchCase::IgnoreCase);
    };

    auto IsSlidingDoorOpening = [](const FTopologyOpening& Opening) -> bool
    {
        return Opening.kind.Contains(TEXT("sliding"), ESearchCase::IgnoreCase) || Opening.type.Contains(TEXT("Sliding"), ESearchCase::IgnoreCase);
    };

    auto IsEntranceDoorOpening = [](const FTopologyOpening& Opening) -> bool
    {
        return Opening.kind.Contains(TEXT("entrance"), ESearchCase::IgnoreCase) || Opening.type.Contains(TEXT("Entrance"), ESearchCase::IgnoreCase);
    };

    auto ResolveOpeningMesh = [&](const FTopologyOpening& Opening) -> UStaticMesh*
    {
        UStaticMesh* ExplicitMesh = nullptr;
        if (IsWindowOpening(Opening))
        {
            ExplicitMesh = DefaultWindowMesh;
        }
        else if (IsSlidingDoorOpening(Opening) && DefaultSlidingDoorMesh)
        {
            ExplicitMesh = DefaultSlidingDoorMesh;
        }
        else if (IsEntranceDoorOpening(Opening) && DefaultEntranceDoorMesh)
        {
            ExplicitMesh = DefaultEntranceDoorMesh;
        }
        else
        {
            ExplicitMesh = DefaultDoorMesh;
        }

        if (ExplicitMesh)
        {
            return ExplicitMesh;
        }

        UInteriorPlacementSubsystem* PlacementSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UInteriorPlacementSubsystem>() : nullptr;
        APlacementVisualizerActor* Visualizer = PlacementSubsystem ? PlacementSubsystem->GetVisualizer() : nullptr;
        if (!Visualizer || !Visualizer->FurnitureDataTable)
        {
            return nullptr;
        }

        static const FString ContextString(TEXT("ResolveOpeningMesh"));
        TArray<FFurnitureDataRow*> Rows;
        Visualizer->FurnitureDataTable->GetAllRows<FFurnitureDataRow>(ContextString, Rows);

        const bool bNeedWindow = IsWindowOpening(Opening);
        const bool bNeedSliding = IsSlidingDoorOpening(Opening);
        const bool bNeedEntrance = IsEntranceDoorOpening(Opening);
        UStaticMesh* GenericDoorCandidate = nullptr;
        UStaticMesh* GenericWindowCandidate = nullptr;
        for (const FFurnitureDataRow* Row : Rows)
        {
            if (!Row || !Row->FurnitureMesh)
            {
                continue;
            }

            if (bNeedWindow)
            {
                if (Row->AssetKind == EPlacementAssetKind::Window)
                {
                    return Row->FurnitureMesh;
                }
                if (!GenericWindowCandidate && Row->DisplayName.ToString().ToLower().Contains(TEXT("window")))
                {
                    GenericWindowCandidate = Row->FurnitureMesh;
                }
                continue;
            }

            if (bNeedSliding && Row->AssetKind == EPlacementAssetKind::SlidingDoor)
            {
                return Row->FurnitureMesh;
            }
            if (bNeedEntrance && Row->AssetKind == EPlacementAssetKind::EntranceDoor)
            {
                return Row->FurnitureMesh;
            }
            if (Row->AssetKind == EPlacementAssetKind::Door)
            {
                return Row->FurnitureMesh;
            }
            if (!GenericDoorCandidate && Row->DisplayName.ToString().ToLower().Contains(TEXT("door")))
            {
                GenericDoorCandidate = Row->FurnitureMesh;
            }
        }

        return bNeedWindow ? GenericWindowCandidate : GenericDoorCandidate;
    };

    auto FindOpeningCenterAndDirection = [&](const FTopologyOpening& Opening, FVector2D& OutCenter, FVector2D& OutDirection) -> bool
    {
        if (Opening.bHasSpan)
        {
            const FVector2D SpanStart = FloorData.ToHarnessPoint(Opening.span_start_cm);
            const FVector2D SpanEnd = FloorData.ToHarnessPoint(Opening.span_end_cm);
            const FVector2D Span = SpanEnd - SpanStart;
            const float SpanLength = Span.Size();
            if (SpanLength > KINDA_SMALL_NUMBER)
            {
                OutCenter = (SpanStart + SpanEnd) * 0.5f;
                OutDirection = Span / SpanLength;
                return true;
            }
        }

        const FTopologyHalfEdge* HostEdge = nullptr;
        if (!Opening.target_edge_id.IsEmpty())
        {
            if (const FTopologyHalfEdge* EdgePtr = EdgeCache.Find(Opening.target_edge_id))
            {
                HostEdge = EdgePtr;
            }
        }
        if (!HostEdge && !Opening.host_wall_id.IsEmpty())
        {
            for (const auto& Pair : EdgeCache)
            {
                if (Pair.Value.wall_id == Opening.host_wall_id)
                {
                    HostEdge = &Pair.Value;
                    break;
                }
            }
        }
        if (!HostEdge)
        {
            return false;
        }

        const FVector2D* Start = VertexCache.Find(HostEdge->vertex_start);
        const FVector2D* End = VertexCache.Find(HostEdge->vertex_end);
        if (!Start || !End)
        {
            return false;
        }

        const FVector2D Segment = *End - *Start;
        const float SegmentLength = Segment.Size();
        if (SegmentLength <= KINDA_SMALL_NUMBER)
        {
            return false;
        }

        OutDirection = Segment / SegmentLength;
        const float Offset = FMath::Clamp(Opening.offset_to_center_cm, 0.0f, SegmentLength);
        OutCenter = Opening.offset_from.Equals(TEXT("end"), ESearchCase::IgnoreCase) ? *End - OutDirection * Offset : *Start + OutDirection * Offset;
        return true;
    };

    for (const FTopologyOpening& Opening : FloorData.openings)
    {
        UStaticMesh* OpeningMesh = ResolveOpeningMesh(Opening);
        if (!OpeningMesh)
        {
            continue;
        }

        FVector2D OpeningCenter = FVector2D::ZeroVector;
        FVector2D OpeningDirection = FVector2D::UnitX();
        if (!FindOpeningCenterAndDirection(Opening, OpeningCenter, OpeningDirection))
        {
            continue;
        }

        UStaticMeshComponent* OpeningComp = NewObject<UStaticMeshComponent>(GetOwner());
        if (!OpeningComp)
        {
            continue;
        }

        OpeningComp->SetMobility(EComponentMobility::Movable);
        OpeningComp->RegisterComponent();
        OpeningComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        OpeningComp->SetStaticMesh(OpeningMesh);

        const float DesiredWidth = FMath::Max(Opening.measured_width_cm > UE_SMALL_NUMBER ? Opening.measured_width_cm : Opening.width_cm, 1.0f);
        const float DesiredHeight = FMath::Max(Opening.height_cm, 1.0f);
        const FBox MeshBox = OpeningMesh->GetBounds().GetBox();
        const FVector MeshSize = MeshBox.GetSize();
        FVector Scale = FVector::OneVector;
        const float WallThickness = HarnessDefaultWallThicknessCm;
        float MeshYawOffset = 0.0f;
        
        if (MeshSize.Y > MeshSize.X)
        {
            // Mesh width is along Y axis
            MeshYawOffset = 90.0f;
            if (MeshSize.Y > KINDA_SMALL_NUMBER)
            {
                Scale.Y = DesiredWidth / MeshSize.Y;
            }
            if (MeshSize.X > KINDA_SMALL_NUMBER)
            {
                Scale.X = FMath::Min(1.0f, FMath::Max(WallThickness * 0.85f, 1.0f) / MeshSize.X);
            }
        }
        else
        {
            // Mesh width is along X axis
            if (MeshSize.X > KINDA_SMALL_NUMBER)
            {
                Scale.X = DesiredWidth / MeshSize.X;
            }
            if (MeshSize.Y > KINDA_SMALL_NUMBER)
            {
                Scale.Y = FMath::Min(1.0f, FMath::Max(WallThickness * 0.85f, 1.0f) / MeshSize.Y);
            }
        }

        if (MeshSize.Z > KINDA_SMALL_NUMBER)
        {
            Scale.Z = DesiredHeight / MeshSize.Z;
        }

        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(OpeningDirection.Y, OpeningDirection.X)) + MeshYawOffset;
        const float BottomZ = Opening.z_offset_cm;
        const float CenterZ = BottomZ + DesiredHeight * 0.5f;
        OpeningComp->SetRelativeScale3D(Scale);
        OpeningComp->SetRelativeLocationAndRotation(FVector(OpeningCenter.X, OpeningCenter.Y, CenterZ), FRotator(0.0f, Yaw, 0.0f));

        TArray<FString> MetadataTags = {
            FString::Printf(TEXT("HarnessOpeningId=%s"), *Opening.id),
            FString::Printf(TEXT("HarnessOpeningType=%s"), *Opening.type),
            FString::Printf(TEXT("HarnessOpeningKind=%s"), *Opening.kind),
            FString::Printf(TEXT("HarnessOpeningHostWallId=%s"), *Opening.host_wall_id),
            FString::Printf(TEXT("HarnessOpeningTargetEdgeId=%s"), *Opening.target_edge_id),
            FString::Printf(TEXT("HarnessOpeningWidthCm=%.3f"), DesiredWidth),
            FString::Printf(TEXT("HarnessOpeningHeightCm=%.3f"), DesiredHeight),
            FString::Printf(TEXT("HarnessOpeningDepthCm=%.3f"), WallThickness)
        };
        AddGeneratedComponentTags(OpeningComp, TEXT("Opening"), Opening.id, MetadataTags);
        OpeningComp->ComponentTags.AddUnique(FName(*FString::Printf(TEXT("Opening_%s"), *Opening.id)));
        OpeningComp->ComponentTags.AddUnique(FName(TEXT("EditableOpening")));
        OpeningComp->ComponentTags.AddUnique(FName(TEXT("OpeningAsset")));
        const bool bIsWindow = IsWindowOpening(Opening);
        OpeningComp->ComponentTags.AddUnique(bIsWindow ? FName(TEXT("WindowAsset")) : FName(TEXT("DoorAsset")));

        OpeningComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        OpeningComp->SetCollisionObjectType(bIsWindow ? ECC_WorldStatic : ECC_WorldDynamic);
        OpeningComp->SetCollisionResponseToAllChannels(ECR_Block);
        OpeningComp->SetCollisionResponseToChannel(ECC_Pawn, bIsWindow ? ECR_Block : ECR_Ignore);
        OpeningComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        OpeningComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        OpeningComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
        OpeningComp->bReceivesDecals = false;

        if (DefaultFallbackMaterial && OpeningComp->GetNumMaterials() == 0)
        {
            OpeningComp->SetMaterial(0, DefaultFallbackMaterial);
        }

        SpawnedComponents.Add(OpeningComp);
    }
}
