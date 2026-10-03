# Engine cleanup

## Status

`Completed` — 묶음 0~5. 큐브 규칙 중복 일부와 코어 경량화는 뒤로 남겼습니다([남긴 것](#남긴-것)).

Phase 번호가 없는 횡단 작업입니다. 2026-10-03~04에 `3f8dee4` 위에 커밋 74개로 들어갔습니다.

## Objective

엔진을 ThorVG 예제로도 읽을 수 있게 정리합니다. 새 기능은 없고, 기준은 셋입니다.

- 엔진이 쓰지 않는 코드는 엔진에 두지 않습니다. Test만 부르는 편의 함수는 test helper로 갑니다.
- 같은 규칙과 같은 값은 한 곳에만 적습니다.
- 폴더와 파일 이름은 그 안에 든 것을 말합니다.

찾는 일은 둘로 나눠 했습니다. 라이브러리마다 header의 선언을 모두 나열해 엔진 안의 호출처와 test의 호출처를 따로 세었고, 중복은 이름·큐브 규칙·수학 helper·구조·한계값의 다섯 갈래로 찾았습니다. 후보는 하나씩 반박 검증을 거쳤습니다. 엔진이 쓰지 않는 코드는 108건 중 107건이, 중복은 80개 묶음 중 65개가 확인됐습니다.

## 한 일

| 묶음 | 내용 | 커밋 |
|---|---|---|
| 0 선행 | 배포본 Smoke를 깨던 `page.goto('/')` 14곳, test helper 자리 `tests/support/{cube,graphics}` | `7dd7dfd`, `68dd8d6` |
| 1 죽은 코드 | 아무도 읽지 않는 필드·매개변수·overload, model transform, 쓰지 않는 quaternion 함수, 16.6 측정 코드 | `c303091`..`b84e19e` |
| 2 C ABI | export 78 → 68, 한 캔버스 API 삭제, `EMSCRIPTEN_KEEPALIVE` 삭제, `bindings/capi.cpp` | `4bfb7be`..`3a9b9f2` |
| 3 Test 전용 코드 | `cube::face_move` 하나, `moves::R..B`·`apply(vector)`·`default_camera` 등은 test helper로 | `cb710c7`..`0f6e865` |
| 4 중복 | `axes()`, `normalized_turns`, `in_layers`, `math/Angle.hpp`, Vec2 사본, 전개도 배치 산수, test helper, `install_cube` | `30759be`..`2564132` |
| 5 폴더 | `graphics/`를 루트, `3d/`, `2d/`(`net/`, `rings/`), `frame/`으로 | `90af009`..`7f215a6` |

### 결정과 근거

- **Model transform을 지웠습니다.** 엔진은 identity만 넘겼고 다른 값을 넣는 곳은 test뿐이었습니다. 돌고 있는 layer를 그리는 `quaternion_from_axis_angle`과 `quaternion_rotate`는 그대로입니다.
- **16.6의 측정 코드(`centre_locality`)를 지웠습니다.** 숫자는 [16.6 문서](./16.6-clusterwise-reduction.md)에 남고, 보관 패치 `16.6-clusterwise-centres.patch`는 `Reduction.cpp`를 고친 커밋마다 다시 떠서 `git apply --check`가 통과합니다.
- **Face와 depth를 `CubeMove`로 바꾸는 규칙은 `cube::face_move` 하나입니다.** Solver의 `to_move`와 application의 `depth_move`가 이것을 부릅니다. Test의 `R(3)` 같은 이름은 `tests/support/cube/CubeSupport.hpp`의 별칭이고, 규칙 자체는 literal과 비교하는 test가 지킵니다.
- **Test helper는 계층을 따릅니다.** `tests/support/cube`는 cube만, `tests/support/graphics`는 graphics와 math까지 봅니다. 각 test 실행 파일은 자기 라이브러리가 볼 수 있는 디렉터리만 받습니다. 두 header는 Catch2를 include하지 않습니다.
- **무엇을 export할지는 `engine/src/meson.build`의 목록 하나가 정합니다.** 이름이 네 곳(정의, 목록, `thorvg-rubiks.d.ts`, `CubeEngine.ts`의 호출)에 손으로 적혀 있어서, `web/tests/unit/AbiContract.test.ts`가 넷이 같은지 봅니다.
- **`graphics/`의 규칙:** 루트의 header는 하위 폴더를 include하지 않고, `3d/`와 `2d/`는 서로를 include하지 않으며, `frame/`만 전부를 읽습니다. `render/`는 루트의 header만 읽습니다. 확인하는 grep은 `engine/src/graphics/meson.build`의 주석에 있습니다.
- **`render/`는 `graphics/` 아래로 옮기지 않았습니다.** ThorVG를 link하지 않는 `graphics` 라이브러리와의 경계가 빌드 단위이기 때문입니다.

### 일부러 바꾼 동작

둘뿐이고 둘 다 test가 고정합니다.

- 칠한 큐브를 읽을 때의 크기 상한이 64에서 layer mask의 폭으로 바뀌었습니다. 엔진이 만드는 큐브는 28까지라 페이지에서는 보이지 않습니다.
- `set_brush`가 범위 밖의 색을 거절합니다. 전에는 bindings만 검사했습니다.

## 남긴 것

### 큐브 저장 방식을 바꾼 뒤에 다시 볼 것

큐브 상태를 cubie 배열에서 면 단위 저장으로 바꾸는 작업이 뒤따를 예정이라, 저장 방식에 기대는 중복은 정리하지 않았습니다. 그 작업 뒤에 다시 조사합니다.

- `Surface.hpp`가 `is_positive_face`, `faces_along`, `set_coordinate`, `is_exposed`를 내보내고 solver가 쓰는 일
- Scramble의 layer mask를 `depth_layers`에서 얻는 일
- `CubeGeometry.cpp`에 있는 `Surface` 함수의 사본 넷
- `pick_cube`의 여섯 평면 표
- Sticker가 `surface_stickers()`의 몇 번째인지(역참조 표 셋과 cubie index 식)
- `(position, face)`를 담는 두 번째 struct들
- `assembled()`가 cubie 배열을 다시 쓰는 부분
- Turn이 sticker 하나에 하는 일(`turn_role`)
- Orbit flood fill과 permutation parity(`Assembly.cpp`와 `Reduction.cpp`)
- `Reduction.cpp`와 solver 어휘 안의 반복

### 하지 않은 것

- **지난 phase 문서의 옛 이름.** `moves::`, `default_camera`처럼 지워진 이름이 04, 05.5, 06, 08.5 등에 남아 있습니다. 그 시점의 기록으로 둡니다.
- **Test만 쓰는 기본 인자.** 지우면 엔진 줄 수는 그대로이고 test 약 160곳을 고쳐야 합니다.
- **자기 `.cpp`만 읽는 header 상수 31개.** 같은 블록의 형제 상수를 test가 읽어서, 옮기면 한 묶음의 수치가 header와 `.cpp`로 갈라집니다.
- **한 canvas에 여러 장면을 그리는 경로.** `initialize()`가 매번 쓰고 test의 표준 fixture가 서 있는 자리입니다.

### 코어 경량화 (조사만, 미실행)

"빌드 옵션으로 파일을 골라 코어를 가볍게 한다"를 조사했고, 임시 복사본에서 변형 빌드를 재어 결론을 냈습니다. 아래 숫자는 이 정리 전(`3f8dee4`, wasm 492,780 bytes) 기준입니다.

- Wasm link 줄에 최적화 flag가 없습니다. `-O2` 한 줄로 426,030 bytes가 되고 glue는 84,713에서 19,623으로 줄며, 그려지는 pixel은 같습니다.
- 속도에 영향 없는 flag(engine `-Oz`, ThorVG `-O2`, `NDEBUG`, link `-O2`)를 모두 쓰면 기능을 전부 둔 채 329,794입니다.
- Source에서 의미 있는 것은 solver 하나입니다(약 72~85 KB). Paint·ambient·restore는 export 목록에서 빼면 linker가 이미 버리고, 그 이상은 2~3 KB입니다.
- `Application.cpp`를 meson option으로 쪼개는 것은 권하지 않습니다. Option은 build 디렉터리 단위라 wasm build가 하나 더 생기고, 3~4일을 들여 더 줄어드는 양이 10 KB 안팎입니다.
- 읽을 예제가 목적이면 같은 라이브러리 위에 작은 host를 따로 두는 편이 낫습니다. 453줄짜리 시안이 전체 application과 10개 지점에서 같은 pixel을 냈습니다.

## Verification

`7f215a6`에서:

- Native: `meson test -C build/native` 30개 모두 통과, 기본 실행 436 case. Solver의 `[.big]`도 통과(단언 24,700개).
- Wasm: `./build_wasm.sh` 통과. Export 68개, `thorvg-rubiks.wasm` 487,316 bytes, glue 80,871 bytes.
- Web: unit 369개(20 files), e2e 77개 통과.
- 수학 helper와 폴더 이동 앞뒤로, 같은 조작 17,593 frame의 hash가 같습니다.
- `git apply --check docs/tasks/16.6-clusterwise-centres.patch` 통과.

묶음마다 구현 뒤에 독립 검토 둘(계획 대비 diff, 깨진 곳 찾기)을 거쳤고, 나온 지적은 보완 커밋으로 고쳤습니다.
