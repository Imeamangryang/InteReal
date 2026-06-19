#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/PointLightComponent.h" 
#include "Public/HarnessPipelineManager.h"
#include "Public/HarnessSaveManagerComponent.h"

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
    WallSideMeasurementCache.Reset();
    SurfaceMeasurementCache.Reset();

    bIsSpawning = false;
}

// ==============================================================================
// ?袁ⓦ늺 2D JSON ?怨쀬뵠?怨? ???뼓??뤿연 ?紐꺿봺??3D ?ル슦紐닸?Z-Up)??筌띿쉳苡?筌?Ŋ???몃빍??
// ==============================================================================
void UHarnessGeneratorComponent::BuildTopologyCaches(const FHarnessFloorData& FloorData)
{
    WallSideMeasurementCache = FloorData.wall_side_measurements;
    SurfaceMeasurementCache = FloorData.surface_measurements;

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

    CachedFloorData = FloorData;

    // ?袁ⓦ늺????덉쨮 域밸챶?곫묾??袁る퉸 疫꿸퀣???袁ⓦ늺??筌앸맦而???볤탢??몃빍??
    ClearHarness();
    BuildTopologyCaches(FloorData);

    AssembleStructuralWalls(FloorData);      // 甕곗럩猿???밴쉐 獄?筌≪럥揆 ?닌됱컞 ??る┛(Boolean)
    FabricateDynamicPlanes(FloorData);       // 獄쏅뗀?? 筌ｌ뮇????겹늺(Triangulation) 獄????袁⑹뒭 ?됰뗀以???밴쉐
    InstallOpeningComponents(FloorData);     // ?????닌됱컞????筌≪럥揆 3D ?癒??獄쏄퀣??
    
    if (bEnableInteriorLights)
    {
        InstallInteriorLights(FloorData);    // 揶?獄?餓λ쵐釉?鈺곌퀡梨?獄쏄퀣??
    }

    // 筌뤴뫀諭?筌롫뗄????밴쉐????멸텢??겹늺, Z??疫꿸퀡而????살カ筌△뫁???醫딅빍筌롫뗄???륁뱽 ??뽰삂??몃빍??
    if (AnimatedWalls.Num() > 0)
    {
        bIsSpawning = true;
        WallAnimationProgress = 0.01f;
        SetComponentTickEnabled(true); 
    }
}

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
                    if (Wall->ComponentHasTag(TEXT("EditableWall")) && !Wall->ComponentHasTag(TEXT("WallExterior")))
                    {
                        Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                        Wall->SetCollisionObjectType(ECC_WorldStatic);
                        Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
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
            SetComponentTickEnabled(false); // Tick ?ル굝利?(?源낅뮟 筌ㅼ뮇???
        }
    }
}

// ==============================================================================
// 揶?獄쎻뫗???얜떯苡뜸빳臾믩뼎???④쑴沅??뤿연 ??산땀 鈺곌퀡梨?Point Light)??獄쏄퀣???몃빍??
// ==============================================================================
void UHarnessGeneratorComponent::InstallInteriorLights(const FHarnessFloorData& FloorData)
{
    auto DoesOpeningTargetFace = [&](const FTopologyOpening& Opening, const FTopologyFace& Face) -> bool
    {
        if (!Opening.type.Equals(TEXT("Window"), ESearchCase::IgnoreCase))
        {
            return false;
        }

        const int32 NumVerts = Face.contour_vertex_ids.Num();
        for (int32 i = 0; i < NumVerts; ++i)
        {
            const FString& CurrId = Face.contour_vertex_ids[i];
            const FString& NextId = Face.contour_vertex_ids[(i + 1) % NumVerts];

            for (const FTopologyHalfEdge& Edge : FloorData.half_edges)
            {
                const bool bSameSegment =
                    (Edge.vertex_start == CurrId && Edge.vertex_end == NextId) ||
                    (Edge.vertex_start == NextId && Edge.vertex_end == CurrId);

                if (bSameSegment && (Opening.target_edge_id == Edge.id || Opening.target_edge_id == Edge.twin_id))
                {
                    return true;
                }
            }
        }

        return false;
    };

    for (const FTopologyFace& Face : FloorData.faces)
    {
        if (Face.contour_vertex_ids.Num() < 3) continue;

        bool bHasWindow = false;
        for (const FTopologyOpening& Opening : FloorData.openings)
        {
            if (DoesOpeningTargetFace(Opening, Face))
            {
                bHasWindow = true;
                break;
            }
        }

        if (!bHasWindow)
        {
            continue;
        }

        float DynamicWallHeight = Face.height_cm;

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

        // 筌ｌ뮇??癒?퐣 30cm ?袁⑥삋 筌왖?癒?퓠 鈺곌퀡梨??獄쏄퀣???뤿연 揶쏄쑴??鈺곌퀡梨???ｋ궢???醫딅즲??몃빍??
        FVector LightPos(Centroid.X, Centroid.Y, Face.z_offset + DynamicWallHeight - 30.0f);

        UPointLightComponent* PointLight = NewObject<UPointLightComponent>(GetOwner());
        PointLight->SetMobility(EComponentMobility::Movable);
        PointLight->RegisterComponent();
        PointLight->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        PointLight->SetRelativeLocation(LightPos);
        PointLight->SetIntensity(2500.0f);
        PointLight->SetAttenuationRadius(1000.0f);
        PointLight->SetCastShadows(true);
        PointLight->LightColor = FColor(255, 245, 230); // ?怨뺤몴????ㅺ컶???怨몄뒠
        PointLight->ComponentTags.Add(TEXT("InteriorLight"));

        SpawnedComponents.Add(PointLight);
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
    
    if (UHarnessPipelineManager* Pipeline = GetWorld()->GetSubsystem<UHarnessPipelineManager>())
    {
        if (UHarnessSaveManagerComponent* SaveComp = Pipeline->GetSaveManagerComp())
        {
            FString CurrentState = SaveComp->SaveInteriorState();
            BuildHarness(CachedFloorData);
            SaveComp->LoadInteriorState(CurrentState);
            return;
        }
    }
    
    BuildHarness(CachedFloorData);
}
