# UI Polish and Showcase

## Status

`Completed`

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

`#solver-note`는 Solve가 이 cube size에서 영영 불가능한 이유를 적는 한 줄이므로 dock 바로 아래, status line 위에 둡니다. Solve가 가능한 동안에는 `hidden`이며, 이 계약은 지금과 같습니다.

### View bar

- Scene: `3D / Split / 2D`
- Diagram: `Net / Rings / Both`
- Ambient: `Watch` toggle

기존 view mode의 `Both` label은 `Split`으로 바꿉니다. ABI와 `data-view="both"` 값은 바꾸지 않고 보이는 문구만 개정합니다. Diagram control은 Scene이 2D 영역을 포함하는 `Split` 또는 `2D`일 때만 활성 상태로 보입니다. 3D에서 숨기더라도 선택값은 유지되어 다음 Split/2D 진입 때 그대로 복원됩니다.

`Watch`(`#ambient`)는 cube를 바꾸는 game command가 아니라 화면이 스스로 도는 presentation mode이므로 action dock이 아니라 view bar 오른쪽 끝에 둡니다. Scene과 Diagram이 “무엇을 보는가”를 정하고 Watch가 “보고만 있는가”를 정하므로 세 control이 한 줄에서 같은 축을 이룹니다. 켜는 것과 끄는 것이 모두 이 버튼 하나이고 `aria-pressed`로 상태를 말하는 기존 toggle 계약을 그대로 유지합니다. [GameController](../../web/src/game/GameController.ts)의 `WATCHABLE`이 `idle`과 `completed`만 허용하므로 진행 중인 sitting에서는 disabled로 보입니다. Mobile에서는 Scene/Diagram 다음 줄의 오른쪽 끝입니다.

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
│  Scene: 3D / Split / 2D   Diagram: Net / ...   Watch  │
│  solver note (hidden by default)                      │
│  status line                                          │
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
[ Net ]     [ Rings ]     [ Both ]     [ Watch ]
status line
Moves | Session
Advanced controls ▾
```

- Canvas가 항상 첫 viewport의 대부분을 차지합니다.
- Settings는 modal bottom sheet입니다.
- Bottom sheet가 열리면 focus를 내부에 가두고, Escape와 backdrop click으로 닫으며, 닫힌 뒤 trigger로 focus를 되돌립니다.
- 320px에서도 horizontal page overflow가 없어야 합니다.
- Touch target은 최소 44×44 CSS px입니다.

## State visibility contract

가시성은 한 축이 아니라 두 축이 정합니다. [GameSession](../../web/src/game/GameSession.ts)의 `GameState`는 `idle | scrambling | ready | running | completed` 다섯 값이고, 그 위에 engine이 매 frame 답하는 `busy`와 `watching`이 겹칩니다. 하나의 표로 적으면 `completed`가 빠지거나 재생 상태가 session 상태인 것처럼 읽히므로 둘로 나눕니다.

### Session state

| `GameState` | Primary center | History | Watch |
| --- | --- | --- | --- |
| `idle` | Scramble | disabled | 활성 |
| `scrambling` | Scramble | disabled | disabled |
| `ready` | Scramble | history에 따라 | disabled |
| `running` | Scramble | history에 따라 | disabled |
| `completed` | Scramble | history에 따라 | 활성 |

`completed`는 timer가 멈추고 record가 쓰인 뒤입니다. 화면 구성은 `ready`/`running`과 같지만 Watch를 다시 누를 수 있는 두 상태 중 하나이므로 표에 따로 적습니다.

### Engine overlay

| Engine | Primary center | Move/History | Stop | Diagram |
| --- | --- | --- | --- | --- |
| 한가함 | Scramble | 평소대로 | hidden | view에 따라 |
| Scramble 재생 (`busy`) | Scramble, disabled | disabled | hidden | 활성 |
| Rewind/Solve/Undo/Redo 재생 (`busy`, stoppable) | **Stop** | disabled | visible | 활성 |
| Watching (`busy` + `watching`) | Scramble, disabled | move는 활성, history는 disabled | hidden | 활성 |
| Unsupported/Error | 없음 | hidden | hidden | hidden |

**Stop은 재생 전체가 아니라 끊을 수 있는 재생만 끊습니다.** [Application.cpp](../../engine/src/app/Application.cpp)의 재생기는 `stoppable`을 기본 `false`로 두고 rewind와 replay에서만 켭니다. 반쯤 재생된 scramble은 아무도 요청하지 않은 cube이고, 이렇게 멈춘 watched pattern은 빌려 간 cube를 되돌려 놓지 못하기 때문에 의도적으로 그렇게 되어 있습니다. 이 작업은 UI 재배치이지 engine invariant 개정이 아니므로 그 결정을 따릅니다. 따라서 dock 중앙의 Scramble/Stop 교체는 rewind 계열 재생에만 적용되고, scramble 재생 중에는 Scramble이 disabled 상태로 자리를 지키며, watching을 끄는 것은 Stop이 아니라 Watch toggle 자신입니다.

Watching 중에도 move button과 keyboard letter가 살아 있고 누르면 watching을 먼저 끄고 그 turn을 실행하는 것 역시 기존 계약이며 그대로 둡니다.

상태 메시지는 stage와 control 바로 아래의 한 줄 status/toast 영역에 표시합니다. 오류가 아닌 일반 상태는 layout을 밀지 않도록 최소 높이를 예약합니다. 사소한 명령 거절은 modal을 만들지 않고 해당 control과 status에 짧게 표시합니다.

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
      <!-- output is an implicit polite live region; the timer must not be one -->
      <output id="timer" aria-label="Elapsed time" aria-live="off">00:00.00</output>
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
    <!-- the label keeps the full instruction; the visible hint only repeats it -->
    <canvas
      id="view"
      aria-label="Interactive Rubik's Cube. Drag a sticker to turn a layer, or drag empty space to orbit the view."
    ></canvas>
    <p class="interaction-hint" aria-hidden="true">Drag a sticker to turn · drag empty space to orbit</p>
  </section>

  <div class="action-dock" role="group" aria-label="Cube actions">
    <button class="button button--secondary" id="undo" type="button">Undo</button>
    <button class="button button--secondary" id="rewind" type="button">Rewind</button>
    <button class="button button--primary" id="scramble" type="button">Scramble</button>
    <button class="button button--danger" id="stop" type="button" hidden>Stop</button>
    <button class="button button--secondary" id="solve" type="button">Solve</button>
    <button class="button button--secondary" id="redo" type="button">Redo</button>
  </div>

  <p class="solver-note" id="solver-note" hidden>
    No solver for this cube size yet — Rewind still works.
  </p>

  <div class="view-bar">
    <div class="segmented-control" role="group" aria-label="Scene">
      <button type="button" data-view="3d" aria-pressed="false">3D</button>
      <button type="button" data-view="both" aria-pressed="true">Split</button>
      <button type="button" data-view="2d" aria-pressed="false">2D</button>
    </div>

    <div class="segmented-control" role="group" aria-label="Diagram">
      <button type="button" data-flat="net" aria-pressed="true">Net</button>
      <button type="button" data-flat="rings" aria-pressed="false">Rings</button>
      <button type="button" data-flat="both" aria-pressed="false">Both</button>
    </div>

    <button class="button button--ghost" id="ambient" type="button" aria-pressed="false">
      Watch
    </button>
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
| Danger border | `#f3b3b3` | `#63373c` |
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
  --color-danger-border: #f3b3b3;
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
  --color-danger-border: #63373c;
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
  --color-danger-border: #63373c;
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
  border-color: var(--color-danger-border);
  color: var(--color-danger);
  background: transparent;
}

.segmented-control button[aria-pressed="true"] {
  border-color: transparent;
  color: var(--color-accent);
  background: var(--color-accent-soft);
}
```

Danger border는 `color-mix()`로 계산하지 않고 theme마다 literal token 하나를 둡니다. 이 값이 필요한 곳은 danger button 하나뿐이라 계산식이 사는 값이 없고, token 두 줄이 browser 지원 확인과 fallback 규칙보다 짧기 때문입니다. 다른 곳에서 `color-mix()`가 필요해지면 그때 지원 대상과 production build를 확인합니다.

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

Setter는 concrete class가 아니라 `Renderer` 경계에 둡니다. [Application](../../engine/src/app/Application.cpp)은 renderer를 `std::unique_ptr<render::Renderer>`로 소유하므로 `ThorVGSoftwareRenderer`에만 method를 붙이면 application에서 호출할 방법이 없습니다. `pixel_buffer()`와 `pixel_byte_length()`가 이미 기본 구현을 가진 virtual이므로 같은 형태를 따릅니다. Background를 갖지 않는 backend는 기본 구현이 아무것도 하지 않습니다.

```cpp
class Renderer {
public:
    /**
     * 다음 frame부터 사용할 불투명 target background를 정합니다.
     *
     * Background는 geometry가 아니라 target의 성질이므로 scene이 아니라
     * renderer가 들고 있습니다. 기본 구현은 아무것도 하지 않습니다.
     */
    virtual void set_background(graphics::Color color) noexcept
    {
        static_cast<void>(color);
    }
};
```

```cpp
class ThorVGSoftwareRenderer : public Renderer {
public:
    void set_background(graphics::Color color) noexcept override;

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
- Status는 `aria-live="polite"`를 유지하되 timer와 move log는 live region으로 만들지 않습니다. `<output>`은 암묵적으로 polite live region이므로 timer는 지금처럼 `aria-live="off"`를 명시해 그것을 끕니다.
- Canvas의 조작 설명은 `aria-label`에 남기고 화면에 보이는 hint는 `aria-hidden="true"`로 둡니다. 첫 gesture 뒤 hint를 지워도 끊어진 `aria-describedby` 참조가 남지 않게 하기 위해서입니다.
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

순서는 “초록 상태로 멈출 수 있는 지점”을 기준으로 정합니다. Canvas theme은 C++/ABI/WASM만 건드리는 독립 수직 slice여서 DOM을 하나도 옮기지 않고 끝까지 갈 수 있고, 정보 구조 재배치는 e2e를 가장 크게 흔들므로 뒤에 둡니다. 그래서 원래 목록의 2와 4를 앞으로 당겼습니다.

### 1. Theme tokens and controller

- [x] 이 문서의 최종 정보 구조 확정
- [x] CSS color literal을 semantic token으로 이동
- [x] System/Light/Dark theme selector
- [x] `ThemeController`와 system media-query listener/teardown
- [x] theme preference 한 값의 fail-soft persistence
- [x] first-paint theme 적용
- [x] Phase 14의 theme persistence 예외 개정 기록

### 2. Canvas theme

DOM 재배치 없이 현재 화면 위에서 끝낼 수 있는 단계입니다. 여기까지가 통과하면 theme은 shell 작업과 무관하게 완성되어 있습니다.

- [x] `CanvasTheme`과 `canvas_background()`
- [x] `Renderer::set_background()` virtual과 software renderer override
- [x] Application state, C ABI, generated WASM boundary
- [x] `CubeEngine` theme method와 `ThemeController` 연결
- [x] Classic/HighContrast × Light/Dark 조합 검증

### 3. Contract and shell extraction

- [x] `web/src/ui/GameShell.ts`로 markup과 typed query 이동
- [x] `bootstrap.ts`를 support check와 lifecycle wiring으로 축소
- [x] 기존 id, data attribute, `GameUi` field 유지
- [x] bootstrap unit test를 새 landmark와 settings/activity 구조로 개정

### 4. Responsive information architecture

- [x] desktop side rail 제거, header/stage/action dock/view bar 구성
- [x] mobile action hierarchy와 settings bottom sheet
- [x] Moves/Session tab panel
- [x] contextual Diagram controls, view bar의 Watch toggle, rewind 계열의 Scramble/Stop 자리 교체
- [x] status와 solver note를 control 가까이 이동
- [x] interaction hint와 첫 gesture 후 제거

### 4b. e2e migration

이 단계에서 가장 큰 덩어리이고 따로 세지 않으면 빠지는 작업입니다. Settings로 들어가는 control(`#reset`, `#cube-size`, `#speed`, `#mute`, `[data-palette]`, `#home-view`)과 Advanced로 내려가는 control(`#turn-depth`, `#turn-wide`)을 지금은 spec이 화면에서 곧바로 누릅니다. 서랍이 닫혀 있으면 전부 실패합니다.

현재 e2e 14개 spec 중 10개가 여기에 해당합니다.

| Spec | 해당 참조 |
| --- | --- |
| `gameplay-ui.spec.ts` | 18 |
| `cube-size.spec.ts` | 13 |
| `ambient.spec.ts` | 12 |
| `turn-sound.spec.ts` / `speed.spec.ts` / `ambient-mix.spec.ts` | 각 7 |
| `solve.spec.ts` | 5 |
| `palette.spec.ts` | 3 |
| `page-states.spec.ts` | 2 |
| `move-log.spec.ts` | 1 |

- [x] `openSettings()` / `closeSettings()` helper를 `tests/e2e`에 추가
- [x] 위 10개 spec을 helper 경유로 개정
- [x] `#ambient`는 view bar에 남으므로 서랍을 거치지 않는지 확인
- [x] 기존 spec이 id와 attribute selector만 쓰는 것을 유지해 `Both` → `Split` 같은 label 개정이 spec을 건드리지 않게 함

### 5. Visual polish

- [x] Primary/secondary/ghost/danger/segmented control variant
- [x] typography, spacing, radius, shadow token 정리
- [x] inline SVG share/settings icon과 accessible label
- [x] hover, active, focus-visible, disabled, busy state
- [x] reduced-motion drawer/feedback transition

### 6. Verification

- [x] Native renderer/background tests
- [x] TypeScript unit tests
- [x] Desktop/mobile browser e2e
- [x] 두 theme의 visual QA
- [x] WASM와 production build

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
- Rewind/Solve/Undo/Redo 재생에서 Scramble 자리에 Stop이 나타나고 종료 뒤 원래 자리로 돌아옵니다.
- Scramble 재생과 watching 중에는 Stop이 나타나지 않고, watching은 view bar의 Watch를 다시 눌러 끝납니다.
- Watch는 view bar에서 눌리고 `idle`/`completed` 밖에서는 disabled입니다.
- Settings 안의 control이 helper로 서랍을 연 뒤 desktop과 mobile 모두에서 조작됩니다.
- 모든 기존 gameplay, pointer, sharing, solve e2e가 DOM 재배치 뒤에도 통과합니다.

### Visual QA

Screenshot baseline을 CI에 넣지 않습니다. 저장소에는 snapshot 도구 설정도, 이를 돌릴 workflow도 없고([playwright.config.ts](../../web/playwright.config.ts), `.github/workflows/deploy-pages.yml`), baseline은 플랫폼마다 달라 실패가 대개 회귀가 아니라 렌더링 차이입니다. 이 작업의 크기에 비해 유지 비용이 큽니다.

대신 아래를 사람이 한 번 훑는 checklist로 둡니다. 나중에 shell이 안정된 뒤 screenshot을 도입한다면 canvas animation, timer, scroll position을 고정하거나 mask한 control shell만 대상으로 삼습니다.

- [x] Desktop Light / Dark
- [x] Mobile Light / Dark
- [x] Settings open Light / Dark
- [x] Error/unsupported Light / Dark
- [x] Classic/HighContrast × Light/Dark 네 조합의 sticker 판독

## Acceptance criteria

- 첫 화면에서 stage, timer, primary action이 모든 설정과 기록보다 먼저 읽힙니다.
- Desktop에 canvas 옆 absolute side rail이 없습니다.
- Mobile 첫 control 영역에는 primary action과 scene selector가 먼저 오고, 설정은 bottom sheet에 있습니다.
- `Both`라는 보이는 label은 scene mode에 남지 않고 `Split`으로 개정되어 Diagram의 `Both`와 구별됩니다.
- 모든 기존 command와 view/palette 의미가 유지되고 UI framework dependency가 추가되지 않습니다.
- `Watch`는 view bar에서 Scene/Diagram과 한 줄을 이루고 `aria-pressed` toggle 계약을 유지합니다.
- Stop은 rewind 계열 재생에서만 나타나며 scramble 재생과 watching의 engine invariant는 바뀌지 않습니다.
- `#solver-note`는 dock과 status 사이에 있고 solver가 있는 cube size에서는 `hidden`입니다.
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

## Implementation record

구현하면서 이 문서의 계획과 달라진 결정들입니다.

### Theme token은 `light-dark()` 하나로 선언합니다

문서는 light block과 dark block, 그리고 media query 안의 dark 복사본까지 세 벌을 적고 “실제 구현은 selector를 합친다”고 적어 두었습니다. 순수 CSS에서 media query 경계를 넘어 selector를 합칠 수는 없으므로, 대신 token마다 `light-dark(light, dark)` 한 줄을 씁니다. `light-dark()`는 used `color-scheme`을 읽으므로 theme을 고르는 일은 `:root[data-theme]`에서 `color-scheme`을 좁히는 것이 전부가 됩니다. 복사본이 사라지므로 한쪽만 고쳐 두 theme이 어긋나는 경로도 없어집니다.

### `CubeCanvasTheme`은 `enum`이 아니라 `as const` 객체입니다

문서 예시는 `export enum`이었지만 이 저장소의 `tsconfig`는 `erasableSyntaxOnly`를 켜 두어 코드를 생성하는 형태를 받지 않습니다. `CubePalette`, `CubeViewMode`가 이미 쓰는 `as const` 객체 + 동명 type alias 형태를 따랐습니다.

### `[hidden]`이 component `display`를 이깁니다

`#stop`, `#solver-note`, settings panel은 모두 `hidden` 속성으로 숨기면서 `display`를 지정하는 class를 답니다. 명시도에서 class가 이기므로 `hidden`인 Stop이 화면에 그대로 남았습니다. `[hidden] { display: none !important }` 한 줄을 두어 속성이 언제나 이기게 했습니다.

### e2e는 `colorScheme: 'dark'`로 고정합니다

저장된 값이 없으면 page는 기계를 따르고, Playwright의 기본 기계는 light입니다. Rendered scene contract가 어두운 ground에 대해 쓰였으므로 suite 전체를 dark로 고정하고, 다른 ground는 `theme.spec.ts`가 `emulateMedia`로 직접 확인합니다.

### Scramble은 watching 중에도 살아 있습니다

State 표의 “Watching → Scramble disabled”는 구현하지 않았습니다. 지금 Scramble을 누르면 watching을 먼저 끄고 scramble을 시작합니다 — move button과 같은 계약이고, watching에서 빠져나오는 유효한 경로입니다. 이 작업은 UI 재배치이므로 그 동작을 그대로 두었습니다. Stop이 나타나지 않는다는 점은 계획대로입니다.

### Error page에서도 theme은 바꿀 수 있습니다

`unsupported`와 `error`에서 timer와 Share는 숨기지만 settings trigger는 남깁니다. Theme은 engine이 없어도 동작하는 page 자신의 것이고, cube를 못 돌리는 browser도 이 메시지를 읽는 browser이기 때문입니다.

### 시작 시 disable은 gameplay control에만 겁니다

기존 bootstrap은 `button, input` 전체를 disable했습니다. 그때는 모든 control이 game control이었지만 지금은 settings trigger, theme selector, activity tab이 함께 걸려 처음에 서랍이 열리지 않았습니다. `GameUi` field에서 목록을 만들어 game이 소유하는 control만 끕니다.

### `ActivityTabs.ts`가 늘고 `StatusPresenter.ts`는 만들지 않았습니다

Moves/Session 전환은 tab list의 keyboard 계약까지 있어 자기 모듈이 되었습니다. 반대로 status는 `GameSession`이 이미 한 곳에서 쓰고 있어 옮길 것이 없었으므로, 별도 presenter를 만들지 않고 markup에서 위치만 stage 아래로 옮겼습니다.

### Stage는 두 변 중 작은 쪽으로 정한 정사각형입니다

Engine이 정사각 영역에 그리므로 넓고 낮은 canvas는 같은 그림에 여백만 더합니다. `.game-stage { width: min(100%, 70dvh) }`로 두고 좁은 화면에서는 폭이 결정하게 합니다.

### 좁은 화면에서 Share는 숨깁니다

Header에 timer와 gear가 함께 들어가야 하고, Share는 solve가 끝난 뒤에야 쓰이는 명령이라 첫 화면에서 자리를 차지할 이유가 없습니다.

## Completion

모든 acceptance criteria와 verification command를 통과한 뒤 다음을 수행합니다.

- 이 문서의 status를 `Completed`로 변경합니다.
- 기존 Phase 6, 13, 14 문서에서 UI placement 또는 persistence 계약이 개정된 부분에 짧은 개정 기록을 남깁니다.
- 실제 구현에서 달라진 layout, theme, canvas boundary 결정을 이 문서에 기록합니다.
- 상위 계획에는 Phase 17을 바꾸지 않고 “UI polish and showcase completed”라는 별도 완료 항목 또는 횡단 작업 기록으로 연결합니다.
