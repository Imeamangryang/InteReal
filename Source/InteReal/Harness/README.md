# Harness Module (Runtime)

## Overview
Creates 3D wiring harness routes from PlanGraph-derived wall data. The generator builds a spline per segment and attaches a spline mesh for rendering.

## Files
- Public/HarnessData.h: data structures and style data asset
- Public/HarnessGeneratorComponent.h: runtime generator component
- Public/HarnessJsonParser.h: JSON loader
- Private/HarnessGeneratorComponent.cpp: spline + mesh creation
- Private/HarnessJsonParser.cpp: JSON parsing
- Tests/HarnessTestActor.*: minimal runner actor

## Quick Use (Editor)
1. Add `AHarnessTestActor` to a level.
2. Set `JsonFilePath` to a valid JSON file (see `floor.json` example format).
3. Assign a `UHarnessStyleDataAsset` with a cable mesh and material.
4. Play the level.

## Notes
- The spline mesh scale assumes the cable mesh is 1cm diameter at scale 1.0.
- The loader expects strict JSON (quotes, colons, valid arrays).
- Supported arrays: `outer_walls`, `inner_walls`, `unknown_walls`, `openings`, `doors`, `windows`.
- `warnings` and `errors` are logged on load in `AHarnessTestActor`.
