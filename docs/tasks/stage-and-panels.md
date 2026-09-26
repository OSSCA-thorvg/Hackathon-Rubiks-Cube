# Stage and panels

## Status

`Completed`

Phase 번호가 없는 횡단 작업입니다. [UI polish and showcase](./ui-polish-and-showcase.md)가 정한 화면 구성을 새 방향으로 개정합니다. 기능은 하나도 빠지지 않고, 엔진(C++)은 한 줄도 바뀌지 않습니다. 방향은 Design 캔버스에서 네 시안(A Stage, B Console, C Toy Box, D Exhibit)을 비교한 뒤 정했습니다. A를 바탕으로 B의 상세 기능을 토글 패널로 얹은 "E · Stage + Panels" 시안을 그대로 옮긴 것입니다.

## Objective

- **Stage.** 캔버스가 화면의 주인공이 되도록, 페이지 배경을 엔진이 캔버스를 지우는 색과 같게 합니다. 무대에 테두리가 없어지고, 큐브가 그림 속이 아니라 페이지 위에 섭니다.
- **Floating controls.** 나머지 control은 무대 위에 뜬 유리 면으로 둡니다. 왼쪽 View rail, 오른쪽 Details rail, 아래쪽 타임라인과 dock입니다.
- **Toggle panels.** Moves, Session, Turn 상세는 rail 버튼으로 여닫는 패널입니다. 팝업이 아닙니다.
- **Four additions.**
  - 무브 타임라인. 기록을 한 줄로 펼쳐 보이고, 누른 수로 큐브를 옮깁니다.
  - ⌘K 명령 메뉴. 페이지 명령을 검색하고, 입력한 무브를 재생합니다.
  - 면 색 무브 칩.
  - 스크램블 수열 표시.
- **No engine change.** 엔진 ABI, WASM, C++ 테스트는 그대로입니다.

## 결정

### 엔진은 건드리지 않고, 배경은 엔진의 색에 맞춥니다

시안의 다크 배경은 `#0b0c10`이었지만, 캔버스를 지우는 색은 [CanvasTheme.cpp](../../engine/src/graphics/CanvasTheme.cpp)의 상수입니다(다크 `#202020`, 라이트 `#e7ebf0`). 페이지가 고를 수 있는 것은 Light/Dark 둘 중 하나뿐입니다. 이 상수를 바꾸지 않은 이유는 셋입니다.

- 두 값을 그대로 확인하는 C++ 테스트가 일곱 파일에 있습니다.
- 3D 바닥 그림자의 색은 배경에서 계산됩니다(`shadow_tint`, `kShadowColor {40, 48, 64}`). `#0b0c10`에서는 그림자가 `(1, 2, 4)`가 되어 배경과 거의 구분되지 않습니다. Phase 19와 19.5에서 맞춘 조명을 다시 맞춰야 합니다.
- WASM을 다시 빌드해야 합니다.

그래서 `--color-page`를 `--color-canvas`와 같게 두었습니다. 다크 무대는 시안보다 조금 밝은 차콜이 되고, 라이트는 시안과 거의 같습니다. 유리 면의 색은 이 두 배경에 맞춰 정했습니다.

### 패널은 팝업이 아닙니다

패널은 자기 버튼을 다시 눌러야만 닫힙니다. 바깥을 누르거나 Escape를 누르거나 다른 명령을 실행해도 그대로입니다. 플레이하면서 곁눈으로 보는 곳이지, 페이지가 답을 기다리는 질문이 아니기 때문입니다.

- **넓은 화면**에서는 세 패널이 서로 독립적이어서 여러 개를 함께 열 수 있습니다. 패널은 무대와 오른쪽 rail 사이의 열에 열리고, 무대는 가려지는 대신 옆으로 비켜납니다.
- **좁은 화면**에서는 무대 아래에 하나만 들어갑니다. 둘째 패널을 열면 첫째가 닫히고, 창이 좁아질 때 여러 개가 열려 있었다면 마지막에 연 것만 남습니다.
- 열림 상태는 저장하지 않습니다. 새로고침은 새로 시작한다는 Phase 14의 원칙을 따릅니다.

### 타임라인 이동은 Undo와 Redo를 이어 부릅니다

엔진에는 "몇 번째 수로 가라"는 명령이 없습니다. 그래서 컨트롤러가 *walk*로 걸어갑니다. 한 걸음은 보통의 Undo, Redo 또는 face turn이고, 앞 걸음이 착지한 뒤의 첫 한가한 프레임에 다음 걸음을 요청합니다([GameController.ts](../../web/src/game/GameController.ts)의 `stepWalk`).

- 멀리 가면 한 수씩 애니메이션으로 지나갑니다. 열두 수 뒤로 가는 것은 열두 수가 빠지는 모습으로 보입니다.
- walk가 진행되는 동안에는 걸음 사이에도 Stop이 Scramble 자리에 있습니다. Stop은 남은 걸음을 버립니다.
- 다른 명령(스크램블, 손으로 한 회전, 리셋 등)이 들어오면 walk를 버립니다. 엔진이 걸음을 거절해도 그 자리에서 멈춥니다. 사람이 드래그나 칠하기로 큐브를 다른 곳에 두었는데 그 위를 계속 걸어가면 안 되기 때문입니다.
- 재생 중인 되감기는 그 자리에서 멈추고 새 요청을 따릅니다. 끊을 수 없는 스크램블은 끝나기를 기다렸다가 끝나는 프레임에 첫 걸음을 넣습니다.

walk의 다음 걸음은 `afterEngineFrame` 안에서 요청되고, 이때 프레임 루프도 새로 요청합니다. [AppLifecycle.ts](../../web/src/AppLifecycle.ts)의 `drawFrame`은 그 뒤에 한 번 더 요청하지 않도록 바뀌었습니다. 두 번 요청하면 한 tick에 프레임이 두 번 돌아 시계가 두 배로 흐릅니다.

### 입력한 무브는 face 명령으로 읽습니다

`parseMoves()`([notation.ts](../../web/src/game/notation.ts))는 무브 로그가 쓰는 표기와 스크램블 시트에서 복사해 온 줄을 모두 읽습니다. 공백이 없어도 되고, 굽은 prime(`’`, `′`), 쉼표도 받습니다. 결과는 `turnFace()`가 받는 깊이 범위입니다.

- **대소문자는 구분하지 않습니다.** 소문자를 wide로 읽는 표기법도 있지만, 이 로그는 `Rw`로 씁니다. 검색창에 `r u r' u'`를 치는 사람은 키보드 R, U와 같은 네 수를 뜻합니다.
- **M, E, S**는 따르는 면(L, D, F)의 명령을 한 층 안쪽으로 준 것입니다. 가운데 층이 있는 홀수 큐브에서만 받습니다.
- **한 토큰이라도 틀리면 줄 전체를 거절합니다.** 중간까지 재생하고 멈춘 큐브는 아무도 원하지 않은 큐브이기 때문입니다. 한 줄은 200수까지 받습니다.
- **로그와 표기가 어긋나지 않음**을 왕복 테스트로 고정합니다. 2×2부터 7×7까지 로그가 쓸 수 있는 모든 수(693개)를 쓰고 다시 읽으면 같은 packed word가 나옵니다.

재생은 타임라인 이동과 같은 walk입니다. 한 수씩 turn하므로 첫 수가 시계를 시작하는 것까지 손으로 한 회전과 같습니다.

### 명령 메뉴는 페이지의 버튼을 누릅니다

메뉴의 명령은 모두 페이지에 이미 있는 control을 누르는 것입니다.

- 버튼이 켜져 있고 화면에 있을 때만 명령을 목록에 보입니다.
- 명령이 하는 일은 그 버튼이 하는 일입니다.
- 버튼을 가진 컨트롤러는 메뉴가 있다는 것을 모릅니다.

예외는 무브 재생 하나입니다. 메뉴는 컨트롤러보다 먼저 존재하므로 메서드 대신 `GameUi.root`에 `PLAY_MOVES_EVENT`를 보냅니다. 컨트롤러가 없을 때 보낸 줄은 아무도 받지 않습니다. 엔진이 뜨지 않은 페이지에서 버튼을 누른 것과 같습니다.

메뉴는 combobox와 listbox로 되어 있습니다. focus는 입력란에 머물고, 화살표로 활성 항목을 옮기고, Enter로 실행하고, Escape나 바깥 클릭으로 닫습니다. 패널과 달리 이것은 답을 받으면 사라지는 질문입니다.

### Share는 만든 링크를 카드로 보여 줍니다

Share를 누르면 링크를 클립보드에 복사하고, 그 링크를 Share 버튼 아래 카드에 보여 줍니다([ShareCard.ts](../../web/src/game/ShareCard.ts)). 카드에는 링크를 읽고, 선택하고, 다시 복사할 수 있습니다. dialog가 아니므로 뒤의 조작을 막지 않습니다.

- **복사된 링크**: 카드가 7초 뒤 스스로 사라집니다. 포인터가 위에 있거나 focus가 안에 있는 동안에는 사라지지 않고, 떠나면 7초를 처음부터 다시 셉니다.
- **클립보드가 거절한 링크**: 사람이 직접 복사해야 합니다. 그래서 링크를 선택한 채 focus를 두고, 닫을 때까지 남깁니다.

상태 줄은 모든 명령에서처럼 무슨 일이 있었는지를 그대로 말합니다. 카드는 그 결과로 사람이 할 수 있는 일을 보여 주는 곳입니다.

### Rewind의 뜻은 바뀌지 않습니다

시안의 명령 메뉴에는 "Rewind to scramble"이 있었지만, Rewind는 지금처럼 스크램블까지 되감아 맞춘 큐브로 돌아갑니다. 메뉴의 이름도 "Rewind to solved"입니다. 되감기는 타임라인에서 재생 헤드가 점선인 스크램블 구간을 거꾸로 지나는 모습으로 보입니다.

### 면 색은 엔진의 규칙과 음영을 따릅니다

칩의 점 색은 solved 상태에서 그 면이 가진 색입니다(`cube::solved_color`: R 빨강, L 주황, U 흰색, D 노랑, F 초록, B 파랑). 음영은 [Palette.cpp](../../engine/src/graphics/Palette.cpp)의 두 팔레트 값을 CSS에 옮겨 적었습니다. 컨트롤러가 셸에 `data-sticker-palette`를 쓰므로, 고대비 팔레트를 고르면 칩도 그 색이 됩니다. 칠하기 swatch도 같은 token을 쓰게 되어 고대비에서 그림과 swatch가 같은 색을 말합니다(이전에는 classic 색으로 고정이었습니다).

M, E, S는 두 색 사이에 있는 층이라 어느 색으로도 칠하지 않습니다. 색은 글자의 장식일 뿐이므로 칩은 무브 표기로만 읽힙니다.

### 캔버스는 플레이 중에 크기가 바뀌지 않습니다

엔진의 `resize()`는 진행 중인 드래그를 취소합니다. 드래그의 화면 방향이 이전 viewport에서 읽혔기 때문입니다([Application.cpp](../../engine/src/app/Application.cpp)). 그런데 새 레이아웃에서는 무대가 주변 상자가 남긴 공간을 차지합니다. 그래서 무대를 둘러싼 상자는 내용과 관계없이 높이를 고정합니다.

- 상태 줄은 한 줄이든 두 줄이든 두 줄 높이를 잡아 두고, 세 줄로 늘어나지 않습니다.
- 타임라인의 칩 줄은 비어 있을 때도 44px입니다.
- 좁은 화면에서 패널 셋은 모두 같은 높이이고, 안에서 스크롤됩니다. 목록이 한 수씩 길어져도 무대가 줄지 않습니다.

이 문제는 e2e가 찾았습니다. 첫 수가 들어가자 빈 타임라인의 안내 문구가 칩으로 바뀌면서 줄 높이가 1px 달라졌고, 캔버스가 다시 크기를 잡으면서 이어진 두 번째 드래그가 사라졌습니다.

패널을 여닫거나, 뷰를 바꾸거나, 칠하기에 들어가거나, 큐브 크기를 바꾸는 것은 크기가 바뀌어도 되는 명령입니다.

### 좁은 화면의 열은 정확히 화면 높이입니다

무대는 자기 상자의 두 변 중 짧은 쪽으로 정사각형을 정합니다(`min(100cqw, 100cqh)`, `.game-stage`가 size container). 좁은 화면에서 열을 `min-height: 100dvh`로 두면 무대의 높이가 정해지지 않은 크기가 됩니다. 그러면 Chrome은 container 단위를 0으로 풀어 캔버스가 사라졌습니다. 그래서 열을 `height: 100dvh`로 고정했습니다. 패널을 열어 내용이 넘치면 문서가 아니라 열 안에서 스크롤합니다.

### 폰트는 시스템 폰트입니다

시안은 Geist와 Geist Mono를 썼지만, 외부 폰트 요청이나 패키지를 더하지 않고 시스템 sans와 mono(`ui-sans-serif`, `ui-monospace`)를 씁니다.

## Layout

### 넓은 화면: 1024px 이상

```text
┌ brand · size ────────── timer ──────────── ⌘K  Share  ⚙ ┐
│                          status                          │
│ ┌──┐                                        ┌─────┐ ┌──┐ │
│ │3D│                                        │Moves│ │Mv│ │
│ │Sp│               ThorVG stage             │     │ │Se│ │
│ │2D│                                        ├─────┤ │Tu│ │
│ │──│                                        │Sess.│ └──┘ │
│ │Wa│                                        └─────┘      │
│ │Pa│   R′ U′ [F]   ── timeline ──                        │
│ └──┘   ⏮  ↶  [ Scramble / Stop ]  ↷  ✦                   │
└──────────────────────────────────────────────────────────┘
```

- 셸은 네 열 grid입니다: rail 96px, 무대, 패널 0 또는 352px, rail 96px. 패널 열의 너비만 transition으로 바뀌므로 무대가 미끄러지듯 비켜납니다.
- 행은 header와 clock, 무대(`minmax(0, 1fr)`), 타임라인 또는 칠하기 막대, solver note, dock 순서입니다. 행 사이 간격은 `gap`이 아니라 각 요소의 margin으로 둡니다. 숨겨진 solver note가 빈 행 간격 두 개를 남기지 않게 하기 위해서입니다.
- 패널 열은 오른쪽 rail 쪽에 붙습니다. 열이 넓어지는 동안 패널은 rail이 아니라 무대의 여백 위로 들어옵니다. 모든 패널이 닫혀 있으면 열 자체가 사라져 무대를 누르는 것을 가로채지 않습니다.
- 1280px 미만에서는 ⌘K 버튼이 아이콘과 단축키만 남깁니다. 760px보다 낮은 창에서는 timer와 rail과 dock이 조금씩 작아집니다.

### 좁은 화면: 1023px 이하

```text
▪ Rubik's Cube          ⌕  ⇪  ⚙
          00:42.18
        ● status
┌───────────────────────────────┐
│          ThorVG stage         │
└───────────────────────────────┘
( 3D | Split | 2D )        ◉  ✎
( Net | Rings | Both )
┌ one open panel ───────────────┐
└───────────────────────────────┘
   R′  U′ [F]   ── timeline ──
( ⏮  ↶  [ Scramble ]  ↷  ✦ )
( Moves | Session | Turn )
```

- 문서 순서가 곧 화면 순서이고 Tab 순서입니다. 넓은 화면은 같은 문서에서 rail과 패널만 옆으로 들어 올립니다.
- 무대의 최소 크기는 `min(64vw, 300px)`입니다. 그보다 작아져야 할 때는 열이 스크롤됩니다.
- dock의 Rewind, Undo, Redo, Solve는 아이콘만 남기고, 이름은 화면 낭독기에만 읽힙니다.

## State visibility

[UI polish의 표](./ui-polish-and-showcase.md#state-visibility-contract)는 그대로이고, 다음이 더해집니다.

| 상황 | Primary center | Timeline 자리 |
| --- | --- | --- |
| walk 진행 중(타임라인 이동, 입력한 무브) | **Stop** (걸음 사이에도) | 타임라인 |
| 칠하기 draft | Scramble | 칠하기 막대 |

Unsupported와 Error에서는 무대, 두 rail, 패널, 타임라인, dock, timer, Share, 명령 메뉴, 크기 칩을 숨깁니다. 설정 기어는 남습니다.

## Source organization

```text
web/src/
├── AppLifecycle.ts         프레임 루프가 한 tick에 두 번 요청되지 않게
├── bootstrap.ts            패널, 명령 메뉴, 크기 칩, 깊이 stepper 연결; 명령 목록
├── game/
│   ├── GameController.ts   walk(이동과 재생), PLAY_MOVES_EVENT, 팔레트·크기 표시
│   ├── MoveLog.ts          무브 칩: 면 색, 누르면 이동, roving tabindex, focus 유지
│   ├── Timeline.ts         칩 줄, 기록 막대, 진행 표시, 스크램블 수열
│   ├── SessionRecords.ts   부분으로 나눈 기록(같은 문장), 풀이 횟수
│   ├── ShareCard.ts        Share가 만든 링크를 보여 주는 카드
│   └── notation.ts         parseMoves, faceOfNotation
└── ui/
    ├── GameShell.ts        새 markup, 아이콘, typed 참조
    ├── DetailPanels.ts     ActivityTabs.ts를 대신하는 토글 패널
    ├── CommandPalette.ts   ⌘K 명령 메뉴
    └── SettingsPanel.ts    open({ focus }) — 크기 칩이 크기 입력란으로 엽니다
```

모든 element id와 `GameUi`의 기존 필드, 그리고 그 의미는 그대로입니다. `GameUi`에는 `timeline`, `cubeSizeLabel`, `recordTally`, `shareCard`가 더해졌습니다.

## Verification

- **TypeScript unit**:
  - notation: 해석과 693개 왕복.
  - MoveLog: 칩, 면 색, tab 정지점, 다시 그린 뒤의 focus.
  - DetailPanels: 독립, 한 번에 하나, 창이 좁아질 때.
  - CommandPalette: 단축키, 검색, 화살표, 재생, 거절, 다른 dialog 위.
  - GameController: 뒤로·앞으로 이동, 되감기 끊기, Stop과 다른 명령, 입력한 무브 재생과 거절, 크기·팔레트 표시.
  - SettingsPanel: 특정 field로 열기.
  - bootstrap: 패널과 rail.
- **Browser e2e**: 새 spec인 `timeline.spec.ts`는 타임라인으로 큐브 옮기기, 명령 메뉴의 무브 재생, 명령 실행과 거절을 확인합니다. `gameplay-ui.spec.ts`에서는 두 rail 사이의 무대와 페이지와 캔버스의 같은 배경, 그리고 패널이 제자리에 남고 무대가 비켜나는 것을 확인합니다. `<details>`를 열던 spec은 Turn 패널을 열도록 고쳤습니다.
- **Visual QA**: 1440×900, 1280×720, 390×844에서 다크와 라이트, 패널 열림, 칠하기, 설정, 명령 메뉴를 확인했습니다.

```bash
npm --prefix web run test:unit
npm --prefix web run test:e2e
npm --prefix web run build
```

엔진이 바뀌지 않았으므로 native 테스트와 WASM 빌드는 다시 돌리지 않았습니다.
