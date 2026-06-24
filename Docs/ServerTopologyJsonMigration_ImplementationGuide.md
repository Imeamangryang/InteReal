# Server Topology JSON v3.1 Migration Implementation Guide

이 문서는 현재 `Server/` 폴더의 실제 코드를 기준으로, 서버가 Unreal에 전달할 도면 JSON을 장기 권장 형식인 `Topology Contract JSON v3.1`로 생성하기 위한 개선 사항을 정리한다.

대상 독자는 서버 코드를 수정할 개발자 또는 서버 쪽 AI 에이전트다. 이 문서는 “무엇을 바꿔야 하는지”뿐 아니라, “현재 코드가 무엇을 하고 있는지”, “어떤 필드를 어떻게 변환해야 하는지”, “어떤 검증과 테스트가 필요한지”까지 구체적으로 설명한다.

## 목표

현재 서버는 최종 Unreal 전달용으로 `vertices / half_edges / faces / openings` 중심의 UE 전용 JSON을 생성한다. 앞으로는 Unreal 또는 다른 3D 클라이언트가 더 명확하게 사용할 수 있도록 아래 구조의 중립적인 토폴로지 계약 JSON을 생성해야 한다.

```json
{
  "schema_version": "3.1",
  "unit": "mm",
  "coordinate_system": {
    "origin": "top_left",
    "x_axis": "right",
    "y_axis": "down",
    "rotation_degrees": 0
  },
  "plan": {},
  "levels": [],
  "bounds": {},
  "walls": [],
  "spaces": [],
  "openings": []
}
```

핵심 요구 사항:

- 서버 내부 편집용 JSON은 유지해도 된다.
- 최종 export 또는 mark-final 시점에 `Topology Contract JSON v3.1`을 생성한다.
- 좌표계는 반드시 이미지/도면 기준 `top_left / x-right / y-down`을 유지한다.
- 최종 계약 JSON에서는 Unreal 전용 Y 반전, half-edge 분할, lintel edge 같은 런타임 표현을 사용하지 않는다.
- 방/공간 바닥 생성을 위해 `spaces.boundary`를 반드시 제공한다.
- 문/창문은 `host_wall_id + offset_to_center` 기준으로 전달한다.

## 현재 코드 구조

### 분석 실행

파일:

- `Server/plangraph_service.py`

주요 함수:

- `analyze_floorplan()` at `Server/plangraph_service.py:205`
- `_analyze_with_plangraph_engine()` at `Server/plangraph_service.py:234`

현재 동작:

1. `PlanGraphEngine`을 실행한다.
2. `result.to_floorplan_json()`으로 엔진 결과를 받는다.
3. `analysis_json.graph.floorplan`에 아래 데이터를 저장한다.

```json
{
  "image_width": 100,
  "image_height": 80,
  "scale_mm_per_px": 10,
  "outer_walls": [],
  "inner_walls": [],
  "unknown_walls": [],
  "rooms": []
}
```

분석 결과는 아직 최종 Unreal JSON이 아니다. 이 데이터는 편집용 floorplan JSON의 원천 데이터다.

### 편집용 JSON 생성

파일:

- `Server/plangraph_editor_service.py`

주요 함수:

- `build_editable_floorplan_from_graph_data()` at `Server/plangraph_editor_service.py:366`
- `normalize_floorplan_units()` at `Server/plangraph_editor_service.py:560`
- `_normalize_room()` at `Server/plangraph_editor_service.py:1368`
- `_normalize_opening()` at `Server/plangraph_editor_service.py:1406`

현재 편집용 JSON 구조:

```json
{
  "schema_version": "1.1",
  "image_width": 100,
  "image_height": 80,
  "scale_mm_per_px": 10,
  "coordinate_policy": "image_top_left_y_down",
  "nodes": [],
  "edges": [],
  "walls": [],
  "openings": [],
  "doors": [],
  "windows": [],
  "rooms": []
}
```

현재 벽 구조:

```json
{
  "id": "w1",
  "x1": 0,
  "y1": 0,
  "x2": 100,
  "y2": 0,
  "wall_type": "outer",
  "wall_class": "outer",
  "thickness_px": 6,
  "length_px": 100,
  "length_mm": 1000
}
```

현재 문/창문 구조:

```json
{
  "id": "op_1",
  "wall_id": "w1",
  "opening_type": "door",
  "offset_ratio": 0.5,
  "width_mm": 900,
  "height_mm": 2100,
  "z_offset_mm": 0,
  "door_kind": "swing",
  "door_direction": "left"
}
```

현재 방 구조:

```json
{
  "id": "room_1",
  "name": "Living Room",
  "room_type": "living_room",
  "points": [
    { "x": 0, "y": 0 },
    { "x": 100, "y": 0 },
    { "x": 100, "y": 80 },
    { "x": 0, "y": 80 }
  ]
}
```

이 구조는 내부 편집용으로는 유지 가능하다. 단, 최종 서버-클라이언트 계약 JSON으로 그대로 쓰기에는 부족하다.

### 현재 UE JSON 생성

파일:

- `Server/plangraph_ue_topology.py`

주요 함수:

- `build_ue_topology_json()` at `Server/plangraph_ue_topology.py:31`
- `to_ue_runtime_topology_json()` at `Server/plangraph_ue_topology.py:267`
- `_validate_ue_topology_json()` at `Server/plangraph_ue_topology.py:952`

현재 최종 출력 구조:

```json
{
  "project_info": {},
  "vertices": [],
  "half_edges": [],
  "wall_side_measurements": [],
  "surface_measurements": [],
  "openings": [],
  "faces": [],
  "metadata": {}
}
```

이 구조는 Unreal 런타임 생성에는 사용할 수 있지만, 서버-클라이언트 계약 데이터로는 다음 문제가 있다.

- `schema_version`이 없다.
- `unit`이 최상위에 없다.
- `coordinate_system`이 계약 형태가 아니다.
- 벽이 `walls`가 아니라 `half_edges`로 분해되어 있다.
- 문/창문이 `host_wall_id + offset_to_center`가 아니라 `target_edge_id`에 붙는다.
- 방은 `spaces`가 아니라 `faces`로 표현된다.
- Unreal 전용 Y 반전 정책이 들어간다.
- `entrance_door`, `sliding_door`, `connects` 같은 도면 의미 정보가 보존되지 않는다.

### 현재 API 연결

파일:

- `Server/plangraph_backend_api.py`
- `Server/unreal.py`

주요 함수:

- `mark_editable_final()` at `Server/plangraph_backend_api.py:840`
- `_build_ue_transform_payload()` at `Server/plangraph_backend_api.py:905`
- `export_ue_topology_json()` at `Server/plangraph_backend_api.py:942`
- `get_unreal_plan_base()` at `Server/unreal.py:198`

현재 흐름:

```text
editable.floorplan_json
→ normalize_floorplan_units()
→ build_ue_topology_json()
→ to_ue_runtime_topology_json()
→ editable.ue_topology_json 저장
→ /api/unreal/plans/{plan_id}/base 에서 그대로 반환
```

## 권장 아키텍처

기존 half-edge 변환기는 바로 제거하지 말고, 새 계약 JSON 변환기를 추가한다.

권장 흐름:

```text
PlanGraphEngine analysis_json
→ editable floorplan_json v1.1
→ user edits
→ normalize_floorplan_units()
→ build_topology_contract_json_v3_1()
→ validate_topology_contract_json_v3_1()
→ editable.ue_topology_json 저장
→ /api/unreal/plans/{plan_id}/base 반환
```

기존 half-edge 출력이 필요하면 아래처럼 별도 이름으로 유지한다.

```text
build_ue_half_edge_runtime_json()
```

또는 기존 `build_ue_topology_json()` 이름은 유지하되, 새 함수는 다음 중 하나로 추가한다.

추천 파일:

- 새 파일: `backend/app/services/plangraph_contract_topology.py`

또는 현재 샘플 폴더 기준:

- `Server/plangraph_contract_topology.py`

추천 함수:

```python
def build_topology_contract_json_v3_1(
    floorplan: dict[str, Any],
    *,
    job_id: int,
    editable_floorplan_id: int,
    editable_floorplan_version: int,
    plan_id: int | None = None,
    plan_name: str | None = None,
    default_level_id: str = "level_1",
    default_wall_height_mm: float = 2400.0,
    default_outer_wall_thickness_mm: float = 200.0,
    default_inner_wall_thickness_mm: float = 150.0,
    default_unknown_wall_thickness_mm: float = 150.0,
) -> dict[str, Any]:
    ...
```

추천 검증 함수:

```python
def validate_topology_contract_json_v3_1(payload: dict[str, Any]) -> None:
    ...
```

## 목표 JSON 형식

### 최상위

```json
{
  "schema_version": "3.1",
  "unit": "mm",
  "coordinate_system": {
    "origin": "top_left",
    "x_axis": "right",
    "y_axis": "down",
    "rotation_degrees": 0
  },
  "plan": {
    "id": 6,
    "version": 1,
    "name": "sample_plan",
    "source_job_id": 10,
    "editable_floorplan_id": 20
  },
  "levels": [
    {
      "id": "level_1",
      "name": "1F",
      "elevation": 0,
      "default_height": 2400
    }
  ],
  "bounds": {
    "min": { "x": 0, "y": 0 },
    "max": { "x": 12000, "y": 8500 }
  },
  "walls": [],
  "spaces": [],
  "openings": [],
  "metadata": {}
}
```

### walls

```json
{
  "id": "wall_001",
  "level_id": "level_1",
  "kind": "outer",
  "centerline": [
    { "x": 0, "y": 0 },
    { "x": 12000, "y": 0 }
  ],
  "thickness": 200,
  "height": 2400,
  "metadata": {
    "source_wall_id": "wall_001",
    "source_wall_type": "outer"
  }
}
```

규칙:

- `centerline`은 mm 좌표다.
- 좌표는 `top_left / x-right / y-down`을 유지한다.
- Y축을 음수로 뒤집지 않는다.
- 벽은 중심선 기준이다.
- `kind`는 `outer`, `inner`, `unknown`, `virtual` 중 하나를 허용하는 것을 권장한다.
- `unknown`을 허용하지 않는 strict 클라이언트라면 export 단계에서 `unknown` 벽을 validation error로 막는다.

### spaces

```json
{
  "id": "space_living_001",
  "level_id": "level_1",
  "kind": "living_room",
  "name": "Living Room",
  "boundary": [
    { "x": 0, "y": 3000 },
    { "x": 6000, "y": 3000 },
    { "x": 6000, "y": 8500 },
    { "x": 0, "y": 8500 }
  ],
  "boundary_walls": [],
  "floor_material": "wood",
  "metadata": {
    "source_room_id": "room_1"
  }
}
```

규칙:

- `spaces.boundary`는 바닥 생성을 위한 필수 데이터다.
- `rooms.points`를 mm 변환해서 `spaces.boundary`로 만든다.
- `rooms`라는 이름 대신 `spaces`를 사용한다.
- 공간 타입은 `room_type`에서 변환한다.

권장 타입 매핑:

| 현재 `room_type` | v3.1 `kind` |
|---|---|
| `living_room` | `living_room` |
| `bedroom` | `bedroom` |
| `bathroom` | `bathroom` |
| `kitchen` | `kitchen_dining` |
| `balcony` | `balcony` |
| `hallway` | `corridor` |
| `utility` | `utility` |
| `office` | `office` |
| `other` | `unknown` |

### openings

```json
{
  "id": "door_001",
  "level_id": "level_1",
  "kind": "door",
  "host_wall_id": "wall_003",
  "offset_from": "start",
  "offset_to_center": 1200,
  "width": 900,
  "height": 2100,
  "bottom": 0,
  "connects": [
    "space_living_001",
    "space_bedroom_001"
  ],
  "swing": {
    "direction": "inward",
    "hinge": "left",
    "angle": 90
  },
  "metadata": {
    "source_opening_id": "op_1",
    "source_offset_ratio": 0.5
  }
}
```

창문 예시:

```json
{
  "id": "window_001",
  "level_id": "level_1",
  "kind": "window",
  "host_wall_id": "wall_001",
  "offset_from": "start",
  "offset_to_center": 3000,
  "width": 1800,
  "height": 1200,
  "bottom": 900,
  "connects": [
    "space_bedroom_001",
    "exterior"
  ]
}
```

규칙:

- 현재 `wall_id`는 `host_wall_id`로 변환한다.
- 현재 `offset_ratio`는 `offset_to_center` mm로 변환한다.
- 현재 `width_mm`는 `width`로 변환한다.
- 현재 `height_mm`는 `height`로 변환한다.
- 현재 `z_offset_mm`는 `bottom`으로 변환한다.
- 문/창문 타입은 가능한 의미를 보존한다.

권장 opening kind 매핑:

| 현재 필드 | v3.1 `kind` |
|---|---|
| `opening_type=window` | `window` |
| `opening_type=door`, `door_kind=sliding` | `sliding_door` |
| `opening_type=door`, metadata/category/name이 entrance를 의미 | `entrance_door` |
| `opening_type=door` | `door` |
| `opening_type=opening` | `opening` |

현재 서버에는 `entrance_door`를 안정적으로 판단할 필드가 없다. 자동 판정이 불가능하면 `door`로 내리고, 추후 편집기에서 현관문 속성을 명시할 수 있게 해야 한다.

## 변환 로직 상세

### 1. 단위 변환

현재 편집 JSON은 px 좌표를 쓴다. `scale_mm_per_px`가 있으면 반드시 그 값을 사용한다.

```python
unit_scale = float(floorplan["scale_mm_per_px"])
x_mm = x_px * unit_scale
y_mm = y_px * unit_scale
```

중요:

- 계약 JSON은 `top_left / x-right / y-down`이다.
- `plangraph_ue_topology.py`의 `image_point_to_unreal_point()`처럼 Y를 음수로 뒤집으면 안 된다.
- 기존 `coordinate_policy="ue_z_up_y_negative"` 로직을 재사용하지 않는다.

`scale_mm_per_px`가 없으면 두 가지 정책 중 하나를 선택한다.

권장 정책:

- strict export에서는 실패 처리한다.
- 임시 preview export에서는 `visual_scale_factor_mm_per_px`를 fallback으로 쓰고 `metadata.warnings`에 기록한다.

### 2. bounds 계산

우선순위:

1. `image_width`, `image_height`, `scale_mm_per_px`가 있으면:

```json
{
  "min": { "x": 0, "y": 0 },
  "max": {
    "x": "image_width * scale_mm_per_px",
    "y": "image_height * scale_mm_per_px"
  }
}
```

2. 이미지 크기가 없으면 모든 wall centerline, space boundary 점의 min/max로 계산한다.

### 3. walls 변환

현재 함수 `_export_walls_from_floorplan()` at `Server/plangraph_ue_topology.py:303`는 `walls` 또는 `nodes/edges`에서 export wall 목록을 만든다. 이 로직은 재사용해도 된다.

변환 절차:

1. `export_walls = _export_walls_from_floorplan(floorplan)` 또는 동일 로직 사용.
2. 각 wall에 대해 `_wall_segments(wall)` 또는 동일 로직으로 선분 목록을 얻는다.
3. 선분이 1개면 `centerline`은 `[start, end]`.
4. 선분이 여러 개면 연결 순서대로 polyline centerline을 만든다.
5. 곡선 벽은 현재 `_wall_segments()`가 32개 segment로 샘플링하므로, 그대로 polyline으로 export한다.

예시:

```python
centerline = [
    {"x": x1 * unit_scale, "y": y1 * unit_scale},
    {"x": x2 * unit_scale, "y": y2 * unit_scale},
]
```

벽 두께:

1. `wall.thickness_mm`
2. `wall.metadata.thickness_mm`
3. `wall.wall_thickness_mm`
4. `outer`면 200mm
5. `inner`면 150mm
6. `unknown`이면 150mm 또는 validation warning

벽 높이:

- 기본 `2400mm`
- 기존 코드 기본값은 `2600mm`인데, 아파트 표준 기본값으로 `2400mm`를 권장한다.
- 클라이언트와 합의된 값이 있으면 그 값을 따른다.

### 4. spaces 변환

현재 room은 `floorplan.rooms[].points`에 있다.

변환 절차:

1. `floorplan.get("rooms", [])` 순회.
2. `points`가 3개 미만이면 strict export 실패.
3. 각 point를 mm로 변환해서 `boundary`에 넣는다.
4. 자기 교차, 0 면적 polygon이면 strict export 실패.
5. `room_type`을 `kind`로 변환한다.

현재 `plangraph_ue_topology.py`는 room을 `faces`로 만들 때 `face.get("points")`를 사용한다. 이 점은 v3.1 변환기에서도 그대로 활용 가능하다.

주의:

- `spaces.boundary`가 없으면 Unreal에서 바닥 생성이 불안정하다.
- 벽만 보고 방 영역을 추론하는 방식은 다시 도입하지 않는다.
- room이 하나도 없으면, final export는 실패시키는 것을 권장한다. 최소한 `"ROOM_BOUNDARY_MISSING"` validation error를 반환해야 한다.

### 5. openings 변환

현재 opening 수집 로직은 `_collect_opening_specs_by_wall()` at `Server/plangraph_ue_topology.py:668`에 있다.

이 함수는 현재 half-edge용 spec을 만든다. v3.1 변환기에서는 비슷한 로직을 사용하되 결과 필드를 다르게 만든다.

현재 입력:

```json
{
  "wall_id": "wall_001",
  "opening_type": "door",
  "offset_ratio": 0.5,
  "width_mm": 900,
  "height_mm": 2100,
  "z_offset_mm": 0
}
```

목표 출력:

```json
{
  "host_wall_id": "wall_001",
  "offset_from": "start",
  "offset_to_center": 1500,
  "width": 900,
  "height": 2100,
  "bottom": 0
}
```

`offset_to_center` 계산:

```python
wall_length_px = wall_path_length_px(host_wall)
offset_to_center_mm = offset_ratio * wall_length_px * unit_scale
```

`offset_ratio`가 없고 opening에 `x1/y1/x2/y2`가 있으면 기존 `_infer_opening_offset_ratio()` at `Server/plangraph_ue_topology.py:854`와 같은 로직을 사용한다.

중요:

- v3.1에서는 `target_edge_id`를 만들지 않는다.
- opening 구간 때문에 wall을 split하지 않는다.
- 문/창문은 항상 원본 wall ID에 붙는다.

### 6. connects 계산

`connects`는 문/창문이 연결하는 공간 ID 목록이다.

초기 구현 정책:

1. opening metadata에 `connects`가 있으면 그대로 사용한다.
2. 없으면 자동 추론을 시도한다.
3. 자동 추론이 실패하면 `connects: []`로 두고 validation warning을 기록한다.

권장 자동 추론 알고리즘:

1. host wall의 centerline에서 opening 중심점을 구한다.
2. host wall 방향 벡터를 구한다.
3. 벽 normal의 양쪽으로 probe point 두 개를 만든다.
4. probe distance는 `wall.thickness / 2 + 150mm` 정도를 사용한다.
5. 각 probe point가 어떤 `space.boundary` polygon 안에 있는지 point-in-polygon으로 찾는다.
6. 한쪽이 space이고 다른 쪽이 없으면 `["space_id", "exterior"]`.
7. 양쪽이 서로 다른 space면 `["space_a", "space_b"]`.
8. 둘 다 없거나 같은 space면 warning.

주의:

- 좌표계는 y-down이므로 normal 방향이 수학 좌표계와 다르게 느껴질 수 있다. 하지만 polygon 포함 여부만 보므로 좌우 이름보다 공간 ID 검출이 중요하다.
- `swing.direction`의 inward/outward 기준까지 자동 판단하려면 `connects[0]`를 기준 공간으로 정의해야 한다.

### 7. swing 변환

현재 서버는 `door_direction`에 `left/right`만 가진다.

v3.1 권장 구조:

```json
{
  "direction": "inward",
  "hinge": "left",
  "angle": 90
}
```

초기 매핑:

```python
swing = {
    "direction": "inward",
    "hinge": door_direction or "left",
    "angle": 90
}
```

미닫이문:

```json
{
  "direction": "sliding",
  "hinge": "none",
  "angle": 0
}
```

현재 데이터만으로 inward/outward를 정확히 알 수 없으면 기본값은 `inward`로 두고 `metadata.review_status="needs_review"`를 붙인다.

## 새 검증 규칙

`validate_topology_contract_json_v3_1()`에서 아래를 검사한다.

최상위:

- `schema_version == "3.1"`
- `unit == "mm"`
- `coordinate_system.origin == "top_left"`
- `coordinate_system.x_axis == "right"`
- `coordinate_system.y_axis == "down"`
- `coordinate_system.rotation_degrees == 0`
- `levels`가 비어 있지 않아야 한다.

ID:

- `walls[].id` 중복 금지
- `spaces[].id` 중복 금지
- `openings[].id` 중복 금지
- 모든 `wall.level_id`는 `levels[].id`에 존재
- 모든 `space.level_id`는 `levels[].id`에 존재
- 모든 `opening.level_id`는 `levels[].id`에 존재
- 모든 `opening.host_wall_id`는 `walls[].id`에 존재
- 모든 `opening.connects` 값은 `spaces[].id` 또는 `"exterior"`여야 한다.

walls:

- `centerline`은 최소 2개 point
- 모든 point의 `x`, `y`는 finite number
- 전체 centerline 길이는 0보다 커야 한다.
- `thickness > 0`
- `height > 0`
- `kind`는 허용된 값이어야 한다.

spaces:

- `boundary`는 최소 3개 point
- polygon 면적은 0보다 커야 한다.
- 자기 교차가 없어야 한다.
- 모든 point의 `x`, `y`는 finite number

openings:

- `host_wall_id` 존재
- `offset_from == "start"`
- `0 <= offset_to_center <= host wall length`
- `width > 0`
- `height > 0`
- `bottom >= 0`
- `kind`는 `door`, `entrance_door`, `sliding_door`, `window`, `opening` 중 하나
- 문 폭이 900mm보다 지나치게 작으면 error 또는 warning

권장 opening 폭 검증:

- door/entrance_door/sliding_door: 700mm 미만이면 warning, 400mm 미만이면 error
- window: 300mm 미만이면 warning

## API 변경 권장안

### Phase 1: 새 export endpoint 추가

기존 endpoint를 깨지 않기 위해 먼저 새 endpoint를 추가한다.

파일:

- `Server/plangraph_backend_api.py`

새 endpoint 예:

```python
@router.post(
    "/editable-floorplans/{editable_floorplan_id}/exports/topology-contract-json",
    response_model=ExportArtifactResponse,
)
def export_topology_contract_json(...):
    editable = db.get(FpDraft, editable_floorplan_id)
    floorplan = normalize_floorplan_units(editable.floorplan_json)
    payload = build_topology_contract_json_v3_1(
        floorplan,
        job_id=editable.job_id,
        editable_floorplan_id=editable.id,
        editable_floorplan_version=editable.version,
        plan_id=editable.floorplan_id,
    )
    validate_topology_contract_json_v3_1(payload)
    ...
```

저장 format:

```text
topology_contract_json_v3_1
```

### Phase 2: mark-final에서 v3.1 저장

현재 `mark_editable_final()`은 아래 흐름을 사용한다.

```python
ue_payload = to_ue_runtime_topology_json(_build_ue_transform_payload(editable, UeTopologyExportRequest()))
editable.ue_topology_json = ue_payload
```

변경 후:

```python
payload = build_topology_contract_json_v3_1(
    normalize_floorplan_units(editable.floorplan_json),
    job_id=editable.job_id,
    editable_floorplan_id=editable.id,
    editable_floorplan_version=editable.version,
    plan_id=editable.floorplan_id,
)
validate_topology_contract_json_v3_1(payload)
editable.ue_topology_json = payload
```

이렇게 하면 `Server/unreal.py:get_unreal_plan_base()`는 별도 변경 없이 v3.1 JSON을 그대로 반환한다.

### Phase 3: 기존 half-edge endpoint 정리

기존 half-edge JSON이 필요하면 endpoint 이름을 명확히 분리한다.

권장 이름:

```text
/plangraph/editable-floorplans/{id}/exports/ue-half-edge-json
```

기존 `/exports/ue-topology-json`는 v3.1을 반환하거나, 하위 호환 기간 동안 query parameter로 선택하게 한다.

예:

```text
POST /exports/ue-topology-json?format=contract_v3_1
POST /exports/ue-topology-json?format=legacy_half_edge
```

단, 장기적으로는 `contract_v3_1`을 기본값으로 둔다.

## 구현 시 주의할 점

### 기존 `image_point_to_unreal_point()` 재사용 금지

`Server/plangraph_ue_topology.py:369`의 `image_point_to_unreal_point()`는 현재 Unreal 좌표계를 만들기 위해 Y를 뒤집는다.

v3.1 계약 JSON은 아래 좌표계를 유지해야 한다.

```json
{
  "origin": "top_left",
  "x_axis": "right",
  "y_axis": "down"
}
```

따라서 v3.1 변환기에는 단순 변환 함수를 별도로 둔다.

```python
def point_px_to_contract_mm(point: dict[str, Any], unit_scale: float) -> dict[str, float]:
    return {
        "x": round(float(point["x"]) * unit_scale, 4),
        "y": round(float(point["y"]) * unit_scale, 4),
    }
```

### 현재 `_opening_type_to_ue()`는 의미를 잃는다

현재 함수:

- `window`만 `Window`
- 나머지는 모두 `Door`

v3.1에서는 `entrance_door`, `sliding_door`, `opening`을 보존해야 한다. 새 변환기에서는 별도 mapping 함수를 만든다.

```python
def opening_kind_to_contract(item: dict[str, Any]) -> str:
    opening_type = str(item.get("opening_type") or item.get("type") or "").lower()
    door_kind = str(item.get("door_kind") or "").lower()
    metadata = item.get("metadata") or {}

    if opening_type == "window":
        return "window"
    if door_kind == "sliding":
        return "sliding_door"
    if metadata.get("is_entrance") is True or str(metadata.get("category") or "").lower() == "entrance":
        return "entrance_door"
    if opening_type == "opening":
        return "opening"
    return "door"
```

### unknown wall 처리 정책 필요

현재 편집 모델은 `wall_type=unknown`을 허용한다. v3.1에서도 `kind=unknown`을 허용할지 결정해야 한다.

권장:

- 편집 중 export: `unknown` 허용 + warning
- final export: `unknown`이 있으면 validation error 또는 사용자 확인 필요

### room이 없으면 바닥 생성 불가

현재 floorplan에 `rooms`가 비어 있을 수 있다. 하지만 Unreal 바닥 생성을 안정화하려면 `spaces.boundary`가 필요하다.

권장:

- final export에서 `spaces`가 비어 있으면 validation error
- preview export만 허용하고 warning 표시

error 예:

```json
{
  "code": "SPACE_BOUNDARY_MISSING",
  "message": "No spaces with boundary were found. Floor generation requires spaces.boundary."
}
```

## 테스트 추가 목록

파일:

- `Server/test_floorplans.py`
- `Server/test_unreal_api.py`

### 1. walls 변환 테스트

테스트 이름 예:

```python
def test_topology_contract_export_v3_1_maps_walls_to_mm_centerlines(db_session):
    ...
```

검증:

- `schema_version == "3.1"`
- `unit == "mm"`
- `walls[0].centerline[1].x == x2_px * scale_mm_per_px`
- `walls[0].centerline[1].y == y2_px * scale_mm_per_px`
- `kind == "outer"`
- `thickness == 200`
- `height == 2400`

### 2. 좌표계 반전 방지 테스트

테스트 이름 예:

```python
def test_topology_contract_keeps_top_left_y_down_coordinates(db_session):
    ...
```

입력:

```json
{ "x1": 0, "y1": 100, "x2": 200, "y2": 100, "scale_mm_per_px": 10 }
```

기대:

```json
centerline[0].y == 1000
centerline[1].y == 1000
```

절대 `-1000`이 되면 안 된다.

### 3. spaces 변환 테스트

테스트 이름 예:

```python
def test_topology_contract_maps_rooms_to_spaces_with_boundary(db_session):
    ...
```

검증:

- `rooms`가 `spaces`로 변환됨
- `room_type=kitchen`은 `kind=kitchen_dining`
- `points`가 `boundary` mm 좌표로 변환됨
- boundary point가 3개 이상이어야 함

### 4. openings 변환 테스트

테스트 이름 예:

```python
def test_topology_contract_maps_opening_ratio_to_offset_to_center_mm(db_session):
    ...
```

입력:

```json
{
  "wall_id": "wall_001",
  "offset_ratio": 0.5,
  "width_mm": 900
}
```

벽 길이:

```text
100px * 10mm = 1000mm
```

기대:

```json
{
  "host_wall_id": "wall_001",
  "offset_to_center": 500,
  "width": 900
}
```

### 5. connects 추론 테스트

테스트 이름 예:

```python
def test_topology_contract_infers_opening_connects_between_spaces(db_session):
    ...
```

검증:

- 문이 두 room boundary 사이 벽에 있으면 `connects`가 두 space ID를 포함
- 외벽 창문이면 `["space_id", "exterior"]`

초기 구현에서 connects 자동 추론을 아직 넣지 않는다면:

- `connects == []`
- `metadata.warnings` 또는 `metadata.review_status == "needs_review"` 검증

### 6. validation 실패 테스트

테스트 이름 예:

```python
def test_topology_contract_rejects_missing_room_boundaries_on_final_export(db_session):
    ...
```

검증:

- room이 없거나 boundary가 3점 미만이면 400
- detail에 `SPACE_BOUNDARY_MISSING` 또는 `INVALID_SPACE_BOUNDARY` 포함

### 7. Unreal base API 테스트

테스트 이름 예:

```python
def test_unreal_plan_base_returns_topology_contract_v3_1_after_mark_final(db_session):
    ...
```

검증:

- `GET /api/unreal/plans/{plan_id}/base`
- 응답에 `schema_version == "3.1"`
- 응답에 `walls`, `spaces`, `openings` 존재
- 응답에 `half_edges` 없음

## 샘플 변환

입력 편집 JSON:

```json
{
  "schema_version": "1.1",
  "image_width": 400,
  "image_height": 300,
  "scale_mm_per_px": 10,
  "walls": [
    {
      "id": "wall_001",
      "x1": 0,
      "y1": 0,
      "x2": 400,
      "y2": 0,
      "wall_type": "outer",
      "thickness_mm": 200
    }
  ],
  "rooms": [
    {
      "id": "room_001",
      "name": "Living Room",
      "room_type": "living_room",
      "points": [
        { "x": 0, "y": 0 },
        { "x": 400, "y": 0 },
        { "x": 400, "y": 300 },
        { "x": 0, "y": 300 }
      ]
    }
  ],
  "openings": [
    {
      "id": "door_001",
      "wall_id": "wall_001",
      "opening_type": "door",
      "offset_ratio": 0.5,
      "width_mm": 900,
      "height_mm": 2100,
      "z_offset_mm": 0
    }
  ]
}
```

목표 출력:

```json
{
  "schema_version": "3.1",
  "unit": "mm",
  "coordinate_system": {
    "origin": "top_left",
    "x_axis": "right",
    "y_axis": "down",
    "rotation_degrees": 0
  },
  "plan": {
    "id": null,
    "version": 1,
    "source_job_id": 1,
    "editable_floorplan_id": 1
  },
  "levels": [
    {
      "id": "level_1",
      "name": "1F",
      "elevation": 0,
      "default_height": 2400
    }
  ],
  "bounds": {
    "min": { "x": 0, "y": 0 },
    "max": { "x": 4000, "y": 3000 }
  },
  "walls": [
    {
      "id": "wall_001",
      "level_id": "level_1",
      "kind": "outer",
      "centerline": [
        { "x": 0, "y": 0 },
        { "x": 4000, "y": 0 }
      ],
      "thickness": 200,
      "height": 2400
    }
  ],
  "spaces": [
    {
      "id": "space_room_001",
      "level_id": "level_1",
      "kind": "living_room",
      "name": "Living Room",
      "boundary": [
        { "x": 0, "y": 0 },
        { "x": 4000, "y": 0 },
        { "x": 4000, "y": 3000 },
        { "x": 0, "y": 3000 }
      ],
      "boundary_walls": []
    }
  ],
  "openings": [
    {
      "id": "door_001",
      "level_id": "level_1",
      "kind": "door",
      "host_wall_id": "wall_001",
      "offset_from": "start",
      "offset_to_center": 2000,
      "width": 900,
      "height": 2100,
      "bottom": 0,
      "connects": [],
      "swing": {
        "direction": "inward",
        "hinge": "left",
        "angle": 90
      }
    }
  ]
}
```

## 최종 체크리스트

- [ ] `Topology Contract JSON v3.1` builder 추가
- [ ] v3.1 validator 추가
- [ ] Y축 반전 없는 mm 좌표 변환 함수 추가
- [ ] `walls` 변환 구현
- [ ] `spaces` 변환 구현
- [ ] `openings` 변환 구현
- [ ] `offset_ratio`를 `offset_to_center` mm로 변환
- [ ] `connects` 자동 추론 또는 review warning 구현
- [ ] `mark-final`에서 v3.1 JSON 저장하도록 변경
- [ ] v3.1 export endpoint 추가
- [ ] 기존 half-edge export는 legacy로 분리
- [ ] 테스트 추가
- [ ] `/api/unreal/plans/{plan_id}/base` 응답이 v3.1인지 검증

## 결론

현재 서버는 v3.1 JSON을 만들 수 있는 원천 데이터는 이미 가지고 있다. 문제는 현재 최종 변환기가 Unreal half-edge 런타임 구조를 만들고 있다는 점이다.

따라서 가장 안전한 개선 방향은 다음과 같다.

1. 내부 편집 JSON v1.1은 유지한다.
2. 기존 half-edge 변환기는 legacy로 유지한다.
3. 새 `build_topology_contract_json_v3_1()` 변환기를 추가한다.
4. final/export 시점에 v3.1 JSON을 검증 후 `FpDraft.ue_topology_json`에 저장한다.
5. Unreal base API는 저장된 v3.1 JSON을 그대로 반환한다.

이 방식이면 서버 내부 편집기와 기존 분석 파이프라인을 크게 흔들지 않으면서, Unreal 쪽에서 안정적으로 도면을 생성할 수 있는 이상적인 계약 JSON으로 이동할 수 있다.
