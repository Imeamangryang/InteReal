#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"

#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Public/HarnessPipelineManager.h"
#include "Public/HarnessSaveManagerComponent.h"
#include "InteReal/ViewMode/ViewModeManager.h"

#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

UHarnessGeneratorComponent::UHarnessGeneratorComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false; 

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMat(TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
    if (DefaultMat.Succeeded())
    {
        DefaultFallbackMaterial = DefaultMat.Object;
    }
}

// ==============================================================================
// 疫꿸퀣?????밴쉐??筌뤴뫀諭??類ㅼ읅 筌롫뗄?? 鈺곌퀡梨? ??곕???쇱뱽 筌롫뗀?덄뵳?肉???袁⑹읈????볤탢??몃빍??
// ==============================================================================
void UHarnessGeneratorComponent::ClearHarness()
{
    TSet<UActorComponent*> ComponentsToDestroy;
    for (UActorComponent* Comp : SpawnedComponents)
    {
        if (IsValid(Comp))
        {
            ComponentsToDestroy.Add(Comp);
        }
    }

    if (AActor* Owner = GetOwner())
    {
        TArray<UActorComponent*> OwnerComponents;
        Owner->GetComponents(OwnerComponents);
        for (UActorComponent* Comp : OwnerComponents)
        {
            if (IsValid(Comp) && Comp->ComponentTags.Contains(FName(TEXT("HarnessGenerated"))))
            {
                ComponentsToDestroy.Add(Comp);
            }
        }
    }

    for (UActorComponent* Comp : ComponentsToDestroy)
    {
        if (IsValid(Comp))
        {
            Comp->DestroyComponent();
        }
    }

    SpawnedComponents.Reset();
    VertexCache.Reset();
    EdgeCache.Reset();
}

void UHarnessGeneratorComponent::AddGeneratedComponentTags(UActorComponent* Component, const FString& ComponentType, const FString& EntityId, const TArray<FString>& ExtraMetadataTags) const
{
    if (!Component)
    {
        return;
    }

    Component->ComponentTags.AddUnique(FName(TEXT("HarnessGenerated")));

    if (!ComponentType.IsEmpty())
    {
        Component->ComponentTags.AddUnique(FName(*FString::Printf(TEXT("Harness%s"), *ComponentType)));
        Component->ComponentTags.AddUnique(FName(*FString::Printf(TEXT("HarnessComponentType=%s"), *ComponentType)));
    }

    if (!EntityId.IsEmpty())
    {
        Component->ComponentTags.AddUnique(FName(*FString::Printf(TEXT("HarnessEntityId=%s"), *EntityId)));
    }

    for (const FString& Tag : ExtraMetadataTags)
    {
        if (!Tag.IsEmpty())
        {
            Component->ComponentTags.AddUnique(FName(*Tag));
        }
    }
}

void UHarnessGeneratorComponent::RebuildHarnessFromRuntimeData(const FHarnessFloorData& FloorData)
{
    ClearHarness();
    BuildTopologyCaches(FloorData);

    AssembleStructuralWalls(FloorData);
    FabricateDynamicPlanes(FloorData);
    InstallOpeningComponents(FloorData);

    bool bShowCeiling = false;
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AViewModeManager> It(World); It; ++It)
        {
            bShowCeiling = (*It)->GetCurrentViewMode() == EHarnessViewMode::FirstPerson;
            break;
        }
    }
    SetCeilingVisibility(bShowCeiling);
}

void UHarnessGeneratorComponent::SetCeilingVisibility(bool bVisible)
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    TArray<UActorComponent*> CeilingComponents = Owner->GetComponentsByTag(UPrimitiveComponent::StaticClass(), FName(TEXT("Ceiling")));
    for (UActorComponent* Component : CeilingComponents)
    {
        if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
        {
            Primitive->SetVisibility(true, true);
            Primitive->SetHiddenInGame(!bVisible, true);
            Primitive->SetCastShadow(true);
            Primitive->bCastHiddenShadow = true;
            Primitive->SetAffectDistanceFieldLighting(true);
            Primitive->SetVisibleInRayTracing(true);

            // 1인칭 모드에서만 천장을 표시/충돌 대상으로 사용한다.
            // 탑뷰, 편집, 배치 모드에서는 보이지 않는 천장 콜리전이 ECC_Visibility 클릭 트레이스를 막지 않도록 Query collision을 꺼 둔다.
            if (bVisible)
            {
                Primitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                Primitive->SetCollisionResponseToAllChannels(ECR_Block);
                Primitive->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
            }
            else
            {
                Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
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

    CachedFloorData = SourceFloorData;
    RebuildHarnessFromRuntimeData(CachedFloorData);
}

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

FVector UHarnessGeneratorComponent::GetSafeSpawnLocation() const
{
    FVector2D MinBounds;
    FVector2D MaxBounds;
    GetFloorBounds(MinBounds, MaxBounds);

    const FVector2D Center = (MinBounds + MaxBounds) * 0.5f;
    float FloorZ = 0.0f;
    bool bHasFace = false;
    for (const FTopologyFace& Face : CachedFloorData.faces)
    {
        FloorZ = bHasFace ? FMath::Min(FloorZ, Face.z_offset) : Face.z_offset;
        bHasFace = true;
    }

    return FVector(Center.X, Center.Y, FloorZ + 120.0f);
}

// ==============================================================================
// ?諭???닌딅열??筌ｌ뮇???誘れ뵠????륁젟??랁???????源?源딅???덈뼄.
// ==============================================================================
void UHarnessGeneratorComponent::UpdateCeilingHeight(FString FaceId, float NewHeight)
{
    (void)FaceId;
    const float UnifiedHeight = FMath::Max(NewHeight, 1.0f);
    CachedFloorData.common_wall_height_cm = UnifiedHeight;
    SourceFloorData.common_wall_height_cm = UnifiedHeight;

    for (FTopologyFace& Face : CachedFloorData.faces)
    {
        Face.height_cm = UnifiedHeight;
    }

    for (FTopologyFace& Face : SourceFloorData.faces)
    {
        Face.height_cm = UnifiedHeight;
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
