# ThorVG Rubik's Cube Implementation Plan

이 문서는 [`DESIGN.md`](../DESIGN.md)를 실제 구현으로 옮기기 위한 상위 계획입니다.
각 phase의 세부 요구사항, acceptance criteria, 구현 단계, 검증 방법은 작업을 시작할 때 `docs/tasks/` 아래 별도 문서로 작성합니다.

## 진행 상태

- `[ ]` 시작 전
- `[-]` 진행 중
- `[x]` 완료

## 개발 원칙

- Cube domain은 graphics와 ThorVG를 알지 못해야 합니다.
- Browser는 DOM, UI, 접근성, pointer state를 담당합니다.
- C++ engine은 cube state, interaction 해석, animation, rendering을 담당합니다.
- ThorVG 의존성은 graphics pipeline의 마지막 rendering boundary에 한정합니다.
- Browser가 필요하지 않은 동작은 native unit test로 우선 검증합니다.
- 각 phase는 구현과 검증이 모두 끝난 뒤 완료로 표시합니다.

---

## [x] Phase 0: Project foundation

Native와 WebAssembly에서 동일한 C++ engine을 빌드할 수 있는 프로젝트 기반을 마련합니다.
Vite application, ThorVG submodule, Meson native build, Emscripten cross build, WASM build script와 최소 application lifecycle을 포함합니다.

## [x] Phase 1: [WebAssembly vertical slice](./01-wasm-vertical-slice.md)

Vite application에서 C++ engine을 초기화하고 ThorVG software renderer로 browser canvas에 간단한 도형 하나를 렌더링합니다.
WASM loading, TypeScript boundary, canvas와 pixel buffer 연결, resize 및 initialization failure까지 하나의 end-to-end 경로로 검증합니다.

## [x] Phase 2: [Production and deployment](./02-production-and-deployment.md)

Phase 1의 vertical slice를 기준으로 재현 가능한 production build와 GitHub Pages 배포 흐름을 구성합니다.
GitHub Actions에서 submodule checkout, native test, WASM build, Vite build와 배포를 자동화하여 이후 phase의 변경을 production 환경에서 지속적으로 검증할 수 있게 합니다.

## [x] Phase 3: [Math and graphics foundation](./03-math-and-graphics-foundation.md)

ThorVG와 독립적인 vector, matrix, quaternion, transform, camera와 graphics pipeline을 구축합니다.
Model, view, projection, back-face culling, depth sorting을 거쳐 하나의 3D cube를 2D render scene으로 변환하고 ThorVG로 렌더링합니다.

## [x] Phase 4: [Rubik's Cube domain](./04-rubiks-cube-domain.md)

Rendering과 독립적인 3×3×3 Rubik's Cube logical state와 move system을 구현합니다.
면별로 독립적인 색을 칠할 수 있는 1×1×1 cubie를 N×N×N 격자에 저장하고, axis와 layer 기반 move, quarter turn, inverse를 지원합니다.
같은 상태를 3D cube와 6면 전개도 두 가지로 함께 렌더링하여 큐브 전체를 한눈에 확인할 수 있게 합니다.

## [x] Phase 5: [Pointer interaction and animation](./05-pointer-interaction-and-animation.md)

Browser pointer 입력을 cube-local move로 해석하는 interaction state machine을 구현합니다.
Face picking, drag 중 transient rotation, pointer release 시 90° snapping과 animation 완료 후 logical state commit을 처리합니다.

## [ ] Phase 5.5: [Camera orbit](./05.5-camera-orbit.md)

빈 공간 drag를 turntable camera orbit으로 해석해 큐브의 여섯 면을 모두 둘러볼 수 있게 합니다.
Picking과 drag 해석은 camera만 통하므로 수정되지 않고, 초기 시점의 rendered scene contract도 그대로 유지됩니다.

## [ ] Phase 6: Gameplay and UI

Scramble, reset, solved-state 판정, timer와 접근 가능한 UI를 구현해 최소 gameplay flow를 완성합니다.
Loading, error, unsupported state와 responsive layout을 포함하여 desktop과 mobile browser에서 사용할 수 있게 합니다.

---

## 세부 문서 관리

각 phase를 시작할 때 `NN-kebab-case.md` 형식의 세부 문서를 추가하고 이 계획에서 해당 phase 제목에 링크합니다.
세부 문서에는 최소한 scope, requirements, out of scope, implementation steps, acceptance criteria와 verification을 기록합니다.
