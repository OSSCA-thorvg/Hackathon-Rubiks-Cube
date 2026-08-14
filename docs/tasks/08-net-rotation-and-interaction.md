# Phase 8: Net Rotation and Interaction

## Status

`In progress` — 링 설계로 개정 (v2)

**1차 구현 (`5913e33`, 완료)**: 이 문서의 원래 설계대로 축별 3전략(Y축 가로 슬라이드, F layer 링 강체 회전, 나머지는 제자리 cross-fade 축약)의 렌더링과, 전개도 interaction 전체(점-사각형 picking, cube topology 기반 축·부호 유도, 각도 ±90° clamp, 기존 snap/commit 경로 재사용, pointer 라우팅)를 구현해 전 test를 통과했습니다. 그중 **interaction은 이번 개정에서도 그대로 유효**합니다. 개정하는 것은 회전을 그리는 방식입니다: 축약 전략이 "색만 바뀌고 돌아가는 것으로는 보이지 않는다"는 시각 품질 문제가 확인되어, 세 전략을 링 하나로 통일합니다. 1차 설계의 상세 결정 기록은 문서 끝 개정 기록에 있습니다.

## Objective

한 번의 layer 회전이 전개도 위에서 **닫힌 링을 따라 도는 이동**으로 보이게 합니다.

모든 (axis, layer)의 밴드 12칸은 하나의 닫힌 곡선 위의 슬롯입니다. 회전 진행도만큼 각 셀이 곡선을 따라 이동하고(1/4 턴 = 3슬롯 = 링의 1/4바퀴), 축 위의 도는 면은 제자리 회전합니다. 기존 3전략은 전부 이 규칙의 특수한 경우가 되어, 렌더링 로직의 분기가 곡선 데이터로 내려갑니다.

셀을 누르면 그 셀이 돌 수 있는 두 링이 안내선으로 표시되고, 회전하면 예고된 바로 그 선을 따라 움직입니다. 같은 링 기계로 그리는 세 번째 화면(링 다이어그램 view)은 [Phase 8.5](./08.5-ring-diagram-view.md)로 분리했습니다.

## Scope

- 표면 스티커 identity와 슬롯 순환을 유도하는 순수 domain 함수 (`cube/Surface`; 기존 `rotated`·`sticker_cycle`의 노출)
- (axis, layer) → 닫힌 cardinal spline loop 생성과 밴드 셀의 곡선 추종 이동 (3전략 switch와 handover 잔재 대체)
- 출발 슬롯 접선 대비 상대 회전의 셀 기울기
- Renderer 경계 확장: `RenderScene`의 stroke 목록과 ThorVG cubic path / stroke 렌더링
- Scene 합성 helper: `faces`와 `strokes`를 함께 합치기
- 셀 press 시 두 후보 링의 안내선 표시, 방향 lock 후 한 링, release/cancel 시 제거
- rect 이탈 비례 alpha fade 유지와 안내선의 bounds 정책 (cube 영역 무침범)
- Native scene test 개정과 browser e2e 유지

## Out of scope

- 링 다이어그램 view — [Phase 8.5](./08.5-ring-diagram-view.md)로 분리. 교차점 topology 계약과 ViewMode 전 계층 migration이 이 phase에 담기에 큽니다
- 3D view에의 안내선 표시
- 전개도 interaction의 변경 — 1차 구현 그대로
- Quad를 잘라내는 clipping — 만들지 않습니다 (fade로 대신)
- N ≠ 3 지원 — Phase 15

## Architecture decisions

### 모든 링은 12개 resting slot을 지나는 닫힌 cubic loop

처음 초안은 Z축을 "F 중심의 동심원"으로 정의했으나 **기하적으로 성립하지 않습니다**: F 밴드의 12개 셀 중심은 F 중심에서 √5, 2, √5 cell 거리에 있어 한 원 위에 있지 않습니다 (v1의 강체 회전도 하나의 원이 아니라 셀마다 반지름이 다른 원호였습니다). 슬롯을 지나지 않는 곡선은 0°/90° 일치 계약과 충돌하므로, 정의를 뒤집습니다: **곡선이 슬롯에 맞춥니다.** 어느 축이든 링은 12개 resting slot을 순환 순서로 지나는 닫힌 cubic loop이고, 축별 차이는 loop의 모양뿐입니다.

| 축 | Loop의 모양 | 흡수되는 기존 전략 |
|---|---|---|
| Z (F/S/B) | F 블록을 감싸는 **중첩된 둥근 사각형 고리** 3개 | v1의 F층 강체 회전이 만들던 궤적의 근사. 모서리가 꺾이는 곳은 면과 면 사이라 슬롯은 전부 변 위에 놓입니다 |
| Y (U/E/D) | 가로줄 12칸을 지나 좌우로 돌아오는 **가로 고리** | 직선 구간 = 기존 슬라이드 그대로. 좌우 캡이 화면 밖 wrap을 대체해, 셀이 fade로 사라졌다 나타나는 대신 고리를 돌아서 갑니다 |
| X (R/M/L) | U-F-D 열을 지나 cross의 **빈 모서리 블록을 통과해** B 열과 이어지는 세로 고리 | 축약(v1)과 handover(작업 중이던 개선)가 하던 "끊긴 곳 처리"를 눈에 보이는 경로로 대체합니다 |

- **1/4 턴 = 3슬롯 = 링의 1/4바퀴.** 90° 회전은 밴드를 한 면 폭(3칸)만큼 순환시키므로, 어느 링이든 진행도→이동량 변환이 같습니다.
- **셀의 기울기는 절대 접선이 아니라 상대 회전입니다**: `현재 위치의 접선 − 출발 슬롯의 접선`만큼 돕니다. 0°에서 기울기가 정확히 0이라 axis-aligned quad가 유지되고, 직선 구간에서는 돌지 않아 슬라이드의 모양이 유지됩니다.
- 90° 도착이 픽셀까지 일치하는 근거는 **3칸 떨어진 두 슬롯의 접선 차가 90°의 배수**라는 것입니다 — 정사각형 quad에는 보이지 않습니다. 실제로 Z 고리는 0° 또는 90°(변 안 / 모서리 하나 통과), X 고리의 연결부는 180°, Y 고리의 캡은 360°입니다. ("슬롯의 접선이 전부 축 정렬"이라는 더 강한 성질은 모서리 곡선이 슬롯 **사이**에만 있을 때만 참이고, 슬롯을 지나는 spline이면 각 변 끝 슬롯의 접선이 살짝 대각이 됩니다. 계약에 필요한 것은 위의 약한 성질뿐이므로 생성기를 그쪽으로 묶지 않습니다.)
- 진행도는 한 칸(90°)에서 포화하고 net gesture의 각도 clamp도 1차 그대로입니다.

### 생성기는 닫힌 cardinal spline 하나입니다

곡선을 만드는 방법으로 control point를 축마다 손으로 잡는 대신 **닫힌 cardinal spline(Catmull-Rom)** 을 씁니다. 입력은 축마다 "12슬롯 + 연결 waypoint 몇 개"라는 **점 목록** 하나뿐이고, 나머지는 전부 같은 코드입니다. Z는 waypoint가 없어도 12슬롯만으로 둥근 고리가 되고, Y는 좌우 캡에, X는 두 연결부에 각각 몇 개가 붙습니다.

이렇게 고르는 이유는 세 가지입니다.

- **C1이 구성상 참입니다.** 접선이 이웃 점의 차분으로 정의되므로 join에서 자동으로 이어집니다. 기울기가 접선에 의존하는 이상 C1은 반드시 필요한데, 이것을 24개 control point를 맞춰 가며 test로 확인할 일이 아닙니다. C1 test는 설계 과제가 아니라 값싼 회귀 test로 남습니다.
- **튜닝 손잡이가 tension 하나**로 줄어, 아래 bounds 문제를 조일 수 있습니다.
- 축별 분기가 데이터로 완전히 내려갑니다 — "같은 생성기"가 말뿐이 아니게 됩니다.

### 슬롯 순환은 domain의 표면 topology에서

`net_step_turn()`은 {axis, layer, sign}만 반환하므로 "다음 슬롯이 어디인가"를 알지 못합니다 — 12-cycle을 만드는 데 부족합니다. 그리고 **스티커의 목적지를 색으로 대조할 수 없습니다**: `CubeState`가 위치에서 읽어 주는 것은 여섯 가지 `FaceColor`뿐이라, 한 면 안에서 순서가 틀려도 색은 같아 test를 통과합니다. 색은 identity가 아닙니다. 그래서 스티커에 identity를 줍니다.

```cpp
// cube/Surface.hpp — 화면을 모르는 순수 domain.

/** 큐브 표면의 스티커 한 장: 어느 cubie의 어느 면인가. */
struct SurfaceSticker { int x, y, z; Face face; };

/** 양의 quarter turn 뒤 그 스티커가 놓이는 자리. */
SurfaceSticker turned_sticker(const SurfaceSticker&, Axis axis, int size) noexcept;

/** 한 layer가 순환시키는 12장을, 양의 회전 방향 순서로. */
RingSlots ring_slots(Axis axis, int layer, int size);
```

**이 회전은 새로 쓰는 것이 아니라 이미 있는 것을 노출하는 것입니다.** `CubeState.cpp`의 익명 namespace에 위치 순열 `rotated(axis, Position, last)`와 면 순환 `sticker_cycle(axis)`가 이미 있고, 그 둘이 정확히 `turned_sticker()`입니다.

```cpp
SurfaceSticker turned_sticker(const SurfaceSticker& s, Axis axis, int size) noexcept
{
    const auto p = rotated(axis, Position{s.x, s.y, s.z}, size - 1);
    // 축에 수직인 두 면은 순환에 없으므로 그대로 남습니다.
    return SurfaceSticker{p.x, p.y, p.z, next_in_cycle(axis, s.face)};
}
```

- **표면 이웃 stepping으로 successor를 다시 유도하지 않습니다.** 그것은 같은 회전의 세 번째 구현이 되고(첫째가 `rotated`, 둘째가 test의 손표), 세 벌이 어긋날 자리를 하나 더 만드는 일입니다.
- **그래서 검증도 가벼워집니다.** `rotated`/`sticker_cycle`은 이미 실물 대조 known-answer로 고정돼 있습니다 — solved 3×3에서 R이 UFR을 UBR로 보내고 위가 초록, 뒤가 흰색. 그 위에 `turned_sticker()`가 더하는 것은 노출과 배선뿐이므로, 54 × 3축 × 양·음 전수 손표 대신 **손으로 확인한 known-answer 몇 개 + 구조 성질**(54장 위의 전단사, 4회 적용 시 항등, 밴드 membership 보존)로 충분합니다. 위치가 이미 검증된 함수에서 나오므로 "한 면 안에서 순서가 틀리는" 오류가 애초에 생길 수 없고, 이것이 색 대조로는 잡히지 않던 바로 그 구멍이었습니다.
- **`SurfaceSticker`는 `NetCell`과 같은 struct입니다.** `NetGeometry.hpp`의 `NetCell{int x, y, z; Face face;}`와 필드까지 동일하므로 쌍둥이를 만들지 않습니다. `cube/`에 정의하고 graphics 쪽은 `using NetCell = cube::SurfaceSticker;` 한 줄로 별칭을 둡니다(`cube`는 graphics를 볼 수 없으므로 방향은 이쪽뿐입니다).
- **`ring_slots()`도 짧아집니다.** 밴드에 걸린 한 면의 3칸을 travel 순서로 늘어놓고 거기에 `turned_sticker()`를 반복 적용하면 12장이 순서대로 나옵니다. 별도의 순열표가 없습니다.
- **`cube/`에 두는 이유**: 슬롯 순환은 화면이 아니라 큐브의 성질이고, Phase 8.5의 링 다이어그램이 같은 순환에 다른 좌표만 얹습니다. 화면 좌표를 아는 `NetGeometry`에 두면 8.5가 이름부터 net에 묶인 것에 의존하게 됩니다. `cube` 모듈이 의존성을 갖지 않는다는 불변식도 그대로입니다 — 인덱스와 순열뿐입니다.
- **canonical cycle 하나만** 반환합니다. 음의 방향은 호출부가 index를 거꾸로 밟으므로 방향 인자가 없습니다.
- `net_step_turn()`은 gesture 방향 → move 변환용으로 그대로 둡니다. 역할이 다릅니다.
- `NetGeometry`는 슬롯을 화면 좌표로 옮기고 곡선을 만드는 일만 합니다.

### 링은 ThorVG의 cubic path, 안내선은 ThorVG의 stroke

**Renderer 경계를 넓힙니다.** 지금까지 `RenderScene`은 채워진 quad 목록 하나였고, 곡선 안내선을 그리려면 짧은 선분으로 쪼개 얇은 사각형 사슬로 근사해야 했습니다. 그러나 ThorVG는 `cubicTo`와 `strokeWidth`/`strokeFill`/`strokeCap`/`strokeJoin`을 이미 갖고 있고, **이 프로젝트가 ThorVG 쇼케이스**라는 점에서 엔진이 잘하는 일을 다각형으로 흉내내는 것은 정확히 반대 방향입니다. 진짜 곡선과 진짜 stroke를 쓰면 anti-aliasing과 round join·cap을 렌더러가 처리하고, 확대·굵기 변화에도 품질이 유지되며, 코드에서 "이건 선이다"가 그대로 읽힙니다.

경계에 더하는 것은 순수 2D 자료구조입니다 — 시작점, cubic 구간의 목록, 닫힘 여부, 굵기, 색. `RenderScene`이 ThorVG type을 포함하지 않는다는 Phase 3의 원칙은 유지되고, ThorVG 호출은 여전히 renderer 안에만 있습니다.

```cpp
/** 이전 점에서 이어지는 cubic 한 구간. */
struct RenderSegment { math::Vec2 control_a, control_b, to; };

/** 채우지 않고 선으로 그리는 경로. */
struct RenderStroke {
    math::Vec2 start;
    std::vector<RenderSegment> segments;
    bool closed;
    float width;
    Color color;
};

struct RenderScene {
    std::vector<RenderFace> faces;
    std::vector<RenderStroke> strokes;  // faces 위에 그립니다
};
```

**그래서 링은 자료구조 하나입니다.** 슬롯 사이를 잇는 cubic 구간의 닫힌 목록이고, 같은 것을 두 곳이 씁니다: 셀 이동은 구간을 매개변수로 평가해 위치와 접선을 얻고, 안내선은 그 경로를 그대로 `RenderStroke`로 내보냅니다. 근사도 중복도 없으므로 예고선과 실제 궤적이 어긋날 수 없습니다.

ThorVG의 `Shape`은 fill의 초기 alpha가 0이므로 stroke 전용 shape은 `fill()`을 부르지 않으면 됩니다.

안내선의 수명은 gesture입니다: press로 두 후보 링이 나타나고, 방향이 lock되면 도는 링 하나만 남고, release나 cancel로 사라집니다. 유지되는 선택 상태를 따로 두지 않으므로 새 상태 기계가 없습니다.

### 안내선의 합성과 bounds

두 가지가 조용히 어긋날 수 있어 명시합니다.

- **합성**: `Application::render()`는 지금 net scene의 `faces`만 복사해 합칩니다. 이대로면 `strokes`는 그려지지 않고 사라집니다. `faces`와 `strokes`를 함께 옮기는 작은 `append_scene()` helper를 두고 모든 합성이 그것을 지나게 합니다.
- **Bounds**: stroke는 faces 위에 그려지므로, 링이 cube 영역까지 뻗으면 Both mode에서 안내선이 3D cube 위를 가로지릅니다 — "3D view 안내선은 범위 밖"이라는 계약과 충돌합니다. 따라서 링의 경로는 **net rect와 그 얇은 배경 여백 안**에 둡니다. 안내선이 전개도의 다른 면 위를 지나는 것은 허용입니다 — 참조 이미지의 링들이 정확히 그렇게 그려져 있고, 그것이 이 표현의 의도입니다.
- **좁은 쪽은 좌우가 아니라 위입니다.** `Layout.hpp`의 Both mode 수치로 cube는 net **위**에 있고(cube 하단 `0.59·unit`, net 상단 `0.62·unit`), 여유는 `0.03·unit` — 셀 하나가 `0.04·unit`이니 **약 0.75칸**입니다. Y 고리의 캡이 좌우로 나가는 자리가 안전한 것은 net이 cube보다 좁아서가 아니라(오히려 net 0.48이 cube 0.58보다 좁습니다) 그 높이에 cube가 없기 때문입니다. 정작 그 0.75칸을 노리는 곡선이 둘 있습니다: X 고리의 U-top → B-top 연결 arc와, **Z 바깥 고리(B층)의 모서리 arc** — 후자는 U 윗줄 오른쪽 끝에서 R 오른쪽 열 위 끝까지 대각으로 3칸을 건너뛰므로 바깥으로 불룩해집니다. 그래서 위 여백이 실질 예산이고, spline의 tension을 overshoot가 그 안에 들어오도록 잡습니다.
- X 고리의 다리는 cross의 빈 모서리 블록 (2,0)·(3,0)과 (2,2)·(3,2)을 통과시키면 좌우로는 rect 안에서 닫힙니다.
- 이 정책은 raster test로 고정합니다: **Both mode에서 안내선을 표시하기 전후로 cube rect의 버퍼가 byte 단위로 같아야** 합니다. stroke 색과 정확히 일치하는 픽셀을 찾는 검사는 anti-aliasing으로 배경과 섞인 침범 픽셀을 놓치고, 버퍼 비교는 그것까지 잡으면서 구현도 더 단순합니다.

### fade 유지

셀의 rect 이탈 비례 fade는 안전망으로 유지합니다. Y 고리의 캡을 도는 셀과 U·D 블록의 제자리 회전 모서리가 rect를 벗어나는 곳이고, 셀이 화면에서 흐려지는 유일한 구간입니다.

## Implementation steps

### 1. 링 topology

- [ ] `cube/Surface.{hpp,cpp}`: `SurfaceSticker` identity와 `turned_sticker()` — `CubeState.cpp`의 `rotated()`·`sticker_cycle()`을 노출해 조립 (새 회전 구현 금지)
- [ ] `NetCell`을 `cube::SurfaceSticker`의 별칭으로 바꾸고 중복 struct 제거
- [ ] `turned_sticker()` test: 손으로 확인한 known-answer 몇 개 + 구조 성질(54장 전단사, 4회 항등, 밴드 membership 보존)
- [ ] `ring_slots(axis, layer, size)`: 한 면의 3칸을 travel 순서로 놓고 `turned_sticker()` 반복 적용 — canonical positive cycle 하나
- [ ] `ring_slots()` test: 12슬롯이 밴드와 일치, 3칸 이동이 `CubeState::apply` 결과와 모순 없음, 4회 적용 시 제자리
- [ ] `net_step_turn()`은 gesture용으로 유지, interaction 호출 확인

### 2. 링 기계

- [ ] 닫힌 cardinal spline 생성기 하나: 점 목록(12슬롯 + 연결 waypoint) → cubic 구간의 닫힌 목록. 축별 차이는 점 목록과 tension뿐 (Z 중첩 고리 / Y 가로 고리 / X 세로 고리)
- [ ] 밴드 셀의 곡선 추종 이동 + 상대 접선 기울기로 교체하며 v1/handover 잔재 폐기: `NetStrategy`와 전략 switch, handover의 arriving/leaving quad, 그 전용 test
- [ ] 양끝 일치 test 통과 (기존 `require_ends_match` 무수정 유지가 목표)
- [ ] 성질 test: 12슬롯 endpoint 정확성; **3칸 떨어진 슬롯의 접선 차가 90°의 배수**(양끝 일치의 근거); 구간 join의 위치 연속성과 C1 접선 연속성(생성기가 보장하므로 회귀 test); 이동 중 셀이 사라지지 않음(rect 밖 fade 제외); 45° known-answer는 축 family당 대표 하나만
- [ ] fade 유지 test

### 3. Stroke 경계와 안내선

- [ ] `RenderSegment` / `RenderStroke`와 `RenderScene::strokes` 추가
- [ ] ThorVG renderer가 stroke를 `moveTo`/`cubicTo`/`close` + `strokeWidth`/`strokeFill`/round cap·join으로 그리기 (faces 위에)
- [ ] Renderer test: 곡선 위의 sample point가 선 색, 곡선에서 떨어진 점은 배경
- [ ] `append_scene()` helper로 `faces`와 `strokes`를 함께 합치기
- [ ] Controller가 net gesture의 선택 상태(눌린 셀, lock 여부)를 노출
- [ ] press 시 두 후보 링, lock 시 한 링을 같은 링 자료구조에서 stroke로 내보내기
- [ ] tension을 overshoot가 net rect 위 여백(약 0.75칸) 안에 들어오도록 조정
- [ ] Both mode에서 안내선 표시 전후 cube rect 버퍼가 동일한 raster test
- [ ] release/cancel 시 사라지는 scene test

### 4. Verification

- [ ] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- 정지 상태(`nullopt`)의 전개도 렌더링은 기존 코드 경로를 그대로 호출하며 기존 contract가 수정 없이 통과합니다.
- 어느 축의 회전이든 밴드 셀이 12개 resting slot을 지나는 닫힌 링을 따라 이동하고, 0°와 90°에서 논리 상태와 정확히 일치합니다.
- 셀이 화면에서 흐려지는 구간은 net rect를 벗어나는 구간뿐이고, 이탈 비례 fade로 점진적입니다.
- 셀을 누르면 두 후보 링의 안내선이, 방향이 lock되면 도는 링의 안내선이 보이고, release/cancel로 사라집니다. 안내선과 실제 이동 경로는 같은 링 자료구조입니다.
- Both mode에서 안내선은 cube rect를 침범하지 않습니다.
- 전개도의 셀 drag → snap/commit 경로와 ±90° clamp는 1차 구현 그대로 동작합니다.
- 링은 ThorVG의 cubic path와 stroke로 그려지고, `RenderScene`은 ThorVG type을 포함하지 않으며, ThorVG 호출은 renderer 안에만 있습니다.
- Native, WASM, TypeScript unit, browser e2e와 production build가 모두 통과합니다.

## Verification commands

```bash
meson test -C build/native --print-errorlogs
source /path/to/emsdk/emsdk_env.sh && ./build_wasm.sh
npm --prefix web run test:unit
npm --prefix web run test:e2e
npm --prefix web run build
```

## Completion

모든 acceptance criteria와 verification command를 통과한 뒤 다음 작업을 수행합니다.

- 이 문서의 status를 `Completed`로 변경합니다.
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 8을 완료 처리합니다.
- 실제 구현과 차이가 생긴 결정을 이 문서에 기록합니다.

## 개정 기록

### v2 (진행 중): 링 설계로 개정

- 사유: 시각 품질. v1의 축약 전략은 X축 전부와 Z축 B·S의 띠를 제자리 cross-fade로 그렸는데, 사용자 검토에서 "색만 바뀔 뿐 돌아가는 것으로 보이지 않는다"고 확인되었습니다. 밴드를 닫힌 링 위의 이동으로 통일하면 세 전략의 분기 자체가 사라지고, 모든 회전이 연속된 운동으로 보입니다.
- 함께 추가: 안내선(누른 셀이 돌 수 있는 링 표시). 링 다이어그램 view는 [Phase 8.5](./08.5-ring-diagram-view.md)로 분리.
- **Renderer 경계 개정**: 안내선을 얇은 quad 사슬로 근사하려던 초안을 버리고 ThorVG의 cubic path와 stroke를 직접 씁니다. 소비자가 하나라는 이유로 경계를 좁게 유지하려 했으나, ThorVG 쇼케이스에서 엔진의 stroke를 우회해 다각형으로 흉내내는 것이 앞뒤가 맞지 않고, 진짜 곡선이 확장성·품질·가독성 모두에서 낫다는 판단입니다. Phase 3 문서의 `RenderScene` 절에 개정을 기록했습니다.
- **외부 리뷰 반영 (설계 확정 전)**:
  - Z축 "동심원" 정의는 기하 오류였습니다 — 밴드 셀 중심이 한 원 위에 있지 않습니다(F 중심에서 √5, 2, √5). "모든 링 = 12슬롯을 지나는 닫힌 cubic loop"로 정의를 뒤집었습니다.
  - 셀 기울기를 절대 접선에서 **출발 슬롯 접선 대비 상대 회전**으로 수정 — 0°에서 axis-aligned 보장.
  - `net_step_turn()`은 {axis, layer, sign}만 반환해 슬롯 순환을 만들 수 없습니다. `ring_slots()`를 별도 함수로 분리하고, successor는 표면 이웃 stepping으로 유도. (2차 리뷰에서 이 함수의 자리가 `NetTopology`가 아니라 domain이어야 한다는 점이 다시 드러났습니다 — 아래 참고.)
  - 안내선의 합성(`strokes`가 조용히 사라지는 문제 → `append_scene()`)과 bounds(cube rect 무침범 정책 + raster test)를 명시.
  - 45° 전수 known-answer를 성질 검증 + family당 대표 하나로 완화 — control point 튜닝마다 test 재계산을 피합니다.
  - 링 다이어그램 view는 교차점 topology 계약(54 스티커 = 링 2개씩의 교차점)과 ViewMode 전 계층 migration이 커서 Phase 8.5로 분리.
- **2차 리뷰 반영**: 스티커의 목적지를 `CubeState`의 색으로 대조하려던 계약은 성립하지 않습니다 — 6색을 54장이 나눠 가지므로 면 안 순서 오류를 통과시킵니다. `SurfaceSticker` identity를 도입하고, 그 김에 슬롯 순환 전체를 화면을 모르는 `cube/Surface`로 옮겼습니다(8.5가 net에 묶인 이름에 의존하지 않게 되는 것이 덤). `ring_slots()`는 canonical positive cycle 하나만 반환합니다. 곡선 join의 C1 접선 연속성 test와, 안내선 bounds를 색 일치 대신 버퍼 비교로 고정하는 것도 함께 반영했습니다.
- **3차 검토 반영 (구현 직전, 코드 대조)**:
  - `turned_sticker()`는 새 구현이 아니라 노출입니다 — `CubeState.cpp`에 `rotated()`와 `sticker_cycle()`이 이미 있고 실물 대조 known-answer로 고정돼 있습니다. "표면 이웃 stepping" 유도(세 번째 구현)와 54 × 3축 × 양·음 전수 손표를 폐기하고, known-answer 몇 개 + 구조 성질로 줄였습니다. `ring_slots()`도 "한 면 3칸 + `turned_sticker()` 반복"으로 짧아집니다.
  - `SurfaceSticker`가 기존 `NetCell`과 같은 struct임을 확인하고 별칭으로 통합 — 쌍둥이 타입을 만들지 않습니다.
  - 곡선 생성기를 **닫힌 cardinal spline**으로 고정. C1이 구성상 참이 되어 설계 과제가 회귀 test로 내려가고, 튜닝 손잡이가 tension 하나로 줄며, 축별 분기가 점 목록으로 완전히 내려갑니다.
  - 기울기 계약의 근거를 "슬롯 접선이 전부 축 정렬"에서 **"3칸 떨어진 슬롯의 접선 차가 90°의 배수"** 로 약화 — 앞의 강한 성질은 모서리가 슬롯 사이에 있을 때만 참이라 생성기를 과제약합니다. 약한 쪽으로도 양끝 일치가 성립하며(Z 0°/90°, X 180°, Y 360°) 이제 test 항목입니다.
  - Bounds의 좁은 변이 좌우가 아니라 **위**(cube 하단 0.59 → net 상단 0.62, 약 0.75칸)임을 `Layout.hpp` 수치로 확인하고, 그 예산을 노리는 곡선 둘(X 연결 arc, Z 바깥 고리 모서리 arc)을 명시했습니다.
- v1과 v2 사이에 커밋되지 않은 중간 단계(handover: 끊긴 면 경계를 슬라이드로 넘기고 fade로 처리)가 있었으나, 링 설계가 이를 포함하므로 그 위에 재작업합니다. 재사용: `net_step_turn`의 graphics 이동과 interaction 호출 변경, 전수 topology test. 폐기: `NetStrategy`와 전략 switch, handover 전용 arriving/leaving quad와 그 test.

### v1 (완료, `5913e33`): 축별 3전략

원래 설계(Y 슬라이드 / F 링 / 축약)로 구현·검증 완료. 렌더링 전략은 v2가 대체하지만 아래 결정들은 계속 유효합니다.

- **라우팅은 rect 포함 판정이 아니라 `pick_net` 결과로 분기.** cross의 빈 모서리는 net rect 안이지만 어느 면도 아니므로, Both mode에서 빈 모서리 press는 orbit이 됩니다(화면상 배경). Net mode에서는 돌릴 시점이 없어 아무 일도 하지 않습니다.
- **`net_pointer_down`은 pick을 인자로 받습니다.** "거절 사유는 전부 snap 확정보다 먼저"(Phase 7)를 지키려면 Application이 확정 전에 면 위 여부를 알아야 하고, 판정을 두 번 하면 어긋날 수 있어 한 번 찾아 넘깁니다.
- **`Net` mode의 press 거절은 "전개도 밖"으로 좁혀졌습니다.** 이때는 snap 확정도 일어나지 않습니다.
- **계약이 뒤집힌 test 교체**: ApplicationTest의 net mode 일괄 거부, gameplay-ui의 "Net is display-only", 3D drag 중 전개도 불변 e2e 단언, 회전 중 전개도 픽셀을 읽던 다섯 단언(→ `committed_move_count()`로 대체).
- **e2e 순서 조정**: 전개도는 90°에서 snap 종료보다 먼저 최종 그림에 도달하므로, "잘린 면이 안 보인다" 단언을 poll로 전환.
- 축약 전략의 cross-fade(띠를 alpha 강조 대신 목적지 색으로 혼합)와 "fade가 실제로 걸리는 곳은 F 링이 아니라 슬라이드 wrap과 U·D 블록 모서리"라는 관찰은 v1의 기록으로 남기며, 전자는 v2가 폐기하고 후자는 v2에서도 참입니다.
