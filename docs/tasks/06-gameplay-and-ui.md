# Phase 6: Gameplay and UI

## Status

`Completed`

## Objective

Scramble부터 첫 유효 move, solved 판정과 timer 정지까지 이어지는 최소 gameplay flow를 완성합니다.
Cube state와 move 해석은 계속 C++ engine이 담당하고, Browser는 timer clock, DOM UI, 접근성, responsive layout과 pointer/keyboard event plumbing을 담당합니다.

UI는 ThorVG scene 안에 그리지 않습니다. Canvas를 render surface로 유지하고, timer와 control을 semantic HTML로 만든 DOM HUD로 canvas 주변에 배치합니다. Desktop에서는 view control과 game action을 canvas 좌우 바깥에 나누고, mobile에서는 canvas 아래 normal flow로 전환합니다. 기본 view mode는 3D cube와 전개도를 모두 보여 주는 `Both`이며, 사용자는 `3D`, `Both`, `Net` 세 모드 사이를 전환할 수 있습니다.

```text
                         Browser DOM
           timer / status / buttons / keyboard / a11y
                               │
                               ▼
                      TypeScript GameController
                     clock + UI state transitions
                               │
                               ▼
                         CubeEngine.ts
                         primitive C ABI
                               │
                               ▼
                         C++ Application
        scramble / reset / move count / view mode / rendering
                    │                         │
                    ▼                         ▼
             CubeState + moves        Graphics + ThorVG
             solved / scramble         canvas pixel buffer
```

## Scope

- `cube::CubeState::is_solved()`와 deterministic seeded scramble sequence 생성
- Application의 scramble, cube reset, solved query와 committed user move count
- Pointer와 keyboard/button move가 같은 commit 경로와 solved 판정을 사용하도록 연결
- `graphics::ViewMode`: `Cube3D`, `Both`, `Net`; 기본값은 `Both`
- View mode별 canvas layout과 rendering, mode 전환 시 interaction lifecycle
- Camera 방향만 home으로 되돌리는 별도 `reset_view()` command
- Gameplay와 view command를 위한 C ABI 및 `CubeEngine.ts` 확장
- Browser-owned timer와 `idle → ready → running → completed` gameplay state
- Canvas 위 DOM HUD: timer, status, view selector, Scramble, Reset, Home view
- Keyboard와 DOM move controls: `R/L/U/D/F/B`, Shift 또는 prime control로 inverse
- Loading, ready, unsupported와 error page state
- Semantic HTML, keyboard focus, live announcement와 touch target을 포함한 접근 가능한 UI
- Desktop과 mobile에서 canvas와 control이 겹치지 않는 responsive layout
- Native, TypeScript unit, browser e2e와 production build verification

## Out of scope

- N ≠ 3 제품 지원과 cube size selector
- Scramble notation 표시, 문자열 notation parser와 algorithm 입력
- Solver, hint, undo/redo와 move history UI
- WCA inspection, +2/DNF 규칙과 공식 scramble 생성 규격
- Best time, local storage, account, leaderboard와 network backend
- Scramble sequence 자체의 animation
- Zoom, pinch, inertia, camera roll과 keyboard camera orbit
- Theme editor, sticker palette 설정과 sound/haptic feedback
- Screen reader만으로 54개 sticker 전체를 탐색하는 비시각적 cube representation
- Net에서 sticker를 직접 눌러 move하는 interaction

접근성 범위는 주변 UI와 모든 gameplay command를 keyboard/DOM control로 사용할 수 있게 하는 데까지입니다. Canvas 속 3D 공간과 54개 sticker를 완전히 비시각적으로 설명하는 별도 representation은 후속 작업입니다.

## Requirements

### 1. Ownership and dependency direction

기존 dependency 원칙을 유지합니다.

- `cube`는 solved 판정과 scramble move 생성만 알고 graphics, ThorVG, Browser를 알지 못합니다.
- `interaction`은 pointer와 programmatic move를 animation하고 완료된 `CubeMove`만 Application에 전달합니다.
- `graphics`는 `ViewMode`에 맞는 layout과 scene을 만들지만 gameplay 상태와 timer를 알지 못합니다.
- Application은 cube state, interaction, orbit과 view mode를 조정하는 유일한 engine composition root입니다.
- Browser는 clock과 UI state만 소유하며 sticker나 move 결과를 자체 계산하지 않습니다.
- ThorVG 의존성은 계속 renderer boundary에만 둡니다.

### 2. Solved-state contract

`CubeState`에 명시적인 query를 추가합니다.

```cpp
class CubeState {
public:
    /** Returns true when every exposed sticker matches its solved face. */
    [[nodiscard]] bool is_solved() const noexcept;
};
```

- 판정은 외부에 노출되는 여섯 face의 sticker를 검사합니다.
- 숨은 중앙/내부 cubie 값은 solved 판정의 관찰 대상이 아닙니다.
- 판정은 animation 중 `ActiveRotation`을 보지 않고, commit된 discrete state만 봅니다.
- 초기 상태와 reset 직후는 solved입니다.
- 한 move와 scramble 이후에는 unsolved, sequence와 inverse를 적용한 뒤에는 solved여야 합니다.
- Application이나 Browser에서 `CubeState(size)`를 매번 만들어 비교하지 않습니다. 도메인 의미는 cube module 한 곳에 둡니다.

### 3. Deterministic scramble

Scramble은 cube module의 순수한 move generator로 구현합니다.

```cpp
/** Number of moves in a normal 3x3 scramble. */
inline constexpr std::size_t kScrambleMoveCount = 20;

/** Builds a reproducible outer-face scramble for an N x N x N cube. */
[[nodiscard]] std::vector<CubeMove> make_scramble(
    int size, std::uint32_t seed,
    std::size_t move_count = kScrambleMoveCount);
```

- Browser가 `std::uint32_t` seed를 만들고 C++에 전달합니다.
- Production seed source는 `crypto.getRandomValues()`이며 unit test는 injectable seed source를 사용합니다.
- Native와 WASM에서 같은 seed가 같은 sequence를 만들도록 repository-owned `uint32_t` PRNG와 index 선택 규칙을 사용합니다. `std::random_device`와 구현별 결과가 달라질 수 있는 distribution에 의존하지 않습니다.
- Move는 바깥 layer 하나만 선택하고 quarter turn은 `-1`, `+1`, `2` 중 하나입니다.
- 연속 move는 같은 axis를 선택하지 않습니다. 같은 face 반복과 바로 이웃한 반대 face의 교환 가능한 move를 함께 피합니다.
- Application은 생성한 sequence를 atomic하게 적용합니다. 결과가 우연히 solved라면 deterministic fallback move를 추가해 gameplay가 반드시 unsolved 상태에서 시작되게 합니다.
- Scramble은 committed user move count에 포함하지 않습니다.
- Scramble은 진행 중인 pointer/programmatic interaction을 버리고 이전 timer session을 대체합니다.
- Camera와 현재 view mode는 보존합니다. Cube와 시점을 함께 초기화하는 의미를 `Reset` 하나에 섞지 않습니다. Gesture를 버리기 전에 아직 frame에 반영되지 않은 orbit delta를 먼저 소비하므로, 보존은 마지막 sweep까지 포함합니다.

### 4. Engine gameplay state

Application은 Browser가 timer 전이를 결정할 수 있는 최소 query만 제공합니다.

```cpp
/** Replaces the cube with a deterministic unsolved scramble. */
[[nodiscard]] bool scramble(std::uint32_t seed) noexcept;

/** Restores only the logical cube and clears the current solve session. */
void reset_cube() noexcept;

/** Returns whether the committed logical state is solved. */
[[nodiscard]] bool is_solved() noexcept;

/** Number of user moves committed since the latest scramble. */
[[nodiscard]] std::uint32_t committed_move_count() noexcept;

/** Restores the turntable camera without changing the cube or view mode. */
void reset_view() noexcept;

/** Returns whether a gesture, animation, or unconsumed commit is active. */
[[nodiscard]] bool is_busy() noexcept;
```

`committed_move_count`의 규칙은 다음과 같습니다.

- Initialize, reset과 scramble 직후에는 `0`입니다.
- Pointer drag가 non-zero `CubeMove`로 commit될 때 한 번 증가합니다.
- Keyboard/DOM move가 commit될 때도 같은 위치에서 한 번 증가합니다.
- Dead-zone tap, snap-back to zero, orbit, view change와 camera reset은 증가시키지 않습니다.
- Quarter turn이 `2`인 half turn도 사용자 command 한 번이므로 한 move입니다.
- Scramble sequence의 내부 move 수는 포함하지 않습니다.
- `shutdown()` 후 재초기화하면 solved, count `0`, home camera, `Both`로 돌아갑니다.

### 5. Programmatic face turns

Pointer 없이도 같은 cube를 조작할 수 있어야 keyboard와 DOM button이 실질적인 대체 입력이 됩니다.

```cpp
/** Starts one animated face turn when the interaction controller is idle. */
[[nodiscard]] bool start_move(const cube::CubeMove& move) noexcept;
```

- `InteractionController::start_move()`는 idle일 때 `0°`에서 목표 quarter turn까지 snap animation을 시작합니다.
- Pointer drag가 만드는 snap과 같은 easing, duration, `take_committed_move()` 경로를 사용합니다.
- Quarter turn은 크기만 4로 wrap하고 부호는 호출자의 것을 유지합니다. `L2`처럼 음의 face에서 온 half turn도 그 face가 도는 방향으로 animation합니다.
- Cube 범위를 벗어난 layer mask는 거부합니다. Pointer는 pick에서 mask를 만들지만 이 진입점은 받은 값을 검사해야 합니다.
- Gesture 또는 snap이 이미 진행 중이면 false를 반환하고 queue하지 않습니다.
- Application은 `Face`와 inverse 여부를 named move factory의 `CubeMove`로 변환한 뒤 controller에 전달합니다.
- Browser는 command가 수락된 경우에만 frame loop를 시작합니다.
- Button과 keyboard 입력은 engine이 busy일 때 disabled/ignored되고 중복으로 쌓이지 않습니다.
- Animation 완료 뒤 Application의 기존 `advance → take_committed_move → apply` 지점에서 move count와 solved query가 함께 갱신됩니다.

### 6. View modes

세 모드를 명시적인 enum으로 둡니다.

```cpp
namespace rubiks::graphics {

/** Which logical views are rendered into the canvas. */
enum class ViewMode : std::uint8_t {
    Cube3D = 0,
    Both = 1,
    Net = 2,
};

/** Places the visible regions for one view mode. */
[[nodiscard]] CanvasLayout layout(std::uint32_t width,
                                  std::uint32_t height,
                                  ViewMode mode = ViewMode::Both) noexcept;

}  // namespace rubiks::graphics
```

- 기본값은 `Both`입니다. 기본 인자로도 같은 값을 두어 mode를 신경 쓰지 않는 호출부가 contract v3 layout을 그대로 얻습니다.
- Mode 분기는 exhaustive `switch`로 작성해 mode가 늘어나면 컴파일러가 누락을 잡습니다.
- `Both`는 Phase 4 contract v3의 기존 split layout을 그대로 사용합니다. 초기 화면 pixel contract가 바뀌지 않아야 합니다.
- `Cube3D`는 정사각형 3D viewport를 canvas 중앙에 더 크게 배치하고 net을 방출하지 않습니다.
- `Net`은 4×3 net을 canvas 안에서 가능한 크게 중앙 배치하고 3D scene을 방출하지 않습니다.
- `Net`은 표시 전용입니다. 이 모드의 `pointer_down`은 false를 반환하여 invisible cube orbit이나 layer drag를 시작하지 않습니다. *(Phase 8에서 개정 — 아래 참고)*
- Mode 전환은 CubeState, committed move count, timer session과 camera yaw/pitch를 보존합니다.
- Mode 전환 중 `Dragging`/`Orbiting`은 commit 없이 취소합니다. 이미 release되어 `Snapping`인 move는 끝까지 진행하고 commit합니다.
- Net에서 `Cube3D` 또는 `Both`로 돌아오면 보존된 camera로 다시 렌더링합니다.
- Resize는 현재 mode로 layout을 다시 계산합니다.
- 정수 C ABI mode 값은 세 enum 값만 허용하고 잘못된 값은 false를 반환하며 기존 mode를 유지합니다. *(Phase 8.5에서 네 번째 값이 추가됩니다 — 아래 참고)*

> **개정 (Phase 8 / 8.5)**: 이 절의 view mode 계약 두 가지가 이후 phase에서 바뀌었습니다.
>
> - **`Net`은 더 이상 표시 전용이 아닙니다.** [Phase 8](./08-net-rotation-and-interaction.md)이 전개도의 셀 drag로 layer를 돌릴 수 있게 했습니다. 이 mode의 `pointer_down`은 전개도의 면 위 press를 받아들이고, **전개도 밖** press에 대해서만 false를 반환합니다(그 경우 진행 중인 snap도 확정하지 않습니다). 돌릴 시점이 없어 orbit이 없다는 점은 그대로입니다.
> - **네 번째 mode `Rings = 3`이 추가됩니다.** [Phase 8.5](./08.5-ring-diagram-view.md)의 링 다이어그램 view이고, C ABI는 네 값을 허용하게 됩니다. 이 mode는 표시 전용이라 `pointer_down`이 부작용 없이 false를 반환합니다 — 이 절이 원래 `Net`에 대해 규정했던 것과 같은 규칙입니다.
>
> "Mode 전환 중 Dragging/Orbiting은 취소하고 Snapping은 끝까지 진행한다"는 이 절의 계약은 그대로이며, Phase 8.5의 회전 동조 test가 그 위에 세워집니다.

### 7. Gameplay and page state machines

Page lifecycle과 gameplay lifecycle을 분리합니다.

```ts
/** Top-level availability of the Web application. */
export type AppState = 'loading' | 'ready' | 'unsupported' | 'error';

/** One local solve session inside a ready application. */
export type GameState = 'idle' | 'ready' | 'running' | 'completed';
```

```text
Page:
Loading ───────────────▶ Ready
   ├─ missing feature ─▶ Unsupported
   └─ load/runtime fail ▶ Error

Gameplay:
Idle(solved) ── Scramble ──▶ Ready(unsolved, 00:00.00)
                                  │
                         first committed user move
                                  ▼
                               Running
                                  │
                         committed solved state
                                  ▼
                              Completed

Reset: Ready / Running / Completed ──▶ Idle(solved, 00:00.00)
Scramble: every ready-page gameplay state ──▶ Ready(new cube, 00:00.00)
```

- Timer는 scramble click 시점이 아니라 첫 non-zero user move가 commit된 frame에서 시작합니다.
- Dead-zone release와 snap-back은 timer를 시작하지 않습니다.
- Timer는 solved move가 commit된 같은 frame에서 정지합니다.
- `GameController`는 이전 `committed_move_count`와 현재 query를 비교해 commit을 관찰합니다.
- `completed`는 scramble로 시작한 session에서 적어도 한 user move가 commit된 뒤 solved가 된 경우에만 도달합니다. 초기 solved와 Reset은 completion이 아닙니다.

### 8. Browser-owned timer

Clock은 C++ engine에 넣지 않습니다. Engine은 이미 animation elapsed time 외에 wall clock을 읽지 않으며, timer는 DOM 표시와 page lifecycle에 속합니다.

```ts
/** Monotonic clock and scheduling seams used by SolveTimer. */
export type TimerEnvironment = {
  readonly now: () => number;
  readonly requestFrame: (callback: FrameRequestCallback) => number;
  readonly cancelFrame: (handle: number) => void;
};

/** Browser-owned elapsed-time model for one solve session. */
export class SolveTimer {
  /** Current lifecycle state: idle, armed, running, or stopped. */
  get state(): SolveTimerState;
  /** Arms a zeroed timer without starting it. */
  arm(): void;
  /** Starts an armed timer; later calls keep the original timestamp. */
  start(): void;
  /** Freezes and returns the current elapsed milliseconds. */
  stop(): number;
  /** Returns to the idle 00:00.00 state. */
  reset(): void;
  /** Cancels scheduling without publishing another display update. */
  teardown(): void;
}
```

- Production clock은 monotonic `performance.now()`입니다.
- 표시 형식은 `MM:SS.cc`이며 minute는 99에서 잘라내지 않고 필요한 만큼 늘어납니다.
- 내부 elapsed time은 millisecond number로 유지하고 centisecond는 표시할 때 내림합니다.
- Timer DOM 갱신용 rAF는 `running`에서만 돌고 canvas render loop와 분리합니다.
- Timer rAF는 DOM text만 바꾸며 engine render를 호출하지 않습니다. 따라서 정지한 cube의 canvas frame loop는 계속 0 frame입니다.
- Timer 숫자 자체는 매 tick `aria-live`로 읽지 않습니다. Completion message에서 최종 시간만 한 번 알립니다.
- Reset, 새 Scramble, error와 teardown은 timer rAF를 반드시 취소합니다.

### 9. C ABI and TypeScript boundary

새 C ABI도 primitive type만 사용합니다.

```text
thorvg_rubiks_scramble(seed: uint32) -> int
thorvg_rubiks_reset_cube() -> void
thorvg_rubiks_is_solved() -> int
thorvg_rubiks_committed_move_count() -> uint32
thorvg_rubiks_turn_face(face: int, quarter_turns: int) -> int
thorvg_rubiks_set_view_mode(mode: int) -> int
thorvg_rubiks_view_mode() -> int
thorvg_rubiks_reset_view() -> void
thorvg_rubiks_is_busy() -> int
```

- Mutation command는 initialization 전에는 실패/no-op하고 invalid enum과 invalid turn을 거부합니다.
- `turn_face`는 외부 face 여섯 개와 `quarter_turns ∈ {-1, 1, 2}`만 받습니다.
- Query는 initialization 전 안전한 sentinel을 반환하며 TypeScript boundary가 이를 검증합니다.
- Generated Emscripten declaration, exported function list와 fake module test fixture를 함께 갱신합니다.
- `CubeEngine.ts`만 generated module을 만지고 typed method로 변환합니다.
- Seed source, clock과 DOM listener는 unit test에서 주입할 수 있게 합니다.

### 10. DOM HUD and canvas layering

HUD는 semantic HTML이며 canvas pixel buffer와 독립적입니다.

```html
<main class="game-shell">
  <section class="game-stage" aria-labelledby="game-title">
    <canvas id="view" aria-label="Interactive Rubik's Cube"></canvas>

    <div class="hud">
      <header class="hud__header">
        <h1 id="game-title">ThorVG Rubik's Cube</h1>
        <output id="timer" aria-label="Elapsed time">00:00.00</output>
      </header>

      <div class="view-switch" role="group" aria-label="View mode">
        <button type="button" data-view="3d">3D</button>
        <button type="button" data-view="both" aria-pressed="true">Both</button>
        <button type="button" data-view="net">Net</button>
      </div>

      <div class="game-actions" aria-label="Game actions">
        <button type="button" id="scramble">Scramble</button>
        <button type="button" id="reset">Reset</button>
        <button type="button" id="home-view">Home view</button>
      </div>
    </div>
  </section>

  <details class="move-controls">
    <summary>Keyboard and move controls</summary>
    <!-- R/R′, L/L′, U/U′, D/D′, F/F′, B/B′ buttons -->
  </details>

  <p id="status" role="status" aria-live="polite"></p>
</main>
```

- `.game-stage`는 `position: relative`, canvas는 render surface, `.hud`는 desktop 좌우 rail의 positioning context입니다.
- HUD의 비어 있는 영역은 `pointer-events: none`, 실제 interactive element만 `pointer-events: auto`로 둡니다. 따라서 투명 HUD가 canvas orbit을 막지 않습니다.
- Button event는 DOM에서 끝나며 canvas `PointerController`로 전달되지 않습니다.
- `Both` button이 초기 `aria-pressed="true"`입니다. 선택 상태는 색만이 아니라 `aria-pressed`로도 표현합니다.
- Desktop에서는 title/timer만 canvas header에 유지하고, view selector는 canvas 왼쪽 바깥, game action은 오른쪽 바깥에 배치합니다.
- Mobile과 짧은 viewport에서는 HUD 전체를 canvas 아래 normal flow로 이동합니다.
- Loading 중 control은 disabled이고 canvas는 숨깁니다. Ready에서만 gameplay control을 활성화합니다.

### 11. Keyboard and accessibility

- 모든 button은 native `<button type="button">`으로 만들고 의미가 분명한 accessible name을 제공합니다.
- `R`, `L`, `U`, `D`, `F`, `B` key는 각 face clockwise turn을 시작합니다.
- `Shift + face key`는 inverse turn입니다.
- Shortcut은 input, textarea, select, contenteditable 또는 modifier shortcut이 이미 사용되는 target에서는 실행하지 않습니다.
- Repeat keydown은 무시하여 key hold가 move queue가 되지 않게 합니다.
- DOM move button은 `Turn right face clockwise`, `Turn right face counter-clockwise`처럼 방향까지 `aria-label`에 기록합니다.
- Focus indicator를 제거하지 않고 high-contrast outline을 제공합니다.
- Touch target은 최소 `44 × 44 CSS px`입니다.
- Timer에는 고정 폭 숫자(`font-variant-numeric: tabular-nums`)를 사용해 layout shift를 막습니다.
- Scramble 준비, unsupported/error와 completion의 최종 시간은 `role="status"`에서 한 번 알립니다.
- Timer tick과 pointer movement는 live region을 갱신하지 않습니다.
- `prefers-reduced-motion`에서는 DOM transition을 제거합니다. Pointer snap 자체를 제거하는 engine 설정은 이번 phase의 범위가 아닙니다.
- Canvas instruction에는 “sticker drag = layer turn, empty drag = view orbit”을 짧게 제공합니다.

### 12. Unsupported and error states

Unsupported는 알려진 browser capability 부재, Error는 지원되는 환경에서 발생한 load/initialization/runtime failure입니다.

- Bootstrap 전에 WebAssembly, Canvas 2D context, Pointer Events, ResizeObserver와 `crypto.getRandomValues` availability를 검사합니다. Seed source가 없으면 Scramble이 불가능하므로 이것도 필수 capability입니다.
- 필수 capability가 없으면 WASM module을 load하지 않고 `unsupported`를 표시합니다.
- Unsupported UI는 필요한 기능과 최신 desktop/mobile browser 사용 안내를 짧게 제공합니다.
- Module fetch, initialization, invalid buffer, render/resize와 post-ready command failure는 `error`입니다.
- Error 원인은 console에 기록하되 UI에는 내부 주소나 stack을 노출하지 않습니다.
- Error 전환은 pointer, observer, canvas frame, timer frame과 engine을 한 teardown 경로에서 정리합니다.
- Error와 Unsupported에서 gameplay button은 숨기거나 disabled 처리합니다.

### 13. Responsive layout

- Desktop에서는 stage를 정사각형에 가깝게 유지하고 view selector와 action을 각각 canvas 왼쪽과 오른쪽 바깥에 배치합니다.
- Mobile/짧은 viewport에서는 HUD와 move control을 canvas 아래 normal flow로 이동합니다.
- Canvas CSS size는 `min(available width, available height)`를 기준으로 하되 최소 1 physical pixel과 engine `MAX_DIMENSION` clamp는 기존 boundary를 재사용합니다.
- `100dvh`와 safe-area inset을 고려해 mobile browser chrome과 notch 뒤에 control이 숨지 않게 합니다.
- Toolbar는 wrap 가능하며 horizontal scroll을 요구하지 않습니다.
- 320 CSS px 폭부터 desktop wide viewport까지 content overflow 없이 사용할 수 있어야 합니다.
- Orientation change와 DPR change는 기존 resize path를 사용하고 현재 view mode, camera와 gameplay state를 보존합니다.
- Canvas에만 `touch-action: none`을 적용합니다. Page와 DOM control의 정상 scroll/tap까지 막지 않습니다.

## Implementation steps

### 1. Domain gameplay primitives

- [x] `CubeState::is_solved()` 구현과 외부 face 전수 test
- [x] Repository-owned deterministic PRNG와 `make_scramble()` 구현
- [x] Move count, axis adjacency, valid layer/turn과 fixed-seed known-answer test
- [x] Scramble + reverse inverse가 solved로 돌아오는 test
- [x] Application fallback을 포함해 scramble 결과가 항상 unsolved인 test

### 2. Programmatic move and Application state

- [x] `InteractionController::start_move()`와 busy rejection 구현
- [x] Pointer/programmatic move가 같은 commit path를 사용하는 test
- [x] Application scramble/reset/solved/move-count command 구현
- [x] Reset/Scramble이 in-flight interaction을 제거하고 stale commit을 남기지 않는 test
- [x] Camera만 복원하는 `reset_view()` 구현과 cube/view-mode 보존 test

### 3. View modes and rendering

- [x] `ViewMode`와 mode별 `layout()` 구현
- [x] `Both`가 기존 contract v3를 byte/pixel 기준으로 유지하는 test
- [x] `Cube3D`의 확대·중앙 배치와 net 미방출 test
- [x] `Net`의 확대·중앙 배치와 cube 미방출 test
- [x] Net mode pointer 거부, mode 전환 lifecycle과 camera/state 보존 test
- [x] Resize와 shutdown default(`Both`) test

### 4. WASM boundary

- [x] Gameplay/view C ABI 추가와 invalid input contract test
- [x] Emscripten exported functions 및 generated TypeScript declaration 갱신
- [x] `CubeEngine.ts` typed method와 disposed/error semantics 추가
- [x] Fake WASM module unit fixture와 boundary unit test 갱신
- [x] Native/WASM fixed seed parity 검증

### 5. Browser gameplay controller and timer

- [x] `GameState`, `SolveTimer`, `GameController` 구현
- [x] Injectable clock, scheduler와 seed source 제공
- [x] First commit start, solved commit stop, reset/rescramble transition unit test
- [x] Timer format, repeated start, teardown cancellation과 long-duration test
- [x] Canvas frame loop와 timer DOM loop가 독립임을 검증
- [x] Button/keyboard command가 accepted일 때만 canvas frame loop 시작

### 6. Accessible DOM UI

- [x] Bootstrap markup을 canvas + DOM HUD 구조로 확장
- [x] `Both` 기본 view selector와 `aria-pressed` 동기화
- [x] Scramble, Reset, Home view와 12개 face/inverse move control 연결
- [x] Keyboard shortcut, repeat/modifier/editable-target guard와 cleanup 구현
- [x] Timer tick을 제외한 status live announcement 구현
- [x] Loading/ready/unsupported/error DOM state integration test

### 7. Responsive CSS

- [x] Canvas/HUD layer와 pointer-events 규칙 구현
- [x] Desktop 좌우 side rail과 mobile normal-flow toolbar breakpoint 구현
- [x] 44px touch target, focus-visible, tabular timer와 reduced-motion CSS
- [x] 320px mobile, portrait/landscape와 desktop viewport e2e screenshot/geometry 검증
- [x] Control이 3D cube와 net의 핵심 영역을 가리지 않는지 검증

### 8. End-to-end verification

- [x] 초기 상태가 `Both`, solved, idle timer인지 검증
- [x] Scramble 후 cube와 net pixel이 변하고 timer는 armed 상태인지 검증
- [x] 첫 committed pointer/keyboard move에서 timer가 시작하는지 검증
- [x] Fixed-seed scramble의 inverse를 keyboard command로 적용해 completion과 timer 정지를 검증
- [x] Reset이 solved contract와 `00:00.00`을 복원하는지 검증
- [x] 3D/Both/Net 전환 시 state와 camera가 보존되는지 검증
- [x] Net mode에서 canvas pointer가 cube/camera를 바꾸지 않는지 검증
- [x] Mobile touch와 desktop mouse/keyboard 주요 flow 검증
- [x] Unsupported와 runtime error가 control을 정리하고 올바른 message를 표시하는지 검증
- [x] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- 사용자는 Scramble로 항상 unsolved 3×3×3 gameplay session을 시작할 수 있습니다.
- Reset은 cube와 timer session만 초기화하고 camera와 view mode를 보존합니다.
- Home view는 camera만 초기화하며 cube, timer와 view mode를 변경하지 않습니다.
- Solved 판정은 C++ cube domain의 committed state에서만 이루어집니다.
- Timer는 첫 non-zero user move commit에 한 번 시작하고 solved commit과 함께 정지합니다.
- Pointer와 keyboard/DOM face turn은 같은 animation, commit, move-count와 solved 경로를 사용합니다.
- 기본 view mode는 `Both`이고 기존 Phase 4/5 rendered scene contract가 그대로 통과합니다.
- `3D`, `Both`, `Net` 전환은 cube, timer와 camera 상태를 보존합니다.
- Net-only mode는 표시 전용이며 invisible 3D interaction을 시작하지 않습니다.
- UI는 ThorVG scene이 아닌 semantic DOM HUD이고, desktop side control과 header overlay가 canvas pointer input을 막지 않습니다.
- Keyboard만으로 scramble, face turn, reset과 view 전환을 수행할 수 있습니다.
- Loading, unsupported, error와 ready가 사용자에게 구분되어 표시되고 모든 failure가 하나의 teardown 경로로 정리됩니다.
- 320px mobile부터 desktop까지 horizontal overflow 없이 사용 가능하고 control이 핵심 render 영역을 가리지 않습니다.
- Timer DOM loop가 돌아도 정지한 cube의 canvas render loop는 돌지 않습니다.
- Native, WASM, TypeScript unit, browser e2e와 production build가 모두 통과합니다.

## Verification commands

```bash
meson test -C build/native --print-errorlogs
source /path/to/emsdk/emsdk_env.sh && ./build_wasm.sh
npm --prefix web run test:unit
npm --prefix web run test:e2e
npm --prefix web run build
```

## Divergences

[`DESIGN.md`](../DESIGN.md)의 초기 스케치와 갈라진 결정입니다.

- Timer는 스케치의 `<span>`이 아니라 `<output>`입니다. 계산 결과를 나타내는 live value이므로 semantic이 맞고, `aria-live` 처리도 기본값이 적절합니다.
- 스케치의 `engine.scramble()`은 인자가 없지만 구현은 `uint32` seed를 받습니다. Engine이 스스로 난수를 만들면 native와 WASM이 갈라지고 재현 가능한 test가 불가능합니다. Seed 생성은 Browser(`crypto.getRandomValues()`)가 맡습니다.
- 스케치의 toolbar는 canvas 아래 한 줄이지만 구현은 desktop에서 canvas 좌우 rail, mobile에서 canvas 아래 normal flow로 나뉩니다. 정사각형 canvas를 최대한 크게 유지하기 위해서입니다.

## 개정 기록

- **`committed_move_count()`가 저장 counter에서 파생값이 됩니다 ([Phase 11](./11-move-history-solve-and-undo.md)의 결정).** 이름과 시그니처는 그대로이고 반환값이 timeline의 `cursor - scramble_end`(cursor가 그 위일 때, 아니면 0)가 됩니다. 되감기가 생기면 저장 counter가 timeline과 어긋나기 때문입니다 — undo는 재생이라 count에서 빠지므로 5수 뒤 두 번 undo한 판이 counter 5, 실제 적용 3이 됩니다. 감소 규칙을 더하는 대신 파생으로 옮기면 절단·solve·복원에서의 보정도 함께 필요 없어집니다. Phase 10까지는 저장 counter가 정확하므로 이 개정은 Phase 11에서 일어납니다. Web과 ABI 시그니처는 바뀌지 않습니다.

- **`is_busy()`에서 orbit 포함을 제거합니다 ([Phase 9](./09-move-queue-and-animated-scramble.md)의 결정, [Phase 6.5](./06.5-pre-enhancement-cleanup.md)가 선행 적용).** 이 phase의 busy가 지키려는 것은 "cube를 바꾸는 입력이 겹치지 않는다"인데 orbit은 commit이 없어(Phase 5.5의 "up == cancel") 막을 대상이 아니고, orbit이 포함된 것은 `InteractionController::is_busy()` 구현의 조합이 그대로 노출된 결과였습니다. 이 포함을 고정하는 test는 없으며, 개정 후에는 orbit drag 중에도 `turn_face`와 move 버튼이 동작합니다. Phase 9의 queue 재생을 orbit이 멈추지 않게 하는 것이 계기입니다.

## Completion

모든 acceptance criteria와 verification command를 통과한 뒤 다음 작업을 수행합니다.

- 이 문서의 status를 `Completed`로 변경합니다.
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 6를 완료 처리합니다.
- 실제 구현과 차이가 생긴 architecture decision을 이 문서와 `DESIGN.md`에 기록합니다.
