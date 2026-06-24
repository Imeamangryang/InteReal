#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"

#include "Components/DynamicMeshComponent.h"

#include "Public/HarnessPipelineManager.h"
#include "Public/HarnessSaveManagerComponent.h"

float UHarnessGeneratorComponent::CalculateEffectivePlanScale(const FHarnessFloorData& FloorData) const
{
    float EffectiveScale = FMath::Max(EditorPlanScale * OverallPlanScale, UE_SMALL_NUMBER);

    if (!bAutoScaleFromDoorWidth || DoorReferenceWidthCm <= UE_SMALL_NUMBER)
    {
        return EffectiveScale;
    }

    TArray<float> DoorWidthsBelowReference;
    for (const FTopologyOpening& Opening : FloorData.openings)
    {
        if (!Opening.type.Equals(TEXT("Door"), ESearchCase::IgnoreCase))
        {
            continue;
        }

        if (Opening.width_cm > UE_SMALL_NUMBER && Opening.width_cm < DoorReferenceWidthCm)
        {
            DoorWidthsBelowReference.Add(Opening.width_cm);
        }
    }

    if (DoorWidthsBelowReference.Num() == 0)
    {
        return EffectiveScale;
    }

    DoorWidthsBelowReference.Sort();

    float SourceDoorWidth = 0.0f;
    const int32 MidIndex = DoorWidthsBelowReference.Num() / 2;
    if (DoorWidthsBelowReference.Num() % 2 == 0)
    {
        SourceDoorWidth = (DoorWidthsBelowReference[MidIndex - 1] + DoorWidthsBelowReference[MidIndex]) * 0.5f;
    }
    else
    {
        SourceDoorWidth = DoorWidthsBelowReference[MidIndex];
    }

    if (SourceDoorWidth <= UE_SMALL_NUMBER)
    {
        return EffectiveScale;
    }

    return EffectiveScale * (DoorReferenceWidthCm / SourceDoorWidth);
}

FHarnessFloorData UHarnessGeneratorComponent::MakeRuntimeFloorData(const FHarnessFloorData& FloorData, float PlanScale) const
{
    FHarnessFloorData RuntimeFloorData = FloorData;
    if (FMath::IsNearlyEqual(PlanScale, 1.0f, UE_SMALL_NUMBER))
    {
        return RuntimeFloorData;
    }

    for (FTopologyVertex& Vertex : RuntimeFloorData.vertices)
    {
        Vertex.x *= PlanScale;
        Vertex.y *= PlanScale;
    }

    for (FTopologyHalfEdge& Edge : RuntimeFloorData.half_edges)
    {
        Edge.wall_thickness *= PlanScale;
    }



    for (FTopologyOpening& Opening : RuntimeFloorData.openings)
    {
        Opening.width_cm *= PlanScale;
        Opening.height_cm *= PlanScale;
        Opening.z_offset_cm *= PlanScale;
        Opening.offset_to_center_cm *= PlanScale;
    }

    return RuntimeFloorData;
}

UHarnessGeneratorComponent::UHarnessGeneratorComponent()
{
    // ?醫딅빍筌롫뗄???筌ｌ꼶?곭몴??袁る퉸 Tick???????렽? ?귐딅꺖????됰튋???袁る퉸 疫꿸퀡???怨밴묶????쑵??源딆넅(false)嚥???쇱젟??몃빍??
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false; 
}

// ==============================================================================
// 疫꿸퀣?????밴쉐??筌뤴뫀諭??類ㅼ읅 筌롫뗄?? 鈺곌퀡梨? ??곕???쇱뱽 筌롫뗀?덄뵳?肉???袁⑹읈????볤탢??몃빍??
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

void UHarnessGeneratorComponent::RebuildHarnessFromRuntimeData(const FHarnessFloorData& FloorData)
{
    ClearHarness();
    BuildTopologyCaches(FloorData);

    AssembleStructuralWalls(FloorData);
    FabricateDynamicPlanes(FloorData);
    InstallOpeningComponents(FloorData);



    if (AnimatedWalls.Num() > 0)
    {
        bIsSpawning = true;
        WallAnimationProgress = 0.01f;
        SetComponentTickEnabled(true);
    }
}

// ==============================================================================
// ?袁ⓦ늺 2D JSON ?怨쀬뵠?怨? ???뼓??뤿연 ?紐꺿봺??3D ?ル슦紐닸?Z-Up)??筌띿쉳苡?筌?Ŋ???몃빍??
// ==============================================================================
void UHarnessGeneratorComponent::BuildTopologyCaches(const FHarnessFloorData& FloorData)
{


    for (const FTopologyVertex& V : FloorData.vertices)
    {
        // ?袁ⓦ늺???怨밸릭 獄쏆꼷???롫뮉 野껉퍔??筌띾맦由??袁る퉸 X, Y?곕벡???대Ŋ媛?筌띲끋釉??몃빍??
        VertexCache.Add(V.id, FloorData.ToHarnessPoint(V));
    }
    for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
    {
        // Half-Edge ?닌듼?癒?퐣 餓λ쵎???롫뮉 ?봔??Twin Edge)?? ??뽰뇚??랁??⑥쥙???甕곗럥???怨쀬뵠?怨뺤춸 ??ｍ돥??덈뼄.
        if (!EdgeCache.Contains(Edge.twin_id))
        {
            EdgeCache.Add(Edge.id, Edge);
        }
    }
}

// ==============================================================================
// ?袁ⓦ늺 ??밴쉐 筌롫뗄??筌욊쑴???
// ==============================================================================
void UHarnessGeneratorComponent::BuildHarness(const FHarnessFloorData& FloorData)
{
    if (!GetOwner()) return;

    SourceFloorData = FloorData;
    RebuildHarnessWithCurrentScale();
}

void UHarnessGeneratorComponent::RebuildHarnessWithCurrentScale()
{
    if (!GetOwner() || SourceFloorData.vertices.IsEmpty())
    {
        return;
    }

    LastAppliedPlanScale = CalculateEffectivePlanScale(SourceFloorData);
    CachedFloorData = MakeRuntimeFloorData(SourceFloorData, LastAppliedPlanScale);
    RebuildHarnessFromRuntimeData(CachedFloorData);
}

void UHarnessGeneratorComponent::SetEditorPlanScale(float NewScale, bool bRebuild)
{
    EditorPlanScale = FMath::Max(NewScale, 0.01f);
    if (bRebuild)
    {
        RebuildHarnessWithCurrentScale();
    }
}

#if WITH_EDITOR
void UHarnessGeneratorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.Property
        ? PropertyChangedEvent.Property->GetFName()
        : NAME_None;

    if (PropertyName == GET_MEMBER_NAME_CHECKED(UHarnessGeneratorComponent, EditorPlanScale) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UHarnessGeneratorComponent, OverallPlanScale) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UHarnessGeneratorComponent, bAutoScaleFromDoorWidth) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UHarnessGeneratorComponent, DoorReferenceWidthCm))
    {
        EditorPlanScale = FMath::Max(EditorPlanScale, 0.01f);
        OverallPlanScale = FMath::Max(OverallPlanScale, 0.01f);
        RebuildHarnessWithCurrentScale();
    }
}
#endif

// ==============================================================================
// 甕곗럩???袁⑥삋?癒?퐣 ?袁⑥쨮 ??쏅툡??삘뀮??Scale-Up) ?醫딅빍筌롫뗄???륁뱽 筌ｌ꼶???몃빍??
// ==============================================================================
void UHarnessGeneratorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsSpawning)
    {
        // 癰귣떯而?InterpTo)?????퉸 筌ｌ꼷??? ??쥓?ㅵ칰? ??밸퓦 ?봔??뺤쓦野?1.0(?癒?삋 ??由????袁⑤뼎??롫즲嚥??④쑴沅??몃빍??
        WallAnimationProgress = FMath::FInterpTo(WallAnimationProgress, 1.0f, DeltaTime, 1.5f);

        for (UDynamicMeshComponent* Wall : AnimatedWalls)
        {
            if (IsValid(Wall))
            {
                Wall->SetRelativeScale3D(FVector(1.0f, 1.0f, WallAnimationProgress));
            }
        }

        // ?醫딅빍筌롫뗄???륁뵠 椰꾧퀣????멸텢??????쇨컧甕곕뗄??0.005 ??沅? 筌ㅼ뮇伊?癰귣똻??筌ｌ꼶???몃빍??
        if (FMath::IsNearlyEqual(WallAnimationProgress, 1.0f, 0.005f))
        {
            for (UDynamicMeshComponent* Wall : AnimatedWalls)
            {
                if (IsValid(Wall))
                {
                    Wall->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f)); 
                    // [餓λ쵐?? 筌ㅼ뮇??遺? ?袁る퉸 ?醫딅빍筌롫뗄???륁뵠 ??멸텆 ??뽰젎???얠눖???겸뫖猷?Collision)???????
                    if (Wall->ComponentHasTag(TEXT("EditableWall")))
                    {
                        Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                        Wall->SetCollisionObjectType(ECC_WorldStatic);
                        Wall->SetCollisionResponseToAllChannels(ECR_Block);
                        Wall->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
                        Wall->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
                        Wall->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
                        Wall->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
                    }
                    else
                    {
                        Wall->SetCollisionProfileName(TEXT("BlockAll"));
                        Wall->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
                        Wall->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
                    }
                }
            }
            
            
            bIsSpawning = false;
        }
    }
}

// ==============================================================================
// 3D 甕곗럩猿???밴쉐 獄?筌≪럥揆 ?닌됱컞(Boolean) ??る┛ ?④쑴沅?
// ==============================================================================
// ==============================================================================
// ?袁⑷퍥 ?袁ⓦ늺??Bounds(野껋럡?????④쑴沅??뤿연 燁삳?李??餓??源놁뱽 ?袁る퉸 獄쏆꼹???몃빍??
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
        const FVector2D Point = CachedFloorData.ToHarnessPoint(V);
        if (Point.X < OutMin.X) OutMin.X = Point.X;
        if (Point.X > OutMax.X) OutMax.X = Point.X;
        if (Point.Y < OutMin.Y) OutMin.Y = Point.Y;
        if (Point.Y > OutMax.Y) OutMax.Y = Point.Y;
    }
}

// ==============================================================================
// ?諭???닌딅열??筌ｌ뮇???誘れ뵠????륁젟??랁???????源?源딅???덈뼄.
// ==============================================================================
void UHarnessGeneratorComponent::UpdateCeilingHeight(FString FaceId, float NewHeight)
{
    for (FTopologyFace& Face : CachedFloorData.faces)
    {
        if (Face.face_id == FaceId)
        {
            Face.height_cm = NewHeight;
            break;
        }
    }

    for (FTopologyFace& Face : SourceFloorData.faces)
    {
        if (Face.face_id == FaceId)
        {
            Face.height_cm = NewHeight;
            break;
        }
    }
    
    if (UHarnessPipelineManager* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
    {
        if (UHarnessSaveManagerComponent* SaveComp = Pipeline->GetSaveManagerComp())
        {
            FString CurrentState = SaveComp->SaveInteriorState();
            RebuildHarnessFromRuntimeData(CachedFloorData);
            SaveComp->LoadInteriorState(CurrentState);
            return;
        }
    }
    
    RebuildHarnessFromRuntimeData(CachedFloorData);
}

FVector UHarnessGeneratorComponent::GetSafeSpawnLocation() const
{
    FVector LocalSpawnPos = FVector::ZeroVector;
    bool bFoundSafeLocation = false;

    // 1. 방(Face) 데이터가 있다면 첫 번째 유효한 방의 가장 긴 벽에서 안쪽으로 들어온 위치 사용
    // (단순 중심점은 L자형 방에서 벽에 걸칠 수 있음)
    if (!bFoundSafeLocation && CachedFloorData.faces.Num() > 0)
    {
        for (const FTopologyFace& Face : CachedFloorData.faces)
        {
            if (Face.contour_vertex_ids.Num() < 3) continue;

            TArray<FVector2D> Pts;
            for (const FString& VId : Face.contour_vertex_ids)
            {
                if (VertexCache.Contains(VId))
                {
                    Pts.Add(VertexCache[VId]);
                }
            }
            
            if (Pts.Num() >= 3)
            {
                // 다각형의 대략적인 중심점 계산
                FVector2D Centroid(0.0, 0.0);
                for (const FVector2D& Pt : Pts)
                {
                    Centroid += Pt;
                }
                Centroid /= Pts.Num();

                // 가장 긴 선분(벽) 찾기
                float MaxLenSq = -1.0f;
                FVector2D BestMid(0.0, 0.0);
                FVector2D BestEdgeDir(0.0, 0.0);

                for (int i = 0; i < Pts.Num(); ++i)
                {
                    FVector2D V0 = Pts[i];
                    FVector2D V1 = Pts[(i + 1) % Pts.Num()];
                    float LenSq = FVector2D::DistSquared(V0, V1);
                    if (LenSq > MaxLenSq)
                    {
                        MaxLenSq = LenSq;
                        BestMid = (V0 + V1) * 0.5f;
                        BestEdgeDir = (V1 - V0).GetSafeNormal();
                    }
                }

                if (MaxLenSq > 0.0f)
                {
                    // 가장 긴 벽의 중앙에서 중심점 방향을 향하는 수직 벡터(방 안쪽 방향) 계산
                    FVector2D InwardDir = (Centroid - BestMid).GetSafeNormal();
                    FVector2D Normal1(-BestEdgeDir.Y, BestEdgeDir.X);
                    FVector2D Normal2(BestEdgeDir.Y, -BestEdgeDir.X);
                    FVector2D EdgeNormal = (FVector2D::DotProduct(Normal1, InwardDir) > 0.0f) ? Normal1 : Normal2;

                    // 벽에서 100cm(1미터) 안쪽으로 들어온 위치를 안전 소환 지점으로 결정
                    FVector2D SpawnPos2D = BestMid + EdgeNormal * 100.0f;
                    LocalSpawnPos = FVector(SpawnPos2D.X, SpawnPos2D.Y, 150.0f);
                    bFoundSafeLocation = true;
                    break;
                }
            }
        }
    }

    // 2. 방 데이터가 없는 경우(Test5 등), 전체 벽면(Half-Edge) 중에서 가장 긴 벽을 찾아 건물 안쪽으로 이동
    if (!bFoundSafeLocation && CachedFloorData.half_edges.Num() > 0)
    {
        FVector2D MinBounds, MaxBounds;
        GetFloorBounds(MinBounds, MaxBounds);
        FVector2D BoundsCenter = (MinBounds + MaxBounds) * 0.5f;

        float MaxLenSq = -1.0f;
        FVector2D BestMid(0.0, 0.0);
        FVector2D BestEdgeDir(0.0, 0.0);

        for (const FTopologyHalfEdge& Edge : CachedFloorData.half_edges)
        {
            if (VertexCache.Contains(Edge.vertex_start) && VertexCache.Contains(Edge.vertex_end))
            {
                FVector2D V0 = VertexCache[Edge.vertex_start];
                FVector2D V1 = VertexCache[Edge.vertex_end];
                float LenSq = FVector2D::DistSquared(V0, V1);
                if (LenSq > MaxLenSq)
                {
                    MaxLenSq = LenSq;
                    BestMid = (V0 + V1) * 0.5f;
                    BestEdgeDir = (V1 - V0).GetSafeNormal();
                }
            }
        }

        if (MaxLenSq > 0.0f)
        {
            // 가장 긴 벽에서 도면 중심점(BoundsCenter)을 향하는 방향을 안쪽(실내)으로 간주
            FVector2D InwardDir = (BoundsCenter - BestMid).GetSafeNormal();
            FVector2D Normal1(-BestEdgeDir.Y, BestEdgeDir.X);
            FVector2D Normal2(BestEdgeDir.Y, -BestEdgeDir.X);
            FVector2D EdgeNormal = (FVector2D::DotProduct(Normal1, InwardDir) > 0.0f) ? Normal1 : Normal2;

            // 벽에서 120cm 안쪽으로 들어온 위치 (외벽 두께 등을 고려하여 120cm로 넉넉히 설정)
            FVector2D SpawnPos2D = BestMid + EdgeNormal * 120.0f;
            LocalSpawnPos = FVector(SpawnPos2D.X, SpawnPos2D.Y, 150.0f);
            bFoundSafeLocation = true;
        }
    }

    // 3. 어떠한 데이터도 사용할 수 없는 최후의 경우 Bounding Box 중심 사용
    if (!bFoundSafeLocation)
    {
        FVector2D MinBounds, MaxBounds;
        GetFloorBounds(MinBounds, MaxBounds);

        if (MinBounds.IsZero() && MaxBounds.IsZero())
        {
            return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
        }

        FVector2D Center2D = (MinBounds + MaxBounds) * 0.5f;
        LocalSpawnPos = FVector(Center2D.X, Center2D.Y, 150.0f);
    }

    // 구해진 Local 좌표를 현재 액터의 Transform(World 공간)으로 변환하여 반환
    return GetOwner() ? GetOwner()->GetTransform().TransformPosition(LocalSpawnPos) : LocalSpawnPos;
}
