#pragma once

#include "CoreMinimal.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "HarnessData.h"
#include "HarnessGeneratorGeometry.h"

// Forward declarations
class UHarnessGeneratorComponent;
class UMaterialInterface;

// ============================================================================
//  Data types shared across wall generation modules
// ============================================================================

namespace HarnessWall
{
    using namespace InteReal::HarnessGenerator;

    // One opening (door / window) projected onto a wall run or face side.
    struct FMergedOpening
    {
        const FTopologyOpening* Opening = nullptr;
        float CenterX  = 0.0f;  // cm, relative to the run / face-side center
        float WidthCm  = 0.0f;
    };

    // One contiguous wall side seen from inside a specific room face.
    // One component is generated per FWallSurfaceSide instance.
    struct FWallSurfaceSide
    {
        FString FaceId;
        FString SurfaceEdgeId;
        TArray<FString> SourceSurfaceEdgeIds;
        TArray<FString> CoreEdgeIds;
        TSet<FString>   OpeningEdgeIds;
        TArray<FMergedOpening> Openings;

        bool bHasOuterWall     = false;
        bool bTouchesAnotherRoom = false;

        float Height       = HarnessDefaultWallHeightCm;
        float ZOffset      = 0.0f;
        float Length       = 0.0f;
        float Angle        = 0.0f;
        float WallThickness = HarnessDefaultWallThicknessCm;

        FVector2D Center        = FVector2D::ZeroVector;
        FVector2D Direction     = FVector2D::ZeroVector;
        FVector2D InteriorNormal = FVector2D::ZeroVector;
    };

    // A merged group of collinear half-edges that share the same wall plane.
    struct FWallRun
    {
        FString RunId;
        TArray<const FTopologyHalfEdge*> Edges;
        TSet<FString>          OpeningEdgeIds;
        TArray<FMergedOpening> Openings;

        FVector2D Start     = FVector2D::ZeroVector;
        FVector2D End       = FVector2D::ZeroVector;
        FVector2D Center    = FVector2D::ZeroVector;
        FVector2D Direction = FVector2D::ZeroVector;

        float Length       = 0.0f;
        float Angle        = 0.0f;
        float WallThickness = HarnessDefaultWallThicknessCm;

        float CoreBottomZ = 0.0f;
        float CoreTopZ    = 0.0f;
    };

    // Overlap of a core half-edge against one face-contour segment.
    struct FFaceSegmentEdgeMatch
    {
        const FTopologyHalfEdge* Edge = nullptr;
        FString SurfaceEdgeId;
        float StartT       = 0.0f;
        float EndT         = 0.0f;
        float OverlapLength = 0.0f;
    };

    // ========================================================================
    //  FHarnessWallAssembler
    //  Builds all wall geometry for one floor.
    //  Constructed inside AssembleStructuralWalls(); split into multiple .cpp
    //  files so each logical concern lives in its own translation unit.
    // ========================================================================
    class FHarnessWallAssembler
    {
    public:
        FHarnessWallAssembler(UHarnessGeneratorComponent* InGenerator, const FHarnessFloorData& InFloorData);

        // Main entry point called by AssembleStructuralWalls
        void Run();

    private:
        // ----------------------------------------------------------------
        // Context (owned by the outer UHarnessGeneratorComponent)
        // ----------------------------------------------------------------
        UHarnessGeneratorComponent* Generator = nullptr;
        const FHarnessFloorData&    FloorData;

        // Pre-built lookup maps (filled in the constructor)
        TMap<FString, const FTopologyHalfEdge*> HalfEdgeById;
        TMap<FString, FString> CoreEdgeIdByAnyEdgeId;

        // Wall-to-face mapping built from boundary_wall_ids
        TMap<FString, TArray<const FTopologyFace*>> WallToFacesMap;

        // ----------------------------------------------------------------
        // Convenience accessors into Generator's public/private members
        // (implementations in HarnessWallAssembler_EdgeUtils.cpp)
        // ----------------------------------------------------------------
        bool BuildFacePoints(const FTopologyFace& Face, TArray<FVector2D>& OutPoints) const;
        bool GetEdgePoints(const FTopologyHalfEdge& Edge, FVector2D& OutStart, FVector2D& OutEnd) const;
        void AddOpeningEdgeIds(const FTopologyHalfEdge& Edge, TSet<FString>& OutEdgeIds) const;
        bool FindFaceSide(const FTopologyHalfEdge& Edge, const FTopologyFace& Face, FWallSurfaceSide& OutSide) const;

        // ----------------------------------------------------------------
        // Opening query helpers (HarnessWallAssembler_EdgeUtils.cpp)
        // ----------------------------------------------------------------
        TArray<FMergedOpening> BuildOpeningsForRun(
            const TSet<FString>& EdgeIds,
            const FVector2D& RunCenter,
            const FVector2D& RunDirection) const;

        bool FindEdgesOverlappingFaceSegment(
            const FVector2D& SegmentStart,
            const FVector2D& SegmentEnd,
            TArray<FFaceSegmentEdgeMatch>& OutMatches) const;

        // ----------------------------------------------------------------
        // Wall run merging (HarnessWallAssembler_Runs.cpp)
        // ----------------------------------------------------------------
        TArray<FWallRun> BuildMergedWallRuns() const;

        // ----------------------------------------------------------------
        // Face side building & merging (HarnessWallAssembler_FaceSides.cpp)
        // ----------------------------------------------------------------
        static FString MakeMergedSurfaceEdgeId(const TArray<FString>& SourceIds);
        static void MergeSurfaceSideInto(FWallSurfaceSide& Target, const FWallSurfaceSide& Source);
        static bool CanMergeSurfaceSides(const FWallSurfaceSide& A, const FWallSurfaceSide& B);
        TArray<FWallSurfaceSide> BuildMergedFaceSides(const FTopologyFace& Face) const;

        // ----------------------------------------------------------------
        // Structural core mesh (HarnessWallAssembler_Core.cpp)
        // ----------------------------------------------------------------
        void ApplyOpeningsToWall(
            UDynamicMesh* DynMesh,
            const TArray<FMergedOpening>& Openings,
            float WallDepth,
            float BottomZ) const;

        void BuildWallBox(
            const FVector2D& Center2D,
            float Angle,
            float Length,
            float Depth,
            float BottomZ,
            float TopZ,
            const TArray<FMergedOpening>& Openings,
            const TArray<FName>& Tags,
            bool bEditable);

        void GenerateWallCores(
            TArray<FWallRun>& WallRuns,
            const TArray<FWallSurfaceSide>& InteriorSides);

        // ----------------------------------------------------------------
        // Decorative surface panels (HarnessWallAssembler_Surface.cpp)
        // ----------------------------------------------------------------
        void BuildWallSurfacePanels(
            const FWallSurfaceSide& Side,
            const FVector2D& SurfaceNormal,
            float WallThickness,
            float ZMin,
            float ZMax,
            const TArray<FName>& Tags);

        void GenerateWallSurfaces(
            const TArray<FWallSurfaceSide>& InteriorSides,
            const TArray<FWallRun>& WallRuns);
    };

} // namespace HarnessWall
