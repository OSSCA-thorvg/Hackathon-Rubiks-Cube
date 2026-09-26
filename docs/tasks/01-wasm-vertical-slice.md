# Phase 1: WebAssembly Vertical Slice

## Status

`Completed`

## Objective

Vite application에서 WebAssembly로 빌드된 C++ engine을 명시적으로 생성하고 초기화합니다.
Software renderer가 소유한 WASM pixel buffer에 ThorVG로 사각형 하나를 그린 뒤, TypeScript가 해당 memory의 view를 browser `<canvas>`에 표시하여 다음 경로를 end-to-end로 검증합니다.

```text
Vite
  ↓
TypeScript CubeEngine
  ↓
Emscripten ES module
  ↓
C++ Application
  ↓
ThorVG SwCanvas
  ↓
RGBA pixel buffer
  ↓
CanvasRenderingContext2D
```

이 vertical slice는 이후 graphics, cube domain, interaction 기능이 사용할 Web–WASM–ThorVG 경계를 확정하는 작업입니다.

## Scope

- WASM artifact를 Emscripten ES module factory로 생성
- Vite에서 JavaScript와 WASM artifact 제공
- TypeScript `CubeEngine` wrapper 구현
- C++ application lifecycle 확장
- Software renderer 내부 WASM pixel buffer 구성
- ThorVG `SwCanvas`와 pixel buffer lifetime 통합
- Canvas 크기 및 device pixel ratio 동기화
- 단일 사각형 렌더링
- Resize와 resource cleanup 처리
- 초기화 및 rendering failure 전달
- Native rendering test와 browser smoke test 추가

## Out of scope

- Rubik's Cube state와 move
- 3D vector, matrix, camera, projection
- Graphics pass와 `RenderScene`
- Pointer interaction
- Animation loop
- Scramble, reset, timer와 toolbar
- WebGL 및 WebGPU renderer
- GitHub Actions와 GitHub Pages
- 성능 최적화와 dirty-region rendering

## Architecture decisions

### Emscripten module

WASM output은 page load 시 자동 실행되는 script가 아니라 ES module factory로 생성합니다.
TypeScript가 module instance의 생성 시점과 lifecycle을 소유합니다.

필요한 Emscripten 특성은 다음과 같습니다.

- `MODULARIZE`
- `EXPORT_ES6`
- Web environment 전용 output
- Memory growth 허용
- 종료되지 않는 browser runtime
- 명시적으로 선언한 C ABI만 export
- `EXPORTED_RUNTIME_METHODS`로 `HEAPU8` export

최근 Emscripten은 heap view를 module instance에 기본으로 export하지 않습니다.
TypeScript가 pixel buffer view를 만들 때 사용하는 `HEAPU8`은 `-sEXPORTED_RUNTIME_METHODS=HEAPU8`로 명시적으로 export합니다.

Native smoke executable의 `main()`은 유지합니다.
WASM target은 `main.cpp`를 소스에서 제외하고 `--no-entry`로 링크하여 entry point가 없는 library로 빌드합니다.
Module 로드 시 자동 실행되는 코드가 없으므로 초기화 시점과 실패 전달은 전적으로 C ABI와 TypeScript boundary가 소유합니다.
`main.cpp`는 native 전용 소스가 되므로 `#ifndef __EMSCRIPTEN__` 분기를 제거합니다.

### Artifact placement

Meson은 build directory에 원본 JavaScript와 WASM artifact를 생성합니다.
`build_wasm.sh`는 빌드가 성공한 뒤 두 artifact를 Vite module graph 안의 generated 디렉터리로 동기화합니다.

```text
build/wasm/engine/src/
├── thorvg-rubiks.js
└── thorvg-rubiks.wasm
             ↓
web/src/wasm/generated/
├── thorvg-rubiks.d.ts   (source, Git 추적)
├── thorvg-rubiks.js     (generated)
└── thorvg-rubiks.wasm   (generated)
```

생성된 JavaScript와 WASM은 source가 아니므로 Git에서 추적하지 않습니다.
`thorvg-rubiks.d.ts`는 손으로 관리하는 선언 파일로 Git에서 추적하며, export한 C ABI가 바뀔 때만 함께 수정합니다.
TypeScript는 generated `.js` import를 이 선언으로 resolve하므로 artifact가 없어도 typecheck는 통과하고, artifact가 없으면 Vite build가 명확한 오류로 실패합니다.

`public/`의 파일은 module graph 바깥이라 source에서 import할 수 없습니다.
Dev server는 dynamic import를 `?import` query가 붙은 module 요청으로 처리하면서 public 파일 import를 거부하므로, artifact를 `public/`에 두는 방식은 사용하지 않습니다.
대신 generated module을 module graph 안에 두면 Vite가 다음을 직접 처리합니다.

- Factory는 literal 경로의 동적 import로 로드하며 별도 chunk로 code-split됩니다. `import()`의 결과는 module namespace이고 Emscripten factory는 `default` export입니다.
- Emscripten이 `new URL('thorvg-rubiks.wasm', import.meta.url)`로 참조하는 WASM binary는 Vite asset 처리로 dev와 production 모두에서 자동 resolve되며, production에서는 해시된 asset으로 방출됩니다.
- Asset URL 재작성에 Vite `base` 설정이 자동 반영되므로 GitHub Pages 하위 경로(`/Hackathon-Rubiks-Cube/`) 배포를 위한 수동 URL 조합이 필요 없습니다. `base` 설정 자체는 Phase 2 배포에서 지정합니다.

```ts
const { default: createModule } = await import(
  './generated/thorvg-rubiks.js'
);

const module: ThorvgRubiksModule = await createModule();
```

### Rendering boundary

`ThorVGSoftwareRenderer`는 ThorVG `SwCanvas`와 pixel buffer를 함께 소유합니다.
Pixel buffer는 WASM linear memory에 위치하며 `SwCanvas::target()`에 직접 전달됩니다.
ThorVG는 별도의 최종 buffer를 생성하지 않고 renderer가 소유한 buffer에 rasterization 결과를 기록합니다.

`Application`과 일반 graphics pipeline은 pixel buffer의 allocation, pointer와 pixel format을 알지 않습니다.
향후 OpenGL 또는 WebGPU backend는 동일한 renderer 역할을 구현하되 backend 전용 context 또는 surface를 render target으로 사용합니다.

TypeScript는 C ABI로 pointer와 byte length를 조회하고 같은 WASM memory를 가리키는 `Uint8ClampedArray` view로 `ImageData`를 만듭니다.
그 뒤 `CanvasRenderingContext2D.putImageData()`로 화면에 표시합니다.
별도의 JavaScript pixel array를 만들지 않으므로 WASM과 JavaScript 사이의 중간 복사는 발생하지 않습니다.

Canvas 2D가 `putImageData()`를 통해 내부 surface에 반영하는 복사는 software backend의 최종 출력 비용으로 허용합니다.

### Rendering sequence

한 frame의 ThorVG rendering은 다음 순서를 따르며 각 호출 결과를 확인합니다.

```text
Canvas::update()
    ↓
Canvas::draw()
    ↓
Canvas::sync()
    ↓
Pixel buffer view
    ↓
CanvasRenderingContext2D.putImageData()
```

`Canvas::draw()`는 `clear`를 `true`로 호출합니다.
이전 frame의 내용과 resize 직후의 초기화되지 않은 buffer를 지운 뒤 rasterization을 수행하기 위함입니다.

TypeScript는 `sync()`가 성공하여 C++ render 함수가 반환된 뒤에만 pixel buffer를 읽습니다.

### Pixel format

Web boundary에서 사용하는 pixel은 다음 계약을 따릅니다.

- C++ storage: `std::uint32_t` pixel array
- ThorVG color space: `tvg::ColorSpace::ABGR8888S`
- JavaScript byte view: RGBA on little-endian WebAssembly
- Channel size: 8 bit
- Alpha: unpremultiplied
- Buffer size: `width * height * 4`
- Ownership: `ThorVGSoftwareRenderer`
- JavaScript access: borrowed read view
- Lifetime: 성공한 initialize 또는 resize부터 다음 resize 또는 shutdown까지

`SwCanvas::target()`의 stride는 byte가 아니라 한 row에 포함되는 pixel 수이며 이 phase에서는 width와 동일합니다.
WebAssembly는 little-endian이므로 `ABGR8888S`의 32-bit pixel을 byte 단위로 보면 Canvas `ImageData`가 요구하는 RGBA 순서가 됩니다.

ThorVG와 engine은 thread 없이 빌드하며 이 설정은 zero-copy 설계의 전제 조건입니다.
Pthread를 활성화하면 WASM memory가 `SharedArrayBuffer`가 되는데, `SharedArrayBuffer` 위의 `Uint8ClampedArray`로는 `ImageData`를 생성할 수 없습니다.
또한 `SharedArrayBuffer`는 GitHub Pages 배포에 COOP와 COEP header를 추가로 요구합니다.
Thread 도입을 검토할 때는 이 pixel 경로 전체를 다시 설계해야 합니다.

## API contract

Browser에 노출하는 C ABI는 primitive type만 사용합니다.

```cpp
extern "C" {

int thorvg_rubiks_initialize(std::uint32_t width,
                             std::uint32_t height) noexcept;

int thorvg_rubiks_resize(std::uint32_t width,
                         std::uint32_t height) noexcept;

int thorvg_rubiks_render() noexcept;

std::uintptr_t thorvg_rubiks_pixel_buffer() noexcept;

std::uint32_t thorvg_rubiks_pixel_byte_length() noexcept;

void thorvg_rubiks_shutdown() noexcept;

}
```

반환값 규칙은 다음과 같습니다.

- 성공: `1` // true
- 실패: `0` // false
- 유효한 software buffer가 없을 때 pointer와 byte length: `0`
- `shutdown()`은 초기화 여부와 관계없이 안전하게 호출 가능

### Input validation

- `width`또는 `height`가 0이면 initialize와 resize는 실패합니다.
- `width * height * 4` 계산은 overflow를 검사합니다.
- 합리적인 최대 canvas dimension을 정해 과도한 memory allocation을 거부합니다.
- Dimension 검증은 이미 초기화된 상태에서도 idempotent 성공 분기보다 먼저 수행합니다. 초기화 이후의 `initialize(0, 0)` 같은 호출도 실패하며, 유효한 dimension의 반복 initialize는 활성 buffer를 재구성하지 않는 no-op 성공입니다. 크기 변경은 resize만 담당합니다.
- Resize 실패 처리는 아래 resize failure semantics를 따릅니다.
- Resize는 진행 중인 rendering이 없는 상태에서만 수행합니다. `SwCanvas::target()`은 마지막 `sync()`가 완료되지 않은 상태에서 호출하면 실패합니다.
- 초기화되지 않았거나 유효한 buffer가 없는 상태의 render는 실패합니다.
- Pointer와 byte length query는 software renderer가 소유한 현재 buffer만 반환합니다.
- GPU backend에서는 software pixel buffer query가 0을 반환하거나 backend별 API에서 노출되지 않아야 합니다.

### Resize failure semantics

ThorVG `SwRenderer::target()`은 새 pointer와 크기를 내부 surface에 먼저 기록한 뒤 후속 설정을 수행하므로, 실패 시 이전 target이 보존된다는 보장이 ThorVG 자체에는 없습니다.
기존 상태 보존은 engine이 다음 순서로 직접 보장합니다.

1. Dimension, overflow와 최대 크기 검사를 모두 통과한 뒤에만 다음 단계로 진행합니다.
2. 새 pixel buffer를 non-throwing allocation으로 먼저 확보합니다. 실패하면 ThorVG를 호출하지 않고 `0`을 반환하며 기존 buffer와 target은 그대로 유지됩니다.
3. 새 buffer로 `SwCanvas::target()`을 호출합니다. 성공하면 그때 이전 buffer를 해제합니다.
4. 3이 실패하면 이전 buffer로 `SwCanvas::target()`을 다시 호출해 rollback을 시도합니다.
5. Rollback까지 실패하면 renderer는 unusable 상태가 됩니다. 이후 render는 실패하고 pointer와 byte length query는 `0`을 반환하며, shutdown 후 initialize로만 복구합니다.
6. Target 교체가 성공한 뒤의 scene 재구성 실패도 unusable 상태로 처리합니다. 새 크기가 이미 반영되어 되돌릴 이전 target이 없으므로 rollback 대상이 아니며, 복구 경로는 5와 같습니다.

이 프로젝트는 `-fno-exceptions`로 빌드하므로 allocation 실패를 exception으로 받을 수 없습니다.
Pixel buffer는 `std::vector` 대신 `new (std::nothrow)` 기반 allocation을 사용해 실패를 null 검사로 확인하고 return code로 전달합니다.

Allocation과 target 설정은 실제 입력으로는 결정적으로 실패시킬 수 없으므로, renderer는 이 두 연산을 protected virtual seam으로 노출합니다.
Native test는 fault-injecting subclass로 2(allocation 실패 시 보존), 4(target 실패 후 rollback 성공), 5(rollback 실패 후 unusable)의 세 분기를 결정적으로 검증합니다.
두 seam은 초기화와 resize에서만 호출되고 frame 단위 render 경로에서는 호출되지 않으며, renderer는 이미 `Renderer` interface로 polymorphic하므로 추가 비용이 없습니다.

## TypeScript boundary

`web/src/wasm/CubeEngine.ts`가 generated module을 직접 사용하는 유일한 Web module이 됩니다.

```ts
type CubeEngineSize = {
  readonly width: number;
  readonly height: number;
};

type CubeEngineOptions = {
  readonly loadModule?: () => Promise<ThorvgRubiksModule>;
};

class CubeEngine {
  static create(
    canvas: HTMLCanvasElement,
    options?: CubeEngineOptions,
  ): Promise<CubeEngine>;

  resize(size: CubeEngineSize): void;
  render(): void;
  dispose(): void;
}
```

구현 시 다음 규칙을 지킵니다.

- Public method와 type의 역할을 doc comment로 명시합니다.
- Generated Emscripten API는 별도 TypeScript type으로 감쌉니다.
- WASM return code를 확인하고 실패 시 구체적인 `Error`를 발생시킵니다.
- `dispose()` 이후 호출은 실패해야 합니다.
- 같은 instance에서 `dispose()`를 반복 호출해도 안전해야 합니다.
- TypeScript는 pixel buffer를 할당하거나 해제하지 않습니다.
- Pixel pointer와 byte length는 initialize와 resize 성공 후 다시 조회합니다.
- WASM memory growth 이후에는 기존 `Uint8ClampedArray`와 `ImageData`를 버리고 현재 `HEAPU8.buffer`, pointer와 length로 다시 생성합니다.
- 크기, pointer와 WASM memory가 바뀌지 않았다면 기존 view와 `ImageData`를 재사용합니다.
- `dispose()`는 C++ shutdown을 호출한 뒤 borrowed view 참조를 제거합니다.
- `ImageData`는 WASM memory를 직접 가리키는 view로 생성하며 별도의 pixel array로 복사하지 않습니다.
- `create()`는 C++ 초기화가 성공한 뒤 발생하는 모든 실패에서 shutdown을 호출하고 오류를 다시 던집니다. 초기화 성공 이후 유일한 소유자가 사라지는 leak을 막기 위함입니다.
- Module factory는 `loadModule` option으로 주입할 수 있으며 기본값은 module graph의 generated module입니다. Unit test는 fake module을 주입해 boundary 계약을 검증합니다.
- Pointer와 byte length는 0 여부만이 아니라 전체 metadata 계약을 검증한 뒤에만 commit합니다: safe integer, `width * height * 4`와 정확히 일치하는 길이, 4-byte pixel 정렬, pointer + length가 현재 heap 안에 있는지. Resize 이후 이 검증이 실패하면 instance는 dispose되어 unusable 상태가 됩니다.
- Public `resize()`는 runtime에서 dimension을 검증합니다. TypeScript type은 runtime 값을 보호하지 못하므로, finite positive integer와 최대 dimension을 벗어난 값은 Emscripten i32 경계에 닿기 전에 `Error`로 거부합니다.
- Page lifecycle은 `startApp` controller가 소유하며 engine, `ResizeObserver`, window listener를 하나의 teardown 경로로 함께 정리합니다. Resize 또는 render 실패와 실제 unload 모두 이 경로를 사용합니다. Resize 실패 후 native 쪽이 보존과 unusable 중 어느 상태인지 boundary에서 관찰할 수 없으므로 재시도 대신 전체 정리를 선택합니다.
- Engine이 live가 된 이후의 setup(observer 생성·등록, listener 등록, ready 전환)은 하나의 transaction으로 보호합니다. 도중 실패하면 이미 설치된 자원을 정리하고 engine을 dispose한 뒤 오류를 전달합니다.
- Teardown은 active flag를 먼저 내린 뒤 정리를 수행합니다. 반복 teardown과 teardown 이후 도착하는 stale observer/event callback은 no-op이 되어 disposed engine을 다시 건드리지 않습니다.
- Teardown의 각 해제 단계(observer disconnect, listener 제거, engine dispose)는 독립적으로 시도하는 best-effort 정리입니다. 앞 단계의 예외가 뒤 단계를 중단시키지 않고, teardown 자체는 예외를 던지지 않으므로 원래의 resize/render 오류가 항상 `onError()`에 도달합니다. 첫 cleanup 오류는 모든 자원 해제를 시도한 뒤 console에 기록합니다.
- Page bootstrap(DOM 배선, state 반영, 실패 → error UI)은 lifecycle을 주입받는 순수 함수로 분리하여, startup 실패와 ready 이후 실패가 실제 error UI로 이어지는지 integration test로 검증합니다.
- `pagehide`는 `persisted`가 아닐 때(실제 unload)만 정리합니다. BFCache에 보관된 page는 살아있는 상태로 복원되므로 dispose하지 않으며, `pageshow`의 `persisted` 복원 시 cache에 있는 동안 놓친 viewport 변화를 재적용합니다.

## Canvas sizing

Canvas drawing buffer는 CSS size와 device pixel ratio를 사용해 결정합니다.

```text
drawingWidth  = round(cssWidth  * devicePixelRatio)
drawingHeight = round(cssHeight * devicePixelRatio)
```

- 결과는 최소 1 pixel로 제한합니다.
- C++ API가 허용하는 최대 dimension을 넘지 않도록 제한합니다.
- `NaN`이나 무한대 같은 비유한 입력은 1 pixel로 정규화합니다.
- `ResizeObserver`로 CSS size 변화를 감지합니다.
- Device pixel ratio 변화도 다음 resize 또는 window resize에서 반영합니다.
- 실제 drawing buffer 크기가 달라졌을 때만 C++ resize를 호출하고 pixel view를 갱신합니다.

## Rendered scene contract

Native test와 browser smoke test는 같은 scene 계약을 검증합니다.
빈 화면, channel 순서 오류, alpha 오류와 resize 후 stale buffer를 구별할 수 있도록 관찰 가능한 출력을 고정합니다.

- 배경: 불투명 단색 `RGBA(32, 32, 32, 255)`
- 사각형: 불투명 단색 `RGBA(230, 57, 70, 255)`
- 위치와 크기: drawing buffer 중앙, 각 변은 drawing buffer 크기의 50%
- 두 색 모두 alpha `255`로 고정하여 premultiplication 차이가 pixel 비교에 영향을 주지 않게 합니다.

검증 규칙은 다음과 같습니다.

- 중앙 pixel `(width / 2, height / 2)`은 사각형 색과 일치합니다.
- 네 모서리 pixel `(0, 0)`, `(width - 1, 0)`, `(0, height - 1)`, `(width - 1, height - 1)`은 배경색과 일치합니다.
- 색 비교는 R, G, B, A channel을 각각 exact match로 검사하여 channel 순서 오류를 잡습니다.
- Resize 후에는 pointer와 byte length를 다시 조회하고 새 크기 기준으로 중앙과 모서리 pixel 검증을 반복합니다.

## UI states

Phase 1의 page는 다음 상태를 명확히 표시합니다.

- Loading: WASM module을 가져오고 초기화하는 중
- Ready: 사각형이 canvas에 표시됨
- Error: module fetch, initialization 또는 rendering 실패

Error 상태에서는 원인을 개발자 console에 기록하고 사용자에게 짧은 실패 메시지를 표시합니다.

## Implementation steps

### 1. Build output

- [x] Native executable과 WASM module target 분리
- [x] WASM target에서 `main.cpp` 제외 및 `--no-entry` 링크
- [x] `main.cpp`의 `#ifndef __EMSCRIPTEN__` 분기 제거
- [x] WASM target을 ES module factory로 구성
- [x] 필요한 C ABI만 export
- [x] `EXPORTED_RUNTIME_METHODS`로 `HEAPU8` export
- [x] `build_wasm.sh`에 artifact 검증 및 동기화 추가
- [x] Generated artifact를 `.gitignore`에 추가

### 2. C++ rendering lifecycle

- [x] `ThorVGSoftwareRenderer`가 `SwCanvas`와 pixel buffer를 소유하도록 구현
- [x] Application이 renderer의 구체적인 target을 알지 않도록 경계 정의
- [x] 크기 변경 시 software target 재구성
- [x] 사전 검증과 nothrow allocation을 선행하는 resize failure semantics 구현
- [x] Target 실패 시 rollback과 unusable 상태 처리 구현
- [x] Target 재설정 전 마지막 `sync()` 완료 보장
- [x] Rendered scene contract를 따르는 단일 사각형 scene 생성
- [x] ThorVG update, clear를 포함한 draw와 sync 결과 확인
- [x] Pixel pointer와 byte length query 구현
- [x] 모든 resource를 해제하는 idempotent shutdown 구현
- [x] Allocation과 target 설정을 protected virtual seam으로 노출
- [x] 이미 초기화된 상태에서도 dimension 검증을 먼저 수행하는 initialize 구현

### 3. Web bindings

- [x] Width와 height를 받도록 initialize binding 변경
- [x] Resize binding 추가
- [x] Render binding 추가
- [x] Pixel pointer와 byte length binding 추가
- [x] Binding type과 return contract 문서화

### 4. TypeScript integration

- [x] Generated Emscripten module type 정의
- [x] Module graph 안의 literal 동적 import로 module factory 로드
- [x] `CubeEngine.create()` 구현
- [x] Resize, render와 dispose 구현
- [x] WASM memory view로 `ImageData` 생성
- [x] Resize와 memory growth 시 pixel view 재생성
- [x] Buffer가 유지되는 동안 `ImageData` 재사용
- [x] Canvas `ResizeObserver` 연결
- [x] Loading, ready와 error UI 구현
- [x] Pixel metadata 계약과 runtime dimension 검증 구현
- [x] `startApp` controller로 page lifecycle과 단일 teardown 경로 분리
- [x] Engine 생성 이후 setup을 transaction으로 보호하고 실패 시 unwind
- [x] Active guard로 teardown 반복과 stale callback 차단
- [x] Best-effort teardown으로 cleanup 예외가 남은 해제와 오류 전달을 막지 않도록 구현
- [x] `pagehide`/`pageshow`의 BFCache `persisted` 처리
- [x] DOM 배선을 담당하는 `bootstrap` 함수 분리
- [x] Vite starter content 제거

### 5. Verification

- [x] Catch2(meson wrapdb subproject) 기반 native test framework 구성
- [x] Native rendering lifecycle test 추가
- [x] Rendered scene contract의 중앙과 모서리 pixel 검증 native test 추가
- [x] Invalid dimension과 overflow test 추가
- [x] 초기화 이후 invalid dimension initialize 거부 test 추가
- [x] Fault-injection으로 resize rollback 세 분기(보존, rollback, unusable) test 추가
- [x] Vitest와 fake module 기반 TypeScript boundary unit test와 `test:unit` script 추가
- [x] Pixel metadata 계약과 invalid runtime size 거부 unit test 추가
- [x] Mock engine과 observer 기반 `startApp` lifecycle unit test 추가 (실패 cleanup, BFCache 왕복 포함)
- [x] Setup 실패 unwind, stale callback 차단과 teardown 반복 no-op unit test 추가
- [x] Cleanup 예외 시 나머지 해제 완료와 원래 오류 보존 unit test 추가
- [x] 실제 DOM error/ready UI 전환을 검증하는 bootstrap integration test 추가
- [x] Playwright 기반 browser e2e 환경과 `test:e2e` script 설정
- [x] Rendered scene contract pixel 검증을 포함한 browser smoke test 추가
- [x] Resize 후 pointer, byte length와 pixel 재검증 browser test 추가
- [x] Native, WASM과 Vite production build 실행

Native test assertion은 `assert()`가 아니라 Catch2 `REQUIRE`를 사용합니다.
`assert()` 기반 검증은 `NDEBUG` 빌드에서 호출째로 제거되어 빈 테스트가 통과하므로 사용하지 않습니다.
Catch2는 meson wrapdb subproject로 받아오므로 최초 configure에는 네트워크 접근이 필요합니다.

TypeScript unit test는 `loadModule` 주입으로 fake module을 사용해 실패 전파, dispose 계약, memory growth 후 view 재생성을 검증합니다.
Browser e2e는 실제 WASM 연결과 pixel 계약만 담당하며, WASM artifact가 `web/src/wasm/generated`에 동기화된 뒤에만 의미가 있으므로 `build_wasm.sh` 이후에 실행합니다.
CI에서의 Playwright browser 설치는 Phase 2 production and deployment에서 처리합니다.

## Acceptance criteria

- `./build_wasm.sh` 한 번으로 Vite가 사용할 JavaScript와 WASM artifact가 생성됩니다.
- `npm run dev`에서 별도의 generated HTML 없이 Vite page가 WASM module을 로드합니다.
- ThorVG software renderer가 그린 사각형이 canvas에 표시되고 rendered scene contract의 중앙과 모서리 pixel 검증을 통과합니다.
- Canvas drawing buffer가 CSS size와 device pixel ratio를 반영합니다.
- Resize 후 pointer와 byte length를 다시 조회하고 새 크기 기준의 pixel 검증을 통과합니다.
- Resize 실패 시 resize failure semantics에 정의된 보존, rollback 또는 unusable 상태가 관찰됩니다.
- Pixel buffer는 software renderer가 소유하고 Application과 graphics pipeline에 노출되지 않습니다.
- TypeScript는 software pixel buffer를 할당하거나 해제하지 않습니다.
- JavaScript에 별도의 pixel array를 생성하지 않고 WASM memory view를 사용합니다.
- ThorVG rendering은 update, draw와 sync가 성공한 뒤 browser canvas에 반영됩니다.
- Initialization, resize 또는 rendering 실패가 TypeScript error로 전달됩니다.
- C++ 초기화 성공 이후의 create 실패는 shutdown을 수행한 뒤 전달됩니다.
- 초기화된 상태의 invalid dimension initialize도 실패합니다.
- Shutdown과 dispose를 반복 호출해도 crash나 invalid access가 발생하지 않습니다.
- Resize rollback 세 분기가 native test에서 결정적으로 검증됩니다.
- Native unit test, TypeScript unit test, WASM build, browser smoke test와 Vite production build가 모두 통과합니다.
- Cube domain, 3D math, pointer interaction 코드는 이 phase에 포함되지 않습니다.

## 개정 기록

- **Resize failure semantics의 rollback을 fail-stop으로 개정합니다 ([Phase 6.5](./06.5-pre-enhancement-cleanup.md)의 결정).** 이 문서의 4(rollback 시도)를 없애고, target 교체 실패는 곧바로 unusable로 갑니다. 유일한 소비자인 web이 resize 실패를 언제나 전체 teardown으로 처리해 rollback의 성공 여부를 관찰하지 않고, rollback 분기는 production에서 도달할 수 없어 test 전용 virtual seam(`allocate_pixels`/`set_target` override)으로만 검증되고 있었습니다. 검증·allocation 선검사 실패 시 기존 target 보존(1~3)과 render 경로의 unusable 처리는 그대로입니다. Fault-injection seam과 그 전용 test는 rollback과 함께 제거됩니다.
- **캔버스 하나에서 surface 여럿으로 넓힙니다 ([Scenes and surfaces](./scenes-and-surfaces.md)의 결정, 2026-09-26).** 엔진은 최대 네 surface에 그리고, 페이지는 view마다 캔버스를 하나씩 둡니다. 이 문서의 ABI는 surface 0에 대한 것으로 뜻이 그대로입니다. `initialize()`가 만드는 surface 0이 모든 장면을 가지고, `resize`, `pixel_buffer`, `pixel_byte_length`가 surface 0을 가리키기 때문입니다. 다만 페이지(TypeScript)는 이제 surface 0도 다른 surface와 같은 호출(`resize_surface`, `surface_pixel_buffer`, …)로 다루므로, 이 문서의 한 캔버스 호출은 엔진에 남아 있을 뿐 페이지에서는 쓰지 않습니다. 다른 surface는 `resize_surface`, `surface_pixel_buffer`, `surface_pixel_byte_length`로 같은 계약(primitive만, 버퍼는 크기가 바뀔 때까지 유효, 초기화 전에는 0)을 따릅니다. 0×0은 surface를 치워 두는 뜻으로 새로 생겼고, `resize()`는 그것을 받지 않습니다. `render()`는 이제 바뀐 surface만 그립니다. 그래서 TypeScript는 `surface_frame`이 움직인 캔버스만 복사합니다.

## Verification commands

```bash
# Native build and tests
meson setup build/native
meson compile -C build/native
meson test -C build/native --print-errorlogs

# WASM build
source /path/to/emsdk/emsdk_env.sh
./build_wasm.sh

# Vite production build
npm --prefix web run build

# TypeScript boundary unit tests
npm --prefix web run test:unit

# Browser smoke test
npm --prefix web run test:e2e
```

## Completion

모든 acceptance criteria와 verification command를 통과한 뒤 다음 작업을 수행합니다.

- 이 문서의 status를 `Completed`로 변경합니다.
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 1을 완료 처리합니다.
