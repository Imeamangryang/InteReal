#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshNormals.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Algo/Reverse.h"

// ==============================================================================
// 다각형(Polygon) 삼각분할(Triangulation)을 위한 자체 Ear-Clipping 알고리즘
// ==============================================================================

// 특정 점(Pt)이 세 정점(V1, V2, V3)으로 이루어진 삼각형 내부에 있는지 판별 (외적 부호 기반)
static bool IsPointInTriangle(const FVector2D& Pt, const FVector2D& V1, const FVector2D& V2, const FVector2D& V3)
{
    auto Sign = [](const FVector2D& p1, const FVector2D& p2, const FVector2D& p3) {
        return (p1.X - p3.X) * (p2.Y - p3.Y) - (p2.X - p3.X) * (p1.Y - p3.Y);
    };
    bool b1 = Sign(Pt, V1, V2) < 0.0f;
    bool b2 = Sign(Pt, V2, V3) < 0.0f;
    bool b3 = Sign(Pt, V3, V1) < 0.0f;
    return ((b1 == b2) && (b2 == b3)); // 세 내각의 부호가 모두 같으면 내부에 존재
}

// 오목(Concave) 다각형을 포함한 2D 평면도를 삼각형 배열로 분할하여 인덱스 반환
static void CustomTriangulateSimplePolygon(const TArray<FVector2D>& Vertices, TArray<int32>& OutTriangles)
{
    OutTriangles.Empty();
    if (Vertices.Num() < 3) return;

    TArray<int32> Indices;
    for (int32 i = 0; i < Vertices.Num(); ++i) Indices.Add(i);

    // 정점이 3개 남을 때까지 반복하여 '귀(Ear)'를 잘라냄
    while (Indices.Num() > 3)
    {
        bool bEarFound = false;
        int32 Count = Indices.Num();
        for (int32 i = 0; i < Count; ++i)
        {
            int32 Prev = Indices[(i - 1 + Count) % Count];
            int32 Curr = Indices[i];
            int32 Next = Indices[(i + 1) % Count];

            FVector2D VPrev = Vertices[Prev];
            FVector2D VCurr = Vertices[Curr];
            FVector2D VNext = Vertices[Next];
            
            // 외적(Cross Product)을 통해 내각이 180도 미만(볼록)인지 확인
            float Cross = (VCurr.X - VPrev.X) * (VNext.Y - VCurr.Y) - (VCurr.Y - VPrev.Y) * (VNext.X - VCurr.X);
            if (Cross > 0.0f)
            {
                bool bIsEar = true;
                // 해당 삼각형 내부에 다른 정점이 포함되어 있는지 검사
                for (int32 j = 0; j < Count; ++j)
                {
                    int32 TestIdx = Indices[j];
                    if (TestIdx == Prev || TestIdx == Curr || TestIdx == Next) continue;
                    
                    if (IsPointInTriangle(Vertices[TestIdx], VPrev, VCurr, VNext))
                    {
                        bIsEar = false;
                        break;
                    }
                }

                // 귀(Ear)로 판명되면 삼각형 인덱스 배열에 추가하고 원본 리스트에서 제거
                if (bIsEar)
                {
                    OutTriangles.Add(Prev);
                    OutTriangles.Add(Curr);
                    OutTriangles.Add(Next);
                    Indices.RemoveAt(i);
                    bEarFound = true;
                    break;
                }
            }
        }
        // 무한 루프 방지(Failsafe): 꼬인 다각형 등 예외 발생 시 강제 분할
        if (!bEarFound) 
        {
            OutTriangles.Add(Indices[0]);
            OutTriangles.Add(Indices[1]);
            OutTriangles.Add(Indices[2]);
            Indices.RemoveAt(1);
        }
    }
    
    // 마지막 남은 3개의 정점으로 최종 삼각형 구성
    if (Indices.Num() == 3)
    {
        OutTriangles.Add(Indices[0]);
        OutTriangles.Add(Indices[1]);
        OutTriangles.Add(Indices[2]);
    }
}

UHarnessGeneratorComponent::UHarnessGeneratorComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // 매 프레임 업데이트 불필요
}

// 런타임에 생성된 모든 동적 컴포넌트 및 캐시 데이터 초기화
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

// JSON에서 파싱된 정점(Vertex)과 엣지(Half-Edge) 데이터를 빠른 탐색을 위해 TMap에 캐싱
void UHarnessGeneratorComponent::BuildTopologyCaches(const FHarnessFloorData& FloorData)
{
    for (const FTopologyVertex& V : FloorData.vertices)
    {
        VertexCache.Add(V.id, V.ToVector2D());
    }
    for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
    {
        // 중복되는 Twin 엣지를 제외하고 유니크한 엣지만 캐싱
        if (!EdgeCache.Contains(Edge.twin_id))
        {
            EdgeCache.Add(Edge.id, Edge);
        }
    }
}

// 도면 생성을 위한 메인 파이프라인 함수
void UHarnessGeneratorComponent::BuildHarness(const FHarnessFloorData& FloorData)
{
    CachedFloorData = FloorData;
    if (!GetOwner() || !StyleDataTable) return;

    ClearHarness();
    BuildTopologyCaches(FloorData);

    AssembleStructuralWalls(FloorData);      // 벽체 생성 및 구멍 타공
    FabricateDynamicPlanes(FloorData);       // 바닥 및 천장 생성
    InstallOpeningComponents(FloorData);     // 문/창문 프롭 배치
}

// ==============================================================================
// 벽체(Wall) 메쉬 절차적 생성 및 Boolean 타공(구멍 뚫기) 로직
// ==============================================================================
void UHarnessGeneratorComponent::AssembleStructuralWalls(const FHarnessFloorData& FloorData)
{
    // 특정 엣지(vStart -> vEnd)가 속한 방(Face)의 이름을 찾는 람다 헬퍼 함수
    auto GetFaceLabel = [&](const FString& vStart, const FString& vEnd) -> FString {
        for (const FTopologyFace& Face : FloorData.faces) {
            int32 NumPts = Face.contour_vertex_ids.Num();
            for (int32 i = 0; i < NumPts; ++i) {
                if (Face.contour_vertex_ids[i] == vStart && Face.contour_vertex_ids[(i + 1) % NumPts] == vEnd) {
                    return Face.label; // 예: "Bedroom1", "LivingRoom"
                }
            }
        }
        return TEXT(""); 
    };

    // 데이터 주도(Data-Driven): JSON의 첫 번째 방 높이를 전체 벽체 높이로 참조 (단일 진실 공급원)
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

        // 벽체의 법선(Normal) 벡터 계산 (두께 오프셋 적용을 위함)
        FVector2D Dir = (pEnd - pStart).GetSafeNormal();
        FVector2D Normal(-Dir.Y, Dir.X); 

        FString ActualWallType = Edge.type;
        // WallLintel은 독립 엣지가 아닌 타공 시스템으로 통합되었으므로 WallInner로 일괄 치환
        if (ActualWallType.Equals(TEXT("WallLintel"))) {
            ActualWallType = TEXT("WallInner"); 
        }

        float HalfThickness = Edge.wall_thickness / 2.0f;
        float OffsetDist = HalfThickness / 2.0f;

        // 개별 벽체(Half-Wall)를 생성하고 머티리얼을 적용하는 내부 람다 함수
        auto BuildWallHalf = [&](FVector2D CenterPos, FString FaceLabel) {
            UDynamicMeshComponent* WallComp = NewObject<UDynamicMeshComponent>(GetOwner());
            WallComp->SetMobility(EComponentMobility::Movable);
            WallComp->RegisterComponent();
            WallComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            
            // 컴포넌트 위치 및 회전 설정 (바닥 중앙 피벗 기준)
            WallComp->SetRelativeLocationAndRotation(FVector(CenterPos.X, CenterPos.Y, 0.0f), FRotator(0.0f, Angle, 0.0f));

            UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(WallComp);
            WallComp->SetDynamicMesh(DynMesh);

            // 1. Geometry Scripting을 이용한 기본 벽 메쉬 박스 생성
            FGeometryScriptPrimitiveOptions PrimOptions;
            FTransform BaseTransform(FRotator::ZeroRotator, FVector(0, 0, WallHeight / 2.0f), FVector::OneVector);

            FVector BoxMin(-Length / 2.0f, -HalfThickness / 2.0f, -WallHeight / 2.0f);
            FVector BoxMax(Length / 2.0f, HalfThickness / 2.0f, WallHeight / 2.0f);
            FBox WallBox(BoxMin, BoxMax);

            UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBoundingBox(
                DynMesh, PrimOptions, BaseTransform, WallBox, 0, 0, 0
            );

            // 2. 개구부(Opening) 탐색 및 차집합(Subtract) 연산
            for (const FTopologyOpening& Opening : FloorData.openings)
            {
                if (Opening.target_edge_id == Edge.id || Opening.target_edge_id == Edge.twin_id)
                {
                    // 구멍을 낼 도구(Tool) 메쉬 생성
                    UDynamicMesh* HoleMesh = NewObject<UDynamicMesh>();
                    float HoleZCenter = Opening.z_offset_cm + (Opening.height_cm / 2.0f);
                    FTransform HoleTransform(FRotator::ZeroRotator, FVector(0, 0, HoleZCenter), FVector::OneVector);

                    // 렌더링 오류 방지를 위해 타공 박스 두께를 벽체보다 크게 설정
                    float HoleThickness = HalfThickness + 10.0f; 
                    FVector HoleMin(-Opening.width_cm / 2.0f, -HoleThickness / 2.0f, -Opening.height_cm / 2.0f);
                    FVector HoleMax(Opening.width_cm / 2.0f, HoleThickness / 2.0f, Opening.height_cm / 2.0f);
                    FBox HoleBox(HoleMin, HoleMax);

                    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBoundingBox(
                        HoleMesh, PrimOptions, HoleTransform, HoleBox, 0, 0, 0
                    );

                    FGeometryScriptMeshBooleanOptions BoolOptions;
            
                    // 타겟 메쉬(벽)에서 툴 메쉬(개구부)를 깎아냄
                    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
                        DynMesh, FTransform::Identity, HoleMesh, FTransform::Identity,
                        EGeometryScriptBooleanOperation::Subtract, BoolOptions
                    );
                }
            }

            // 3. 머티리얼 동적 할당 (StyleDataTable 룩업)
            UMaterialInterface* TargetMat = nullptr;
            if (StyleDataTable) {
                // 방별 지정 벽지 (예: Bedroom1_Wall) 우선 적용
                if (!FaceLabel.IsEmpty()) {
                    if (FHarnessStyleRow* RoomRow = StyleDataTable->FindRow<FHarnessStyleRow>(FName(*(FaceLabel + TEXT("_Wall"))), TEXT("MatLookup"))) {
                        TargetMat = RoomRow->Material;
                    }
                }
                // 없으면 외벽/내벽 기본 머티리얼 적용
                if (!TargetMat) {
                    if (FHarnessStyleRow* BaseRow = StyleDataTable->FindRow<FHarnessStyleRow>(FName(*ActualWallType), TEXT("MatLookup"))) {
                        TargetMat = BaseRow->Material;
                    }
                }
            }
            WallComp->SetMaterial(0, TargetMat ? TargetMat : DefaultFallbackMaterial.Get());
            
            // 플레이어 통과 방지를 위한 물리 충돌 활성화
            WallComp->SetComplexAsSimpleCollisionEnabled(true, true);
            WallComp->SetCollisionProfileName(TEXT("BlockAll"));
            SpawnedComponents.Add(WallComp);
        };

        // 데이터베이스의 Half-Edge 구조에 따라 양방향으로 두 겹의 벽체를 겹쳐 생성
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

        // 다각형 와인딩 방향 확인 및 교정 (면적 부호 판별)
        double SignedArea = 0.0;
        int32 NumPts = TriangulationPoints.Num();
        for (int32 i = 0; i < NumPts; ++i)
        {
            FVector2D P1 = TriangulationPoints[i];
            FVector2D P2 = TriangulationPoints[(i + 1) % NumPts];
            SignedArea += (P1.X * P2.Y - P2.X * P1.Y);
        }
        
        // 반시계 방향일 경우 시계 방향으로 배열 역전
        if (SignedArea > 0.0) 
        {
            Algo::Reverse(TriangulationPoints);
        }

        // --- 모듈화된 삼각분할 함수 재활용 ---
        TArray<int32> TriangleIndices;
        CustomTriangulateSimplePolygon(TriangulationPoints, TriangleIndices);

        // --- 1. 바닥(Floor) 메쉬 조립 ---
        UE::Geometry::FDynamicMesh3 DynMesh;
        DynMesh.EnableAttributes(); 
        
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
            // 바닥 밑면은 밖을 향하도록 와인딩 순서(C->B->A) 역전 적용
            DynMesh.AppendTriangle(BottomVIds[C], BottomVIds[B], BottomVIds[A]); 
        }

        // 바닥 측면(두께) 폴리곤 마감
        for (int32 i = 0; i < NumPts; ++i)
        {
            int32 NextI = (i + 1) % NumPts;
            DynMesh.AppendTriangle(TopVIds[i], BottomVIds[i], TopVIds[NextI]);
            DynMesh.AppendTriangle(TopVIds[NextI], BottomVIds[i], BottomVIds[NextI]);
        }

        // 바닥 UV 자동 맵핑 (100cm 단위 정규화)
        UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = DynMesh.Attributes()->PrimaryUV();
        for (int32 TID : DynMesh.TriangleIndicesItr())
        {
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
        
        TArray<int32> CeilTopVIds;
        TArray<int32> CeilBottomVIds;
        
        // 천장 높이는 JSON의 Face 데이터 동기화 유지
        double CeilingBottomZ = Face.z_offset + Face.height_cm; 
        double CeilingTopZ = CeilingBottomZ + SlabThickness; 

        for (const FVector2D& V : TriangulationPoints)
        {
            CeilTopVIds.Add(CeilingMesh.AppendVertex(FVector3d(V.X, V.Y, CeilingTopZ)));
            CeilBottomVIds.Add(CeilingMesh.AppendVertex(FVector3d(V.X, V.Y, CeilingBottomZ)));
        }

        // 바닥에서 연산한 Ear-Clipping 결과를 100% 재활용 (CPU 성능 최적화)
        for (int32 i = 0; i < TriangleIndices.Num(); i += 3)
        {
            int32 A = TriangleIndices[i];
            int32 B = TriangleIndices[i+1];
            int32 C = TriangleIndices[i+2];

            // 윗면 및 측면 생성 생략(백페이스 컬링 활용).
            // 방 내부에서 천장이 보이도록 C->B->A 와인딩 적용. 탑뷰에서는 투명 처리됨.
            CeilingMesh.AppendTriangle(CeilBottomVIds[C], CeilBottomVIds[B], CeilBottomVIds[A]); 
        }
        
        // 천장 UV 맵핑
        UE::Geometry::FDynamicMeshUVOverlay* CeilUVOverlay = CeilingMesh.Attributes()->PrimaryUV();
        for (int32 TID : CeilingMesh.TriangleIndicesItr())
        {
            UE::Geometry::FIndex3i Tri = CeilingMesh.GetTriangle(TID);
            FVector3d V0 = CeilingMesh.GetVertex(Tri.A);
            FVector3d V1 = CeilingMesh.GetVertex(Tri.B);
            FVector3d V2 = CeilingMesh.GetVertex(Tri.C);

            int32 UV0 = CeilUVOverlay->AppendElement(FVector2f(V0.X / 100.0f, V0.Y / 100.0f));
            int32 UV1 = CeilUVOverlay->AppendElement(FVector2f(V1.X / 100.0f, V1.Y / 100.0f));
            int32 UV2 = CeilUVOverlay->AppendElement(FVector2f(V2.X / 100.0f, V2.Y / 100.0f));
            CeilUVOverlay->SetTriangle(TID, UE::Geometry::FIndex3i(UV0, UV1, UV2));
        }

        // 법선 설정
        UE::Geometry::FMeshNormals::InitializeOverlayToPerVertexNormals(CeilingMesh.Attributes()->PrimaryNormals(), false);

        // 천장 컴포넌트 스폰
        UDynamicMeshComponent* CeilComp = NewObject<UDynamicMeshComponent>(GetOwner());
        CeilComp->RegisterComponent();
        CeilComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        CeilComp->SetDynamicMesh(NewObject<UDynamicMesh>(CeilComp));
        CeilComp->GetDynamicMesh()->SetMesh(MoveTemp(CeilingMesh));
        CeilComp->SetComplexAsSimpleCollisionEnabled(true, true);
        CeilComp->SetCollisionProfileName(TEXT("BlockAll")); // 천장 콜리전
        
        // 탑뷰에서 천장이 투명해도 광원 누수(Light Leak)를 막기 위해 양면 그림자 캐스팅 강제 적용
        CeilComp->bCastShadowAsTwoSided = true;
        
        CeilComp->NotifyMeshUpdated();

        // 천장 머티리얼 적용 (데이터 테이블에 "천장용" 이름이 지정되어 있다고 가정)
        UMaterialInterface* CeilMaterial = DefaultFallbackMaterial.Get();
        if (StyleDataTable) {
            // 예: "Bedroom1_Ceiling" 이름으로 데이터테이블에서 찾기
            if (FHarnessStyleRow* CeilRow = StyleDataTable->FindRow<FHarnessStyleRow>(FName(*(Face.label + TEXT("_Ceiling"))), TEXT("CeilMatLookup"))) {
                if (CeilRow->Material) CeilMaterial = CeilRow->Material;
            }
        }
        CeilComp->SetMaterial(0, CeilMaterial);

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

        // 개구부가 위치할 엣지의 중앙 좌표와 회전 각도 계산
        FVector2D Center2D = (pStart + pEnd) / 2.0f;
        float Angle = FMath::RadiansToDegrees(FMath::Atan2(pEnd.Y - pStart.Y, pEnd.X - pStart.X));

        // 데이터 테이블(StyleDataTable)에서 Opening 타입(예: "Door", "Window")에 맞는 에셋 로드
        FHarnessStyleRow* OpRow = StyleDataTable ? StyleDataTable->FindRow<FHarnessStyleRow>(FName(*Opening.type), TEXT("OpeningLookup")) : nullptr;
        if (!OpRow || !OpRow->Mesh) continue;

        UStaticMeshComponent* OpComp = NewObject<UStaticMeshComponent>(GetOwner());
        if (!OpComp) continue;

        OpComp->SetMobility(EComponentMobility::Movable);
        OpComp->SetStaticMesh(OpRow->Mesh);
        OpComp->RegisterComponent();
        OpComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        // Z축 오프셋에 높이의 절반을 더해 하단 피벗을 중앙 피벗으로 정렬 보정
        float PivotCorrectedZ = Opening.z_offset_cm + (Opening.height_cm / 2.0f);
        OpComp->SetRelativeLocationAndRotation(FVector(Center2D.X, Center2D.Y, PivotCorrectedZ), FRotator(0.0f, Angle, 0.0f));
        
        // JSON에 정의된 수치를 기준으로 1미터(100cm) 단위 정규화 스케일링 적용
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
