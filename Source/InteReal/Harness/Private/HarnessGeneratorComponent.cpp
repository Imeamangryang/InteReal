#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PointLightComponent.h" // 💡 [추가] 포인트 라이트 컴포넌트 헤더
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshNormals.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeomTools.h" 
#include "Algo/Reverse.h"

UHarnessGeneratorComponent::UHarnessGeneratorComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHarnessGeneratorComponent::ClearHarness()
{
    for (UActorComponent* Comp : SpawnedComponents)
    {
        if (IsValid(Comp)) Comp->DestroyComponent();
    }
    SpawnedComponents.Reset();
    VertexCache.Reset();
    EdgeCache.Reset();
}

void UHarnessGeneratorComponent::BuildTopologyCaches(const FHarnessFloorData& FloorData)
{
    for (const FTopologyVertex& V : FloorData.vertices)
    {
        VertexCache.Add(V.id, V.ToVector2D());
    }
    for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
    {
        if (!EdgeCache.Contains(Edge.twin_id))
        {
            EdgeCache.Add(Edge.id, Edge);
        }
    }
}

void UHarnessGeneratorComponent::BuildHarness(const FHarnessFloorData& FloorData)
{
    CachedFloorData = FloorData;
    if (!GetOwner() || !StyleDataTable) return;

    ClearHarness();
    BuildTopologyCaches(FloorData);

    AssembleStructuralWalls(FloorData);      
    FabricateDynamicPlanes(FloorData);       
    InstallOpeningComponents(FloorData);     
    
    // 💡 [추가] 토글이 켜져 있을 때만 인테리어 조명 스폰
    if (bEnableInteriorLights)
    {
        InstallInteriorLights(FloorData);
    }
}

// ==============================================================================
// 💡 [추가] 방 중앙 위치를 계산하여 Point Light 동적 스폰
// ==============================================================================
void UHarnessGeneratorComponent::InstallInteriorLights(const FHarnessFloorData& FloorData)
{
    const float GlobalWallHeight = (FloorData.faces.Num() > 0) ? FloorData.faces[0].height_cm : 260.0f;

    for (const FTopologyFace& Face : FloorData.faces)
    {
        if (Face.contour_vertex_ids.Num() < 3) continue;

        FVector2D Centroid(0.0, 0.0);
        int32 ValidPts = 0;

        // 1. 방을 구성하는 모든 정점의 평균 좌표(무게중심) 계산
        for (const FString& VId : Face.contour_vertex_ids)
        {
            if (VertexCache.Contains(VId))
            {
                Centroid += VertexCache[VId];
                ValidPts++;
            }
        }
        
        if (ValidPts == 0) continue;
        Centroid /= ValidPts;

        float ActualRoomHeight = Face.height_cm > 0.0f ? Face.height_cm : GlobalWallHeight;
        
        // 2. 조명의 Z 위치를 천장에서 30cm 아래로 설정
        FVector LightPos(Centroid.X, Centroid.Y, Face.z_offset + ActualRoomHeight - 30.0f);

        // 3. 포인트 라이트 생성 및 설정
        UPointLightComponent* PointLight = NewObject<UPointLightComponent>(GetOwner());
        PointLight->SetMobility(EComponentMobility::Movable);
        PointLight->RegisterComponent();
        PointLight->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        
        PointLight->SetRelativeLocation(LightPos);
        
        // 광량, 반경 및 그림자 설정 (테스트용으로 은은하게 설정)
        PointLight->SetIntensity(2500.0f);
        PointLight->SetAttenuationRadius(1000.0f);
        PointLight->SetCastShadows(true);
        PointLight->LightColor = FColor(255, 245, 230); // 약간 따뜻한 톤

        // 나중에 블루프린트에서 찾기 쉽도록 태그 부여
        PointLight->ComponentTags.Add(TEXT("InteriorLight"));

        SpawnedComponents.Add(PointLight);
    }
}

// ==============================================================================
// 벽체(Wall) 메쉬 절차적 생성 및 Boolean 타공(구멍 뚫기) 로직
// ==============================================================================
void UHarnessGeneratorComponent::AssembleStructuralWalls(const FHarnessFloorData& FloorData)
{
    auto GetFaceLabel = [&](const FString& vStart, const FString& vEnd) -> FString {
        for (const FTopologyFace& Face : FloorData.faces) {
            int32 NumPts = Face.contour_vertex_ids.Num();
            for (int32 i = 0; i < NumPts; ++i) {
                if (Face.contour_vertex_ids[i] == vStart && Face.contour_vertex_ids[(i + 1) % NumPts] == vEnd) {
                    return Face.label;
                }
            }
        }
        return TEXT(""); 
    };

    const float WallHeight = (FloorData.faces.Num() > 0) ? FloorData.faces[0].height_cm : 260.0f;
    
    for (const auto& Pair : EdgeCache)
    {
        const FTopologyHalfEdge& Edge = Pair.Value;
        if (!VertexCache.Contains(Edge.vertex_start) || !VertexCache.Contains(Edge.vertex_end)) continue;

        FVector2D pStart = VertexCache[Edge.vertex_start];
        FVector2D pEnd = VertexCache[Edge.vertex_end];
        FVector2D Center2D = (pStart + pEnd) / 2.0f;
        
        float Length = FVector2D::Distance(pStart, pEnd);
        float Angle = FMath::RadiansToDegrees(FMath::Atan2(pEnd.Y - pStart.Y, pEnd.X - pStart.X));

        FVector2D Dir = (pEnd - pStart).GetSafeNormal();
        FVector2D Normal(-Dir.Y, Dir.X); 

        FString ActualWallType = Edge.type;
        if (ActualWallType.Equals(TEXT("WallLintel"))) {
            ActualWallType = TEXT("WallInner"); 
        }

        float HalfThickness = Edge.wall_thickness / 2.0f;
        float OffsetDist = HalfThickness / 2.0f;

        auto BuildWallHalf = [&](FVector2D CenterPos, FString FaceLabel) {
            UDynamicMeshComponent* WallComp = NewObject<UDynamicMeshComponent>(GetOwner());
            WallComp->SetMobility(EComponentMobility::Movable);
            WallComp->RegisterComponent();
            WallComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            
            WallComp->SetRelativeLocationAndRotation(FVector(CenterPos.X, CenterPos.Y, 0.0f), FRotator(0.0f, Angle, 0.0f));

            UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(WallComp);
            WallComp->SetDynamicMesh(DynMesh);

            FGeometryScriptPrimitiveOptions PrimOptions;
            FTransform BaseTransform(FRotator::ZeroRotator, FVector(0, 0, WallHeight / 2.0f), FVector::OneVector);

            FVector BoxMin(-Length / 2.0f, -HalfThickness / 2.0f, -WallHeight / 2.0f);
            FVector BoxMax(Length / 2.0f, HalfThickness / 2.0f, WallHeight / 2.0f);
            FBox WallBox(BoxMin, BoxMax);

            UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBoundingBox(
                DynMesh, PrimOptions, BaseTransform, WallBox, 0, 0, 0
            );

            for (const FTopologyOpening& Opening : FloorData.openings)
            {
                if (Opening.target_edge_id == Edge.id || Opening.target_edge_id == Edge.twin_id)
                {
                    UDynamicMesh* HoleMesh = NewObject<UDynamicMesh>();
                    float HoleZCenter = Opening.z_offset_cm + (Opening.height_cm / 2.0f);
                    FTransform HoleTransform(FRotator::ZeroRotator, FVector(0, 0, HoleZCenter), FVector::OneVector);

                    float HoleThickness = HalfThickness + 10.0f; 
                    FVector HoleMin(-Opening.width_cm / 2.0f, -HoleThickness / 2.0f, -Opening.height_cm / 2.0f);
                    FVector HoleMax(Opening.width_cm / 2.0f, HoleThickness / 2.0f, Opening.height_cm / 2.0f);
                    FBox HoleBox(HoleMin, HoleMax);

                    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBoundingBox(
                        HoleMesh, PrimOptions, HoleTransform, HoleBox, 0, 0, 0
                    );

                    FGeometryScriptMeshBooleanOptions BoolOptions;
                    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
                        DynMesh, FTransform::Identity, HoleMesh, FTransform::Identity,
                        EGeometryScriptBooleanOperation::Subtract, BoolOptions
                    );
                }
            }

            UMaterialInterface* TargetMat = DefaultFallbackMaterial;
            if (StyleDataTable) {
                if (!FaceLabel.IsEmpty()) {
                    if (FHarnessStyleRow* RoomRow = StyleDataTable->FindRow<FHarnessStyleRow>(FName(*(FaceLabel + TEXT("_Wall"))), TEXT("MatLookup"))) {
                        if (RoomRow->Material) TargetMat = RoomRow->Material;
                    }
                }
                if (TargetMat == DefaultFallbackMaterial) {
                    if (FHarnessStyleRow* BaseRow = StyleDataTable->FindRow<FHarnessStyleRow>(FName(*ActualWallType), TEXT("MatLookup"))) {
                        if (BaseRow->Material) TargetMat = BaseRow->Material;
                    }
                }
            }
            if (TargetMat) WallComp->SetMaterial(0, TargetMat);
            
            WallComp->SetComplexAsSimpleCollisionEnabled(true, true);
            WallComp->SetCollisionProfileName(TEXT("BlockAll"));
            SpawnedComponents.Add(WallComp);
        };

        FString ForwardLabel = GetFaceLabel(Edge.vertex_start, Edge.vertex_end);
        FVector2D ForwardCenter = Center2D + (Normal * OffsetDist);
        BuildWallHalf(ForwardCenter, ForwardLabel);

        FString TwinLabel = GetFaceLabel(Edge.vertex_end, Edge.vertex_start);
        FVector2D TwinCenter = Center2D - (Normal * OffsetDist);
        BuildWallHalf(TwinCenter, TwinLabel);
    }
}

// ==============================================================================
// 바닥(Floor) 및 천장(Ceiling) 평면 메쉬 절차적 생성
// ==============================================================================
void UHarnessGeneratorComponent::FabricateDynamicPlanes(const FHarnessFloorData& FloorData)
{
    const float GlobalWallHeight = (FloorData.faces.Num() > 0) ? FloorData.faces[0].height_cm : 260.0f;

    for (const FTopologyFace& Face : FloorData.faces)
    {
        if (Face.contour_vertex_ids.Num() < 3) continue;

        TArray<FVector2D> TriangulationPoints;
        for (const FString& VId : Face.contour_vertex_ids)
        {
            if (VertexCache.Contains(VId))
            {
                TriangulationPoints.Add(VertexCache[VId]);
            }
        }

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

        TArray<FClipSMTriangle> OutTris;
        FGeomTools::TriangulatePoly(OutTris, InPoly);

        TArray<int32> TriangleIndices;
        for (const FClipSMTriangle& Tri : OutTris)
        {
            auto FindVertexIndex = [&](const auto& Pos) -> int32 {
                for (int32 i = 0; i < TriangulationPoints.Num(); ++i) {
                    if (FMath::IsNearlyEqual(TriangulationPoints[i].X, static_cast<double>(Pos.X), 0.1) &&
                        FMath::IsNearlyEqual(TriangulationPoints[i].Y, static_cast<double>(Pos.Y), 0.1)) {
                        return i;
                    }
                }
                return 0;
            };

            TriangleIndices.Add(FindVertexIndex(Tri.Vertices[0].Pos));
            TriangleIndices.Add(FindVertexIndex(Tri.Vertices[1].Pos));
            TriangleIndices.Add(FindVertexIndex(Tri.Vertices[2].Pos));
        }

        // --- 1. 바닥(Floor) 메쉬 조립 ---
        UE::Geometry::FDynamicMesh3 DynMesh;
        DynMesh.EnableAttributes(); 
        
        DynMesh.Attributes()->EnableMaterialID();
        UE::Geometry::FDynamicMeshMaterialAttribute* FloorMatID = DynMesh.Attributes()->GetMaterialID();
        
        TArray<int32> TopVIds;
        TArray<int32> BottomVIds;
        double SlabThickness = 4.0;

        for (const FVector2D& V : TriangulationPoints)
        {
            TopVIds.Add(DynMesh.AppendVertex(FVector3d(V.X, V.Y, Face.z_offset)));
            BottomVIds.Add(DynMesh.AppendVertex(FVector3d(V.X, V.Y, Face.z_offset - SlabThickness)));
        }

        for (int32 i = 0; i < TriangleIndices.Num(); i += 3)
        {
            int32 A = TriangleIndices[i];
            int32 B = TriangleIndices[i+1];
            int32 C = TriangleIndices[i+2];

            DynMesh.AppendTriangle(TopVIds[A], TopVIds[B], TopVIds[C]);
            DynMesh.AppendTriangle(BottomVIds[C], BottomVIds[B], BottomVIds[A]); 
        }

        for (int32 i = 0; i < NumPts; ++i)
        {
            int32 NextI = (i + 1) % NumPts;
            DynMesh.AppendTriangle(TopVIds[i], BottomVIds[i], TopVIds[NextI]);
            DynMesh.AppendTriangle(TopVIds[NextI], BottomVIds[i], BottomVIds[NextI]);
        }

        UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = DynMesh.Attributes()->PrimaryUV();
        for (int32 TID : DynMesh.TriangleIndicesItr())
        {
            FloorMatID->SetValue(TID, 0); 

            UE::Geometry::FIndex3i Tri = DynMesh.GetTriangle(TID);
            FVector3d V0 = DynMesh.GetVertex(Tri.A);
            FVector3d V1 = DynMesh.GetVertex(Tri.B);
            FVector3d V2 = DynMesh.GetVertex(Tri.C);

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

        if (!DyMeshComp->GetDynamicMesh()) {
            DyMeshComp->SetDynamicMesh(NewObject<UDynamicMesh>(DyMeshComp));
        }

        DyMeshComp->GetDynamicMesh()->SetMesh(MoveTemp(DynMesh));
        DyMeshComp->SetComplexAsSimpleCollisionEnabled(true, true);
        DyMeshComp->SetCollisionProfileName(TEXT("BlockAll"));
        DyMeshComp->NotifyMeshUpdated();
        
        UMaterialInterface* TargetMaterial = DefaultFallbackMaterial;
        if (StyleDataTable) {
            if (FHarnessStyleRow* MatRow = StyleDataTable->FindRow<FHarnessStyleRow>(FName(*Face.label), TEXT("PlaneMatLookup"))) {
                if (MatRow->Material) TargetMaterial = MatRow->Material;
            }
        }
        if (TargetMaterial) DyMeshComp->SetMaterial(0, TargetMaterial);

        SpawnedComponents.Add(DyMeshComp);
        
        // --- 2. 천장(Ceiling) 메쉬 조립 ---
        UE::Geometry::FDynamicMesh3 CeilingMesh;
        CeilingMesh.EnableAttributes();
        
        CeilingMesh.Attributes()->EnableMaterialID();
        UE::Geometry::FDynamicMeshMaterialAttribute* CeilMatID = CeilingMesh.Attributes()->GetMaterialID();
        
        TArray<int32> CeilTopVIds;
        TArray<int32> CeilBottomVIds;
        
        float ActualRoomHeight = Face.height_cm > 0.0f ? Face.height_cm : GlobalWallHeight;
        double CeilingBottomZ = Face.z_offset + ActualRoomHeight; 
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
        CeilComp->SetComplexAsSimpleCollisionEnabled(true, true);
        CeilComp->SetCollisionProfileName(TEXT("BlockAll"));
        CeilComp->bCastShadowAsTwoSided = true;
        
        CeilComp->NotifyMeshUpdated();

        UMaterialInterface* CeilMaterial = DefaultFallbackMaterial;
        if (StyleDataTable) {
            if (FHarnessStyleRow* CeilRow = StyleDataTable->FindRow<FHarnessStyleRow>(FName(*(Face.label + TEXT("_Ceiling"))), TEXT("CeilMatLookup"))) {
                if (CeilRow->Material) CeilMaterial = CeilRow->Material;
            }
        }
        if (CeilMaterial) CeilComp->SetMaterial(0, CeilMaterial);

        SpawnedComponents.Add(CeilComp);
    }
}

// ==============================================================================
// 타공된 개구부 위치에 창문 및 문 프롭(StaticMesh) 배치
// ==============================================================================
void UHarnessGeneratorComponent::InstallOpeningComponents(const FHarnessFloorData& FloorData)
{
    TMap<FString, FTopologyHalfEdge> RawEdgeMap;
    for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
    {
        RawEdgeMap.Add(Edge.id, Edge);
    }

    for (const FTopologyOpening& Opening : FloorData.openings)
    {
        if (!RawEdgeMap.Contains(Opening.target_edge_id)) continue;
        FTopologyHalfEdge Edge = RawEdgeMap[Opening.target_edge_id];

        if (!VertexCache.Contains(Edge.vertex_start) || !VertexCache.Contains(Edge.vertex_end)) continue;
        
        FVector2D pStart = VertexCache[Edge.vertex_start];
        FVector2D pEnd = VertexCache[Edge.vertex_end];

        FVector2D Center2D = (pStart + pEnd) / 2.0f;
        float Angle = FMath::RadiansToDegrees(FMath::Atan2(pEnd.Y - pStart.Y, pEnd.X - pStart.X));

        FHarnessStyleRow* OpRow = StyleDataTable ? StyleDataTable->FindRow<FHarnessStyleRow>(FName(*Opening.type), TEXT("OpeningLookup")) : nullptr;
        if (!OpRow || !OpRow->Mesh) continue;

        UStaticMeshComponent* OpComp = NewObject<UStaticMeshComponent>(GetOwner());
        if (!OpComp) continue;

        OpComp->SetMobility(EComponentMobility::Movable);
        OpComp->SetStaticMesh(OpRow->Mesh);
        OpComp->RegisterComponent();
        OpComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        float PivotCorrectedZ = Opening.z_offset_cm + (Opening.height_cm / 2.0f);
        OpComp->SetRelativeLocationAndRotation(FVector(Center2D.X, Center2D.Y, PivotCorrectedZ), FRotator(0.0f, Angle, 0.0f));
        
        OpComp->SetRelativeScale3D(FVector(Opening.width_cm / 100.0f, Edge.wall_thickness / 100.0f, Opening.height_cm / 100.0f));
        
        UMaterialInterface* OpMat = OpRow->Material ? OpRow->Material : DefaultFallbackMaterial;
        if (OpMat) OpComp->SetMaterial(0, OpMat);

        OpComp->SetCollisionProfileName(TEXT("BlockAll"));
        SpawnedComponents.Add(OpComp);
    }
}

void UHarnessGeneratorComponent::GetFloorBounds(FVector2D& OutMin, FVector2D& OutMax) const
{
    OutMin = FVector2D(UE_BIG_NUMBER, UE_BIG_NUMBER);
    OutMax = FVector2D(-UE_BIG_NUMBER, -UE_BIG_NUMBER);

    if (CachedFloorData.vertices.Num() == 0)
    {
        OutMin = OutMax = FVector2D::ZeroVector;
        return;
    }

    for (const FTopologyVertex& V : CachedFloorData.vertices)
    {
        if (V.x < OutMin.X) OutMin.X = V.x;
        if (V.x > OutMax.X) OutMax.X = V.x;
        if (V.y < OutMin.Y) OutMin.Y = V.y;
        if (V.y > OutMax.Y) OutMax.Y = V.y;
    }
}