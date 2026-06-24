// =============================================================================
//  HarnessWallAssembler_Core.cpp
//  구조 벽 Core(뼈대) 메시 생성.
//
//  역할:
//   - ApplyOpeningsToWall: Boolean Subtract로 문/창 구멍 뚫기
//   - BuildWallBox: 단일 Core 박스 UDynamicMeshComponent 생성
//   - GenerateWallCores: 모든 WallRun에 대해 Core 생성 (엔트리포인트)
// =============================================================================

#include "InteReal/Harness/Public/HarnessWallAssembler.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"

namespace HarnessWall
{

// -----------------------------------------------------------------------------
// ApplyOpeningsToWall — punches door/window holes from a wall core mesh
// -----------------------------------------------------------------------------
void FHarnessWallAssembler::ApplyOpeningsToWall(
    UDynamicMesh* DynMesh,
    const TArray<FMergedOpening>& Openings,
    float WallDepth,
    float BottomZ) const
{
    FGeometryScriptPrimitiveOptions PrimOptions;

    for (const FMergedOpening& MergedOpening : Openings)
    {
        const FTopologyOpening* Opening = MergedOpening.Opening;
        if (!Opening)
        {
            continue;
        }

        UDynamicMesh* HoleMesh = NewObject<UDynamicMesh>();
        const float HoleWidth  = FMath::Max(MergedOpening.WidthCm + 2.0f, 1.0f);
        float HoleBottom = Opening->z_offset_cm;
        float HoleTop    = Opening->z_offset_cm + FMath::Max(Opening->height_cm, 1.0f);

        if (HoleBottom <= 0.1f)
        {
            HoleBottom = BottomZ - 1.0f;
        }
        HoleTop += 0.5f;

        const float HoleHeight  = FMath::Max(HoleTop - HoleBottom, 1.0f);
        const float HoleZCenter = (HoleBottom + HoleTop) * 0.5f;
        const float HoleDepth   = WallDepth + 6.0f;

        FTransform HoleTransform(FRotator::ZeroRotator, FVector(MergedOpening.CenterX, 0.0f, HoleZCenter), FVector::OneVector);
        FBox HoleBox(
            FVector(-HoleWidth * 0.5f, -HoleDepth * 0.5f, -HoleHeight * 0.5f),
            FVector( HoleWidth * 0.5f,  HoleDepth * 0.5f,  HoleHeight * 0.5f)
        );

        UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBoundingBox(HoleMesh, PrimOptions, HoleTransform, HoleBox, 0, 0, 0);

        FGeometryScriptMeshBooleanOptions BoolOptions;
        BoolOptions.bAllowEmptyResult = true;
        UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
            DynMesh, FTransform::Identity, HoleMesh, FTransform::Identity,
            EGeometryScriptBooleanOperation::Subtract, BoolOptions
        );
    }
}

// -----------------------------------------------------------------------------
// BuildWallBox — creates one UDynamicMeshComponent for a single wall core box
// -----------------------------------------------------------------------------
void FHarnessWallAssembler::BuildWallBox(
    const FVector2D& Center2D,
    float Angle,
    float Length,
    float Depth,
    float BottomZ,
    float TopZ,
    const TArray<FMergedOpening>& Openings,
    const TArray<FName>& Tags,
    bool bEditable)
{
    UDynamicMeshComponent* WallComp = NewObject<UDynamicMeshComponent>(Generator->GetOwner());
    if (!WallComp)
    {
        return;
    }

    WallComp->SetMobility(EComponentMobility::Movable);
    WallComp->RegisterComponent();
    WallComp->AttachToComponent(Generator->GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

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
    const float ClampedDepth  = FMath::Max(Depth,  0.1f);
    const float Height        = FMath::Max(TopZ - BottomZ, 1.0f);
    const float ZCenter       = (BottomZ + TopZ) * 0.5f;

    FGeometryScriptPrimitiveOptions PrimOptions;
    FTransform BaseTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, ZCenter), FVector::OneVector);
    FBox WallBox(
        FVector(-ClampedLength * 0.5f, -ClampedDepth * 0.5f, -Height * 0.5f),
        FVector( ClampedLength * 0.5f,  ClampedDepth * 0.5f,  Height * 0.5f)
    );
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBoundingBox(DynMesh, PrimOptions, BaseTransform, WallBox, 0, 0, 0);

    ApplyOpeningsToWall(DynMesh, Openings, ClampedDepth, BottomZ);

    if (Generator->DefaultFallbackMaterial)
    {
        WallComp->SetMaterial(0, Generator->DefaultFallbackMaterial);
    }

    WallComp->SetComplexAsSimpleCollisionEnabled(true, true);
    WallComp->SetCollisionProfileName(TEXT("NoCollision"));
    WallComp->bCastShadowAsTwoSided = true;
    WallComp->NotifyMeshUpdated();

    Generator->SpawnedComponents.Add(WallComp);
    Generator->AnimatedWalls.Add(WallComp);
}

// -----------------------------------------------------------------------------
// GenerateWallCores — Pass 1: builds one structural core per WallRun
// -----------------------------------------------------------------------------
void FHarnessWallAssembler::GenerateWallCores(
    TArray<FWallRun>& WallRuns,
    const TArray<FWallSurfaceSide>& InteriorSides)
{
    for (FWallRun& Run : WallRuns)
    {
        if (Run.Edges.Num() == 0 || Run.Length <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        // Determine Z extents from adjacent faces
        float CoreBottomZ     = 0.0f;
        float CoreTopZ        = 0.0f;
        bool bHasAdjacentFace = false;

        for (const FTopologyHalfEdge* Edge : Run.Edges)
        {
            if (!Edge) { continue; }

            for (const FTopologyFace& Face : FloorData.faces)
            {
                FWallSurfaceSide Side;
                if (FindFaceSide(*Edge, Face, Side))
                {
                    const float FaceBottom = Face.z_offset - HarnessFloorSlabThicknessCm - HarnessCoreZSealCm;
                    // Use wall_height if it's larger than the face's height
                    const float EdgeWallHeight = Edge->wall_height > 0.0f ? Edge->wall_height : Face.height_cm;
                    const float FaceTop    = Face.z_offset + FMath::Max(Face.height_cm, EdgeWallHeight) + HarnessCoreZSealCm;

                    if (!bHasAdjacentFace)
                    {
                        CoreBottomZ = FaceBottom;
                        CoreTopZ = FaceTop;
                    }
                    else
                    {
                        CoreBottomZ = FMath::Min(CoreBottomZ, FaceBottom);
                        CoreTopZ    = FMath::Max(CoreTopZ, FaceTop);
                    }
                    bHasAdjacentFace = true;
                }
            }

            // Also consider Edge->wall_height even if no adjacent face is found
            if (!bHasAdjacentFace && Edge->wall_height > 0.0f)
            {
                CoreBottomZ = -HarnessFloorSlabThicknessCm - HarnessCoreZSealCm;
                CoreTopZ = FMath::Max(CoreTopZ, Edge->wall_height + HarnessCoreZSealCm);
                bHasAdjacentFace = true; // Use this as valid fallback
            }
        }

        if (!bHasAdjacentFace)
        {
            CoreBottomZ = -HarnessFloorSlabThicknessCm - HarnessCoreZSealCm;
            CoreTopZ    = HarnessDefaultWallHeightCm  + HarnessCoreZSealCm;
        }

        const FVector2D CoreCenter = (Run.Start + Run.End) * 0.5f;
        const float CoreLength = (Run.End - Run.Start).Size();
        if (CoreLength <= 1.0f)
        {
            continue;
        }

        Run.CoreBottomZ = CoreBottomZ;
        Run.CoreTopZ    = CoreTopZ;

        // Shift opening centers from run-center to core-center coordinates
        const float OpeningCenterShift = FVector2D::DotProduct(Run.Center - CoreCenter, Run.Direction);
        TArray<FMergedOpening> CoreOpenings = Run.Openings;
        for (FMergedOpening& Opening : CoreOpenings)
        {
            Opening.CenterX += OpeningCenterShift;
        }

        // One non-selectable structural core per run
        BuildWallBox(
            CoreCenter,
            Run.Angle,
            CoreLength,
            Run.WallThickness,
            CoreBottomZ,
            CoreTopZ,
            CoreOpenings,
            {
                FName(TEXT("WallCore")),
                FName(FString::Printf(TEXT("WallCore_%s"), *Run.RunId))
            },
            false   // WallCore is not selectable — material editing done on WallSurface
        );
    }
}

} // namespace HarnessWall
