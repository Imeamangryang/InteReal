#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PointLightComponent.h" 
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshNormals.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeomTools.h" 
#include "Algo/Reverse.h"

UHarnessGeneratorComponent::UHarnessGeneratorComponent()
{
    // 애니메이션 처리를 위해 Tick을 사용하지만, 자원 절약을 위해 기본 상태는 비활성화(false)로 둡니다.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false; 
}

// ==============================================================================
// 기존에 생성된 모든 동적 메쉬, 조명, 프롭들을 메모리에서 완전히 파괴(제거)합니다.
// ==============================================================================
void UHarnessGeneratorComponent::ClearHarness()
{
    for (UActorComponent* Comp : SpawnedComponents)
    {
        if (IsValid(Comp)) Comp->DestroyComponent();
    }
    SpawnedComponents.Reset();
    AnimatedWalls.Reset(); 
    VertexCache.Reset();
    EdgeCache.Reset();

    bIsSpawning = false;
}

// ==============================================================================
// 도면 2D JSON 데이터를 파싱하여 언리얼 3D 좌표계(Z-Up)에 맞게 캐싱합니다.
// ==============================================================================
void UHarnessGeneratorComponent::BuildTopologyCaches(const FHarnessFloorData& FloorData)
{
    for (const FTopologyVertex& V : FloorData.vertices)
    {
        // 💡 도면이 상하 반전되는 것을 막기 위해 X, Y축을 교차 매핑합니다.
        VertexCache.Add(V.id, FVector2D(V.y, V.x));
    }
    for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
    {
        // Half-Edge 구조에서 중복 선분(Twin Edge)을 제외하고 고유한 벽면 데이터만 남깁니다.
        if (!EdgeCache.Contains(Edge.twin_id))
        {
            EdgeCache.Add(Edge.id, Edge);
        }
    }
}

// ==============================================================================
// 도면 생성 메인 진입점 (최적화: ExecuteBuildHarness와 통합됨)
// ==============================================================================
void UHarnessGeneratorComponent::BuildHarness(const FHarnessFloorData& FloorData)
{
    // 필수 데이터 테이블이나 소유자가 없으면 안전하게 실행을 중단합니다.
    if (!GetOwner() || !StyleDataTable) return;

    CachedFloorData = FloorData;

    // 새 도면을 그리기 전에 기존 도면을 즉각 파괴합니다.
    ClearHarness();
    BuildTopologyCaches(FloorData);

    AssembleStructuralWalls(FloorData);      // 벽체 및 문/창문 구멍 뚫기(Boolean)
    FabricateDynamicPlanes(FloorData);       // 바닥, 천장 평면(Triangulation) 및 섀도우 블로커 생성
    InstallOpeningComponents(FloorData);     // 뚫린 구멍에 문/창문 3D 에셋 배치
    
    if (bEnableInteriorLights)
    {
        InstallInteriorLights(FloorData);    // 방 중앙에 조명 배치
    }

    // 모든 메쉬 생성이 끝났다면, Z축 스케일 기반의 솟아오름 애니메이션을 가동합니다.
    if (AnimatedWalls.Num() > 0)
    {
        bIsSpawning = true;
        WallAnimationProgress = 0.01f;
        SetComponentTickEnabled(true); 
    }
}

// ==============================================================================
// 매 프레임 벽이 스르륵 솟아오르는(Scale-Up) 애니메이션을 처리합니다.
// ==============================================================================
void UHarnessGeneratorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsSpawning)
    {
        // 보간(InterpTo)을 통해 처음엔 빠르게, 끝엔 부드럽게 1.0(원래 크기)에 도달하도록 계산
        WallAnimationProgress = FMath::FInterpTo(WallAnimationProgress, 1.0f, DeltaTime, 1.5f);

        for (UDynamicMeshComponent* Wall : AnimatedWalls)
        {
            if (IsValid(Wall))
            {
                Wall->SetRelativeScale3D(FVector(1.0f, 1.0f, WallAnimationProgress));
            }
        }

        // 애니메이션이 거의 끝나면(오차범위 0.005 이내) 최종 확정 처리
        if (FMath::IsNearlyEqual(WallAnimationProgress, 1.0f, 0.005f))
        {
            for (UDynamicMeshComponent* Wall : AnimatedWalls)
            {
                if (IsValid(Wall))
                {
                    Wall->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f)); 
                    // 💡 [중요] 최적화를 위해 애니메이션이 끝난 이 시점에 물리 충돌(Collision)을 켭니다.
                    Wall->SetCollisionProfileName(TEXT("BlockAll"));      
                }
            }
            
            bIsSpawning = false;
            SetComponentTickEnabled(false); // Tick 종료 (성능 최적화)
        }
    }
}

// ==============================================================================
// 각 방의 무게중심을 계산하여 실내 조명(Point Light)을 배치합니다.
// ==============================================================================
void UHarnessGeneratorComponent::InstallInteriorLights(const FHarnessFloorData& FloorData)
{
    const float FixedWallHeight = 300.0f;

    for (const FTopologyFace& Face : FloorData.faces)
    {
        if (Face.contour_vertex_ids.Num() < 3) continue;

        FVector2D Centroid(0.0, 0.0);
        int32 ValidPts = 0;

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

        // 천장에서 30cm 아래쪽 허공에 조명을 배치하여 자연스러운 간접 조명 유도
        FVector LightPos(Centroid.X, Centroid.Y, Face.z_offset + FixedWallHeight - 30.0f);

        UPointLightComponent* PointLight = NewObject<UPointLightComponent>(GetOwner());
        PointLight->SetMobility(EComponentMobility::Movable);
        PointLight->RegisterComponent();
        PointLight->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        PointLight->SetRelativeLocation(LightPos);
        PointLight->SetIntensity(2500.0f);
        PointLight->SetAttenuationRadius(1000.0f);
        PointLight->SetCastShadows(true);
        PointLight->LightColor = FColor(255, 245, 230); // 따뜻한 색온도 적용
        PointLight->ComponentTags.Add(TEXT("InteriorLight"));

        SpawnedComponents.Add(PointLight);
    }
}

// ==============================================================================
// 3D 벽체 생성 및 문/창문 구멍(Boolean) 뚫기 연산
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

    const float FixedWallHeight = 300.0f; // 벽 높이 3m 강제 고정
    
    // 💡 Lumen 빛 샘 현상 방지를 위해 벽을 상하좌우로 1cm씩 오버랩 시킴
    const float VerticalOverlap = 1.0f;   
    const float HorizontalOverlap = 1.0f;  
    const float FixedWallThickness = 20.0f; // 모든 벽 두께 20cm 고정
    
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
        FVector2D Normal(Dir.Y, -Dir.X); 

        FString ActualWallType = Edge.type;
        if (ActualWallType.Equals(TEXT("WallLintel"))) {
            ActualWallType = TEXT("WallInner"); 
        }

        float HalfThickness = FixedWallThickness / 2.0f;
        float OffsetDist = HalfThickness / 2.0f;

        // 벽을 절반(방 안쪽/바깥쪽)씩 따로 생성하여 서로 다른 재질을 적용할 수 있도록 함
        auto BuildWallHalf = [&](FVector2D CenterPos, FString FaceLabel) {
            UDynamicMeshComponent* WallComp = NewObject<UDynamicMeshComponent>(GetOwner());
            WallComp->SetMobility(EComponentMobility::Movable);
            WallComp->RegisterComponent();
            WallComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            WallComp->ComponentTags.AddUnique(FName(TEXT("EditableWall")));
            
            // 애니메이션을 위해 최초 높이(Z 스케일)를 0.01로 눌러둠
            WallComp->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.01f));
            WallComp->SetRelativeLocationAndRotation(FVector(CenterPos.X, CenterPos.Y, 0.0f), FRotator(0.0f, Angle, 0.0f));

            UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(WallComp);
            WallComp->SetDynamicMesh(DynMesh);

            float ExtendedLength = Length + HorizontalOverlap;
            float ExtendedWallHeight = FixedWallHeight + (VerticalOverlap * 2.0f);

            FGeometryScriptPrimitiveOptions PrimOptions;
            FTransform BaseTransform(FRotator::ZeroRotator, FVector(0, 0, FixedWallHeight / 2.0f), FVector::OneVector);

            FVector BoxMin(-ExtendedLength / 2.0f, -HalfThickness / 2.0f, -ExtendedWallHeight / 2.0f);
            FVector BoxMax(ExtendedLength / 2.0f, HalfThickness / 2.0f, ExtendedWallHeight / 2.0f);
            FBox WallBox(BoxMin, BoxMax);

            UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBoundingBox(
                DynMesh, PrimOptions, BaseTransform, WallBox, 0, 0, 0
            );

            // Boolean(차집합) 연산을 이용해 벽에 문과 창문 사이즈만큼의 구멍을 뚫음
            for (const FTopologyOpening& Opening : FloorData.openings)
            {
                if (Opening.target_edge_id == Edge.id || Opening.target_edge_id == Edge.twin_id)
                {
                    UDynamicMesh* HoleMesh = NewObject<UDynamicMesh>();
                    
                    // 벽을 확실히 관통하도록 두께에 50cm 여유를 추가
                    float HoleThickness = HalfThickness + 50.0f; 
                    float HoleWidth = Opening.width_cm + 2.0f; 
                    float HoleHeight = Opening.height_cm;
                    float HoleZOffset = Opening.z_offset_cm;

                    // 바닥 오버랩(마진) 때문에 문 밑에 턱이 남지 않도록 구멍 크기도 밑으로 확장
                    if (HoleZOffset <= 0.1f) {
                        HoleZOffset -= (VerticalOverlap + 1.0f);
                        HoleHeight += (VerticalOverlap + 1.0f);
                    }

                    float HoleZCenter = HoleZOffset + (HoleHeight / 2.0f);
                    FTransform HoleTransform(FRotator::ZeroRotator, FVector(0, 0, HoleZCenter), FVector::OneVector);

                    FVector HoleMin(-HoleWidth / 2.0f, -HoleThickness / 2.0f, -HoleHeight / 2.0f);
                    FVector HoleMax(HoleWidth / 2.0f, HoleThickness / 2.0f, HoleHeight / 2.0f);
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

            // DataTable에서 방 이름에 해당하는 재질 매핑
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
            WallComp->SetCollisionProfileName(TEXT("NoCollision")); // 생성 시점엔 충돌 해제
            WallComp->bCastShadowAsTwoSided = true; // 양면 그림자 활성화
            WallComp->NotifyMeshUpdated();
            
            SpawnedComponents.Add(WallComp);
            AnimatedWalls.Add(WallComp); 
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
// 다각형 바닥 및 천장 면 생성 (Ear Clipping 삼각분할 적용)
// ==============================================================================
void UHarnessGeneratorComponent::FabricateDynamicPlanes(const FHarnessFloorData& FloorData)
{
    const float FixedWallHeight = 300.0f;

    for (const FTopologyFace& Face : FloorData.faces)
    {
        if (Face.contour_vertex_ids.Num() < 3) continue;

        TArray<FVector2D> RawPoints;
        for (const FString& VId : Face.contour_vertex_ids)
        {
            if (VertexCache.Contains(VId))
            {
                RawPoints.Add(VertexCache[VId]);
            }
        }

        // 💡 중복(Degenerate) 정점 및 180도 일직선(Collinear) 노드 제거 (삼각분할 오류 방지)
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

        // 면적(Signed Area)을 통해 렌더링 그리기 방향(Winding Order) 판단 (역방향 시 Reverse)
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

        // 바닥을 1cm 확장하여 벽 속으로 맞물리게 함 (빛 샘 원천 차단)
        const float FloorExpansion = 1.0f; 
        TArray<FVector2D> ExpandedPoints;
        for (int32 i = 0; i < NumPts; ++i)
        {
            FVector2D Prev = TriangulationPoints[(i - 1 + NumPts) % NumPts];
            FVector2D Curr = TriangulationPoints[i];
            FVector2D Next = TriangulationPoints[(i + 1) % NumPts];

            FVector2D DirPrev = (Curr - Prev).GetSafeNormal();
            FVector2D DirNext = (Next - Curr).GetSafeNormal();

            FVector2D NPrev(DirPrev.Y, -DirPrev.X);
            FVector2D NNext(DirNext.Y, -DirNext.X);

            FVector2D Bisector = (NPrev + NNext).GetSafeNormal();
            float Dot = FVector2D::DotProduct(Bisector, NPrev);
            
            float OffsetLength = (Dot > 0.1f) ? (FloorExpansion / Dot) : FloorExpansion;
            ExpandedPoints.Add(Curr + Bisector * OffsetLength);
        }
        TriangulationPoints = ExpandedPoints;

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

        // 다각형을 삼각형(Triangle) 배열로 분할
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

        // --- 1. 바닥(Floor) 메쉬 조립 ---
        UE::Geometry::FDynamicMesh3 DynMesh;
        DynMesh.EnableAttributes(); 
        DynMesh.Attributes()->EnableMaterialID();
        UE::Geometry::FDynamicMeshMaterialAttribute* FloorMatID = DynMesh.Attributes()->GetMaterialID();
        
        TArray<int32> TopVIds;
        TArray<int32> BottomVIds;
        double SlabThickness = 20.0; // 바닥 콘크리트 슬래브 두께 20cm 고정

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

            // 외적(Cross Product)을 통해 법선이 의도한 바깥쪽 방향(ExpectedNormal)을 바라보게 정렬합니다.
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

            // 💡 상/하판 정상 렌더링 (Winding Order 역전)
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

            const FVector2D Edge = Next - Current;
            const FVector3d OutwardNormal(Edge.Y, -Edge.X, 0.0);

            AppendTriangleFacing(SideTopA, SideBottomA, SideTopB, OutwardNormal);
            AppendTriangleFacing(SideTopB, SideBottomA, SideBottomB, OutwardNormal);
        }

        UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = DynMesh.Attributes()->PrimaryUV();
        for (int32 TID : DynMesh.TriangleIndicesItr())
        {
            FloorMatID->SetValue(TID, 0); 
            UE::Geometry::FIndex3i Tri = DynMesh.GetTriangle(TID);
            FVector3d V0 = DynMesh.GetVertex(Tri.A);
            FVector3d V1 = DynMesh.GetVertex(Tri.B);
            FVector3d V2 = DynMesh.GetVertex(Tri.C);

            // 타일링 크기 보정 (100.0f = 1m 단위 맵핑)
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
        DyMeshComp->ComponentTags.Add(FName("Floor"));

        if (!DyMeshComp->GetDynamicMesh()) {
            DyMeshComp->SetDynamicMesh(NewObject<UDynamicMesh>(DyMeshComp));
        }

        DyMeshComp->GetDynamicMesh()->SetMesh(MoveTemp(DynMesh));
        DyMeshComp->SetComplexAsSimpleCollisionEnabled(true, true);
        DyMeshComp->SetCollisionProfileName(TEXT("BlockAll"));
        // 💡 바닥 메쉬는 자체적으로 솔리드 구조이며 양면 그림자 처리가 되어 있으므로 섀도우 전용 블로커(FloorShadowBlocker)가 필요 없습니다.
        DyMeshComp->bCastShadowAsTwoSided = true; 
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
        
        double CeilingBottomZ = Face.z_offset + FixedWallHeight; 
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

            // 탑뷰에서 내부가 들여다보이도록 천장은 아래를 바라보는(하판) 메쉬 하나만 남깁니다.
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

        UMaterialInterface* CeilMaterial = DefaultFallbackMaterial;
        if (StyleDataTable) {
            if (FHarnessStyleRow* CeilRow = StyleDataTable->FindRow<FHarnessStyleRow>(FName(*(Face.label + TEXT("_Ceiling"))), TEXT("CeilMatLookup"))) {
                if (CeilRow->Material) CeilMaterial = CeilRow->Material;
            }
        }
        if (CeilMaterial) CeilComp->SetMaterial(0, CeilMaterial);

        SpawnedComponents.Add(CeilComp);

        // 💡 천장은 위에서 보이지 않는 단면 메쉬이므로, 태양광이 지붕을 뚫고 실내로 들어오는 것을 막기 위해 투명한 차폐(Solid) 블록을 생성합니다.
        UE::Geometry::FDynamicMesh3 CeilingShadowMesh;
        CeilingShadowMesh.EnableAttributes();

        TArray<int32> ShadowTopVIds;
        TArray<int32> ShadowBottomVIds;
        for (const FVector2D& V : TriangulationPoints)
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

        for (int32 i = 0; i < NumPts; ++i)
        {
            int32 NextI = (i + 1) % NumPts;
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
            
            // 인게임에 보이지는 않고 보이지 않는 그림자만 드리우도록 설정
            CeilingShadowBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            CeilingShadowBlocker->SetVisibility(false, true);
            CeilingShadowBlocker->SetHiddenInGame(true, true);
            CeilingShadowBlocker->CastShadow = true;
            CeilingShadowBlocker->bCastHiddenShadow = true;
            CeilingShadowBlocker->bCastShadowAsTwoSided = true;
            CeilingShadowBlocker->NotifyMeshUpdated();

            SpawnedComponents.Add(CeilingShadowBlocker);
        }
    }
}

// ==============================================================================
// 타공된 개구부 위치에 문(Door) 또는 창문(Window) 메쉬 프롭을 배치합니다.
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
        
        // 20cm 고정 벽 두께에 맞춰 Y 스케일을 0.2로 강제 할당
        OpComp->SetRelativeScale3D(FVector(Opening.width_cm / 100.0f, 20.0f / 100.0f, Opening.height_cm / 100.0f));
        
        UMaterialInterface* OpMat = OpRow->Material ? OpRow->Material : DefaultFallbackMaterial;
        if (OpMat) OpComp->SetMaterial(0, OpMat);

        OpComp->SetCollisionProfileName(TEXT("BlockAll"));
        SpawnedComponents.Add(OpComp);
    }
}

// ==============================================================================
// 뷰 카메라 자동 프레이밍 등을 위해 도면 전체의 크기(Bounds)를 반환합니다.
// ==============================================================================
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
        if (V.y < OutMin.X) OutMin.X = V.y;
        if (V.y > OutMax.X) OutMax.X = V.y;
        if (V.x < OutMin.Y) OutMin.Y = V.x;
        if (V.x > OutMax.Y) OutMax.Y = V.x;
    }
}