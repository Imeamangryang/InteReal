# Unreal Plan JSON Contract v3.1 - Topology Based Proposal

이 문서는 서버에서 언리얼로 도면 데이터를 전달하기 위한 장기 권장 JSON 형식이다. 이전 v3 문서가 현재 구현 안정화에 초점을 둔 형식이라면, 이 문서는 앞으로 편집, 저장, 버전 관리, 머티리얼 적용, 공간 인식까지 확장 가능한 구조를 목표로 한다.

핵심 결론은 `walls / spaces / openings`를 분리하고, 각 요소를 안정적인 ID로 연결하는 방식이다.

## 설계 목표

- 언리얼이 방 영역, 문 위치, 도면 방향을 추론하지 않게 한다.
- 좌표계, 단위, 회전 기준을 서버와 클라이언트가 명확히 공유한다.
- 벽, 공간, 문, 창문 사이의 관계를 ID로 추적한다.
- 저장, 새 버전 저장, delta 비교 시 같은 객체를 안정적으로 식별할 수 있게 한다.
- 도면 생성 엔진이 Unreal이 아니어도 그대로 사용할 수 있는 중립적인 형식으로 유지한다.

## 최상위 구조

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
    "name": "apartment_plan"
  },
  "levels": [],
  "bounds": {},
  "walls": [],
  "spaces": [],
  "openings": []
}
```

## 절대 고정 규칙

- `unit`은 반드시 `mm`다.
- `origin`은 반드시 `top_left`다.
- `x_axis`는 반드시 `right`다.
- `y_axis`는 반드시 `down`이다.
- `rotation_degrees`는 반드시 `0`이다.
- 서버는 도면을 최종 정방향으로 보정한 뒤 JSON을 내려준다.
- 언리얼 렌더링 보정용 회전, 미러링, 임시 스케일 값은 JSON에 넣지 않는다.
- 모든 객체 ID는 같은 plan/version 안에서 중복되면 안 된다.
- 가능하면 같은 물리 객체는 버전이 바뀌어도 같은 ID를 유지한다.

## 전체 예시

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
    "name": "sample_apartment"
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
  "walls": [
    {
      "id": "wall_001",
      "level_id": "level_1",
      "kind": "outer",
      "centerline": [
        { "x": 0, "y": 0 },
        { "x": 12000, "y": 0 }
      ],
      "thickness": 200,
      "height": 2400
    },
    {
      "id": "wall_002",
      "level_id": "level_1",
      "kind": "inner",
      "centerline": [
        { "x": 4200, "y": 0 },
        { "x": 4200, "y": 3600 }
      ],
      "thickness": 150,
      "height": 2400
    }
  ],
  "spaces": [
    {
      "id": "space_living_001",
      "level_id": "level_1",
      "kind": "living_room",
      "name": "Living Room",
      "boundary": [
        { "x": 4200, "y": 3600 },
        { "x": 9000, "y": 3600 },
        { "x": 9000, "y": 8500 },
        { "x": 4200, "y": 8500 }
      ],
      "boundary_walls": [
        "wall_002",
        "wall_003",
        "wall_004",
        "wall_005"
      ],
      "floor_material": "wood"
    }
  ],
  "openings": [
    {
      "id": "door_001",
      "level_id": "level_1",
      "kind": "door",
      "host_wall_id": "wall_002",
      "offset_from": "start",
      "offset_to_center": 2100,
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
      }
    },
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
  ]
}
```

## levels

```json
{
  "id": "level_1",
  "name": "1F",
  "elevation": 0,
  "default_height": 2400
}
```

`levels`는 현재 단층 아파트만 사용하더라도 넣는 것을 권장한다. 나중에 복층, 단차, 천장 높이 차이, 발코니 다운 플로어 같은 구조를 확장할 때 기존 구조를 깨지 않아도 된다.

필드 설명:

- `id`: 층 고유 ID.
- `name`: 표시용 이름.
- `elevation`: 해당 층 바닥 높이, mm.
- `default_height`: 기본 천장 높이, mm.

## walls

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
  "height": 2400
}
```

벽은 중심선 기준으로 전달한다.

필수 필드:

- `id`: 벽 고유 ID.
- `level_id`: 벽이 속한 층 ID.
- `kind`: `outer`, `inner`, `virtual` 중 하나.
- `centerline`: 벽 중심선. 현재는 2점 직선 벽을 기본으로 한다.
- `thickness`: 벽 두께, mm.
- `height`: 벽 높이, mm.

권장 사항:

- 서버는 코너를 메우기 위해 벽 길이를 임의로 늘리거나 줄이지 않는다.
- 벽끼리 만나는 자연스러운 접합 처리는 언리얼 또는 생성 엔진에서 수행한다.
- 같은 벽은 버전이 바뀌어도 가능한 같은 `id`를 유지한다.

## spaces

기존 `rooms`보다 `spaces`라는 이름을 권장한다. 침실, 거실, 화장실뿐 아니라 현관, 발코니, 복도, 드레스룸, 다용도실까지 모두 공간으로 다룰 수 있기 때문이다.

```json
{
  "id": "space_living_001",
  "level_id": "level_1",
  "kind": "living_room",
  "name": "Living Room",
  "boundary": [
    { "x": 4200, "y": 3600 },
    { "x": 9000, "y": 3600 },
    { "x": 9000, "y": 8500 },
    { "x": 4200, "y": 8500 }
  ],
  "boundary_walls": [
    "wall_002",
    "wall_003",
    "wall_004",
    "wall_005"
  ],
  "floor_material": "wood"
}
```

필수 필드:

- `id`: 공간 고유 ID.
- `level_id`: 공간이 속한 층 ID.
- `kind`: 공간 타입.
- `boundary`: 바닥 생성을 위한 공간 외곽 폴리곤.

권장 필드:

- `name`: 표시용 이름.
- `boundary_walls`: 이 공간을 둘러싸는 벽 ID 목록.
- `floor_material`: 기본 바닥 머티리얼 힌트.

권장 `kind` 값:

- `living_room`
- `kitchen_dining`
- `bedroom`
- `bathroom`
- `balcony`
- `entrance`
- `dress_room`
- `utility`
- `corridor`
- `storage`
- `unknown`

`boundary` 규칙:

- 점은 도면 좌표계 기준 순서대로 정렬되어야 한다.
- 자기 교차가 없어야 한다.
- 최소 3개 이상의 점을 가져야 한다.
- 같은 공간 안에 구멍이 필요한 경우 추후 `holes` 배열을 추가할 수 있다.
- 언리얼은 `boundary`를 기준으로 바닥과 천장을 생성한다.

## openings

문과 창문은 독립 좌표가 아니라 반드시 벽에 종속된 객체로 전달한다.

```json
{
  "id": "door_001",
  "level_id": "level_1",
  "kind": "door",
  "host_wall_id": "wall_002",
  "offset_from": "start",
  "offset_to_center": 2100,
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
  }
}
```

필수 필드:

- `id`: 개구부 고유 ID.
- `level_id`: 개구부가 속한 층 ID.
- `kind`: `door`, `entrance_door`, `sliding_door`, `window`, `opening` 중 하나.
- `host_wall_id`: 개구부가 붙는 벽 ID.
- `offset_from`: 현재는 `start` 고정 권장.
- `offset_to_center`: 벽 시작점에서 개구부 중심까지의 거리, mm.
- `width`: 벽 방향 기준 개구부 폭, mm.
- `height`: 개구부 높이, mm.
- `bottom`: 바닥 기준 개구부 하단 높이, mm.

권장 필드:

- `connects`: 이 개구부가 연결하는 공간 ID 목록. 외부와 연결되면 `exterior`를 사용한다.
- `swing`: 문 여닫이 정보.

문 예시:

- 일반 방문: `kind=door`, `bottom=0`, `height=2100`
- 현관문: `kind=entrance_door`, `connects=["space_entrance_001", "exterior"]`
- 미닫이문: `kind=sliding_door`, `swing.direction=sliding`

창문 예시:

- 창문: `kind=window`, `bottom=900`, `height=1200`

## swing

```json
{
  "direction": "inward",
  "hinge": "left",
  "angle": 90
}
```

필드 설명:

- `direction`: `inward`, `outward`, `sliding`, `none`.
- `hinge`: `left`, `right`, `none`.
- `angle`: 여닫이문 각도. 일반적으로 `90`.

`inward`와 `outward`는 `connects` 첫 번째 공간을 기준으로 해석하는 것을 권장한다. 예를 들어 `connects=["space_living_001", "space_bedroom_001"]`이고 `direction="inward"`이면 첫 번째 공간 쪽으로 열린다는 뜻이다.

## 검증 규칙

서버는 JSON 전달 전 아래 항목을 검증하는 것을 권장한다.

- `unit`은 `mm`여야 한다.
- `coordinate_system.rotation_degrees`는 `0`이어야 한다.
- 모든 `id`는 같은 배열 안에서 중복되지 않아야 한다.
- 모든 `level_id`는 `levels`에 존재해야 한다.
- 모든 `host_wall_id`는 `walls`에 존재해야 한다.
- 모든 `connects`의 space ID는 `spaces`에 존재해야 한다. 단, `exterior`는 예외로 허용한다.
- 모든 `offset_to_center`는 대상 벽 길이 범위 안에 있어야 한다.
- 모든 opening은 `width > 0`, `height > 0`이어야 한다.
- 문 폭이 900mm보다 과하게 작으면 도면 스케일 인식 오류 가능성이 있으므로 서버에서 재검토한다.
- 모든 `space.boundary`는 자기 교차가 없어야 한다.
- 모든 `space.boundary`는 최소 3개 이상의 점을 가져야 한다.
- 바닥 생성 대상 공간은 반드시 `boundary`를 가져야 한다.

## 서버 책임

- 원본 도면을 정방향으로 정규화한다.
- mm 단위 좌표로 변환한다.
- 벽 중심선과 두께를 계산한다.
- 공간 폴리곤을 계산한다.
- 문, 창문, 현관문을 구분한다.
- 개구부가 어떤 벽에 붙는지 계산한다.
- 가능하면 개구부가 어떤 공간들을 연결하는지 계산한다.
- 버전이 바뀌어도 같은 물리 객체의 ID를 최대한 유지한다.

## 언리얼 책임

- mm 좌표를 언리얼 좌표계와 단위로 변환한다.
- 벽 중심선과 두께로 3D 벽 메시를 생성한다.
- 벽 접합부를 자연스럽게 처리한다.
- `spaces.boundary`로 바닥과 천장을 생성한다.
- `openings`를 기준으로 문과 창문을 설치한다.
- 선택, 충돌, 머티리얼, 미니맵, 2D 편집 표시를 처리한다.
- 저장 시 변경된 객체를 같은 ID 기준으로 delta화한다.

## v3 대비 변경점

- `rooms`를 `spaces`로 변경한다.
- `openings.wall_id`를 `host_wall_id`로 더 명확히 표현한다.
- `distance_from_wall_start`를 `offset_to_center`로 변경한다.
- `openings.bottom`을 추가해 문과 창문의 높이 기준을 통일한다.
- `openings.connects`를 추가해 문/창문이 연결하는 공간 관계를 표현한다.
- `levels`를 추가해 단층뿐 아니라 향후 복층/단차 구조까지 확장 가능하게 한다.
- `boundary_walls`를 권장 필드로 추가해 공간과 벽의 연결 관계를 명시한다.

## 서버팀 전달 요약

장기적으로 가장 안정적인 방식은 벽, 공간, 개구부를 각각 독립 객체로 전달하고 ID로 연결하는 토폴로지 기반 JSON이다. 언리얼은 벽 선분만으로 방을 추론하지 않고, 서버가 전달한 `spaces.boundary`로 바닥을 생성한다. 문과 창문은 절대 좌표가 아니라 `host_wall_id + offset_to_center` 기준으로 배치한다. 추가로 `connects`를 제공하면 문이 어느 공간과 어느 공간을 연결하는지 알 수 있어 편집, 저장, 머티리얼, 자동 배치 기능이 안정적으로 확장된다.

권장 고정값은 `mm`, `top_left`, `x-right`, `y-down`, `rotation 0`이다.
