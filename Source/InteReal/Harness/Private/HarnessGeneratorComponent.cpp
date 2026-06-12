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
    // �니메이처리륄해 Tick�용��� �원 �약�해 기본 �태비활�화(false)롡니
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false; 
}

// ==============================================================================
// 기존�성모든 �적 메쉬, 조명, �롭�을 메모리에�전�괴(�거)�니
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
// �면 2D JSON �이�� �싱�여 �리3D 좌표�Z-Up)맞게 캐싱�니
// ==============================================================================
void UHarnessGeneratorComponent::BuildTopologyCaches(const FHarnessFloorData& FloorData)
{
    for (const FTopologyVertex& V : FloorData.vertices)
    {
        // �� �면�하 반전�는 것을 막기 �해 X, Y축을 교차 매핑�니
        VertexCache.Add(V.id, FVector2D(V.y, V.x));
    }
    for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
    {
        // Half-Edge 구조�서 중복 �분(Twin Edge)�외�고 고유벽면 �이�만 �깁�다.
        if (!EdgeCache.Contains(Edge.twin_id))
        {
            EdgeCache.Add(Edge.id, Edge);
        }
    }
}

// ==============================================================================
// �면 �성 메인 진입(최적 ExecuteBuildHarness� �합
// ==============================================================================
void UHarnessGeneratorComponent::BuildHarness(const FHarnessFloorData& FloorData)
{
    // �수 �이�� �으멈전�게 �행중단�니
    if (!GetOwner()) return;

    CachedFloorData = FloorData;

    // �면그리긄에 기존 �면즉각 �괴�니
    ClearHarness();
    BuildTopologyCaches(FloorData);

    AssembleStructuralWalls(FloorData);      // 벽체 ��창문 구멍 �기(Boolean)
    FabricateDynamicPlanes(FloorData);       // 바닥, 천장 �면(Triangulation) 밀�우 블로컝성
    InstallOpeningComponents(FloorData);     // �린 구멍�창문 3D �셋 배치
    
    if (bEnableInteriorLights)
    {
        InstallInteriorLights(FloorData);    // �중앙조명 배치
    }

    // 모든 메쉬 �성�났�면, Z춤�기반�아�름 �니메이�을 가�합�다.
    if (AnimatedWalls.Num() > 0)
    {
        bIsSpawning = true;
        WallAnimationProgress = 0.01f;
        SetComponentTickEnabled(true); 
    }
}

// ==============================================================================
// 맄레벽이 �르륟아�르Scale-Up) �니메이�을 처리�니
// ==============================================================================
void UHarnessGeneratorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsSpawning)
    {
        // 보간(InterpTo)�해 처음빠르� �엔 부�럽�1.0(�래 �기)�달�도�계산
        WallAnimationProgress = FMath::FInterpTo(WallAnimationProgress, 1.0f, DeltaTime, 1.5f);

        for (UDynamicMeshComponent* Wall : AnimatedWalls)
        {
            if (IsValid(Wall))
            {
                Wall->SetRelativeScale3D(FVector(1.0f, 1.0f, WallAnimationProgress));
            }
        }

        // �니메이�이 거의 �나멤차범위 0.005 �내) 최종 �정 처리
        if (FMath::IsNearlyEqual(WallAnimationProgress, 1.0f, 0.005f))
        {
            for (UDynamicMeshComponent* Wall : AnimatedWalls)
            {
                if (IsValid(Wall))
                {
                    Wall->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f)); 
                    // �� [중요] 최적�� �해 �니메이�이 �난 �점물리 충돌(Collision)켋�
                    Wall->SetCollisionProfileName(TEXT("BlockAll"));      
                }
            }
            
            bIsSpawning = false;
            SetComponentTickEnabled(false); // Tick 종료 (�능 최적
        }
    }
}

// ==============================================================================
// �방의 무게중심계산�여 �내 조명(Point Light)배치�니
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

        // 천장�서 30cm �래쪈공조명배치�여 �연�러간접 조명 �도
        FVector LightPos(Centroid.X, Centroid.Y, Face.z_offset + FixedWallHeight - 30.0f);

        UPointLightComponent* PointLight = NewObject<UPointLightComponent>(GetOwner());
        PointLight->SetMobility(EComponentMobility::Movable);
        PointLight->RegisterComponent();
        PointLight->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        PointLight->SetRelativeLocation(LightPos);
        PointLight->SetIntensity(2500.0f);
        PointLight->SetAttenuationRadius(1000.0f);
        PointLight->SetCastShadows(true);
        PointLight->LightColor = FColor(255, 245, 230); // �뜻�온�용
        PointLight->ComponentTags.Add(TEXT("InteriorLight"));

        SpawnedComponents.Add(PointLight);
    }
}

// ==============================================================================
// 3D 벽체 �성 ��창문 구멍(Boolean) �기 �산
// ==============================================================================
void UHarnessGeneratorComponent::AssembleStructuralWalls(const FHarnessFloorData& FloorData)
{
    const float FixedWallHeight = 300.0f; // 벒이 3m 강제 고정
    
    // �� Lumen 비상 방�륄해 벽을 �하좌우�1cm�버�킴
    const float VerticalOverlap = 1.0f;   
    const float HorizontalOverlap = 1.0f;  
    const float FixedWallThickness = 20.0f; // 모든 벐께 20cm 고정
    
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

        float HalfThickness = FixedWallThickness / 2.0f;
        float OffsetDist = HalfThickness / 2.0f;

        // 벽을 �반(밈쪽/바깥쪰로 �성�여 �로 �른 �질�용�도�
        auto BuildWallHalf = [&](FVector2D CenterPos) {
            UDynamicMeshComponent* WallComp = NewObject<UDynamicMeshComponent>(GetOwner());
            WallComp->SetMobility(EComponentMobility::Movable);
            WallComp->RegisterComponent();
            WallComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            WallComp->ComponentTags.Add(TEXT("EditableWall"));
            
            // �니메이�을 �해 최초 �이(Z ���0.01롌러            WallComp->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.01f));
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

            // Boolean(차집 �산�용벽에 문과 창문 �이즈만�의 구멍�음
            for (const FTopologyOpening& Opening : FloorData.openings)
            {
                if (Opening.target_edge_id == Edge.id || Opening.target_edge_id == Edge.twin_id)
                {
                    UDynamicMesh* HoleMesh = NewObject<UDynamicMesh>();
                    
                    // 벽을 �실관�하�록 �께50cm �유�추�
                    float HoleThickness = HalfThickness + 50.0f; 
                    float HoleWidth = Opening.width_cm + 2.0f; 
                    float HoleHeight = Opening.height_cm;
                    float HoleZOffset = Opening.z_offset_cm;

                    // 바닥 �버마진) �문�밑에 �이 �� �도�구멍 �기밑으롕장
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

            // 기본 머티리얼 �용
            if (DefaultFallbackMaterial) WallComp->SetMaterial(0, DefaultFallbackMaterial);
            
            WallComp->SetComplexAsSimpleCollisionEnabled(true, true);
            WallComp->SetCollisionProfileName(TEXT("NoCollision")); // �성 �점충돌 �제
            WallComp->bCastShadowAsTwoSided = true; // �면 그림�성            WallComp->NotifyMeshUpdated();
            
            SpawnedComponents.Add(WallComp);
            AnimatedWalls.Add(WallComp); 
        };

        BuildWallHalf(Center2D + (Normal * OffsetDist));
        BuildWallHalf(Center2D - (Normal * OffsetDist));
    }
}

// ==============================================================================
// �각바닥 �천장 멝성 (Ear Clipping �각분할 �용)
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

        // �� 중복(Degenerate) �점 �180�직Collinear) �드 �거 (�각분할 �류 방�)
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

        // 면적(Signed Area)�해 �더�그리�방향(Winding Order) �단 (��Reverse)
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

        // 바닥1cm �장�여 벍으�맞물리게 (빐천 차단)
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

        // �각�을 �각Triangle) 배열�분할
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
        double SlabThickness = 20.0; // 바닥 콘크리트 �래븐께 20cm 고정

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

            // �적(Cross Product)�해 법선�도바깥�방향(ExpectedNormal)바라보게 �렬�니
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

            // �� �판 �상 �더�(Winding Order ��)
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

            // ��링 �기 보정 (100.0f = 1m �위 맵핑)
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
        // �� 바닥 메쉬�체�으롔리구조�며 �면 그림처리가 �어 �으므례�우 �용 블로�FloorShadowBlocker)가 �요 �습�다.
        DyMeshComp->bCastShadowAsTwoSided = true; 
        DyMeshComp->NotifyMeshUpdated();
        
        if (DefaultFallbackMaterial) DyMeshComp->SetMaterial(0, DefaultFallbackMaterial);

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

            // �뷰�서 ��가 �여�보�도�천장� �래�바라보는(�판) 메쉬 �나맨깁�다.
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

        // �� 천장� �에보이지 �는 �면 메쉬��� �양광이 지붕을 �고 �내롤어�는 것을 막기 �해 �명차폐(Solid) 블록�성�니
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
            
            // �게�에 보이지�고 보이지 �는 그림�만 �리�도롤정
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
// �공된 개구부 �치�Door) �는 창문(Window) 메쉬 �롭배치�니
// ==============================================================================
void UHarnessGeneratorComponent::InstallOpeningComponents(const FHarnessFloorData& FloorData)
{
    // �� [�정] StyleDataTable�거�었��� �재메쉬 배치�건너�거기본 메쉬륬용�야 �니
    // �용�� ��이블이 �요 �다곈으므� �각구멍(Boolean)맨기�메쉬 배치건너�니
}

// ==============================================================================
// �카메�동 �레�밍 �을 �해 �면 �체�기(Bounds)�반환�니
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
