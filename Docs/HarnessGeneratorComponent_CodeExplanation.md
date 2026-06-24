# HarnessGeneratorComponent 코드 설명

이 문서는 아래 3개 파일의 역할과 실행 흐름을 정리한다.

- `Source/InteReal/Harness/Private/HarnessGeneratorComponent.cpp`
- `Source/InteReal/Harness/Private/HarnessGeneratorComponent_Walls.cpp`
- `Source/InteReal/Harness/Private/HarnessGeneratorComponent_Planes.cpp`

## 전체 구조

`UHarnessGeneratorComponent`는 JSON에서 파싱된 `FHarnessFloorData`를 기반으로 3D 도면을 생성하는 컴포넌트다. 생성 대상은 크게 벽, 바닥, 천장, 조명으로 나뉜다.

핵심 진입점은 `BuildHarness()`다. 외부에서 floor data를 넘기면 원본 데이터를 `SourceFloorData`에 저장하고, 현재 스케일 설정을 반영한 뒤 `RebuildHarnessFromRuntimeData()`를 통해 실제 3D 컴포넌트를 다시 만든다.

전체 생성 순서는 다음과 같다.

1. `BuildHarness(FloorData)`
2. `RebuildHarnessWithCurrentScale()`
3. `CalculateEffectivePlanScale()`
4. `MakeRuntimeFloorData()`
5. `RebuildHarnessFromRuntimeData()`
6. `BuildTopologyCaches()`
7. `AssembleStructuralWalls()`
8. `FabricateDynamicPlanes()`
9. `InstallOpeningComponents()`
10. 필요 시 `InstallInteriorLights()`

생성된 컴포넌트는 `SpawnedComponents`에 저장된다. 도면을 다시 생성할 때 `ClearHarness()`가 이 배열을 순회하며 기존 컴포넌트를 삭제한다.

## HarnessGeneratorComponent.cpp

이 파일은 생성 파이프라인의 진입점, 스케일 처리, 캐시 관리, 벽 애니메이션, 조명, 천장 높이 변경을 담당한다.

### 스케일 계산

`CalculateEffectivePlanScale()`은 최종 도면 스케일을 계산한다.

기본 스케일은 다음 두 값을 곱한 결과다.

```cpp
EditorPlanScale * OverallPlanScale
```

`bAutoScaleFromDoorWidth`가 켜져 있으면 문 너비를 기준으로 추가 보정한다. JSON에 들어온 문 중 `DoorReferenceWidthCm`보다 작은 문 너비를 모아 중앙값을 구하고, 기준 문 너비와 비교해 전체 스케일을 곱한다.

예를 들어 기준 문 너비가 90cm인데 JSON 문 너비 중앙값이 45cm라면 최종 스케일은 2배가 된다.

### Runtime 데이터 생성

`MakeRuntimeFloorData()`는 원본 `FHarnessFloorData`를 복사한 뒤 스케일을 적용한 런타임 데이터를 만든다.

스케일이 적용되는 값은 다음과 같다.

- `vertices.x`, `vertices.y`
- `half_edges.wall_thickness`
- `wall_side_measurements.length_cm`
- `surface_measurements`의 거리, 길이, 시작점, 끝점
- `openings.width_cm`

주의할 점은 원본 `SourceFloorData`를 직접 바꾸지 않고, `CachedFloorData`에 스케일 적용 결과를 저장한다는 것이다. 그래서 에디터 스케일을 바꿔도 원본 JSON 기준 데이터는 유지된다.

### 재생성 흐름

`RebuildHarnessFromRuntimeData()`는 실제 도면을 다시 만드는 내부 함수다.

```cpp
ClearHarness();
BuildTopologyCaches(FloorData);
AssembleStructuralWalls(FloorData);
FabricateDynamicPlanes(FloorData);
InstallOpeningComponents(FloorData);
```

`BuildTopologyCaches()`는 이후 벽/바닥 생성에서 빠르게 참조할 수 있도록 다음 캐시를 만든다.

- `VertexCache`: vertex id -> 2D 좌표
- `EdgeCache`: core half-edge id -> half-edge
- `WallSideMeasurementCache`
- `SurfaceMeasurementCache`

좌표 변환은 `FloorData.ToHarnessPoint()`를 통해 수행된다. 이 함수는 JSON 좌표계 메타데이터에 따라 Unreal 좌표계에 맞는 2D 좌표를 반환한다.

### 벽 생성 애니메이션과 충돌

벽 컴포넌트는 생성 직후 `AnimatedWalls`에도 추가된다. `RebuildHarnessFromRuntimeData()`에서 `AnimatedWalls`가 비어 있지 않으면 Tick을 켜고, `TickComponent()`에서 벽의 Z 스케일을 0.01에서 1.0까지 올린다.

애니메이션이 끝나면 벽 충돌을 최종 상태로 바꾼다.

`EditableWall` 태그가 있는 벽은 다음과 같이 설정된다.

- `QueryAndPhysics`
- `WorldStatic`
- 전체 채널 Block
- `ECC_Pawn` Block
- `ECC_Camera` Block
- `ECC_GameTraceChannel1` Block
- `ECC_Visibility` Block

이 설정 때문에 캐릭터는 벽을 통과하지 못하고, 동시에 벽 선택용 라인 트레이스도 정상 동작한다.

### 조명 생성

`InstallInteriorLights()`는 창문이 있는 방에만 포인트 라이트를 생성한다. 방 contour의 vertex 좌표를 평균내 중심점을 구하고, 천장보다 30cm 낮은 위치에 라이트를 둔다.

라이트는 `SpawnedComponents`에 들어가므로 도면 재생성 시 같이 제거된다.

### 도면 bounds

`GetFloorBounds()`는 `CachedFloorData.vertices`를 순회해 전체 도면의 최소/최대 좌표를 계산한다. 카메라, 미니맵, 뷰 모드에서 도면 전체 크기를 맞출 때 사용된다.

### 천장 높이 변경

`UpdateCeilingHeight()`는 특정 `FaceId`의 `height_cm`을 바꾸고 도면을 재생성한다. `CachedFloorData`와 `SourceFloorData`를 둘 다 갱신한다.

`UHarnessSaveManagerComponent`가 있으면 현재 배치 상태를 저장한 뒤 도면을 재생성하고 다시 로드한다. 즉, 천장 높이 변경 후에도 배치된 가구/재질 상태를 최대한 유지하려는 구조다.

## HarnessGeneratorComponent_Walls.cpp

이 파일은 `AssembleStructuralWalls()` 하나에 대부분의 벽 생성 로직이 들어 있다. 역할은 JSON half-edge와 face 정보를 이용해 실제 3D 벽 core, 내부 벽면, 외부 벽면을 만드는 것이다.

### 내부 구조체

`FMergedOpening`

- 벽에 뚫을 문/창 정보를 저장한다.
- `Opening`: 원본 `FTopologyOpening`
- `CenterX`: 벽 로컬 X축 기준 opening 중심 위치
- `WidthCm`: opening 폭

`FWallSurfaceSide`

- 특정 방의 한 벽면 surface를 표현한다.
- 방 내부에서 보이는 벽면, 외벽 쪽 surface, opening, 높이, 두께, normal 등을 포함한다.
- 벽지/재질 적용 대상이 되는 표면 메쉬 생성에 사용된다.

`FWallRun`

- 여러 half-edge를 같은 직선 벽 run으로 병합한 구조다.
- WallCore 생성에 사용된다.
- 최근 구조에서는 하나의 WallCore를 좌/우 반쪽으로 나누어 생성한다.

`FFaceSegmentEdgeMatch`

- 방 contour segment와 실제 half-edge가 겹치는 구간을 찾을 때 쓰는 임시 구조체다.

### 주요 helper 람다

`BuildFacePoints()`

- face의 `contour_vertex_ids`를 `VertexCache`에서 찾아 `FVector2D` 배열로 만든다.

`GetEdgePoints()`

- half-edge의 시작/끝 vertex id를 실제 2D 좌표로 변환한다.

`AddOpeningEdgeIds()`

- opening이 어느 edge에 붙어 있는지 판별하기 위해 edge id, twin id, core edge id를 모두 set에 넣는다.
- door/window가 twin edge로 들어와도 같은 벽으로 인식하기 위한 처리다.

`BuildOpeningsForRun()`

- wall run 또는 wall surface에 속한 opening 목록을 만든다.
- opening 중심을 run 방향의 로컬 X 좌표로 변환한다.
- opening width는 JSON의 width와 실제 edge 길이 중 더 큰 값을 사용한다.

`FindEdgesOverlappingFaceSegment()`

- 방 contour의 한 선분과 겹치는 half-edge들을 찾는다.
- collinear 여부, 선분과 edge 사이 거리, overlap 길이를 검사한다.
- 결과는 segment 기준 시작 위치 순서대로 정렬된다.

`FindFaceSide()`

- 특정 edge가 특정 face contour의 어느 변과 매칭되는지 찾는다.
- 매칭되면 `FWallSurfaceSide`를 채운다.
- 방 내부 방향 normal은 `ComputeHarnessInteriorNormal2D()`로 계산한다.

### opening 뚫기

`ApplyOpeningsToWall()`은 WallCore 메쉬에서 문/창 구멍을 Boolean subtract로 뚫는다.

동작 방식은 다음과 같다.

1. opening마다 box 형태의 `HoleMesh`를 만든다.
2. opening width, height, z offset을 기준으로 box 크기와 위치를 잡는다.
3. `UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean()`으로 벽 메쉬에서 hole box를 뺀다.

문처럼 `z_offset_cm`이 거의 0인 opening은 바닥까지 뚫리도록 `BottomZ - 1.0f`부터 구멍을 만든다.

### WallCore 생성

`BuildWallBox()`는 박스 형태의 벽 core 메쉬를 만든다.

처리 내용은 다음과 같다.

- `UDynamicMeshComponent` 생성
- 필요 시 `EditableWall` 태그 추가
- 전달받은 태그 추가
- 로컬 박스 메쉬 생성
- opening Boolean subtract 적용
- fallback material 설정
- 생성 직후에는 `NoCollision`으로 시작
- `AnimatedWalls`에 추가해 나중에 Tick에서 Z 스케일 애니메이션과 최종 충돌 설정을 적용

현재 WallCore는 하나의 통짜 박스가 아니라 좌/우 반쪽으로 생성된다.

```cpp
const float HalfThickness = Run.WallThickness * 0.5f;
const float OffsetDist = HalfThickness * 0.5f;
const FVector2D CoreNormal(-Run.Direction.Y, Run.Direction.X);

const FVector2D LeftCenter = CoreCenter + (CoreNormal * OffsetDist);
const FVector2D RightCenter = CoreCenter - (CoreNormal * OffsetDist);
```

좌우 core는 각각 다음 태그를 가진다.

- 공통: `WallCore`
- 왼쪽: `WallCore_Left`, `WallCore_<RunId>_L`
- 오른쪽: `WallCore_Right`, `WallCore_<RunId>_R`

두 core 모두 `bEditable = true`로 생성된다. 그래서 왼쪽과 오른쪽 벽 core를 독립적으로 라인 트레이스 선택할 수 있고, 서로 다른 재질을 적용할 수 있다.

길이는 `CoreLength`를 그대로 사용한다. 현재 구조에서는 WallCore의 시작/끝을 별도로 늘리거나 줄이지 않고, 서버에서 들어온 wall run의 실제 길이를 기준으로 생성한다.

### 벽 surface 생성

`BuildWallSurfacePanels()`는 실제 사용자가 보는 벽면 surface를 만든다.

WallCore는 구조용 두께를 가진 박스이고, WallSurface는 방 안쪽 또는 외벽 쪽에 얇게 붙는 표면 메쉬다. 벽지/타일/페인트 같은 재질은 보통 이 surface에 적용된다.

surface 생성 흐름은 다음과 같다.

1. wall side 중심에서 interior/exterior normal 방향으로 surface를 약간 띄운다.
2. 원래 side 길이의 절반을 기준으로 `XMin`, `XMax`를 잡는다.
3. opening 영역을 X/Z cut 값으로 나눈다.
4. opening 내부 cell은 제외하고 나머지 cell만 quad로 만든다.
5. UV는 `X / 100`, `Z / 100` 기준으로 만든다.
6. `EditableWall`, `Wall`, `WallSurface` 또는 `WallExterior` 태그를 부여한다.
7. 충돌은 `QueryAndPhysics`, 전체 채널 Block으로 설정한다.

surface도 `AnimatedWalls`에 들어가므로 WallCore와 같이 Z 스케일 애니메이션을 받는다.

### 벽 run 병합

`BuildMergedWallRuns()`는 `EdgeCache`의 half-edge들을 같은 직선상의 연속 벽으로 병합한다.

병합 조건은 다음과 같다.

- 두 edge가 거의 같은 방향이어야 한다.
- 같은 선 위에 있어야 한다.
- 두께가 거의 같아야 한다.
- endpoint가 허용 오차 안에서 맞닿거나 겹쳐야 한다.

병합된 `FWallRun`은 WallCore 생성의 기준이 된다.

### face surface 병합

`BuildMergedFaceSides()`는 각 방 face의 contour를 순회하면서 실제 half-edge와 겹치는 구간을 찾아 `FWallSurfaceSide`를 만든다.

같은 방향, 같은 높이, 같은 두께, 같은 normal을 가진 인접 side는 하나로 병합된다. 현재는 별도의 miter 길이 보정 없이 병합된 side의 실제 길이만 사용한다.

### 길이 보정 제거

기존에는 다음 두 인셋 함수가 벽 교차부의 길이를 조절했다.

```cpp
ApplyTJointSurfaceInsets(InteriorSurfaceSides, WallRuns);
ApplyWallRunEndpointInsets(WallRuns);
```

현재 코드에서는 위 인셋 함수 블록과 호출을 제거했다. T자 교차점이나 wall run endpoint에서 벽 길이를 자동으로 줄이지 않는다.

또한 WallCore도 `CoreLength + Run.WallThickness` 같은 연장값을 쓰지 않고 `CoreLength` 그대로 생성한다. 내벽/외벽 surface 역시 miter 교차점으로 길이를 늘리지 않고, 원래 side 길이 기준으로 생성한다.

### 내부/외부 surface 생성

마지막 단계에서는 모든 `InteriorSurfaceSides`를 순회한다.

항상 내부 surface를 생성한다.

```cpp
WallSurface
RoomFace_<FaceId>
WallSurface_<FaceId>_<SurfaceEdgeId>
```

그리고 해당 side가 외벽이고 다른 방과 맞닿지 않는 경우 외부 surface도 생성한다.

```cpp
WallExterior
WallExterior_<SurfaceEdgeId>
```

이 구조 덕분에 내부 벽면과 외벽면을 서로 다른 컴포넌트로 선택하고 다른 재질을 적용할 수 있다.

## HarnessGeneratorComponent_Planes.cpp

이 파일은 `FabricateDynamicPlanes()`에서 바닥, 천장, 천장 그림자용 blocker를 만든다. `InstallOpeningComponents()`는 현재 비어 있다.

### 방 polygon 준비

각 `FTopologyFace`에 대해 `contour_vertex_ids`를 `VertexCache`에서 찾아 `RawPoints`를 만든다.

그 다음 다음 정리 과정을 거친다.

1. 너무 가까운 중복 점 제거
2. 마지막 점이 첫 점과 거의 같으면 제거
3. 180도 직선에 가까운 collinear 점 제거
4. 점이 3개 미만이면 해당 face 스킵

이 과정은 `FGeomTools::TriangulatePoly()`가 실패하거나 이상한 삼각형을 만드는 것을 줄이기 위한 전처리다.

### winding 보정과 triangulation

polygon signed area를 계산해 점 순서를 확인한다. 필요하면 `Algo::Reverse()`로 뒤집는다.

그 후 `FClipSMPolygon`에 점을 넣고 `FGeomTools::TriangulatePoly()`로 삼각형 배열을 만든다.

`OutTris`에 들어온 삼각형 vertex 위치는 원래 `TriangulationPoints`의 index로 다시 매핑된다. 이 index 배열이 이후 바닥과 천장 메쉬 생성에 같이 사용된다.

### 바닥 메쉬 생성

바닥은 단순한 plane이 아니라 두께가 있는 slab 형태다.

상단 Z는 `Face.z_offset`, 하단 Z는 `Face.z_offset - SlabThickness`다. 현재 slab thickness는 20cm로 고정되어 있다.

생성되는 면은 다음과 같다.

- 상단 바닥면
- 하단 바닥면
- 외곽 side face

`AppendTriangleFacing()` helper는 삼각형 normal 방향을 검사해 기대 방향과 반대면 vertex 순서를 뒤집는다.

material id는 다음처럼 사용된다.

- material slot 0: 바닥 상단
- material slot 1: slab 측면/하단

바닥 컴포넌트 태그는 다음과 같다.

- `EditableFloor`
- `FloorFace_<face_id>`
- `Floor`

충돌은 `BlockAll`이며, 바닥도 선택/배치 기준으로 사용할 수 있다.

### 천장 메쉬 생성

천장은 `Face.z_offset + Face.height_cm` 위치에 생성된다. 바닥과 달리 현재 코드는 아래에서 보이는 천장 면을 중심으로 만든다.

태그는 `Ceiling`이고 충돌은 `BlockAll`이다. fallback material이 있으면 material slot 0에 적용된다.

### CeilingShadowBlocker

천장 위쪽 그림자 처리를 위해 `CeilingShadowBlocker`라는 별도 메쉬를 만든다.

특징은 다음과 같다.

- 실제 게임 화면에서는 보이지 않도록 `SetRenderInMainPass(false)`
- collision 없음
- shadow만 cast
- polygon을 `HarnessCeilingShadowOverhangCm`만큼 바깥으로 offset해서 생성

즉, 천장 위쪽에서 빛이 새거나 그림자가 어색해지는 문제를 줄이기 위한 보이지 않는 shadow caster다.

### InstallOpeningComponents

`InstallOpeningComponents()`는 현재 비어 있다.

문/창 자체의 3D 프레임, 손잡이, 창호 모델 같은 별도 컴포넌트를 설치하려면 이 함수가 확장 지점이 될 수 있다. 현재 opening은 주로 벽 core/surface 메쉬에서 구멍을 만드는 방식으로 처리된다.

## 태그 체계 요약

생성된 컴포넌트는 태그 기반으로 선택, 저장, 재질 적용 대상을 구분한다.

| 태그 | 의미 |
| --- | --- |
| `EditableWall` | 벽 선택 가능 대상 |
| `Wall` | 일반 벽 surface |
| `WallCore` | 구조용 벽 core |
| `WallCore_Left` | 좌측 half core |
| `WallCore_Right` | 우측 half core |
| `WallSurface` | 방 내부 벽면 surface |
| `WallExterior` | 외벽 바깥쪽 surface |
| `RoomFace_<FaceId>` | 특정 방에 속한 surface |
| `EditableFloor` | 선택/배치 가능한 바닥 |
| `FloorFace_<FaceId>` | 특정 방 바닥 |
| `Floor` | 일반 바닥 |
| `Ceiling` | 천장 |
| `CeilingShadowBlocker` | 보이지 않는 그림자용 천장 blocker |
| `InteriorLight` | 자동 생성된 실내 조명 |

## 충돌 정책 요약

최근 벽 선택과 캐릭터 통과 문제를 같이 해결하기 위해 벽 충돌은 다음 방향으로 정리되어 있다.

- 벽은 `QueryAndPhysics`
- 벽 object type은 `WorldStatic`
- 기본적으로 모든 채널 Block
- 선택용 `Visibility`, `ECC_GameTraceChannel1`도 Block
- 캐릭터 이동용 `ECC_Pawn`도 Block

따라서 벽은 라인 트레이스로 선택 가능하고, 동시에 캐릭터가 통과하지 못한다.

## 스케일과 좌표계 주의사항

현재 런타임 생성은 `CachedFloorData`를 기준으로 수행된다. 이 데이터는 `SourceFloorData`에 `EditorPlanScale`, `OverallPlanScale`, 문 기준 자동 스케일을 적용한 결과다.

좌표는 `FHarnessFloorData::ToHarnessPoint()`에서 변환된다. JSON이 `ue_z_up_y_negative` 또는 `imageYToUnrealAxis = "-Y"` 같은 메타데이터를 가지면 Y 방향을 뒤집어 Unreal 평면 기준에 맞춘다.

이 때문에 좌표 회전/반전 문제를 수정할 때는 다음 세 군데를 같이 확인해야 한다.

- JSON 좌표계 메타데이터
- `ToHarnessPoint()`
- 2D/미니맵에서 화면 좌표로 다시 변환하는 코드

## 변경 시 주의할 부분

벽 gap을 고치려고 인셋이나 길이 연장을 다시 넣으면 실제 도면보다 벽이 짧아지거나 길어질 수 있다. 현재는 core와 내벽/외벽 surface 모두 원 segment 길이를 기준으로 생성한다.

WallCore를 다시 통짜 메쉬로 합치면 좌/우 벽면을 독립적으로 선택하기 어려워진다. 재질을 좌우 다르게 적용해야 한다면 `WallCore_Left`, `WallCore_Right` 분리 구조를 유지해야 한다.

opening 위치는 `CoreOpenings`의 `OpeningCenterShift`를 통해 core 중심 변경에 맞춰 보정된다. WallCore split을 수정할 때 이 보정 로직은 split 전에 유지해야 한다.

바닥/천장 triangulation은 polygon 점 순서와 collinear point에 민감하다. JSON face contour가 이상하면 바닥이 깨져 보일 수 있으므로, 서버 데이터에서 방 영역 contour가 안정적으로 들어오는지 확인하는 것이 중요하다.
