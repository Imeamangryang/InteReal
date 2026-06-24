// =============================================================================
//  HarnessWallAssembler_Surface.cpp
//  방의 인테리어 벽면(Wall Surface) 메시 생성.
//
//  역할:
//   - BuildWallSurfacePanels: 단일 WallSurfaceSide에 대한 표면 패널 메시 생성 (Boolean Subtract 포함)
//   - GenerateWallSurfaces: 모든 WallSurfaceSide에 대해 Surface 패널 생성 (엔트리포인트)
// =============================================================================

#include "InteReal/Harness/Public/HarnessWallAssembler.h"
#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "DynamicMesh/MeshNormals.h"

namespace HarnessWall
{

// -----------------------------------------------------------------------------
// BuildWallSurfacePanels — builds one decorative surface panel per side
// -----------------------------------------------------------------------------
void FHarnessWallAssembler::BuildWallSurfacePanels(
    const FWallSurfaceSide& Side,
    const FVector2D& SurfaceNormal,
    float WallThickness,
    float ZMin,
    float ZMax,
    const TArray<FName>& Tags)
{
    if (Side.Direction.IsNearlyZero() || SurfaceNormal.IsNearlyZero())
    {
        return;
    }

    if (Side.Length <= 1.0f)
    {
        return;
    }

    if (ZMax - ZMin <= 1.0f)
    {
        return;
    }

    const float OffsetDistance = (WallThickness * 0.5f) + HarnessSurfaceGapCm;
    const FVector2D SurfaceCenter = Side.Center + (SurfaceNormal * OffsetDistance);

    UDynamicMeshComponent* SurfaceComp = NewObject<UDynamicMeshComponent>(Generator->GetOwner());
    if (!SurfaceComp)
    {
        return;
    }

    SurfaceComp->SetMobility(EComponentMobility::Movable);
    SurfaceComp->RegisterComponent();
    SurfaceComp->AttachToComponent(Generator->GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

    SurfaceComp->ComponentTags.Add(TEXT("EditableWall"));
    SurfaceComp->ComponentTags.Add(TEXT("Wall"));
    for (const FName& Tag : Tags)
    {
        SurfaceComp->ComponentTags.AddUnique(Tag);
    }

    SurfaceComp->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.01f));
    SurfaceComp->SetRelativeLocationAndRotation(FVector(SurfaceCenter.X, SurfaceCenter.Y, 0.0f), FRotator(0.0f, Side.Angle, 0.0f));

    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(SurfaceComp);
    SurfaceComp->SetDynamicMesh(DynMesh);

    // --- 1. Build thin solid slab ---
    constexpr float SurfaceSlabDepth = 0.5f;
    const float Height  = ZMax - ZMin;
    const float ZCenter = (ZMin + ZMax) * 0.5f;

    FGeometryScriptPrimitiveOptions PrimOptions;
    FBox SurfaceBox(
        FVector(-Side.Length * 0.5f, -SurfaceSlabDepth * 0.5f, -Height * 0.5f),
        FVector( Side.Length * 0.5f,  SurfaceSlabDepth * 0.5f,  Height * 0.5f)
    );
    FTransform BaseTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, ZCenter), FVector::OneVector);
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBoundingBox(DynMesh, PrimOptions, BaseTransform, SurfaceBox, 0, 0, 0);

    // --- 2. Subtract openings ---
    for (const FMergedOpening& MergedOpening : Side.Openings)
    {
        const FTopologyOpening* Opening = MergedOpening.Opening;
        if (!Opening) { continue; }

        const float HoleWidth  = FMath::Max(MergedOpening.WidthCm + 2.0f, 1.0f);
        float HoleBottom = Opening->z_offset_cm;
        float HoleTop    = Opening->z_offset_cm + FMath::Max(Opening->height_cm, 1.0f);

        if (HoleBottom <= 0.1f)
        {
            HoleBottom = ZMin - 1.0f;
        }
        HoleTop += 0.5f;

        const float HoleHeight  = FMath::Max(HoleTop - HoleBottom, 1.0f);
        const float HoleZCenter = (HoleBottom + HoleTop) * 0.5f;
        const float HoleDepth   = SurfaceSlabDepth + 6.0f;

        UDynamicMesh* HoleMesh = NewObject<UDynamicMesh>();
        FBox HoleBox(
            FVector(-HoleWidth * 0.5f, -HoleDepth * 0.5f, -HoleHeight * 0.5f),
            FVector( HoleWidth * 0.5f,  HoleDepth * 0.5f,  HoleHeight * 0.5f)
        );
        FTransform HoleTransform(FRotator::ZeroRotator, FVector(MergedOpening.CenterX, 0.0f, HoleZCenter), FVector::OneVector);
        UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBoundingBox(HoleMesh, PrimOptions, HoleTransform, HoleBox, 0, 0, 0);

        FGeometryScriptMeshBooleanOptions BoolOptions;
        BoolOptions.bAllowEmptyResult = true;
        UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
            DynMesh,  FTransform::Identity,
            HoleMesh, FTransform::Identity,
            EGeometryScriptBooleanOperation::Subtract, BoolOptions
        );
    }

    // --- 3. Regenerate UV and Normals ---
    DynMesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
    {
        if (!EditMesh.HasAttributes())
        {
            EditMesh.EnableAttributes();
        }

        UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = EditMesh.Attributes()->PrimaryUV();
        if (UVOverlay)
        {
            for (int32 TID : EditMesh.TriangleIndicesItr())
            {
                const UE::Geometry::FIndex3i Tri = EditMesh.GetTriangle(TID);
                const FVector3d V0 = EditMesh.GetVertex(Tri.A);
                const FVector3d V1 = EditMesh.GetVertex(Tri.B);
                const FVector3d V2 = EditMesh.GetVertex(Tri.C);

                const int32 UV0 = UVOverlay->AppendElement(FVector2f(V0.X / 100.0f, V0.Z / 100.0f));
                const int32 UV1 = UVOverlay->AppendElement(FVector2f(V1.X / 100.0f, V1.Z / 100.0f));
                const int32 UV2 = UVOverlay->AppendElement(FVector2f(V2.X / 100.0f, V2.Z / 100.0f));
                UVOverlay->SetTriangle(TID, UE::Geometry::FIndex3i(UV0, UV1, UV2));
            }
        }

        if (EditMesh.Attributes()->PrimaryNormals())
        {
            UE::Geometry::FMeshNormals::InitializeOverlayToPerVertexNormals(EditMesh.Attributes()->PrimaryNormals(), false);
        }
    }, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

    if (Generator->DefaultFallbackMaterial)
    {
        SurfaceComp->SetMaterial(0, Generator->DefaultFallbackMaterial);
    }

    SurfaceComp->SetComplexAsSimpleCollisionEnabled(true, true);
    SurfaceComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SurfaceComp->SetCollisionObjectType(ECC_WorldStatic);
    SurfaceComp->SetCollisionResponseToAllChannels(ECR_Block);
    SurfaceComp->SetCollisionResponseToChannel(ECC_Pawn,              ECR_Block);
    SurfaceComp->SetCollisionResponseToChannel(ECC_Camera,            ECR_Block);
    SurfaceComp->SetCollisionResponseToChannel(ECC_Visibility,        ECR_Block);
    SurfaceComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    SurfaceComp->bCastShadowAsTwoSided = true;
    SurfaceComp->NotifyMeshUpdated();

    Generator->SpawnedComponents.Add(SurfaceComp);
    Generator->AnimatedWalls.Add(SurfaceComp);
}

// -----------------------------------------------------------------------------
// GenerateWallSurfaces — Pass 2: build one surface per (room, face-side)
// -----------------------------------------------------------------------------
void FHarnessWallAssembler::GenerateWallSurfaces(
    const TArray<FWallSurfaceSide>& InteriorSides,
    const TArray<FWallRun>& WallRuns)
{
    // Build edge-to-run map to find core heights
    TMap<FString, const FWallRun*> EdgeToRunMap;
    for (const FWallRun& Run : WallRuns)
    {
        for (const FTopologyHalfEdge* Edge : Run.Edges)
        {
            if (Edge)
            {
                EdgeToRunMap.Add(Edge->id, &Run);
            }
        }
    }

    // Determine which core edges are shared between two rooms
    TMap<FString, int32> CoreEdgeFaceUseCounts;
    TSet<FString> CountedCoreFacePairs;
    for (const FWallSurfaceSide& Side : InteriorSides)
    {
        for (const FString& CoreEdgeId : Side.CoreEdgeIds)
        {
            const FString CoreFaceKey = FString::Printf(TEXT("%s|%s"), *CoreEdgeId, *Side.FaceId);
            if (CountedCoreFacePairs.Contains(CoreFaceKey))
            {
                continue;
            }
            CountedCoreFacePairs.Add(CoreFaceKey);
            CoreEdgeFaceUseCounts.FindOrAdd(CoreEdgeId)++;
        }
    }

    // Flag sides that touch another room
    TArray<FWallSurfaceSide> ModifiableSides = InteriorSides;
    for (FWallSurfaceSide& Side : ModifiableSides)
    {
        for (const FString& CoreEdgeId : Side.CoreEdgeIds)
        {
            if (const int32* UseCount = CoreEdgeFaceUseCounts.Find(CoreEdgeId))
            {
                if (*UseCount > 1)
                {
                    Side.bTouchesAnotherRoom = true;
                    break;
                }
            }
        }
    }

    for (const FWallSurfaceSide& Side : ModifiableSides)
    {
        const FString SurfaceId = FString::Printf(TEXT("WallSurface_%s_%s"), *Side.FaceId, *Side.SurfaceEdgeId);

        // Find the min/max Z of all cores this side belongs to
        float SurfaceZMin = Side.ZOffset - HarnessFloorSlabThicknessCm - HarnessCoreZSealCm;
        float SurfaceZMax = Side.ZOffset + FMath::Max(Side.Height, 1.0f) + HarnessCoreZSealCm;

        for (const FString& CoreEdgeId : Side.CoreEdgeIds)
        {
            if (const FWallRun** RunPtr = EdgeToRunMap.Find(CoreEdgeId))
            {
                SurfaceZMin = FMath::Min(SurfaceZMin, (*RunPtr)->CoreBottomZ);
                SurfaceZMax = FMath::Max(SurfaceZMax, (*RunPtr)->CoreTopZ);
            }
        }

        // Interior wall surface
        BuildWallSurfacePanels(
            Side,
            Side.InteriorNormal,
            Side.WallThickness,
            SurfaceZMin,
            SurfaceZMax,
            {
                FName(TEXT("WallSurface")),
                FName(FString::Printf(TEXT("RoomFace_%s"), *Side.FaceId)),
                FName(*SurfaceId)
            }
        );

        // Exterior wall surface (if applicable)
        if (Side.bHasOuterWall && !Side.bTouchesAnotherRoom)
        {
            const FVector2D ExteriorNormal = -Side.InteriorNormal;
            const FString ExteriorSurfaceId = FString::Printf(TEXT("WallExterior_%s"), *Side.SurfaceEdgeId);

            BuildWallSurfacePanels(
                Side,
                ExteriorNormal,
                Side.WallThickness,
                SurfaceZMin,
                SurfaceZMax,
                {
                    FName(TEXT("WallExterior")),
                    FName(*ExteriorSurfaceId)
                }
            );
        }
    }
}

} // namespace HarnessWall
