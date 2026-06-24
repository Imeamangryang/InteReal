# Unreal Plan JSON Contract v3

이 문서는 서버에서 언리얼로 도면 데이터를 전달할 때 권장하는 JSON 형식을 정의한다. 목표는 언리얼이 도면을 추론하거나 회전, 반전, 스케일을 임의 보정하지 않고 그대로 생성할 수 있게 만드는 것이다.

예시 파일은 `TestData/ideal_plan_example_v3.json`에 있다.

## 핵심 원칙

- 단위는 항상 `mm`를 사용한다.
- 좌표계는 항상 `top_left` 원점, `x_axis=right`, `y_axis=down`, `rotation_degrees=0`을 사용한다.
- 언리얼 쪽 변환은 한 곳에서만 수행한다. 서버 JSON에는 언리얼 보정용 회전값이나 임시 스케일 값을 넣지 않는다.
- 벽, 방, 개구부를 분리해서 전달한다.
- 방 바닥 생성을 위해 `rooms.boundary`는 필수로 전달한다.
- 문과 창문은 독립 좌표보다 `wall_id`와 `distance_from_wall_start` 기준으로 전달한다.
- 문, 창문, 현관문은 `type`으로 명확히 구분한다.

## 최상위 구조

```json
{
  "schema_version": "3.0",
  "unit": "mm",
  "coordinate_system": {},
  "plan": {},
  "defaults": {},
  "bounds": {},
  "outline": [],
  "walls": [],
  "rooms": [],
  "openings": []
}
```

## coordinate_system

```json
{
  "origin": "top_left",
  "x_axis": "right",
  "y_axis": "down",
  "rotation_degrees": 0
}
```

서버는 위 값을 고정해서 전달한다. 도면 이미지가 회전되어 있더라도 JSON 좌표는 보정된 최종 도면 기준이어야 한다.

## plan

```json
{
  "id": 6,
  "version": 1,
  "name": "ideal_apartment_example"
}
```

- `id`: 서버의 plan id.
- `version`: 현재 delta/version 번호.
- `name`: 선택값이지만 디버깅을 위해 권장한다.

## bounds

```json
{
  "min": { "x": 0, "y": 0 },
  "max": { "x": 12000, "y": 8500 }
}
```

전체 도면의 외접 박스다. 뷰 정렬, 미니맵, 카메라 초기 위치 계산에 사용한다.

## outline

```json
[
  { "x": 0, "y": 0 },
  { "x": 12000, "y": 0 },
  { "x": 12000, "y": 3600 },
  { "x": 10500, "y": 3600 },
  { "x": 10500, "y": 8500 },
  { "x": 0, "y": 8500 }
]
```

전체 세대 외곽 폴리곤이다. 외벽 선분과 함께 제공하면 검증과 바닥 클리핑에 사용할 수 있다.

## walls

```json
{
  "id": "wall_outer_001",
  "type": "outer",
  "start": { "x": 0, "y": 0 },
  "end": { "x": 12000, "y": 0 },
  "thickness": 200,
  "height": 2400
}
```

필수 필드:

- `id`: 개구부가 참조할 수 있는 고유 ID.
- `type`: `outer` 또는 `inner`.
- `start`, `end`: 벽 중심선의 시작점과 끝점.
- `thickness`: 벽 두께, mm.
- `height`: 벽 높이, mm.

벽 좌표는 중심선 기준을 권장한다. 벽 두께를 고려한 실제 메시 확장은 언리얼에서 처리한다.

## rooms

```json
{
  "id": "room_living_001",
  "type": "living_room",
  "name": "Living Room",
  "boundary": [
    { "x": 3600, "y": 3600 },
    { "x": 7600, "y": 3600 },
    { "x": 7600, "y": 8500 },
    { "x": 3600, "y": 8500 }
  ],
  "floor_material": "wood"
}
```

`rooms.boundary`는 필수다. 언리얼이 벽 선분만 보고 방 영역을 추론하면 L자 구조, 복도, 발코니, 화장실처럼 작은 영역에서 바닥이 깨질 가능성이 높다.

권장 room type:

- `living_room`
- `kitchen_dining`
- `bedroom`
- `bathroom`
- `balcony`
- `entrance`
- `dress_room`
- `utility`
- `corridor`

## openings

문과 창문은 벽에 종속해서 전달한다.

```json
{
  "id": "door_001",
  "type": "door",
  "wall_id": "wall_inner_001",
  "distance_from_wall_start": 1300,
  "width": 900,
  "height": 2100,
  "swing": {
    "direction": "inward",
    "hinge": "right",
    "angle": 90
  }
}
```

```json
{
  "id": "window_001",
  "type": "window",
  "wall_id": "wall_outer_001",
  "distance_from_wall_start": 1000,
  "width": 2200,
  "height": 1200,
  "sill_height": 900
}
```

필수 필드:

- `id`: 고유 ID.
- `type`: `door`, `entrance_door`, `window` 중 하나.
- `wall_id`: 부착될 벽 ID.
- `distance_from_wall_start`: 벽 시작점에서 개구부 중심까지의 거리, mm.
- `width`: 개구부 폭, mm.
- `height`: 개구부 높이, mm.

문 전용 권장 필드:

- `swing.direction`: `inward`, `outward`, `sliding`, `none`.
- `swing.hinge`: `left`, `right`, `none`.
- `swing.angle`: 일반 여닫이문은 `90`.

창문 전용 권장 필드:

- `sill_height`: 바닥에서 창 하단까지의 높이, mm.

## 검증 규칙

서버에서 전달 전 아래 항목을 검증하는 것을 권장한다.

- 모든 `wall.id`는 중복되지 않아야 한다.
- 모든 `room.id`는 중복되지 않아야 한다.
- 모든 `opening.id`는 중복되지 않아야 한다.
- 모든 `opening.wall_id`는 실제 `walls`에 존재해야 한다.
- `distance_from_wall_start`는 `0`보다 크고 벽 길이보다 작아야 한다.
- 문 폭이 900mm보다 작게 인식된 경우 서버에서 원본 도면 스케일을 재검토한다.
- `rooms.boundary`는 최소 3개 이상의 점을 가져야 하고, 자기 교차가 없어야 한다.
- `rooms.boundary`는 전체 `outline` 안에 있어야 한다.
- `unit`은 반드시 `mm`여야 한다.
- `coordinate_system.rotation_degrees`는 반드시 `0`이어야 한다.

## 서버와 언리얼의 책임 분리

서버 책임:

- 도면 이미지 또는 원본 데이터를 해석해 실제 도면 방향으로 정규화한다.
- mm 단위 좌표를 생성한다.
- 벽, 방, 문, 창문을 구분한다.
- 방 영역 폴리곤을 전달한다.
- 문과 창문을 어느 벽에 붙일지 결정한다.

언리얼 책임:

- mm를 언리얼 단위로 변환한다.
- 벽 중심선과 두께로 3D 벽 메시를 생성한다.
- `rooms.boundary`로 바닥과 천장을 생성한다.
- `openings`로 문과 창문 메시를 설치한다.
- 머티리얼, 선택, 충돌, 미니맵 표시를 처리한다.

## 서버팀 전달 요약

언리얼에서는 벽 선분만으로 방 영역을 추론하지 않는 방향이 가장 안정적이다. 서버에서 `rooms.boundary`를 필수로 내려주고, 문과 창문은 `wall_id + distance_from_wall_start` 방식으로 전달해주면 회전, 반전, 바닥 깨짐, 문/창문 오인식 문제를 크게 줄일 수 있다. 좌표계는 `top_left / x-right / y-down / mm / rotation 0`으로 고정한다.
