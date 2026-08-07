# Phase 3: Math and Graphics Foundation

## Status

`Proposed`

## Objective

ThorVG와 독립적인 math 모듈(vector, matrix, quaternion, transform)과 functional-style graphics pipeline을 구축합니다.
고정된 camera로 하나의 3D cube를 model, view, projection, back-face culling, depth sorting을 거쳐 2D `RenderScene`으로 변환하고, ThorVG software renderer가 그 scene을 그리도록 Phase 1의 고정 사각형 scene을 대체합니다.

```text
Cube faces (world space)
    ↓ BuildScene
World-space quads
    ↓ Model transform
    ↓ View transform
    ↓ Projection
Screen-space quads
    ↓ Back-face culling
    ↓ Depth sort
RenderScene (2D)
    ↓
ThorVGSoftwareRenderer
    ↓
Pixel buffer
```

이 phase가 끝나면 rendering 대상은 "renderer 내부에 하드코딩된 사각형"에서 "pipeline이 만들어 낸 scene"으로 바뀌고, Phase 4의 3×3×3 cubie와 Phase 5의 interaction은 이 pipeline의 입력만 바꾸면 됩니다.

## Scope

- Vendored [linalg.h](https://github.com/sgorsten/linalg) 위의 `engine/src/math/` alias 계층과 `Transform`, `look_at`, `perspective`
- `engine/src/graphics/`: `Camera`, `RenderScene`, pipeline pass
- 고정 perspective camera 1개 (orbit 없음)
- 단일 cube(6면, 면당 단색) rendering
- Back-face culling과 painter's algorithm depth sort
- Near-plane guard (clipping이 아니라 face 단위 discard)
- `Renderer` interface를 `render(const RenderScene&)`로 변경
- ThorVG renderer의 per-frame scene 재구성
- Rendered scene contract v2로 native/browser pixel 검증 갱신
- Math와 graphics를 ThorVG 비의존 build target으로 분리

## Out of scope

- 3×3×3 cubie, sticker, `CubeState` (Phase 4)
- Pointer interaction, picking, drag (Phase 5)
- Animation, camera orbit (Phase 5 이후)
- Scramble, reset, timer, UI (Phase 6)
- 다각형 clipping (near plane 포함)
- Lighting, shading, gradient
- C ABI와 TypeScript boundary 변경
- 성능 최적화 (dirty region, shape 재사용)

## Architecture decisions

### Coordinate and matrix conventions

이후 모든 phase가 공유하는 규약이므로 여기서 고정합니다.

- World와 view space는 right-handed, `+Y` up이며 camera는 view space의 `−Z` 방향을 바라봅니다.
- NDC는 x, y ∈ `[−1, 1]`, `+Y` up입니다 (OpenGL 형식).
- Screen space는 origin이 좌상단, `+X` 오른쪽, `+Y` 아래이며 단위는 pixel입니다. Viewport 변환에서 Y를 뒤집습니다.

```text
x_screen = (ndc_x + 1) / 2 * width
y_screen = (1 - ndc_y) / 2 * height
```

- `Mat4`는 column-major 저장, column vector 규약입니다. 변환 적용은 `M * v`, 합성은 `P * V * M` 순서입니다.
- Front face는 바깥에서 보았을 때 counter-clockwise winding입니다.
- Scalar type은 `float`입니다. ThorVG 좌표 API가 `float`이고 rendering 목적에는 충분한 정밀도입니다.

### Math module

기본 vector, matrix, quaternion 산술은 직접 구현하지 않고 [linalg.h](https://github.com/sgorsten/linalg) 단일 헤더를 vendoring해 사용합니다.
손으로 작성한 행렬 연산은 이 프로젝트의 학습 목표가 아니면서 미묘한 버그의 온상이고, linalg는 기본 연산만 제공하므로 projection, camera, pipeline처럼 규약이 실리는 부분은 여전히 직접 구현하기 때문입니다.

- 위치: `engine/third_party/linalg/` (Unlicense이므로 LICENSE 파일과 함께 vendoring)
- 단일 헤더라 wrapdb subproject가 필요 없고 CI에 새 네트워크 의존성이 생기지 않습니다.
- linalg의 `mat`은 column-major 저장이고 `mul(mat, vec)`이 column vector 곱이므로 위 규약과 일치합니다.

`rubiks::math`는 linalg를 감싼 얇은 계층만 노출합니다.

- `Vec2`/`Vec3`/`Vec4` = `linalg::vec<float, N>`, `Mat4` = `linalg::mat<float, 4, 4>` alias
- 대수적 곱은 linalg에서 `mul`입니다 (`operator*`는 elementwise). 혼동을 막기 위해 engine 코드는 alias 계층이 노출하는 연산만 사용하고 linalg를 직접 include하지 않습니다.
- `Quaternion`: linalg quaternion 연산(`rotation_quat`, `qmul` 등)을 감싼 axis-angle 생성과 `Mat4` 변환
- `Transform`: translation(`Vec3`) + rotation(`Quaternion`) + uniform scale(`float`)을 `Mat4`로 합성 (직접 구현)
- `look_at`, `perspective`: 위 규약대로 직접 구현합니다. linalg에도 고수준 transformation helper가 일부 있지만, 규약의 기준을 이 문서와 test에 두기 위해 사용하지 않습니다.

### Camera and fixed view

`Camera`는 view matrix(`look_at`)와 projection matrix(`perspective`)를 소유합니다.
Projection은 orthographic이 아니라 perspective를 사용합니다. 최종 목표인 cube 조작 화면에서 원근이 있어야 면 방향을 읽기 쉽고, Phase 5의 drag 해석도 perspective 기준으로 설계되기 때문입니다.

이 phase의 camera는 다음 값으로 고정합니다. Rendered scene contract의 기대 pixel이 이 값에서 유도되므로 바꾸면 contract도 함께 갱신해야 합니다.

```text
eye        = (3, 3, 3)
target     = (0, 0, 0)
up         = (0, 1, 0)
fov        = 45° (vertical)
near / far = 1.0 / 10.0
aspect     = width / height (resize마다 갱신)
```

Eye가 `(1, 1, 1)` 대각선 위에 있으므로 `+X`, `+Y`, `+Z` 세 면이 대칭으로 보입니다.
이 거리에서 정사각형 viewport의 cube silhouette은 x `[0.13, 0.87]`, y `[0.16, 0.93]` 비율 안에 들어와 어느 변에서도 잘리지 않습니다.

### RenderScene

`RenderScene`은 graphics pipeline의 최종 출력이자 ThorVG renderer의 유일한 입력입니다.
ThorVG type을 일절 포함하지 않는 순수 2D 자료구조입니다.

```cpp
struct RenderFace {
    std::array<math::Vec2, 4> points;  // screen-space pixels
    Color color;                        // RGBA, 8-bit channels
};

struct RenderScene {
    std::vector<RenderFace> faces;  // back-to-front 순서
};
```

`faces`는 depth sort까지 끝난 back-to-front 순서이며 renderer는 순서를 신뢰하고 그대로 그립니다.
Depth 값은 scene에 남기지 않습니다. 정렬은 pipeline의 책임이고 renderer는 2D 그리기만 담당합니다.

### Pipeline passes

각 pass는 상태 없는 `Input → Output` callable로 구현해 native test에서 독립적으로 검증합니다.
DESIGN.md의 스케치대로 pass 적용을 `operator|`로 합성해, pipeline 호출부가 데이터 흐름 순서 그대로 읽히게 합니다.

```cpp
auto scene = build_scene()
    | transform(model)
    | view(camera)
    | project(camera, viewport)
    | cull()
    | depth_sort();
```

`operator|`는 왼쪽 중간 표현에 오른쪽 pass를 적용하는 얇은 문법이며 pass 자체에 로직을 더하지 않습니다.
각 pass는 pipeline 밖에서도 직접 호출할 수 있어야 하고, unit test는 합성이 아니라 pass 단위로 검증합니다.

```text
build_scene()             → world-space face 목록 (기하 + 색)
transform(model)          → world space (Phase 3에서는 identity Transform)
view(camera)              → view space
project(camera, viewport) → screen space (+ near guard)
cull()                    → front face만 유지
depth_sort()              → back-to-front 정렬
```

- `build_scene()`은 이 phase에서는 edge 길이 2, 원점 중심의 axis-aligned cube 6면을 하드코딩으로 생성합니다. Cube는 "6개 면을 독립적인 색으로 칠할 수 있는 정육면체" 모델이며, Phase 4는 같은 단위를 1×1×1 cubie로 바꿔 3×3×3 = 27개(중앙 1개는 보이지 않으므로 생략하면 26개)를 배치하고 `CubeState`가 면 색을 결정하는 자리입니다.
- Model transform은 identity `Transform`을 pipeline에 실제로 통과시킵니다. Quaternion → `Mat4` 경로가 real path에서 한 번은 실행되게 하기 위함입니다.
- Depth sort의 key는 face 4개 vertex의 view-space z 평균입니다. View space에서 camera는 `−Z`를 바라보므로 z가 작을수록(더 음수) 멀고, 오름차순 정렬이 back-to-front입니다. 같은 key는 입력 face index로 tie-break하는 stable sort로 결정성을 보장합니다.
- 단일 convex cube는 culling만으로 가려짐이 해결되지만, Phase 5에서 layer가 회전하는 동안 전체 형상이 non-convex가 되므로 depth sort를 이 phase에서 미리 구축하고 pass 단위로 검증합니다.

### Back-face culling

Culling은 viewport 변환 전 NDC(Y up)에서 shoelace signed area로 판정합니다.

- Front face(바깥에서 CCW)는 NDC에서 signed area가 양수입니다.
- Signed area가 0 이하인 face(back face와 edge-on face)는 제거합니다.

Screen space(Y down)가 아니라 NDC에서 판정하는 이유는 부호 규약을 Y flip과 분리해 단순하게 유지하기 위함입니다.

### Near-plane guard

다각형 clipping은 구현하지 않습니다. 대신 projection pass에서 vertex 하나라도 view-space `z ≥ −near`인 face는 통째로 제거합니다.
고정 camera와 cube 배치에서는 이 조건이 발생하지 않지만, guard가 없으면 perspective divide가 발산하므로 방어선과 unit test는 이 phase에서 확정합니다.
Camera가 움직이는 phase에서 clipping이 필요해지면 그때 별도로 설계합니다.

### ThorVG boundary change

`Renderer::render()`를 `render(const graphics::RenderScene&)`로 변경합니다.

- `ThorVGSoftwareRenderer`는 frame마다 retained paint를 모두 제거하고, 현재 크기의 배경 shape와 face당 `tvg::Shape` path 하나씩을 다시 push한 뒤 update, draw(clear), sync를 수행합니다. Face 수십 개 규모에서는 재구성 비용이 문제 되지 않으며, shape 재사용 최적화는 out of scope입니다.
- Phase 1의 `layout_scene()`과 고정 사각형은 제거됩니다.
- Scene이 per-frame 입력이 되므로 resize는 더 이상 scene을 재구성하지 않습니다. Phase 1 resize failure semantics의 1–5(검증, allocation 보존, target swap, rollback, unusable)는 그대로 유지되고, "target 교체 성공 후 scene 재구성 실패 → unusable"이던 6번 분기는 대상이 사라져 제거됩니다. Resize 성공 후 첫 `render(scene)` 실패는 unusable 상태가 아니라 해당 frame의 render 실패로 전달됩니다.
- Fault-injection seam(`allocate_pixels`, `set_target`)과 세 분기(보존, rollback, unusable) test는 유지하고, scene 재구성 분기에 의존하던 test와 문구만 새 semantics에 맞춥니다.

`Application`은 `Camera`와 고정 scene 구성을 소유하고, `render()` 호출마다 pipeline을 실행해 `RenderScene`을 만들어 renderer에 전달합니다.
Resize 성공 시 camera aspect를 갱신합니다.

C ABI와 TypeScript boundary는 바뀌지 않습니다. `thorvg_rubiks_render()`는 여전히 인자가 없고, Web 쪽 변경은 e2e test의 기대 pixel뿐입니다.

### Structural dependency enforcement

"graphics는 `#include <thorvg.h>`를 하지 않는다"를 리뷰 규칙이 아니라 build 구조로 강제합니다.

- `engine/src/math`와 `engine/src/graphics`는 ThorVG dependency가 없는 별도 Meson target으로 분리합니다. Math는 alias 위주라 header-only dependency여도 됩니다.
- `render/`와 `app/`만 ThorVG를 link합니다.
- `tests/math`와 `tests/graphics`는 ThorVG 없이 해당 library만 link합니다. Graphics 코드에 ThorVG include가 생기면 이 test target의 build가 실패합니다.

## Rendered scene contract v2

Phase 1 contract(중앙 사각형)를 대체합니다. Native test와 browser e2e가 같은 계약을 검증합니다.

- 배경: 불투명 `RGBA(32, 32, 32, 255)` (Phase 1과 동일)
- Cube 면 색 (모두 alpha 255, 표준 배색):

```text
+X (Right) : RGBA(183,  18,  52, 255)  red
−X (Left)  : RGBA(255,  88,   0, 255)  orange
+Y (Up)    : RGBA(255, 255, 255, 255)  white
−Y (Down)  : RGBA(255, 213,   0, 255)  yellow
+Z (Front) : RGBA(  0, 155,  72, 255)  green
−Z (Back)  : RGBA(  0,  70, 173, 255)  blue
```

- 고정 camera에서 보이는 면은 `+X`, `+Y`, `+Z` 세 면이고 `−X`, `−Y`, `−Z`는 culling됩니다.
- 세 면이 서로 다른 색이므로 winding, culling, 좌표 규약 오류가 pixel 검증에서 색 뒤바뀜으로 드러납니다.

Sample point는 각 visible face centroid의 projection에서 유도했으며 정사각형 drawing buffer 기준 비율로 고정합니다.

```text
white (+Y) : (0.50, 0.29)
green (+Z) : (0.31, 0.61)
red   (+X) : (0.69, 0.61)
```

검증 규칙:

- Sample pixel은 `(round(fx * (width - 1)), round(fy * (height - 1)))`로 계산합니다.
- 세 sample pixel은 해당 면 색과 R, G, B, A channel별 exact match여야 합니다. 면은 단색이고 anti-aliasing은 edge에만 나타나며 sample은 face 내부 깊숙이 있습니다.
- 네 모서리 pixel은 배경색과 exact match여야 합니다. 고정 camera에서 cube silhouette은 모서리에 닿지 않습니다.
- Pixel 검증은 정사각형 aspect에서만 정의합니다. Sample 비율이 aspect의 함수이기 때문입니다. Native test는 정사각형 크기를 사용하고, e2e는 정사각형 viewport를 고정합니다.
- Resize 후에는 다른 정사각형 크기로 sample과 모서리 검증을 반복합니다.
- Camera 상수, cube 기하 또는 면 색을 바꾸면 이 contract를 같은 변경에서 함께 갱신해야 합니다.

## Implementation steps

### 1. Math module

- [ ] linalg.h를 LICENSE와 함께 `engine/third_party/linalg/`에 vendoring
- [ ] `rubiks::math` alias 계층 정의 (linalg 직접 include 금지 규칙 포함)
- [ ] `Quaternion` axis-angle 생성과 `Mat4` 변환 wrapper 구현
- [ ] `look_at` 직접 구현
- [ ] `perspective` 직접 구현
- [ ] `Transform` → `Mat4` 합성 구현
- [ ] Math를 ThorVG 비의존 Meson target으로 분리

### 2. Graphics pipeline

- [ ] `Color`, `RenderFace`, `RenderScene` 정의
- [ ] `Camera` 구현 및 Phase 3 고정 상수 정의
- [ ] `build_scene()` cube 6면 생성 구현
- [ ] Model, view, projection pass 구현
- [ ] Near-plane guard 구현
- [ ] NDC signed area back-face culling 구현
- [ ] View-space z 평균 key와 stable tie-break의 depth sort 구현
- [ ] Pass 합성 `operator|` 구현
- [ ] Graphics를 ThorVG 비의존 Meson target으로 분리

### 3. Renderer and application integration

- [ ] `Renderer` interface를 `render(const RenderScene&)`로 변경
- [ ] `ThorVGSoftwareRenderer`의 per-frame scene 재구성 구현
- [ ] `layout_scene()`과 고정 사각형 제거
- [ ] Resize에서 scene 재구성 제거 및 semantics 문구 갱신
- [ ] `Application`에 `Camera` 소유와 pipeline 실행 연결
- [ ] Resize 성공 시 camera aspect 갱신

### 4. Verification

- [ ] Alias 계층 규약(column vector 곱 방향, column-major 저장) 고정 test 추가
- [ ] `look_at`, `perspective` 손계산 기대값 test 추가
- [ ] Quaternion axis-angle 회전과 matrix 회전 일치 test 추가
- [ ] `Transform` 합성 순서 test 추가
- [ ] 알려진 점의 projection 결과 test 추가
- [ ] 두 winding의 culling 판정과 edge-on face 제거 test 추가
- [ ] Near-plane guard test 추가
- [ ] Depth sort 순서와 tie-break 결정성 test 추가
- [ ] Cube 전체 pipeline test 추가 (visible face 3개, 색, back-to-front 순서, centroid projection이 contract 비율과 tolerance 내 일치)
- [ ] Native rendered scene contract v2 test로 기존 contract test 대체
- [ ] Resize failure test를 새 semantics에 맞게 갱신
- [ ] Browser e2e를 정사각형 viewport와 contract v2 sample로 갱신
- [ ] Float 비교는 Catch2 `WithinAbs`/`WithinRel`을 사용 (exact 비교는 pixel channel에만 사용)
- [ ] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- Math와 graphics build target이 ThorVG를 link하지 않고, 해당 test는 ThorVG 없이 build되고 통과합니다.
- 기본 행렬 연산은 vendored linalg.h가 담당하고 engine 코드는 `rubiks::math` alias를 통해서만 사용합니다.
- Pipeline은 `operator|` 합성으로 구성되며 각 pass는 pipeline 밖에서 독립적으로 test됩니다.
- 좌표, winding, matrix 규약이 이 문서에 고정되어 있고 test가 규약대로 판정합니다.
- 고정 camera에서 cube 세 면이 rendered scene contract v2의 sample pixel과 모서리 검증을 통과합니다.
- Back face 세 면이 culling으로 제거되는 것이 pipeline unit test에서 검증됩니다.
- Depth sort가 stable tie-break를 포함해 결정적으로 동작합니다.
- Near-plane guard가 unit test로 검증됩니다.
- `RenderScene`은 ThorVG type을 포함하지 않으며 renderer의 유일한 입력입니다.
- Resize failure semantics 1–5 분기가 기존 fault-injection test로 계속 검증되고, scene 재구성 분기는 제거된 semantics를 반영합니다.
- C ABI와 TypeScript boundary(unit test 포함)는 변경되지 않습니다.
- Native unit test, TypeScript unit test, WASM build, browser e2e와 Vite production build가 모두 통과합니다.
- Cube domain(`CubeState`, move, notation)과 pointer interaction 코드는 이 phase에 포함되지 않습니다.

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
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 3을 완료 처리합니다.
- Phase 4 Rubik's Cube domain 세부 문서를 작성합니다.
