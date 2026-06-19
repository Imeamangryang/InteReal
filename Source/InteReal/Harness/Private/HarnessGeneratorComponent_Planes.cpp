#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"


#include "Algo/Reverse.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/MeshNormals.h"
#include "GeomTools.h"
#include "UDynamicMesh.h"
#include "Public/HarnessGeneratorGeometry.h"

using namespace InteReal::HarnessGenerator;

void UHarnessGeneratorComponent::FabricateDynamicPlanes(const FHarnessFloorData& FloorData)
{
    for (const FTopologyFace& Face : FloorData.faces)
    {
        if (Face.contour_vertex_ids.Num() < 3) continue;

        float DynamicWallHeight = Face.height_cm;

        TArray<FVector2D> RawPoints;
        for (const FString& VId : Face.contour_vertex_ids)
        {
            if (VertexCache.Contains(VId))
            {
                RawPoints.Add(VertexCache[VId]);
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
        if (TriangulationPoints.Num() < 3) continue;

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

        DyMeshComp->ComponentTags.AddUnique(FName(TEXT("EditableFloor")));
        DyMeshComp->ComponentTags.AddUnique(FName(FString::Printf(TEXT("FloorFace_%s"), *Face.face_id)));
        DyMeshComp->ComponentTags.Add(FName("Floor"));

        if (!DyMeshComp->GetDynamicMesh()) {
            DyMeshComp->SetDynamicMesh(NewObject<UDynamicMesh>(DyMeshComp));
        }

        DyMeshComp->GetDynamicMesh()->SetMesh(MoveTemp(DynMesh));
        DyMeshComp->SetComplexAsSimpleCollisionEnabled(true, true);
        DyMeshComp->SetCollisionProfileName(TEXT("BlockAll"));
        // 諛붾떏 硫붿돩???⑤㈃ 援ъ“?대?濡??묐㈃ 洹몃┝??泥섎━媛 ?꾩슂?⑸땲??
        DyMeshComp->bCastShadowAsTwoSided = true; 
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

        for (int32 i = 0; i < TriangleIndices.Num(); i += 3)
        {
            int32 A = TriangleIndices[i];
            int32 B = TriangleIndices[i+1];
            int32 C = TriangleIndices[i+2];

            // ?꾩そ 酉곗뿉??諛묐㈃??蹂댁씠?꾨줉 泥쒖옣???꾨옒濡?諛붾씪蹂대뒗(諛묓뙋) 硫붿돩留??앹꽦?⑸땲??
            CeilingMesh.AppendTriangle(CeilBottomVIds[A], CeilBottomVIds[B], CeilBottomVIds[C]);
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
        
        CeilComp->ComponentTags.Add(FName("Ceiling"));
        CeilComp->SetComplexAsSimpleCollisionEnabled(true, true);
        CeilComp->SetCollisionProfileName(TEXT("BlockAll"));
        CeilComp->bCastShadowAsTwoSided = true; 
        CeilComp->NotifyMeshUpdated();

        if (DefaultFallbackMaterial) CeilComp->SetMaterial(0, DefaultFallbackMaterial);

        SpawnedComponents.Add(CeilComp);

        // 泥쒖옣? 諛묒뿉?쒕쭔 蹂댁씠怨??꾩뿉?쒕뒗 ?щ챸?섎?濡? ?쒖뼇愿?李⑤떒???꾪빐 洹몃┝???꾩슜 遺덊닾紐?Solid) 釉붾줉???앹꽦?⑸땲??
        UE::Geometry::FDynamicMesh3 CeilingShadowMesh;
        CeilingShadowMesh.EnableAttributes();

        TArray<int32> ShadowTopVIds;
        TArray<int32> ShadowBottomVIds;
        const TArray<FVector2D> ShadowPoints = OffsetHarnessPolygon2D(TriangulationPoints, HarnessCeilingShadowOverhangCm);
        const int32 ShadowNumPts = ShadowPoints.Num();
        for (const FVector2D& V : ShadowPoints)
        {
            ShadowTopVIds.Add(CeilingShadowMesh.AppendVertex(FVector3d(V.X, V.Y, CeilingTopZ)));
            ShadowBottomVIds.Add(CeilingShadowMesh.AppendVertex(FVector3d(V.X, V.Y, CeilingBottomZ)));
        }

        for (int32 i = 0; i < TriangleIndices.Num(); i += 3)
        {
            int32 A = TriangleIndices[i];
            int32 B = TriangleIndices[i+1];
            int32 C = TriangleIndices[i+2];

            CeilingShadowMesh.AppendTriangle(ShadowBottomVIds[A], ShadowBottomVIds[B], ShadowBottomVIds[C]);
            CeilingShadowMesh.AppendTriangle(ShadowTopVIds[C], ShadowTopVIds[B], ShadowTopVIds[A]);
        }

        for (int32 i = 0; i < ShadowNumPts; ++i)
        {
            int32 NextI = (i + 1) % ShadowNumPts;
            CeilingShadowMesh.AppendTriangle(ShadowBottomVIds[i], ShadowTopVIds[i], ShadowBottomVIds[NextI]);
            CeilingShadowMesh.AppendTriangle(ShadowBottomVIds[NextI], ShadowTopVIds[i], ShadowTopVIds[NextI]);
        }

        UE::Geometry::FMeshNormals::InitializeOverlayToPerVertexNormals(CeilingShadowMesh.Attributes()->PrimaryNormals(), false);

        UDynamicMeshComponent* CeilingShadowBlocker = NewObject<UDynamicMeshComponent>(GetOwner());
        if (CeilingShadowBlocker)
        {
            CeilingShadowBlocker->SetMobility(EComponentMobility::Movable);
            CeilingShadowBlocker->RegisterComponent();
            CeilingShadowBlocker->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            CeilingShadowBlocker->SetDynamicMesh(NewObject<UDynamicMesh>(CeilingShadowBlocker));
            CeilingShadowBlocker->GetDynamicMesh()->SetMesh(MoveTemp(CeilingShadowMesh));
            CeilingShadowBlocker->ComponentTags.Add(FName("CeilingShadowBlocker"));
            
            // ?ㅼ젣 寃뚯엫?먮뒗 蹂댁씠吏 ?딄퀬 洹몃┝?먮쭔 洹몃━?꾨줉 ?ㅼ젙?⑸땲??
            CeilingShadowBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            CeilingShadowBlocker->SetVisibility(true, true);
            CeilingShadowBlocker->SetHiddenInGame(false, true);
            CeilingShadowBlocker->SetRenderInMainPass(false);
            CeilingShadowBlocker->SetVisibleInRayTracing(true);
            CeilingShadowBlocker->CastShadow = true;
            CeilingShadowBlocker->bCastHiddenShadow = true;
            CeilingShadowBlocker->bCastShadowAsTwoSided = true;
            CeilingShadowBlocker->NotifyMeshUpdated();

            SpawnedComponents.Add(CeilingShadowBlocker);
        }
    }
}

// ==============================================================================
// 臾?李쎈Ц ?꾨젅??諛곗튂 (?꾩옱??踰쎌껜 援щ찉 ?앹꽦留?吏??
// ==============================================================================
void UHarnessGeneratorComponent::InstallOpeningComponents(const FHarnessFloorData& FloorData)
{
}

