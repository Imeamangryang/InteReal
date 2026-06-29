# Harness Step 15 - Generated Component Metadata Tags

Base: Step 14 v3.2-only contract guard + opening kind normalize.

This step does not change v3.2 span opening / measurement / host-wall behavior.

Changes:
- Adds a shared `AddGeneratedComponentTags()` helper.
- Adds stable metadata tags to generated components:
  - `HarnessGenerated`
  - `HarnessComponentType=<Type>`
  - `HarnessEntityId=<Id>`
  - `HarnessWallId=<Id>` / `HarnessSpaceId=<Id>` where applicable
  - `HarnessWallRole=<Role>` / `HarnessSurfaceRole=<Role>` where applicable
  - `HarnessOpeningId=<Id>` / `HarnessOpeningType=<Type>` / `HarnessOpeningConnectionType=<Type>` for wall cores with openings
- Tags generated floor, ceiling, ceiling shadow, wall core, and wall surface components.
- `ClearHarness()` now also destroys stale owner components tagged `HarnessGenerated`, not only components still present in `SpawnedComponents`.

Expected examples:
- Floor component: `HarnessGenerated`, `HarnessFloor`, `HarnessSpaceId=space_room_xxx`
- Ceiling component: `HarnessGenerated`, `HarnessCeiling`, `HarnessSpaceId=space_room_xxx`
- Wall core component: `HarnessGenerated`, `HarnessWall`, `HarnessWallId=<run_id>`, `HarnessOpeningId=<opening_id>`
- Wall surface component: `HarnessGenerated`, `HarnessWallSurface`, `HarnessSpaceId=<space_id>`

## Step16: Space polygon validation

This build adds defensive validation for v3.2 `spaces[].boundary` polygons before floor, ceiling, and ceiling-shadow meshes are generated.

- Adds `bValidateSpacePolygons`, `MinSpacePolygonAreaCm2`, and `MinSpacePolygonEdgeLengthCm` options on `UHarnessGeneratorComponent`.
- Logs raw duplicate/short edges before cleanup.
- Logs duplicate removal, collinear point removal, short edges, duplicate pairs, self-intersections, and very small polygon area after cleanup.
- Skips floor/ceiling generation when triangulation fails instead of creating broken meshes.
- Does not change opening, door, window, span, measurement, or Boolean-cut behavior.


## Step17 - v3.2 Optional Groups Import Guard
- Parses optional `wall_groups`, `finish_groups`, and `asset_requirements` arrays without changing generation behavior.
- Empty arrays are accepted and logged as valid v3.2 placeholders.
- Group entries with missing `id` are skipped with warnings instead of failing the whole import.
- Generation still treats walls/spaces/openings as the authoritative geometry source until server group payloads become populated.
