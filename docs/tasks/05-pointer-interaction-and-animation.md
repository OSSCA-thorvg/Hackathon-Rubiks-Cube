# Phase 5: Pointer Interaction and Animation

## Status

`Not started`

## Objective

Browser pointer 입력을 cube-local move로 해석하는 interaction을 구현합니다.
Pointer down 시 어느 face의 어느 cell을 잡았는지 판정(picking)하고, drag 중에는 해당 layer를 연속 각도로 회전시켜 보여주며(transient rotation), release 시 가장 가까운 90° 배수로 snap animation을 재생한 뒤에만 `CubeState`에 commit합니다.

```text
 PointerDown(x, y)          PointerMove(x, y)            PointerUp()
        │                          │                          │
        ▼                          ▼                          ▼
   Picking: ray cast        DragResolver:              Snap: angle →
   face + cell 판정         axis lock, layer,          nearest 90° 배수
        │                   angle 갱신                       │
        ▼                          ▼                          ▼
   InteractionState ──────▶ ActiveRotation ──────▶ animation 완료 시
   (state machine)          {axis, layers, angle}     CubeState.apply()
                                   │
                                   ▼
                     build_cube_scene(state, active)
                     회전 layer + body face 렌더링
```

DESIGN.md 8절의 원칙을 그대로 따릅니다: `CubeState`는 항상 90° 단위 discrete 상태만 가지며, drag 중의 `23.7°` 같은 연속 각도는 별도의 visual state(`ActiveRotation`)입니다. Phase 4가 끝난 시점에 interaction은 "어느 layer를 얼마나 돌릴지"만 결정하면 되도록 준비되어 있고, 이 phase가 그 결정을 구현합니다.

Commit에 이르는 경로는 정상 release 하나뿐입니다. `pointercancel` 같은 비자발적 종료는 **cancel 경로**로 분리되어 logical state를 절대 변경하지 않습니다.

## Scope

- `engine/src/interaction/`: picking, drag 해석, interaction state machine, snap animation, cancel 경로
- `graphics::ActiveRotation`과 `std::optional<ActiveRotation>`을 받는 `build_cube_scene(state, active)` overload
- 회전 중 노출되는 절단면(body face) 렌더링
- `math::inverse` 추가 (unprojection에 필요; vendored linalg의 `inverse()` 위임)
- C ABI 확장: `pointer_down`, `pointer_move`, `pointer_up`, `pointer_cancel`, `advance`
- Web: `PointerController.ts`(pointer capture, 좌표 변환, cancel 처리)와 animation frame loop
- Interaction을 별도 Meson target으로 분리하고 `tests/interaction/` 추가
- Drag gesture를 수행하고 전개도 전수 검증으로 결과를 확인하는 browser e2e
- DESIGN.md에 phase 문서 우선 원칙과 지금까지의 차이(notation parser, hit test, drag API)를 주석으로 기록

## Out of scope

- 빈 공간 drag의 camera orbit (DESIGN 7절의 확장 항목) — 빈 공간 drag는 이 phase에서 아무 일도 하지 않습니다
- Multi-touch gesture — 두 번째 pointer는 무시합니다 (아래 pointer 계약 참고)
- 관성(flick), 키보드 입력에 의한 move
- Scramble, reset, solved 판정, timer, UI 버튼 (Phase 6)
- 전개도에서의 interaction — 전개도는 계속 표시 전용입니다
- Drag 감도나 animation 곡선의 사용자 설정
- N ≠ 3 지원 UI (도메인과 interaction은 N-generic이지만 engine이 만드는 것은 여전히 N = 3뿐입니다)
- Model transform을 이용한 cube 전체 회전 — Phase 5에서 model은 identity로 유지됩니다

## Architecture decisions

### Interaction module and dependency direction

`engine/src/interaction`은 `cube`, `math`, `graphics`에 의존하는 Meson target입니다.

- 의존 방향은 단방향입니다: `interaction → graphics → cube`, `interaction → math`. graphics는 interaction을 알지 못합니다.
- ThorVG 의존은 여전히 `render`에만 있습니다. Interaction은 pixel이 아니라 기하로 판정하므로 renderer가 필요 없습니다.
- `app`이 interaction과 graphics를 연결합니다: pointer 이벤트를 interaction에 전달하고, interaction이 내놓는 `ActiveRotation`을 `build_cube_scene`에 넘기며, commit된 `CubeMove`를 `CubeState`에 적용합니다.

Camera와 layout `Rect`를 재사용하기 위해 graphics에 의존합니다. 행렬만 plain 타입으로 받는 더 좁은 경계도 가능하지만, unprojection에 필요한 값(fov, eye, viewport)이 결국 camera 그 자체라 분리해도 같은 데이터를 복사해 나르게 됩니다.

### InteractionController interface

Controller는 `CubeState`를 소유하지도 참조하지도 않습니다. Commit은 `CubeMove` 값으로 꺼내 가고 적용은 Application이 합니다 — "commit이 정확히 한 번 일어났는가"를 domain mutation을 들여다보지 않고 반환값만으로 검증할 수 있습니다.

```cpp
namespace rubiks::interaction {

class InteractionController {
public:
    /** cube를 잡았으면 true. Camera와 viewport는 이 시점에 capture한다. */
    [[nodiscard]] bool pointer_down(float x, float y,
                                    const graphics::Camera& camera,
                                    const graphics::Rect& viewport) noexcept;
    void pointer_move(float x, float y) noexcept;
    void pointer_up() noexcept;

    /** Commit 없이 gesture를 버린다. Snapping 중에는 no-op. */
    void cancel() noexcept;

    /** 시간을 전진시킨다. true = 다시 그릴 frame이 남아 있다. */
    [[nodiscard]] bool advance(double elapsed_ms) noexcept;

    /** Animation이 확정한 move를 한 번만 꺼내 간다. 없으면 nullopt. */
    [[nodiscard]] std::optional<cube::CubeMove> take_committed_move() noexcept;

    [[nodiscard]] std::optional<graphics::ActiveRotation>
    active_rotation() const noexcept;
};

}  // namespace rubiks::interaction
```

- Camera와 viewport는 `pointer_down` 시점에 gesture state로 capture합니다. 아래 lifecycle 정책이 resize 시 Dragging을 취소하므로 capture한 값이 stale해질 수 없습니다.
- Frame당 호출 순서는 `advance → take_committed_move → (apply) → active_rotation → render`입니다.

### Picking: ray cast, ThorVG hit test 아님

DESIGN.md 7절과 구현 순서 13번은 "ThorVG hit test 기반 pointerDown"을 언급하지만, 그렇게 하지 않습니다. ThorVG hit test는 renderer 경계 안의 기능이라 interaction이 ThorVG에 의존하게 되고, native test가 renderer 없이 돌 수 없게 됩니다. 대신 **ray cast**로 판정합니다:

1. Pixel 좌표를 cube viewport `Rect` 기준 NDC로 되돌립니다 (`ViewportPass`의 역변환). Viewport 밖이면 즉시 miss입니다.
2. `math::inverse(projection × view)`로 near plane 위의 NDC 점 `(nx, ny, −1)`을 unproject하고 perspective divide를 거쳐 world-space 점을 얻습니다. NDC 규약은 `math::perspective`가 이미 고정한 OpenGL-style(각 축 `[−1, 1]`, near가 `z = −1`)입니다. Ray는 `eye`에서 그 점을 지나는 방향이고, Phase 5에서 model이 identity이므로 world가 곧 cube-local입니다.
3. Cube의 여섯 바깥 평면(`±kCubeHalfExtent`)과 교차시켜, ray를 마주 보고(`normal · dir < 0`) `t > ε`이며 교점의 면내 두 좌표가 `[−kCubeHalfExtent, +kCubeHalfExtent]` 안에 있는 것 중 가장 가까운 t를 택합니다. `ε`은 명명된 상수 하나로 둡니다.
4. 교점의 면내 좌표를 cell index로 바꿉니다: `floor((c + kCubeHalfExtent) / pitch)`의 **half-open 규칙**(경계 좌표는 큰 index 쪽)이되, 결과를 `[0, N−1]`로 clamp해 `c = +kCubeHalfExtent`가 index `N`이 되는 것을 막습니다. Seam 위를 잡아도 이 규칙이 정하는 칸의 grab입니다.

교점이 없으면 빈 공간이고 gesture는 시작되지 않습니다. Picking은 정지 상태의 기하(항상 축 정렬 cube)만 대상으로 하며, 회전 중에는 state machine이 새 pointer down을 무시하므로 회전된 기하를 향한 ray cast는 필요 없습니다.

`math::inverse`는 현재 math API에 없으므로 이 phase에서 추가합니다. Vendored linalg의 `inverse()`에 위임하는 wrapper이며, `inverse(M) × M = I` known-answer test로 고정합니다.

Contract v3의 3D sample이 그대로 picking known-answer가 됩니다: cube viewport 비율 `(0.50, 0.29)`는 Up 면 중앙 cubie `(1, 2, 1)`을, `(0.31, 0.61)`은 Front 면 중앙 `(1, 1, 2)`를, `(0.69, 0.61)`은 Right 면 중앙 `(2, 1, 1)`을 잡아야 합니다.

### Drag resolver: 후보 축 점수화와 axis lock

잡은 face의 normal을 `n`(축 `a`)이라 하면 회전 후보 축은 나머지 두 축 `u`, `v`입니다. 각 후보의 "손가락이 그 방향으로 움직이면 이 축 회전"이 되는 화면 방향을 다음으로 구합니다.

1. Grab 지점 `p`에서 후보 축 회전의 속도 벡터를 계산합니다 (도메인 부호 규약 반영, 아래 참고).
2. 그 world-space 벡터를 같은 view·projection·viewport로 화면에 투영해 2D 방향을 얻습니다.
3. 누적 drag 벡터와의 내적 절대값이 점수입니다.

누적 drag 거리가 **dead zone**(cube viewport 폭의 1% 내외, 상수로 정의)을 넘는 순간 점수가 큰 축으로 **axis lock**을 걸고, gesture가 끝날 때까지 바꾸지 않습니다. 중간에 축이 바뀌면 layer가 통째로 튀므로 lock이 안정적입니다. 경계 규칙 두 가지를 고정합니다:

- **Tie-break**: 두 점수가 같으면 `Axis` enum 값이 작은 축을 택합니다. 규칙 자체보다 결정적이라는 사실이 중요하며 test로 고정합니다.
- **Dead zone 이동분 포함**: angle은 항상 **down 지점부터의 전체 변위**를 lock된 화면 방향에 투영해 계산합니다. Lock 순간 dead zone만큼의 각도(감도 기준 2° 미만)가 즉시 반영되는데, "angle = 전체 변위의 투영"이라는 단일 공식이 lock 시점 기준점을 따로 저장하는 것보다 상태가 적습니다.

Dead zone 안에서 release하면 move 없이 끝납니다.

- **Layer**: lock된 축에 대한 grab cell의 좌표가 layer index입니다. `layers = layer(index)` 단일 layer만 만듭니다 (wide/slice gesture는 scope 밖).
- **Angle**: 감도는 "cube viewport 폭의 절반 drag = 90°"를 기본값으로 하는 튜닝 가능한 상수입니다.
- **부호 규약**: `ActiveRotation.angle`은 `CubeMove.quarter_turns`와 같은 규약(축의 양의 끝에서 원점을 바라볼 때 시계 방향이 양수)을 씁니다. 도메인의 +1 quarter turn은 오른손 규약 기준 −90° 회전이므로, 렌더링 quaternion으로 바꿀 때 한 곳에서만 부호를 뒤집습니다. 부호가 흩어지면 반드시 어긋나므로 known-answer test로 고정합니다: **Front 면 오른쪽 열 `(2, 1, 2)`를 잡고 위로 drag하면 `{X, layer(2), angle > 0}`** — 잡은 sticker가 손가락을 따라 Up으로 넘어가는 R 방향입니다.

부호 검증은 한 gesture로 끝내지 않습니다. 고정 camera에서 잡을 수 있는 면은 보이는 세 면(Up, Front, Right)뿐이므로, **3면 × 후보 2축 × 양·음 방향 = 12 케이스**를 table-driven test로 전수 고정합니다.

### Interaction state machine

```text
        down: cube hit                  up: axis lock 됨
 Idle ──────────────────▶ Dragging ──────────────────▶ Snapping
  ▲                          │                            │
  │   up: lock 전 (no-op)    │                            │ animation 종료:
  ├──────────────────────────┤                            │ commit 후 복귀
  │   cancel: commit 없이    │                            │
  ├──────────────────────────┘                            │
  └───────────────────────────────────────────────────────┘
```

- `Idle`: pointer down이 cube에 맞으면 `Dragging`으로. 빈 공간이면 그대로.
- `Dragging`: move마다 angle 갱신. up 시 axis lock이 없으면 commit 없이 `Idle`, 있으면 `Snapping`.
- `Snapping`: 새 pointer down/move는 **무시**합니다. Snap 시간이 짧아(90°당 수백 ms) 입력 queue의 가치보다 규칙의 단순함이 큽니다.
- Commit: snap 목표는 `round(angle / 90°)` quarter turns입니다. DESIGN의 예시 그대로 `37° → 0`, `67° → 90°`, `143° → 180°`이고, 0이면 apply 없이 끝납니다. Drag가 90°를 넘게 누적되면 quarter_turns가 2 이상이 될 수 있으며 도메인이 이미 mod 4로 처리합니다.

Commit 직후 `ActiveRotation`이 비활성이 되고 논리 상태가 같은 그림을 그리므로 화면은 연속입니다.

### Cancel: commit에 이르지 않는 종료 경로

정상 `pointerup`만 commit으로 이어질 수 있습니다. 브라우저 스크롤 개입, pointer 장치 전환, capture 상실, page teardown 같은 비자발적 종료가 사용자가 놓지 않은 회전을 commit하면 안 되므로, `cancel()`을 별도 경로로 둡니다.

- `Dragging`에서 `cancel()`: transient rotation을 **즉시 제거**하고 commit 없이 `Idle`로 돌아갑니다. Snap-back animation을 재생하지 않는 이유는 teardown 중에는 frame loop가 이미 없어서이며, 취소 경로를 애니메이션 유무로 두 갈래 내느니 하나로 통일합니다.
- `Snapping`에서 `cancel()`: **no-op**입니다. Pointer는 이미 정상적으로 release되었고 사용자 의도(snap 완료)가 확정된 뒤이므로 animation은 끝까지 진행됩니다.
- `cancel()`은 어떤 상태에서도 `CubeState`를 변경하지 않습니다.

### Lifecycle: resize와 shutdown 중의 interaction

Drag 중 창 resize가 일어나면 gesture state에 capture된 viewport와 화면 방향은 이전 좌표계 기준인데 이후 pointer 좌표는 새 drawing buffer 기준이라, 그대로 두면 각도가 튑니다. 정책을 고정합니다:

- **Resize는 Dragging을 commit 없이 취소합니다** (`cancel()`과 같은 경로). Engine `resize()`가 내부에서 수행하므로 web이 순서를 챙길 필요가 없습니다.
- **Snapping은 resize 후에도 계속됩니다.** Snap은 각도만 animation하고 화면 좌표를 읽지 않으므로 viewport와 무관합니다.
- **DPR 변경은 resize와 동일하게 취급합니다.** 기존 web 경로가 DPR 변경을 이미 drawing buffer resize로 전달하므로 추가 작업은 없습니다.
- **`shutdown()`은 interaction과 animation state를 모두 초기화합니다.** 기존의 `placement`·`cube_state` reset과 같은 자리입니다.
- Resize 실패 시 기존 application teardown 정책(복구 시도 없이 해체)이 그대로 적용됩니다.

### ActiveRotation: graphics가 소유하는 visual state

```cpp
namespace rubiks::graphics {
struct ActiveRotation {
    cube::Axis axis;
    cube::LayerMask layers;
    float angle_degrees;  // quarter_turns와 같은 부호 규약
};
}
```

DESIGN.md 8절의 "visual state는 graphics 소유"를 그대로 따릅니다. Interaction이 만들어 app을 거쳐 graphics로 전달되며, 이 방향이면 graphics가 interaction을 알 필요가 없습니다. 도메인 타입(`Axis`, `LayerMask`)은 graphics가 이미 의존하는 cube의 것을 재사용합니다.

비활성 상태는 `angle == 0`이 아니라 **`std::optional<ActiveRotation>`의 `nullopt`**입니다. `angle == 0`에 "transient 없음"과 "활성 gesture가 마침 0°"라는 두 의미를 실으면 기본 생성된 `axis`·`layers`가 의미 없는 값으로 흘러 다니게 됩니다. 계약:

- `nullopt`: transient rotation 없음 (`Idle`)
- 값 있음: `Dragging` 또는 `Snapping` (0°인 순간 포함)

### Transient rendering과 body face

`build_cube_scene(const CubeState&, const std::optional<ActiveRotation>&)` overload를 추가합니다.

- **`nullopt`이면 기존 `build_cube_scene(state)`를 그대로 호출합니다.** 단일 인자 함수가 정지 상태의 canonical 구현으로 남으므로, contract v3의 byte 단위 동일성이 "회전 수학이 0°에서 항등이길 바라는 것"이 아니라 코드 경로 자체로 보장됩니다.
- **Sticker 회전**: `layers`에 속한 cubie의 sticker quad는 world 좌표에서 cube 중심 기준으로 `angle`만큼 회전한 뒤 pipeline에 들어갑니다. 회전은 `math::Quaternion`으로 만들며 부호 변환은 이 한 곳에서만 일어납니다.
- **Body face**: 회전 중에는 절단면이 노출됩니다. 축 방향으로 이웃한 두 layer의 mask 소속이 다르면 그 사이가 절단면이고, 절단면에 접한 cubie마다 그쪽을 향한 사각형을 방출합니다. Sticker와 같은 footprint(`kStickerScale`)로 그려 `append_sticker`를 재사용하고, 색은 배경과 여섯 sticker 색 모두와 구분되는 어두운 중립색 상수입니다. 회전하는 쪽의 body face는 sticker처럼 함께 회전합니다.
- 회전 중인 scene은 non-convex이지만, Phase 3가 바로 이 경우를 위해 depth sort를 미리 넣어 두었습니다. 평균 view-space z 정렬이라 45° 부근에서 이론적 edge case가 있지만 실제 camera 각도에서 관찰되는 결함이 있을 때만 대응합니다.
- 전개도는 **논리 상태만** 그립니다. 회전 중에도 전개도는 commit 전 상태를 보여주고, commit 순간 새 상태로 바뀝니다. 전개도는 "큐브의 이산 상태를 한눈에"가 목적이라 연속 각도를 섞을 이유가 없고, 덕분에 e2e에서 gesture 결과 검증 도구로 그대로 쓸 수 있습니다.

### Snap animation과 주입되는 시간

Engine은 시계를 직접 읽지 않습니다. 시간은 boundary에서 `advance(elapsed_ms)`로 주입되며, native test는 고정 dt를 반복 주입해 animation을 결정적으로 검증합니다.

- Snap은 release 시점 angle에서 목표 angle까지 smoothstep easing으로 보간합니다. Duration은 남은 각도에 비례(90°당 200ms 내외, 최소값 있음)하는 튜닝 가능한 상수입니다.
- **Duration 상한은 필요 없습니다.** 목표가 가장 가까운 90° 배수이므로 남은 각도는 구조적으로 항상 45° 이하이고, duration은 그에 비례해 자동으로 유계입니다. 이 유도 성질을 test로 고정합니다.
- **정확히 ±45°는 0에서 먼 쪽으로 반올림합니다** (`std::round`의 half-away-from-zero): `+45° → +90°`, `−45° → −90°`.
- Release 시점 angle이 이미 목표와 같아도 즉시 commit하지 않고 **다음 `advance()`에서** commit합니다. Commit 경로를 "animation 종료 frame" 하나로 유지하기 위해서입니다.
- `advance`는 "다시 그릴 필요가 있는가"를 반환합니다. `Dragging`과 `Snapping`에서 true(commit이 발생한 종료 frame 포함), `Idle`에서 false입니다.
- **dt 검증**: 음수이거나 non-finite인 `elapsed_ms`는 0으로 취급합니다. 상한(수백 ms, 상수)으로 clamp합니다 — clamp 없이도 큰 dt는 "animation 즉시 완료"라는 올바른 결과를 내지만, BFCache 복귀나 background tab처럼 비정상적으로 큰 값이 한 번에 들어와도 거동이 예측 가능하도록 고정합니다.

### C ABI와 web boundary

Phase 4까지 동결했던 C ABI를 이 phase에서 확장합니다. 기존 함수의 계약은 바뀌지 않습니다.

```c
int  thorvg_rubiks_pointer_down(float x, float y);  // 1 = cube를 잡음
void thorvg_rubiks_pointer_move(float x, float y);
void thorvg_rubiks_pointer_up(void);
void thorvg_rubiks_pointer_cancel(void);
int  thorvg_rubiks_advance(double elapsed_ms);       // 1 = 재렌더링 필요
```

- 좌표는 **drawing buffer pixel**입니다. CSS pixel과 devicePixelRatio 변환은 web이 담당합니다 — 기존 resize 경로와 같은 분담입니다.
- **입력 검증**: non-finite(`NaN`, `±Infinity`) 좌표가 들어온 이벤트는 무시합니다. `elapsed_ms` 규칙은 위 snap 절과 같습니다.
- 초기화 전 호출은 모두 안전합니다: 반환값이 있는 `pointer_down`·`advance`는 0을 반환하고, `pointer_move`·`pointer_up`·`pointer_cancel`은 no-op입니다.

**`web/src/input/PointerController.ts`** (DESIGN.md 3절의 자리 그대로)의 계약:

- **Primary pointer의 주 버튼**(`isPrimary && button === 0`)만 gesture를 시작하고, active pointer가 있는 동안 다른 pointer의 이벤트는 무시합니다.
- `pointer_down()`이 1을 반환한 경우에만 `setPointerCapture`를 걸고 frame loop를 시작합니다. 빈 공간 클릭은 capture도 loop도 만들지 않습니다.
- 좌표 변환은 `getBoundingClientRect()` 기준입니다: `(clientX − rect.left) × canvas.width / rect.width` (y도 동일). CSS 크기와 buffer 크기의 비율이 DPR을 흡수합니다.
- Active pointer의 `pointermove`/`pointerup`만 engine에 전달합니다 (`pointerId` 비교).
- `pointercancel`과 capture 중의 `lostpointercapture`는 `pointer_cancel`로 전달합니다. `pointer_up`이 아닙니다.
- Canvas에 `touch-action: none`을 적용해 touch 기기에서 drag가 스크롤로 새지 않게 합니다.
- Teardown은 capture 해제, listener 제거, 예약된 rAF 취소를 모두 수행합니다.

**Frame loop**: 지금은 render가 초기화·resize 시에만 호출되지만, 이 phase부터 **gesture나 animation이 활성인 동안만** `requestAnimationFrame` loop를 돌립니다. 매 frame `advance(dt)` 후 `render()`를 호출하고, `advance`가 0을 반환하면 마지막 한 frame을 그리고 loop를 멈춥니다. 상시 rAF loop를 돌리지 않는 이유는 정지 상태에서 CPU를 쓰지 않기 위해서입니다. Loop 소유는 `AppLifecycle.ts`이며 resize·BFCache와 마찬가지로 teardown이 loop와 pointer listener를 함께 해제합니다.

## Rendered scene contract

Contract v3은 **정지 상태 그대로 유지**됩니다. 이 phase는 정지 상태의 pixel 출력을 한 byte도 바꾸지 않습니다 (`nullopt` 경로가 기존 builder를 그대로 호출하므로 구조적으로 보장됩니다).

새로 추가되는 검증은 gesture 수준입니다.

1. **Gesture → 상태 검증**: e2e가 Front 면 오른쪽 열에서 위로 drag하는 R gesture를 수행하고, snap 완료를 기다린 뒤 전개도 54칸 전수 검증으로 R 적용 상태와 비교합니다. 전개도가 논리 상태만 그리므로 이 검증이 picking → drag 해석 → snap → commit → 렌더링 전체 경로를 덮습니다.
2. **Snap-back 검증**: dead zone을 넘되 45° 미만인 작은 drag 후 release하면 solved 상태의 contract v3 전체가 다시 성립해야 합니다.
3. **Transient 검증**: drag 중(90° 미만 각도) cube 영역의 pixel이 정지 상태와 달라야 합니다. 특정 pixel 값 대신 "영역이 달라졌다"만 봅니다 — 연속 각도의 정확한 픽셀은 native `RenderScene` test가 검증하고, e2e는 경로가 살아 있는지만 봅니다.

Drag 좌표는 contract의 cube viewport 비율에서 유도하므로, camera나 layout이 바뀌면 contract와 함께 재유도해야 합니다.

## Implementation steps

### 1. Interaction 도메인

- [ ] `math::inverse` wrapper 추가 (`inverse(M) × M = I` known-answer test 포함)
- [ ] Viewport 역변환과 unprojection으로 pointer ray 생성
- [ ] Ray와 cube 바깥 평면의 교차로 `(face, cell)` picking 구현 (`t > ε`, half-open cell 규칙, `[0, N−1]` clamp)
- [ ] 후보 축 화면 투영과 점수화, dead zone, axis lock, tie-break 구현
- [ ] Down 지점 기준 전체 변위 투영으로 angle 계산 구현 (감도 상수 포함)
- [ ] `Idle / Dragging / Snapping` state machine 구현
- [ ] `cancel()` 경로 구현: Dragging에서 commit 없이 transient 즉시 제거, Snapping에서 no-op
- [ ] Snap 목표 산출(half-away-from-zero 반올림)과 smoothstep animation 구현
- [ ] Animation 종료 frame에서 commit을 보관하고 `take_committed_move()`로 전달
- [ ] 입력 검증 구현: non-finite 좌표 무시, dt 정규화와 clamp
- [ ] Interaction을 `cube`·`math`·`graphics` 의존 Meson target으로 분리

### 2. Transient rendering

- [ ] `graphics::ActiveRotation` 정의
- [ ] `build_cube_scene(state, optional<ActiveRotation>)` overload: `nullopt`이면 기존 함수 위임, 값이 있으면 회전 layer의 sticker 회전
- [ ] Mask 경계에서 body face 방출
- [ ] Body 색 상수 추가 (배경·sticker 색과 구분)
- [ ] Application이 `InteractionController`를 소유: pointer 전달, `advance` 후 commit 적용, `ActiveRotation` 전달
- [ ] Engine `resize()`가 Dragging을 취소하고, `shutdown()`이 interaction state를 초기화

### 3. Boundary 연결

- [ ] C ABI에 `pointer_down/move/up/cancel`, `advance` 추가 (초기화 전 안전)
- [ ] `PointerController.ts`: primary pointer 필터, capture, 좌표 변환, cancel·lostpointercapture 처리
- [ ] Canvas에 `touch-action: none` 적용
- [ ] `AppLifecycle.ts`: 활성 시에만 도는 rAF loop와 teardown 통합 (rAF 취소 포함)
- [ ] `CubeEngine.ts`에 새 ABI wrapper 추가

### 4. Verification

- [ ] Picking known-answer test: contract sample 3점이 각 면 중앙 cell을 잡는지
- [ ] Picking 경계 test: 배경 miss, viewport 밖 miss, seam grab, `+kCubeHalfExtent` 경계 clamp
- [ ] Drag resolver table-driven test: 보이는 3면 × 후보 2축 × 양·음 = 12 케이스 부호 전수 검증
- [ ] 축 선택 test: lock 후 안정성, 점수 동률 tie-break
- [ ] State machine test: 빈 공간 down 무시, lock 전 up은 no-commit, Snapping 중 입력 무시
- [ ] Cancel test: Dragging 중 cancel 후 `CubeState` 불변·`active_rotation() == nullopt`, Snapping 중 cancel은 animation 계속
- [ ] Lifecycle test: Dragging 중 resize는 commit 없이 취소, Snapping 중 resize는 계속, shutdown 후 초기화
- [ ] Snap test: `37° → 0`, `67° → 90°`, `143° → 180°`, 음수 각도, `±45°` half-away-from-zero
- [ ] Commit 동치 test: 90°로 끝난 gesture 후 상태 == `apply(moves::R(3))` 상태, `take_committed_move()`가 정확히 한 번 값을 반환
- [ ] Animation test: 고정 dt 주입으로 유한 시간 내 종료, 남은 각도 ≤ 45° 유도 성질, `advance` 반환값 전이
- [ ] 입력 검증 test: non-finite 좌표·dt, 음수 dt, 거대 dt clamp
- [ ] Transient scene test: `nullopt`이면 기존 scene과 동일, 45°에서 sticker 회전과 body face 수(N = 3 단일 layer 절단면 18개) 확인
- [ ] ABI test: 초기화 전 `pointer_down`·`advance`는 0 반환, `pointer_move`·`pointer_up`·`pointer_cancel`은 안전한 no-op
- [ ] TS unit test: 좌표 변환, hit일 때만 capture·loop 시작, 다른 pointer 무시, cancel·lostpointercapture 전달, teardown의 rAF 취소
- [ ] e2e: R gesture 후 전개도 전수 검증, 작은 drag snap-back, drag 중 pixel 변화
- [ ] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- Interaction target은 `cube`·`math`·`graphics`만 link하고 ThorVG를 include하지 않으며, interaction test는 renderer 없이 돕니다.
- `CubeState`는 여전히 discrete 상태만 가지며, 연속 각도는 `ActiveRotation`에만 존재하고, 비활성은 `nullopt`으로 구분됩니다.
- Commit에 이르는 경로는 정상 release 하나뿐입니다: `cancel()`, resize, shutdown은 어떤 상태에서도 `CubeState`를 변경하지 않습니다.
- Picking·drag 해석·snap·commit이 모두 native test로 검증되고, 부호 규약이 12 케이스 table-driven test로 고정됩니다.
- 정지 상태(`nullopt`)의 렌더링이 기존 builder를 그대로 호출하므로 contract v3이 수정 없이 통과합니다.
- 회전 중에는 절단면이 body 색으로 채워져 내부가 비어 보이지 않습니다.
- Browser에서 drag gesture로 R를 수행하면 snap 후 전개도가 R 적용 상태와 일치하고, 작은 drag는 원상 복귀합니다.
- Animation 시간은 boundary에서 주입되며 engine은 시계를 읽지 않고, 비정상 입력(non-finite, 음수, 거대 dt)이 정의된 대로 처리됩니다.
- rAF loop는 gesture·animation 활성 중에만 돌고 정지 상태에서 멈춥니다.
- 기존 C ABI 함수의 계약은 변하지 않습니다.
- Native unit test, TypeScript unit test, WASM build, browser e2e와 Vite production build가 모두 통과합니다.
- Camera orbit, scramble, reset, UI 버튼 코드는 이 phase에 포함되지 않습니다.

## Verification commands

```bash
# Native build and tests
meson setup build/native
meson compile -C build/native
meson test -C build/native --print-errorlogs

# WASM build
source /path/to/emsdk/emsdk_env.sh
./build_wasm.sh

# TypeScript boundary unit tests
npm --prefix web run test:unit

# Browser e2e (build_wasm.sh 이후)
npm --prefix web run test:e2e

# Vite production build
npm --prefix web run build
```

## Completion

모든 acceptance criteria와 verification command를 통과한 뒤 다음 작업을 수행합니다.

- 이 문서의 status를 `Completed`로 변경합니다.
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 5를 완료 처리합니다.
- Phase 6 gameplay and UI 세부 문서를 작성합니다.
