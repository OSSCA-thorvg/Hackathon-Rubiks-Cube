# UI Polish and Showcase

## Status

`Planned`

Phase 번호를 붙이지 않는 횡단 작업입니다. 상위 계획의 Phase 17은 이미 **Solve hint and step-through**로 예약되어 있으므로 그 의미와 번호를 바꾸지 않습니다. 이 문서는 완성된 기능을 다시 설계하는 phase가 아니라, Phase 6~16에서 하나씩 추가된 UI를 ThorVG showcase에 맞는 하나의 제품 화면으로 재편하는 작업을 다룹니다.

## Objective

ThorVG가 그리는 cube와 두 평면 표현을 화면의 주인공으로 두고, DOM UI는 다음 행동을 명확하게 안내하는 조용한 도구가 되게 합니다.

- 기능을 삭제하지 않고 **Play / View / Settings / Activity** 네 영역으로 재분류합니다.
- 데스크톱의 좌우 rail과 모바일의 긴 control wall을 하나의 반응형 정보 구조로 통합합니다.
- 밝은 중성 surface의 Light theme와 깊은 청회색의 Dark theme를 지원합니다.
- OS 설정을 따르는 `System`을 기본값으로 두고 `Light`와 `Dark`를 명시적으로 선택할 수 있게 합니다.
- DOM shell뿐 아니라 ThorVG software canvas의 배경도 effective theme과 일치시킵니다.
- 기존 engine command, element id, `GameUi`와 gameplay state machine의 의미는 유지합니다.

이 작업의 성공 기준은 “버튼이 예뻐졌다”가 아닙니다. 첫 화면에서 cube, timer, 다음 행동이 먼저 읽히고, 나머지 기능은 필요할 때 찾을 수 있으며, 같은 구조가 desktop과 mobile에서 자연스럽게 재배치되는 것입니다.

## Current problems

현재 화면은 기능적으로 완성되어 있지만 Phase마다 버튼을 같은 면에 덧붙인 결과 다음 문제가 있습니다.

- `3D / Both / 2D`, `Net / Rings / Net + Rings`, palette, mute, speed가 왼쪽 rail에서 거의 같은 무게로 보입니다.
- cube size, scramble length, scramble, reset, history, solve, share, ambient, home view가 오른쪽 rail에서 모두 peer처럼 보입니다.
- desktop rail 폭이 좁아 `High contrast`와 긴 label이 줄바꿈되고, rail이 canvas와 분리된 개발 도구처럼 보입니다.
- mobile에서는 canvas 아래에 모든 기능이 펼쳐져 primary action을 찾기 어렵습니다.
- 서로 다른 의미의 `Both`가 두 번 나타납니다. 화면 영역의 Both와 평면 스타일의 Both를 label만으로 구별하기 어렵습니다.
- 모든 버튼이 비슷한 border, fill, shadow를 가져 selected, primary, secondary, destructive action의 위계가 약합니다.
- 상태 메시지가 긴 문서 흐름의 끝에 있어 명령 결과와 멀리 떨어집니다.
- [현재 stylesheet](../../web/src/style.css)은 `color-scheme: dark`와 색상 literal에 묶여 있어 theme을 안전하게 추가할 token 경계가 없습니다.
- [software renderer](../../engine/src/render/ThorVGSoftwareRenderer.cpp)는 배경 `32, 32, 32`를 매 frame 직접 그리므로 `#view`의 CSS background만 바꿔서는 Light canvas가 되지 않습니다.

## Design principles

### 1. Stage first

Cube stage가 가장 크고 대비가 높은 surface입니다. 설정 panel이나 기록 card가 stage와 시선을 경쟁하지 않습니다.

### 2. One obvious primary action

한 순간에 강조되는 명령은 하나입니다.

- Idle/ready: `Scramble`
- Playback: `Stop`
- 사용자 solve 중: 별도의 강조 버튼 없이 cube와 timer가 중심

Solve는 강력하지만 직접 풀이를 대신하는 보조 기능이므로 primary가 아닙니다.

### 3. Stable placement, contextual contents

Primary dock의 자리는 유지하되 상태에 따라 내용만 바뀝니다. `Stop`이 새 줄에 나타나 전체 layout을 미는 대신 `Scramble` 자리를 대체합니다. 사용할 수 없는 기능을 전부 숨겨 화면이 흔들리게 하지 않고, 자주 쓰는 Undo/Redo는 조용한 disabled 상태로 자리를 지킵니다.

### 4. Progressive disclosure

게임을 시작하는 데 필요하지 않은 값은 Settings에 둡니다. 직접 face turn과 keyboard 설명은 기존 `<details>`를 유지합니다.

### 5. One design language in both themes

Dark theme을 반전시켜 Light theme을 만들지 않습니다. 같은 semantic token과 surface hierarchy를 두 palette가 각각 구현합니다. 선택 상태와 primary action은 두 theme 모두 같은 accent hue를 공유합니다.

## Information architecture

### Header

- Product title
- 현재 상태 또는 짧은 interaction hint
- Timer
- Share
- Settings trigger

### Stage

- ThorVG canvas
- 첫 방문용 한 줄 hint: `Drag a sticker to turn · drag empty space to orbit`
- pointer interaction을 한 번 시작하면 hint를 숨깁니다. 저장 상태는 필요 없습니다.

### Primary action dock

- Undo
- Scramble 또는 Stop
- Redo
- Rewind
- Solve

Mobile에서는 첫 세 개만 첫 줄에 두고 Rewind/Solve는 secondary row 또는 overflow에 둡니다.

### View bar

- Scene: `3D / Split / 2D`
- Diagram: `Net / Rings / Both`

기존 view mode의 `Both` label은 `Split`으로 바꿉니다. ABI와 `data-view="both"` 값은 바꾸지 않고 보이는 문구만 개정합니다. Diagram control은 Scene이 2D 영역을 포함하는 `Split` 또는 `2D`일 때만 활성 상태로 보입니다. 3D에서 숨기더라도 선택값은 유지되어 다음 Split/2D 진입 때 그대로 복원됩니다.

### Settings

- Appearance: `System / Light / Dark`
- Sticker colors: `Classic / High contrast`
- Cube size
- Scramble moves
- Animation speed
- Turn sound
- Home view
- Reset session

`Home view`는 cube state를 바꾸지 않지만 자주 쓰는 game command가 아니라 presentation reset이므로 이곳에 둡니다. `Reset session`은 session을 지우는 동작이므로 Settings 하단의 danger zone에 둡니다.

### Activity

- `Moves`
- `Session`

현재의 move log와 records를 두 card로 연속 배치하지 않고 같은 너비의 tab panel 하나로 묶습니다. DOM에는 두 section을 모두 유지하고 선택되지 않은 panel에 `hidden`을 적용합니다. 이동 기록이 길어져도 stage를 밀지 않도록 기존 scroll 상한을 유지합니다.

### Advanced controls

- Keyboard 설명
- Turn depth / Wide
- Face turn buttons

기존 `<details>`를 유지하되 Settings와 Activity 아래에 둡니다.

## Responsive layout

### Desktop: 960px 이상

```text
┌ Title ────────── Status ── Timer ─ Share ─ Settings ┐
│                                                       │
│                    ThorVG Stage                       │
│                                                       │
├─── Undo ── Rewind ── Scramble/Stop ── Solve ── Redo ───┤
│       Scene: 3D / Split / 2D    Diagram: Net / ...    │
└────────────────────────────────────────────────────────┘

┌ Moves | Session ────────────────────────────────┐
│ activity content                                      │
└────────────────────────────────────────────────────────┘
```

- 좌우 absolute rail을 제거합니다.
- Shell 최대 너비는 stage가 여전히 중심이 되도록 `72rem` 안팎으로 둡니다.
- Header와 dock은 stage 너비를 공유합니다.
- Settings는 우측 drawer 또는 header trigger 아래 popover입니다. 넓은 화면이라고 항상 열어 두지 않습니다.

### Tablet/mobile: 959px 이하

```text
Title                         Timer  Settings
┌─────────────────────────────────────┐
│             ThorVG Stage            │
└─────────────────────────────────────┘
[ Undo ]      [ Scramble / Stop ]      [ Redo ]
[ 3D ]              [ Split ]            [ 2D ]
[ Net ]             [ Rings ]           [ Both ]
Moves | Session
Advanced controls ▾
```

- Canvas가 항상 첫 viewport의 대부분을 차지합니다.
- Settings는 modal bottom sheet입니다.
- Bottom sheet가 열리면 focus를 내부에 가두고, Escape와 backdrop click으로 닫으며, 닫힌 뒤 trigger로 focus를 되돌립니다.
- 320px에서도 horizontal page overflow가 없어야 합니다.
- Touch target은 최소 44×44 CSS px입니다.

## State visibility contract

| 상태 | Primary center | History | Stop | Diagram controls |
| --- | --- | --- | --- | --- |
| Idle | Scramble | disabled | hidden | view에 따라 활성 |
| Scrambling | Stop | disabled | visible | 활성 |
| Ready | Scramble | history에 따라 | hidden | 활성 |
| Running | Scramble | history에 따라 | hidden | 활성 |
| Rewind/Solve playback | Stop | disabled | visible | 활성 |
| Ambient | Stop | disabled | visible | 활성 |
| Unsupported/Error | 없음 | hidden | hidden | hidden |

상태 메시지는 stage 바로 아래의 한 줄 status/toast 영역에 표시합니다. 오류가 아닌 일반 상태는 layout을 밀지 않도록 최소 높이를 예약합니다. 사소한 명령 거절은 modal을 만들지 않고 해당 control과 status에 짧게 표시합니다.

## Markup structure example

기존 element id는 controller와 e2e test가 사용하므로 유지합니다. 다음은 구조 예시이며 실제 문자열은 `GameShell.ts`에서 생성합니다.

```html
<main class="game-shell" data-game-state="idle">
  <header class="app-header">
    <div>
      <p class="app-header__eyebrow">ThorVG Showcase</p>
      <h1 id="game-title">Rubik's Cube</h1>
    </div>

    <div class="app-header__tools">
      <output id="timer" aria-label="Elapsed time">00:00.00</output>
      <button class="button button--ghost" id="share" type="button">
        Share
      </button>
      <button
        class="button button--icon"
        id="settings-trigger"
        type="button"
        aria-controls="settings-panel"
        aria-expanded="false"
        aria-label="Open settings"
      >
        <!-- inline SVG; the visible icon is hidden from accessibility APIs -->
      </button>
    </div>
  </header>

  <section class="game-stage" aria-labelledby="game-title">
    <canvas id="view" aria-label="Interactive Rubik's Cube"></canvas>
    <p class="interaction-hint">Drag a sticker to turn · drag empty space to orbit</p>
  </section>

  <div class="action-dock" role="group" aria-label="Cube actions">
    <button class="button button--secondary" id="undo" type="button">Undo</button>
    <button class="button button--secondary" id="rewind" type="button">Rewind</button>
    <button class="button button--primary" id="scramble" type="button">Scramble</button>
    <button class="button button--danger" id="stop" type="button" hidden>Stop</button>
    <button class="button button--secondary" id="solve" type="button">Solve</button>
    <button class="button button--secondary" id="redo" type="button">Redo</button>
  </div>

  <div class="view-bar">
    <div class="segmented-control" role="group" aria-label="Scene">
      <button type="button" data-view="3d" aria-pressed="false">3D</button>
      <button type="button" data-view="both" aria-pressed="true">Split</button>
      <button type="button" data-view="2d" aria-pressed="false">2D</button>
    </div>
  </div>

  <p id="status" class="status-line" role="status" aria-live="polite"></p>
</main>
```

Action dock은 URL 이동이 아닌 application command 모음이므로 `nav`가 아닌 이름 있는 button group으로 표현합니다. View bar도 page navigation이 아니라 현재 presentation을 바꾸므로 tab 역할을 억지로 쓰지 않고 기존 `role="group"`과 `aria-pressed` 계약을 유지합니다.

## Source organization

Framework를 추가하지 않습니다. DOM 생성과 행동을 다음 정도로만 분리합니다.

```text
web/src/ui/
├── GameShell.ts          markup 생성과 typed GameUi query
├── SettingsPanel.ts      drawer/bottom-sheet lifecycle과 focus
├── ThemeController.ts    System/Light/Dark 선택과 effective theme
└── StatusPresenter.ts    status와 짧은 feedback 표시
```

- `bootstrap.ts`는 browser support 확인, shell 생성, lifecycle 시작만 담당합니다.
- `GameController`는 기존 command와 enabled/pressed state를 계속 소유합니다.
- Theme은 cube state가 아니므로 `GameSession`에 넣지 않습니다.
- Settings open/closed는 DOM-only transient state이며 engine ABI를 보지 않습니다.
- 작은 inline SVG는 `GameShell.ts`의 명시적인 markup으로 둡니다. Icon package를 추가하지 않습니다.

### Typed shell factory example

```ts
/** 생성된 game shell과 controller가 요구하는 typed element 모음입니다. */
export interface GameShell {
  readonly root: HTMLElement;
  readonly canvas: HTMLCanvasElement;
  readonly timer: HTMLOutputElement;
  readonly status: HTMLParagraphElement;
  readonly settingsTrigger: HTMLButtonElement;
  readonly settingsPanel: HTMLElement;
}

/** 필수 element를 typed reference로 읽고 빠진 template은 초기화 오류로 다룹니다. */
function requireElement<ElementType extends Element>(
  host: ParentNode,
  selector: string,
): ElementType {
  const element: ElementType | null = host.querySelector<ElementType>(selector);
  if (element === null) {
    throw new Error(`Game shell is missing ${selector}.`);
  }
  return element;
}

/**
 * 정적 application markup을 만들고 필수 element를 typed reference로 묶습니다.
 * 필수 element가 빠진 template은 초기화 오류이므로 nullable 값을 반환하지 않습니다.
 */
export function createGameShell(host: HTMLElement): GameShell {
  host.innerHTML = `<!-- application markup -->`;

  const root: HTMLElement =
    requireElement<HTMLElement>(host, '.game-shell');
  const canvas: HTMLCanvasElement =
    requireElement<HTMLCanvasElement>(root, '#view');
  const timer: HTMLOutputElement =
    requireElement<HTMLOutputElement>(root, '#timer');
  const status: HTMLParagraphElement =
    requireElement<HTMLParagraphElement>(root, '#status');
  const settingsTrigger: HTMLButtonElement =
    requireElement<HTMLButtonElement>(root, '#settings-trigger');
  const settingsPanel: HTMLElement =
    requireElement<HTMLElement>(root, '#settings-panel');

  return { root, canvas, timer, status, settingsTrigger, settingsPanel };
}
```

실제 구현에서는 `as GameUi`로 빠진 필드를 숨기지 않고 모든 필드를 명시합니다. 위 축약은 문서에서 기존 필드 전체를 반복하지 않기 위한 것뿐입니다.

## Visual system

### Theme palette

| Semantic role | Light | Dark |
| --- | --- | --- |
| Page | `#f4f6f9` | `#090c12` |
| Surface | `#ffffff` | `#121720` |
| Raised surface | `#ffffff` | `#181e29` |
| Primary text | `#121826` | `#f5f7fa` |
| Muted text | `#667085` | `#98a2b3` |
| Border | `#dce2ea` | `#29313d` |
| Accent | `#2563eb` | `#60a5fa` |
| Accent soft | `rgb(37 99 235 / 12%)` | `rgb(96 165 250 / 14%)` |
| Danger | `#dc2626` | `#f87171` |
| Canvas | `#e7ebf0` | `#202020` |

Light는 순백 page가 아니라 옅은 중성 page 위에 흰 surface를 놓습니다. Dark는 순검정 대신 청회색 surface 단계를 사용합니다. Canvas의 Light background도 순백이 아닙니다. 흰 sticker와 구분되고 검은 cubie seam은 유지되는 값이어야 합니다.

### CSS token example

```css
:root {
  color-scheme: light;

  --color-page: #f4f6f9;
  --color-surface: #ffffff;
  --color-surface-raised: #ffffff;
  --color-text: #121826;
  --color-text-muted: #667085;
  --color-border: #dce2ea;
  --color-accent: #2563eb;
  --color-accent-soft: rgb(37 99 235 / 12%);
  --color-danger: #dc2626;
  --color-canvas: #e7ebf0;

  --radius-control: 0.625rem;
  --radius-panel: 1rem;
  --shadow-stage: 0 1.5rem 4rem rgb(16 24 40 / 12%);
  --shadow-panel: 0 0.5rem 1.5rem rgb(16 24 40 / 8%);
}

:root[data-theme="dark"] {
  color-scheme: dark;

  --color-page: #090c12;
  --color-surface: #121720;
  --color-surface-raised: #181e29;
  --color-text: #f5f7fa;
  --color-text-muted: #98a2b3;
  --color-border: #29313d;
  --color-accent: #60a5fa;
  --color-accent-soft: rgb(96 165 250 / 14%);
  --color-danger: #f87171;
  --color-canvas: #202020;

  --shadow-stage: 0 1.5rem 4rem rgb(0 0 0 / 40%);
  --shadow-panel: 0 0.5rem 1.5rem rgb(0 0 0 / 24%);
}

@media (prefers-color-scheme: dark) {
  :root:not([data-theme]) {
    color-scheme: dark;

    --color-page: #090c12;
    --color-surface: #121720;
    --color-surface-raised: #181e29;
    --color-text: #f5f7fa;
    --color-text-muted: #98a2b3;
    --color-border: #29313d;
    --color-accent: #60a5fa;
    --color-accent-soft: rgb(96 165 250 / 14%);
    --color-danger: #f87171;
    --color-canvas: #202020;
  }
}
```

Dark token을 두 곳에 복사하는 대신 실제 구현은 `[data-theme="dark"]`와 system-dark가 같은 custom property 묶음을 공유하도록 selector를 합칩니다. 여기서는 두 규칙의 의미를 읽기 쉽게 분리했습니다.

### Control variants

```css
.button {
  min-width: 44px;
  min-height: 44px;
  border: 1px solid var(--color-border);
  border-radius: var(--radius-control);
  color: var(--color-text);
  background: var(--color-surface);
  box-shadow: none;
}

.button--primary {
  border-color: var(--color-accent);
  color: #ffffff;
  background: var(--color-accent);
}

.button--ghost {
  border-color: transparent;
  background: transparent;
}

.button--danger {
  border-color: color-mix(in srgb, var(--color-danger) 35%, transparent);
  color: var(--color-danger);
  background: transparent;
}

.segmented-control button[aria-pressed="true"] {
  border-color: transparent;
  color: var(--color-accent);
  background: var(--color-accent-soft);
}
```

`color-mix()`를 쓰기 전에 지원 대상 browser와 production build를 확인합니다. 지원 범위를 넓혀야 한다면 danger border token을 theme마다 하나 더 두는 쪽이 fallback보다 단순합니다.

## Theme behavior

### Preference and effective theme are different values

사용자가 고르는 것은 세 값이지만 engine이 받는 canvas theme은 두 값입니다.

```ts
/** 사용자가 선택할 수 있는 application theme입니다. */
export type ThemePreference = 'system' | 'light' | 'dark';

/** 실제 DOM과 software canvas에 적용되는 두 theme입니다. */
export type EffectiveTheme = 'light' | 'dark';

/** OS color-scheme 질의를 theme 값으로 바꿉니다. */
export function effectiveTheme(
  preference: ThemePreference,
  prefersDark: boolean,
): EffectiveTheme {
  if (preference === 'system') return prefersDark ? 'dark' : 'light';
  return preference;
}
```

### Persistence decision

Theme preference 하나만 `localStorage`에 저장합니다. Phase 14가 제거한 것은 cube session, record, 여러 option을 복원하는 저장 계층입니다. Theme 한 문자열은 cube/timeline/timer와 무관하고 migration이나 restore 순서를 만들지 않으므로 예외로 허용합니다.

이 결정은 Phase 14의 “옵션 저장 없음”을 좁게 개정합니다. 구현할 때 Phase 14 개정 기록에 다음을 남깁니다.

> Game/session/record/presentation option은 저장하지 않는다. 단, page가 그려지기 전에 필요한 UI theme preference 한 값만 저장한다.

```ts
const THEME_STORAGE_KEY = 'thorvg-rubiks-theme';

/** 저장된 theme을 읽고 없거나 알 수 없는 값은 System으로 강등합니다. */
export function loadThemePreference(storage: Storage): ThemePreference {
  const stored: string | null = storage.getItem(THEME_STORAGE_KEY);
  if (stored === 'light' || stored === 'dark') return stored;
  return 'system';
}

/** System은 override를 지우고 명시적인 theme만 저장합니다. */
export function saveThemePreference(
  storage: Storage,
  preference: ThemePreference,
): void {
  if (preference === 'system') {
    storage.removeItem(THEME_STORAGE_KEY);
    return;
  }
  storage.setItem(THEME_STORAGE_KEY, preference);
}
```

Storage 접근 실패는 theme 기능 전체의 실패가 아닙니다. 읽기/쓰기를 `try/catch`로 감싸고 현재 page에는 선택을 적용하되 persistence만 포기합니다.

### Avoiding the first-paint flash

저장된 명시 theme은 application bootstrap보다 먼저 `<html data-theme>`에 반영해야 합니다. 이 작은 초기 script는 WASM이나 UI module을 기다리지 않습니다. System에는 attribute를 두지 않습니다.

`ThemeController`는 system preference를 선택한 동안만 `matchMedia('(prefers-color-scheme: dark)')`의 `change`를 듣고 effective canvas theme을 갱신합니다. Explicit Light/Dark에서는 OS 변경이 application을 덮어쓰지 않습니다. Teardown에서 listener를 제거합니다.

## Canvas theme boundary

CSS `#view { background: ... }`는 첫 render 전 placeholder일 뿐입니다. 실제 frame은 ThorVG renderer가 불투명 background shape을 먼저 그립니다. 완전한 Light/Dark 지원을 위해 target background를 상수에서 상태로 올립니다.

### C++ types

```cpp
namespace rubiks::graphics {

/** Software canvas 뒤에 그릴 중성 배경 theme입니다. */
enum class CanvasTheme {
    Light,
    Dark,
};

/** Theme이 요구하는 불투명 background color입니다. */
[[nodiscard]] Color canvas_background(CanvasTheme theme) noexcept;

}  // namespace rubiks::graphics
```

```cpp
Color canvas_background(CanvasTheme theme) noexcept
{
    if (theme == CanvasTheme::Light) {
        return Color{231, 235, 240, 255};
    }
    return Color{32, 32, 32, 255};
}
```

Renderer는 theme enum을 알지 않고 최종 color만 받습니다. Theme은 presentation의 언어이고 renderer는 target을 칠하는 도구이기 때문입니다.

```cpp
class ThorVGSoftwareRenderer {
public:
    /** 다음 frame부터 사용할 불투명 target background를 정합니다. */
    void set_background(graphics::Color color) noexcept;

private:
    graphics::Color background_{32, 32, 32, 255};
};
```

`ApplicationState`가 `CanvasTheme`을 소유하고 `set_canvas_theme()`에서 renderer의 background를 바꿉니다. Cube state, camera, timeline, timer, busy state는 건드리지 않습니다. Theme 변경은 palette처럼 animation 중에도 허용합니다.

### ABI

```text
thorvg_rubiks_set_canvas_theme(theme: int) -> int
thorvg_rubiks_canvas_theme() -> int
```

- ABI 값: `Light = 0`, `Dark = 1`
- 초기값은 web이 첫 render 전에 effective theme을 전달합니다.
- 잘못된 값은 0을 반환하고 기존 background와 모든 application state를 유지합니다.
- Query는 lifecycle 밖에서 `Dark`를 반환합니다. 기존 native rendered-scene 기본값을 유지하기 위한 선택입니다.

TypeScript engine boundary도 primitive enum만 사용합니다.

```ts
/** C ABI와 같은 값을 사용하는 software canvas theme입니다. */
export enum CubeCanvasTheme {
  Light = 0,
  Dark = 1,
}

/** Effective UI theme을 engine의 primitive enum으로 변환합니다. */
export function canvasThemeOf(theme: EffectiveTheme): CubeCanvasTheme {
  return theme === 'dark' ? CubeCanvasTheme.Dark : CubeCanvasTheme.Light;
}
```

Sticker의 `Classic / HighContrast` palette와 Canvas theme을 합치지 않습니다. 전자는 여섯 sticker shade의 접근성 preset이고, 후자는 target background 하나입니다. 두 축은 독립적으로 모든 조합이 성립해야 합니다.

## Settings panel behavior

- Desktop: right-side drawer 또는 anchored popover. 구현 하나를 공유하기 위해 DOM은 같은 `aside`를 쓰고 CSS만 위치를 바꿉니다.
- Mobile: viewport 아래에서 올라오는 bottom sheet.
- `aria-modal="true"`인 mobile dialog 또는 동등한 dialog semantics를 제공합니다.
- 열릴 때 첫 interactive control로 focus, 닫힐 때 trigger로 복귀합니다.
- Escape로 닫습니다.
- backdrop은 pointer로 닫을 수 있지만 panel 내부 pointer는 전파로 닫히지 않습니다.
- `prefers-reduced-motion`에서는 slide 대신 짧은 opacity 전환만 사용합니다.
- Reset은 panel을 즉시 닫지 않습니다. 결과 status를 확인할 수 있어야 합니다.

## Accessibility

- 모든 interactive target은 최소 44×44 CSS px입니다.
- Focus ring은 Light/Dark 모두 3:1 이상의 인접 대비를 갖고 mouse focus에는 나타나지 않습니다.
- 선택은 색만으로 표현하지 않고 fill/border와 `aria-pressed`를 함께 씁니다.
- Theme selector는 세 개의 button과 `aria-pressed`를 사용합니다. 자동으로 바뀌는 System effective theme도 preference button은 계속 System으로 표시합니다.
- Settings icon에는 visible tooltip 또는 accessible name이 있습니다. SVG 자체는 `aria-hidden="true"`입니다.
- Status는 `aria-live="polite"`를 유지하되 timer와 move log는 live region으로 만들지 않습니다.
- Drawer open 상태에서 background control은 keyboard focus 대상이 아닙니다.
- Error/unsupported UI는 두 theme에서 같은 의미와 충분한 대비를 가집니다.
- DOM transition은 `prefers-reduced-motion`을 존중합니다. Cube turn animation 속도는 기존 Phase 13 계약을 유지합니다.

## Out of scope

- React/Vue/Svelte 등 UI framework 도입
- 사용자 지정 sticker color editor
- Theme별 sticker palette 복제
- Cloud/user account 기반 preference 동기화
- Game session과 records의 영속 저장
- Phase 17의 hint/step-through UI
- Renderer shape 재사용이나 performance 최적화
- UI 전체 icon화: 의미가 label로 더 잘 읽히는 Scramble, Rewind, Solve는 text를 유지합니다

## Implementation steps

### 1. Contract and shell extraction

- [ ] 이 문서의 최종 정보 구조 확정
- [ ] `web/src/ui/GameShell.ts`로 markup과 typed query 이동
- [ ] `bootstrap.ts`를 support check와 lifecycle wiring으로 축소
- [ ] 기존 id, data attribute, `GameUi` field 유지
- [ ] bootstrap unit test를 새 landmark와 settings/activity 구조로 개정

### 2. Theme tokens and controller

- [ ] CSS color literal을 semantic token으로 이동
- [ ] System/Light/Dark theme selector
- [ ] `ThemeController`와 system media-query listener/teardown
- [ ] theme preference 한 값의 fail-soft persistence
- [ ] first-paint theme 적용
- [ ] Phase 14의 theme persistence 예외 개정 기록

### 3. Responsive information architecture

- [ ] desktop side rail 제거, header/stage/action dock/view bar 구성
- [ ] mobile action hierarchy와 settings bottom sheet
- [ ] Moves/Session tab panel
- [ ] contextual Diagram controls와 Scramble/Stop 자리 교체
- [ ] status를 stage 가까이 이동
- [ ] interaction hint와 첫 gesture 후 제거

### 4. Canvas theme

- [ ] `CanvasTheme`과 `canvas_background()`
- [ ] renderer background 상태와 setter
- [ ] Application state, C ABI, generated WASM boundary
- [ ] `CubeEngine` theme method와 `ThemeController` 연결
- [ ] Classic/HighContrast × Light/Dark 조합 검증

### 5. Visual polish

- [ ] Primary/secondary/ghost/danger/segmented control variant
- [ ] typography, spacing, radius, shadow token 정리
- [ ] inline SVG share/settings icon과 accessible label
- [ ] hover, active, focus-visible, disabled, busy state
- [ ] reduced-motion drawer/feedback transition

### 6. Verification

- [ ] Native renderer/background tests
- [ ] TypeScript unit tests
- [ ] Desktop/mobile browser e2e
- [ ] 두 theme의 visual QA
- [ ] WASM와 production build

## Test plan

### Native

- Dark default가 기존 `{32, 32, 32, 255}` pixel contract를 유지합니다.
- Light가 `{231, 235, 240, 255}`로 전체 background를 채웁니다.
- 잘못된 theme ABI 값은 거절되고 기존 background가 유지됩니다.
- Theme 변경은 cube, timeline, camera, timer, palette, busy 상태를 바꾸지 않습니다.
- Light/Dark에서 scene face와 stroke가 background 이후 같은 순서로 그려집니다.

### TypeScript unit

- 저장값 없음/알 수 없는 값은 `System`입니다.
- Light/Dark만 저장되고 System은 key를 제거합니다.
- Storage 예외에서도 현재 page theme은 적용됩니다.
- System일 때 OS 변경이 DOM과 engine을 함께 갱신합니다.
- Explicit Light/Dark에서는 OS 변경을 무시합니다.
- Teardown이 media-query listener와 settings listener를 제거합니다.
- Settings가 Escape, backdrop, trigger로 닫히고 focus가 복귀합니다.
- 기존 `GameUi` element가 모두 typed query로 연결됩니다.

### Browser e2e

- 1440×1100에서 side rail 없이 stage/header/dock이 같은 폭과 중심을 공유합니다.
- 390×844와 320px 폭에서 horizontal overflow가 없습니다.
- Canvas가 mobile 첫 viewport의 가장 큰 surface입니다.
- Settings가 desktop과 mobile에서 열리고 keyboard로 닫힙니다.
- Theme preference가 reload 뒤 복원됩니다.
- System theme은 `prefers-color-scheme`을 따릅니다.
- Light/Dark 전환 뒤 canvas background pixel도 함께 바뀝니다.
- 3D에서는 Diagram selector가 비활성/숨김이고 Split/2D로 돌아오면 이전 선택을 유지합니다.
- Playback에서 Scramble 자리의 Stop이 나타나고 종료 뒤 원래 자리로 돌아옵니다.
- 모든 기존 gameplay, pointer, sharing, solve e2e가 DOM 재배치 뒤에도 통과합니다.

### Visual regression

Control shell만 안정적인 screenshot 대상으로 삼습니다. Canvas animation, timer, scroll position은 고정하거나 mask하여 불안정한 pixel diff를 만들지 않습니다.

- Desktop Light
- Desktop Dark
- Mobile Light
- Mobile Dark
- Settings open Light/Dark
- Error/unsupported Light/Dark

## Acceptance criteria

- 첫 화면에서 stage, timer, primary action이 모든 설정과 기록보다 먼저 읽힙니다.
- Desktop에 canvas 옆 absolute side rail이 없습니다.
- Mobile 첫 control 영역에는 primary action과 scene selector가 먼저 오고, 설정은 bottom sheet에 있습니다.
- `Both`라는 보이는 label은 scene mode에 남지 않고 `Split`으로 개정되어 Diagram의 `Both`와 구별됩니다.
- 모든 기존 command와 view/palette 의미가 유지되고 UI framework dependency가 추가되지 않습니다.
- System/Light/Dark가 DOM shell과 software canvas에 함께 적용됩니다.
- Theme만 fail-soft로 저장되고 cube session, records와 다른 presentation option은 reload 뒤 복원되지 않습니다.
- Classic/HighContrast palette와 Light/Dark canvas의 네 조합이 모두 읽을 수 있습니다.
- 320px 이상에서 horizontal page overflow가 없고 interactive target은 44×44px 이상입니다.
- Keyboard만으로 settings, theme, view, actions, activity, advanced controls를 순서대로 사용할 수 있습니다.
- Native, WASM, TypeScript unit, browser e2e, production build가 모두 통과합니다.

## Verification commands

```bash
meson test -C build/native --print-errorlogs
source /path/to/emsdk/emsdk_env.sh && ./build_wasm.sh
npm --prefix web run test:unit
npm --prefix web run test:e2e
npm --prefix web run build
```

## Completion

모든 acceptance criteria와 verification command를 통과한 뒤 다음을 수행합니다.

- 이 문서의 status를 `Completed`로 변경합니다.
- 기존 Phase 6, 13, 14 문서에서 UI placement 또는 persistence 계약이 개정된 부분에 짧은 개정 기록을 남깁니다.
- 실제 구현에서 달라진 layout, theme, canvas boundary 결정을 이 문서에 기록합니다.
- 상위 계획에는 Phase 17을 바꾸지 않고 “UI polish and showcase completed”라는 별도 완료 항목 또는 횡단 작업 기록으로 연결합니다.
