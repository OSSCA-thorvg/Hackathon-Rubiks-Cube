# thorvg-rubiks Design Document

> **Vite가 Web shell과 browser interaction을 담당하고, WASM으로 빌드된 C++ engine이 Rubik state와 3D interaction/rendering을 담당하며, ThorVG를 graphics pipeline의 최종 2D rendering backend로 사용하는 정적 Web application**

ThorVG를 활용해 브라우저에서 동작하는 3×3×3 루빅스 큐브를 직접 구현하는 프로젝트입니다.
기존 `OSSCA-thorvg/wasm-example`을 clone하는 대신 **처음부터 직접 구축**하며, wasm-example의 Meson + Emscripten + ThorVG + GitHub Pages 구성은 참고 자료로만 활용합니다.

> **Note.** 이 문서는 초기 설계 스케치이며, 세부 설계가 갈라질 때는 `docs/tasks/`의 phase 문서가 우선합니다. 지금까지의 차이: notation parser는 구현하지 않고([Phase 4](./tasks/04-rubiks-cube-domain.md)), pointer picking은 ThorVG hit test가 아니라 ray cast이며 drag API는 delta 없이 절대 좌표만 전달합니다([Phase 5](./tasks/05-pointer-interaction-and-animation.md)). Timer는 `<span>`이 아니라 `<output>`이고, `scramble()`은 engine이 난수를 만드는 대신 Browser가 만든 `uint32` seed를 받습니다([Phase 6](./tasks/06-gameplay-and-ui.md)).

## 핵심 원칙

- UI는 **Vite + Vanilla TypeScript + HTML + CSS**
- 렌더링과 Cube logic은 **C++ → WASM**
- Browser input state(브라우저 입력 상태)는 Web에서 관리
- Mouse drag의 의미 해석과 Cube rotation은 C++ engine에서 처리
- Graphics는 functional-style pipeline(함수형 스타일 파이프라인)
- ThorVG는 graphics pipeline의 마지막 rendering backend(렌더링 백엔드)

---

## 1. 기술 구성

```text
Frontend
├─ Vite
├─ Vanilla TypeScript
├─ HTML
└─ Vanilla CSS

Engine
├─ C++
├─ ThorVG
└─ Emscripten / WASM

Build
├─ Meson
├─ Ninja
└─ GitHub Actions

Deploy
└─ GitHub Pages
```

React/Vue 같은 UI framework(프레임워크)는 현재 규모에서는 사용하지 않습니다.
CSS도 우선 Vanilla CSS(순정 CSS)로 시작합니다.

---

## 2. 전체 architecture

```text
                      Browser
                         │
          ┌──────────────┴──────────────┐
          │                             │
          ▼                             ▼
      HTML / CSS                    <canvas>
          │                             │
          ▼                             │
    TypeScript UI                       │
          │                             │
          ├──── Pointer Events ─────────┤
          │                             │
          ▼                             ▼
                     WASM Bridge
                          │
                          ▼
                    C++ Application
                          │
           ┌──────────────┼──────────────┐
           │              │              │
           ▼              ▼              ▼
         Cube        Interaction      Animation
         Logic        Controller
           │              │
           └──────┬───────┘
                  ▼
           Graphics Pipeline
                  │
                  ▼
           ThorVG Renderer
                  │
                  ▼
               Canvas
```

---

## 3. Directory structure(디렉터리 구조)

이 프로젝트는 **Web frontend(웹 프론트엔드)** 와 **C++ engine(엔진)** 이 명확히 분리되므로, 최상위에서 `web/`과 `engine/`으로 나눕니다.

```text
thorvg-rubiks/
│
├── web/                     # Vite + TypeScript + HTML/CSS
│   ├── index.html
│   ├── package.json
│   ├── tsconfig.json
│   ├── vite.config.ts
│   │
│   ├── public/
│   │
│   └── src/
│       ├── main.ts
│       ├── style.css
│       │
│       ├── ui/
│       │
│       ├── input/
│       │   └── PointerController.ts
│       │
│       └── wasm/
│           └── CubeEngine.ts
│
├── engine/                  # C++ engine
│   ├── meson.build
│   └── src/
│       ├── app/
│       │   └── Application.cpp
│       │
│       ├── cube/
│       │   ├── CubeState.cpp
│       │   ├── CubeMove.cpp
│       │   ├── Cubie.cpp
│       │   └── CubeNotation.cpp
│       │
│       ├── graphics/
│       │   ├── Camera.cpp
│       │   ├── RenderPipeline.cpp
│       │   ├── RenderScene.cpp
│       │   │
│       │   ├── passes/
│       │   │   ├── BuildScenePass.cpp
│       │   │   ├── TransformPass.cpp
│       │   │   ├── ViewPass.cpp
│       │   │   ├── ProjectionPass.cpp
│       │   │   ├── CullPass.cpp
│       │   │   └── DepthSortPass.cpp
│       │   │
│       │   └── thorvg/
│       │       └── ThorVGRenderer.cpp
│       │
│       ├── interaction/
│       │   ├── InteractionController.cpp
│       │   ├── Picking.cpp
│       │   ├── DragResolver.cpp
│       │   └── InteractionState.cpp
│       │
│       ├── animation/
│       │   └── CubeAnimation.cpp
│       │
│       ├── math/
│       │   ├── Vec2.cpp
│       │   ├── Vec3.cpp
│       │   ├── Mat4.cpp
│       │   ├── Quaternion.cpp
│       │   └── Transform.cpp
│       │
│       └── platform/
│           └── web/
│               └── bindings.cpp
│
├── tests/                   # Native unit tests
│   ├── cube/
│   ├── math/
│   ├── graphics/
│   └── interaction/
│
├── subprojects/             # external C++ dependencies
│   └── thorvg/
│
├── cross/                   # target toolchain configuration
│   └── wasm32.txt
│
├── meson.build
├── meson_options.txt
├── build_wasm.sh
│
└── .github/
    └── workflows/
        └── deploy-pages.yml
```

top-level(최상위)만 봐도 역할이 명확합니다.

```text
web         → Browser application
engine      → C++ / WASM engine
tests       → Native unit tests
subprojects → external C++ dependencies
cross       → target toolchain configuration
```

build output도 root에 두면 전체적으로 대칭이 잘 맞습니다.

```text
build/
├── native/
└── wasm/
```

처음에는 `passes/` 등을 바로 만들지 않고 필요해질 때 분리해도 됩니다.

---

## 4. Cube domain(큐브 도메인)

Cube logic은 rendering이나 Camera를 전혀 모르게 합니다.

내부 move 표현은 `R`, `U`, `L` 문자열 자체보다 일반화합니다.

```cpp
enum class Axis {
    X,
    Y,
    Z
};

struct CubeMove {
    Axis axis;
    LayerMask layers;
    int quarterTurns;
};
```

예를 들어:

```text
R
→ X axis
→ layer {+1}

M
→ X axis
→ layer {0}

Rw
→ X axis
→ layers {0, +1}

x
→ X axis
→ layers {-1, 0, +1}
```

`R U R' U'` 같은 notation(표기법)은:

```text
String
   ↓
CubeNotation
   ↓
CubeMove[]
```

로만 변환합니다.

이렇게 하면 Camera orientation(카메라 방향)과 Rubik notation이 섞이지 않습니다.

---

## 5. Graphics pipeline(그래픽스 파이프라인)

Graphics는 functional style(함수형 스타일)을 유지합니다.

```text
CubeState
    ↓
BuildScene           (stickers + shadow casters)
    ↓
Model Transform
    ↓
Shadow               (casters → ground shadow, world space)
    ↓
View Transform
    ↓
Light                (Blinn-Phong: face colour + highlight, view space)
    ↓
Projection
    ↓
Back-face Culling
    ↓
Depth Sort
    ↓
ThorVG Render
```

코드가 가능하면 이런 느낌이 되도록 설계합니다.

```cpp
auto scene =
    cube
    | BuildScene{}
    | Transform(model)
    | Shadow(light, camera)
    | View(camera)
    | Light(light, camera)
    | Project(projection)
    | Cull{}
    | DepthSort{};

renderer.render(scene);
```

`Shadow`와 `Light`는 [Phase 19](./tasks/19-lighting-and-shadow.md)에서 들어왔습니다. 조명은 renderer가 아니라 pass입니다 — RenderScene은 여전히 ThorVG를 모르고, 그림자 그룹과 하이라이트를 기하와 색으로만 넘기며, 그것을 Scene 합성·GaussianBlur·blend·gradient·mask·clip으로 옮기는 것은 renderer의 일입니다.

꼭 `operator|`를 실제로 구현할 필요는 없습니다. 중요한 건:

```text
Pass = Input → Output
```

구조입니다. 각 pass가 독립적이어야 native unit test(네이티브 단위 테스트)가 쉽습니다.

---

## 6. ThorVG의 위치

ThorVG dependency(의존성)는 최대한 graphics 마지막에만 둡니다.

```text
Graphics Pipeline
       ↓
RenderScene
       ↓
ThorVGRenderer
       ↓
tvg::Shape
       ↓
Canvas
```

그래서 `CubeState`, `ProjectionPass` 같은 코드는:

```cpp
#include <thorvg.h>
```

를 하지 않는 방향이 좋습니다.

이를 통해 ThorVG integration과 일반 3D math/render logic을 분리합니다.

---

## 7. Interaction은 pipeline이 아니라 state machine(상태 머신)

Mouse interaction은 시간에 따라 상태가 계속 유지되므로:

```text
PointerDown
PointerMove
PointerMove
PointerMove
PointerUp
```

을 처리하는 `InteractionController`가 적합합니다.

Web에서는 browser-level pointer state만 관리합니다.

```ts
type PointerState = {
  active: boolean;
  pointerId?: number;

  x?: number;
  y?: number;

  target?: "cube" | "empty";
};
```

### Pointer Down

```text
Browser PointerDown
      ↓
engine.pointerDown(x, y)
      ↓
ThorVG / Picking
      ↓
Cube인가 Empty인가 판정
      ↓
Engine InteractionState 저장
```

Web에서는 아래 정도만 기억합니다.

```text
pointer active
pointerId
현재 위치
cube / empty 여부
```

### Pointer Move

```text
PointerMove
     ↓
Web에서 dx / dy 계산
     ↓
engine.pointerDrag(x, y, dx, dy)
     ↓
Engine
     ↓
현재 InteractionState 확인
```

Cube를 잡고 있으면:

```text
screen drag
    ↓
cube-local rotation axis 해석
    ↓
active rotation angle 변경
```

빈 공간인 경우 초기 버전에서는 아무 일도 하지 않고, 이후:

```text
empty drag
→ camera orbit
```

으로 확장할 수 있습니다.

### Pointer Up

```text
PointerUp
    ↓
engine.pointerUp()
```

Cube rotation 중이었다면:

```text
현재 angle
    ↓
nearest 90°
    ↓
snap animation
    ↓
CubeState commit
```

예:

```text
37° → 0°
67° → 90°
143° → 180°
```

Web에서는 pointer state만 초기화합니다.

---

## 8. Logical state와 visual state 분리

CubeState는 항상 discrete state(이산 상태)만 가집니다.

```text
0°
90°
180°
270°
```

Drag 도중의 `23.7°` 같은 값은 별도 transient state(임시 상태)입니다.

```cpp
struct ActiveRotation {
    CubeMove move;
    float angle;
};
```

렌더링할 때:

```text
CubeState
    +
ActiveRotation
    ↓
Graphics Pipeline
```

형태로 합치고, rotation 종료 후에만:

```cpp
cube.apply(move);
```

해서 logical state를 갱신합니다.

---

## 9. Web 영역

Web은 매우 얇게 유지합니다. `CubeEngine.ts`가 WASM boundary(경계)가 됩니다.

```ts
class CubeEngine {
  pointerDown(x: number, y: number) {}
  pointerDrag(x: number, y: number, dx: number, dy: number) {}
  pointerUp() {}

  scramble() {}
  reset() {}
}
```

UI에서는 아래 정도만 알면 됩니다.

```ts
scrambleButton.onclick = () => {
  engine.scramble();
};
```

---

## 10. UI

UI는 HTML을 canvas 위에 overlay(오버레이)합니다.

```html
<div id="app">
  <canvas id="cube"></canvas>

  <div class="toolbar">
    <button id="scramble">Scramble</button>
    <button id="reset">Reset</button>
    <span id="timer">00:00.00</span>
  </div>
</div>
```

ThorVG로 button이나 timer를 그리지 않고, Browser의 기능을 그대로 활용합니다.

- DOM
- CSS
- accessibility(접근성)
- PointerEvent
- keyboard event

또한 SDL은 사용하지 않습니다. Web input과 Vite를 직접 쓰므로 window/input layer가 별도로 필요하지 않습니다.

---

## 11. Test 전략

빌드는 두 종류로 나눕니다.

```text
Native Build
WASM Build
```

Native unit test는 Catch2를 사용합니다.
`assert()` 기반 검증은 `NDEBUG` 빌드에서 제거되어 빈 테스트가 통과하므로 사용하지 않습니다.

TypeScript boundary는 fake Emscripten module을 주입하는 Vitest unit test로 검증하고,
browser end-to-end test는 실제 WASM 연결과 pixel 계약 검증만 담당합니다.

Native에서 최대한 많이 테스트합니다. 아래는 browser가 없어도 테스트할 수 있습니다.

```text
CubeState
CubeMove
Notation
Math
Projection
Cull
Depth sort
Drag interpretation
```

```text
C++ 변경
   ↓
native incremental build
   ↓
unit test
```

ThorVG/Web integration을 확인할 때만:

```text
WASM build
   ↓
Vite
   ↓
Browser
```

를 사용합니다.

---

## 12. 개발 workflow

```text
Cube / Math 수정
→ Native test

Graphics pipeline 수정
→ Native test
→ 필요할 때 WASM visual test

Interaction logic 수정
→ Native test
→ WASM mouse test

HTML / CSS / TS 수정
→ Vite HMR
```

C++ 수정마다 browser 전체 setup을 다시 할 필요는 없고, WASM도 Ninja incremental build(증분 빌드)를 사용합니다.

---

## 13. Build 흐름

개발:

```text
Meson + Emscripten
       ↓
rubiks.js
rubiks.wasm
       │
       ▼
      Vite
       │
       ▼
Browser
```

Production:

```text
ThorVG
   ↓
Meson / Emscripten
   ↓
WASM artifact
   ↓
Vite build
   ↓
dist/
```

---

## 14. GitHub Pages

GitHub Actions에서는:

```text
Push main
   ↓
Checkout
   ↓
Submodule checkout
   ↓
Install Emscripten
   ↓
Install Meson/Ninja
   ↓
Build WASM
   ↓
npm ci
   ↓
vite build
   ↓
dist/
   ↓
GitHub Pages
```

로 만듭니다.

GitHub Pages는 최종적으로:

```text
index.html
JS
CSS
WASM
```

만 static serving(정적 서빙)합니다. Backend server는 없습니다.

---

## 15. 구현 순서

처음부터 Rubik 전체를 구현하지 말고 이 순서로 진행합니다.

1. Meson + Emscripten 빌드 골격을 직접 구성하고 native/WASM 빌드 확인
2. GitHub Pages workflow가 동작하는지 확인
3. Rubik용 새 repository remote 연결
4. Vite + Vanilla TS 추가
5. Emscripten-generated HTML 대신 Vite `index.html` 사용
6. TypeScript ↔ WASM 최소 API 연결
7. ThorVG canvas에 사각형 하나 렌더링
8. `math` 모듈 작성
9. 3D cube 한 개 projection
10. camera + graphics pipeline 구축
11. 3×3×3 Cubie 생성
12. CubeState / CubeMove 작성
13. ThorVG hit test 기반 `pointerDown`
14. drag에 따른 실시간 layer rotation
15. pointerUp → 90° snapping
16. scramble/reset
17. UI/timer
18. native tests 추가
19. GitHub Pages production build 정리

특히 **1~7까지를 먼저 끝내는 것**이 좋습니다. 여기까지 되면 ThorVG + WASM + Vite + GitHub Pages라는 가장 위험한 integration 부분이 먼저 검증됩니다.

---

## 16. 시작하기

`web/`를 Vite로 먼저 만든 다음, root에서 `engine/`, `subprojects/`, `cross/`를 하나씩 구축합니다.

```bash
mkdir thorvg-rubiks
cd thorvg-rubiks

npm create vite@latest web -- --template vanilla-ts
cd web
npm install
```

`web/package.json`이 안쪽에 있으므로 명령은 기본적으로:

```bash
cd web
npm run dev
```

가 됩니다. 이게 번거로우면 나중에 root에 간단한 `Makefile`을 두면 됩니다.

```makefile
web:
	cd web && npm run dev

wasm:
	./build_wasm.sh

test:
	meson test -C build/native
```

```bash
make web
make wasm
make test
```

초기에는 이것조차 필요 없습니다.
