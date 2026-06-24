#include "InteReal/Harness/Public/HarnessGeneratorComponent.h"
#include "InteReal/Harness/Public/HarnessWallAssembler.h"

void UHarnessGeneratorComponent::AssembleStructuralWalls(const FHarnessFloorData& FloorData)
{
    // Delegate all wall generation logic to the separated wall assembler
    HarnessWall::FHarnessWallAssembler Assembler(this, FloorData);
    Assembler.Run();
}

namespace HarnessWall
{
    void FHarnessWallAssembler::Run()
    {
        // 1. Merge collinear wall segments
        TArray<FWallRun> WallRuns = BuildMergedWallRuns();

        // 2. Extract wall sides for every room face
        TArray<FWallSurfaceSide> InteriorSurfaceSides;
        for (const FTopologyFace& Face : FloorData.faces)
        {
            InteriorSurfaceSides.Append(BuildMergedFaceSides(Face));
        }

        // 3. Generate structural cores
        GenerateWallCores(WallRuns, InteriorSurfaceSides);

        // 4. Generate decorative surfaces (interior + exterior)
        GenerateWallSurfaces(InteriorSurfaceSides, WallRuns);
    }
}
