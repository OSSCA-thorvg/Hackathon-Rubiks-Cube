# Phase 8: Net Rotation and Interaction

## Status

`Not started`

## Objective

2D 전개도를 표시 전용에서, 회전이 보이고 직접 조작할 수 있는 면으로 올립니다.

`build_net_scene`이 `build_cube_scene`과 대칭으로 `ActiveRotation`을 받는 overload를 갖고, Application이 `interaction.active_rotation()` 하나를 두 view에 함께 넘깁니다. 전개도 quad는 이미 화면 공간이므로 필요한 것은 2D 변환뿐이고, pipeline과 ThorVG 경계는 그대로입니다.

이 phase의 핵심은 **전개도의 회전이 3D의 강체 운동이 아니라 순열**이라는 점입니다. 한 번의 layer 회전은 (바깥 layer일 때) 도는 면의 제자리 회전과, 나머지 네 면에 걸친 셀 띠의 순환으로 쪼개집니다. 그 띠가 전개도에서 이어져 있는지는 축마다 다르고, 평면 전개도에서 세 축을 모두 연속으로 만드는 배치는 존재하지 않습니다. 따라서 연속인 축은 실제 이동으로 보여 주고 나머지는 축약하는 **축별 전략**을 씁니다.

## Scope

- `build_net_scene(state, rect, optional<ActiveRotation>)` overload; `nullopt`는 기존 함수 위임
- 축별 회전 전략: Y축 가로 슬라이드, F layer 링 회전, 나머지 축약 표현
- 링 회전이 net rect를 벗어나는 정도에 비례한 alpha fade
- 전개도 interaction: 점-사각형 picking, `net_cell` 기반 축·부호 유도, 기존 snap/commit 경로 재사용
- `Application::pointer_down`의 라우팅: 좌표가 net rect 안이면 net gesture, cube rect 안이면 3D, 그 외는 orbit
- `Net` view mode의 pointer 거부 해제
- Native scene/interaction test와 browser e2e

## Out of scope

- Quad를 잘라내는 clipping — 만들지 않습니다 (아래 결정 참고)
- 3D와 전개도 진행도의 완벽한 1:1 sync — 시작과 90° 시점만 일치하면 됩니다
- 전개도에서의 orbit — 전개도에는 돌릴 시점이 없으므로 빈 곳 drag는 아무 일도 하지 않습니다
- 전개도 배치(cross layout) 자체의 변경
- N ≠ 3 지원 — 전략을 3×3에 고정해 구현하고 Phase 15가 일반화합니다. Phase 12의 표기 변환과 같은 판단으로, 소비자가 없는 동안 일반화해 두면 검증되지 않은 코드만 남습니다

## Architecture decisions

### 축별 전략

전개도 cross 배치는 `U(1,0) / L(0,1) F(1,1) R(2,1) B(3,1) / D(1,2)`입니다. 각 축의 띠가 이 배치에서 이어지는 정도가 전략을 결정합니다.

| 축/layer | 띠의 배치 | 전략 |
|---|---|---|
| Y축 (U, E, D) | 가로 한 줄 `L F R B`가 통째로 순환 | **정확한 슬라이드**: 띠를 가로로 이동, 오른쪽 끝은 왼쪽으로 wrap. 바깥 layer면 U/D 면의 제자리 회전 동반 |
| Z축의 F layer | F의 4 이웃(U 아랫행, R 왼쪽열, D 윗행, L 오른쪽열)이 정확히 F에 인접 | **링 강체 회전**: F 블록과 1셀 테두리를 F 중심으로 회전. 90°에서 목적지 셀에 그대로 떨어지고 띠 내부의 순서 뒤집힘까지 맞음 |
| 그 외 (X축 전부, Z축의 B·S) | 띠가 전개도에서 불연속 (B가 col 3에 있어 U-F-D 열이 끊김) | **축약**: 바깥 layer면 도는 면의 제자리 회전 + 영향받는 띠의 alpha 강조. 가운데 layer는 띠 강조만 |

- 진행도는 어느 전략이든 `ActiveRotation`의 각도에서 유도하되 **한 칸 이동에서 포화**시킵니다. 전개도의 순환은 한 칸에서만 정의되므로 90°를 넘는 drag에서는 더 진행하지 않고 멈춰 있고, 최종 상태는 commit된 `CubeState`가 만듭니다.
- **그래서 net gesture의 각도 자체를 `[-90°, +90°]`로 제한합니다** — 표시만 포화시키면 부족합니다. Phase 7의 snap 규칙은 `90° + kCommitDegrees`부터 두 칸을 확정하는데, 그 구간에서 화면은 90°에 멈춰 있으므로 release 순간 180° 결과가 예고 없이 튀어나옵니다. Phase 7이 과회전을 받아들인 근거가 "놓기 전에 transient로 보인다"는 것이었고, 그 근거가 net에서는 성립하지 않습니다. 각도를 clamp하면 한 drag가 언제나 최대 한 수가 되어 보이는 것과 확정되는 것이 다시 일치합니다.
- 여러 칸을 미리 보여 주는 net preview를 만드는 대안은 쓰지 않습니다. 세 전략 각각에 다중 순환 표현이 필요해져, 이 phase가 전개도를 "축약해서라도 보여 준다"로 잡은 범위를 넘습니다.
- 전략 선택은 `(axis, layers)`에서 기계적으로 유도하는 순수 함수로 두고 exhaustive switch로 작성합니다.
- 시작(0°)과 끝(90°)에서는 세 전략 모두 commit 전후의 논리 상태와 픽셀이 일치해야 합니다. 중간 각도는 그럴듯하면 충분합니다.

### Clipping 대신 alpha fade

- 가로 슬라이드의 wrap은 좌우로 넘쳐 배경에 그려지거나 canvas 밖으로 나가므로 무해합니다.
- F 링 회전은 45° 부근에서 모서리가 net rect 위로 스며 3D cube 영역과 겹칠 수 있습니다. 이는 rect를 벗어난 정도에 비례해 quad의 alpha를 낮추는 것으로 대신합니다.
- `Color`의 alpha는 이미 renderer의 `shape->fill(r, g, b, a)`까지 연결되어 있으므로 renderer는 바뀌지 않습니다.

### 전개도 interaction

3D보다 단순합니다. Ray cast가 아니라 점-사각형 판정이고, 화면 방향이 고정이라 후보 축 투영도 필요 없습니다.

- **Picking**: 좌표가 net rect 안의 어느 면, 어느 `(col, row)` 셀인지 산술로 판정합니다. 면 사이 틈은 miss입니다.
- **축·부호 유도**: 눌린 셀과 drag 방향으로 한 칸 옆 셀에 `net_cell`을 각각 적용하면, 두 결과의 큐브 공간 좌표 차이가 이동 방향이 되어 회전 축과 부호가 나옵니다. 면별 부호 표를 두지 않습니다.
- **옆 셀의 기준은 net 배치가 아니라 cube topology입니다.** 옆 셀이 같은 면을 벗어날 때 cross에서 인접한 면을 찾으면 U 윗행에서 위로 drag하는 경우처럼 **cross에 이웃이 없는 외곽 방향이 실패**합니다. 대신 `net_cell`이 세우는 그 면의 화면 basis(col/row가 큐브 공간에서 향하는 방향)를 면 밖으로 한 칸 연장하면, 그 위치는 큐브 위에서는 항상 인접 면의 셀입니다 — 큐브 표면에는 가장자리가 없기 때문입니다. 따라서 유도는 전개도의 어느 셀, 어느 방향에서도 정의됩니다.
- **Gesture 진행**: dead zone, 가로/세로 방향 lock, angle 계산은 3D drag와 같은 틀입니다. 감도는 "면 하나 폭의 drag = 90°"를 기본값으로 하는 상수이고, **각도는 `[-90°, +90°]`로 clamp합니다**(위 표시 결정 참고). 그래서 net에서 나오는 snap 목표는 언제나 0 또는 ±1이고, 두 칸이 나올 수 없습니다.
- **Commit**: release 시 기존 snap 경로(`Snap` 상태, `take_committed_move`)를 그대로 재사용합니다. Snap이 만드는 것이 `CubeMove`뿐이라 3D에서 온 gesture인지 전개도에서 온 gesture인지 commit 이후는 구분되지 않고, Phase 7의 즉시 확정과 snap 경계 규칙도 그대로 적용됩니다.
- **라우팅**: `Application::pointer_down`이 좌표를 layout의 rect로 분기합니다. Net rect 안이면 net gesture, cube rect 안이면 기존 3D 경로, 어느 쪽도 아니면 (cube가 보이는 mode에서만) orbit입니다. `Net` mode의 일괄 거부(`return false`)를 제거합니다.

### 두 view의 동시 표시

- 현재 `Application::render()`는 `interaction.active_rotation()`을 **3D builder에만** 넘기고 net builder는 받지 않습니다. 이 phase가 net 호출에도 같은 값을 넘기는 wiring을 더하면, 하나의 `ActiveRotation`이 두 scene을 같은 frame에 함께 움직입니다.
- 전개도에서 시작한 gesture도 같은 `ActiveRotation`을 만들므로 3D cube가 함께 돌아갑니다.

## Implementation steps

### 1. Net rotation rendering

- [ ] `build_net_scene` overload 추가, `nullopt`는 기존 함수 위임 (기존 net scene test 무수정 통과)
- [ ] `Application::render()`가 같은 `active_rotation()`을 net builder에도 전달하는 wiring
- [ ] `(axis, layers)` → 전략 유도 함수와 exhaustive switch
- [ ] Y축 슬라이드: 띠 이동, wrap, 바깥 layer의 면 제자리 회전
- [ ] F layer 링 회전: F 블록 + 1셀 테두리의 F 중심 회전
- [ ] 축약 전략: 면 제자리 회전과 띠 alpha 강조
- [ ] 진행도 한 칸 포화 구현
- [ ] Rect 이탈 비례 alpha fade 구현
- [ ] Scene test: 0°와 90°에서 commit 전후 상태와 일치, 45°에서 각 전략의 quad 위치/alpha known-answer

### 2. Net interaction

- [ ] Net picking(점-사각형, 셀 산술) 구현
- [ ] Cube topology 기반 옆 셀 유도와 `net_cell` 2회 적용의 축·부호 유도 구현
- [ ] Table-driven 전수 test (6면 × 가로/세로 × 양·음, cross 밖으로 나가는 외곽 방향 케이스 포함)
- [ ] Dead zone, 방향 lock, angle 계산과 기존 snap 경로 연결
- [ ] **각도 clamp test**: 면 두 개 폭을 넘는 net drag도 각도가 90°에서 멈추고 commit이 한 수인지 (Phase 7의 두 칸 확정이 net에서는 도달 불가)
- [ ] `Application::pointer_down` rect 라우팅과 `Net` mode 거부 해제
- [ ] **기존 test 교체**: `Net` mode의 pointer 일괄 거부를 고정한 ApplicationTest와, 3D drag 중 전개도 픽셀이 변하지 않음을 단언하는 e2e는 이 phase의 계약과 정반대이므로 새 동작(라우팅 수락, 전개도 동시 표시)의 test로 교체합니다
- [ ] 전개도 빈 곳 drag가 아무 일도 하지 않는 test
- [ ] Net gesture와 3D gesture가 서로 배타적인 test (한 번에 하나)

### 3. Verification

- [ ] 전개도 drag → commit → `CubeState` 변화가 3D drag와 동치인 test
- [ ] Native rasterization test: 고정 크기에서 45° 중간 frame을 ThorVG software renderer로 실제 래스터라이즈하고 cube/net 영역의 sample point 픽셀을 검증 (기존 contract의 sample point 방식을 animation 중간 frame으로 확장; golden image는 두지 않음 — submodule 갱신마다 깨지는 유지비 대신 sample 좌표만 재유도)
- [ ] e2e: Net mode에서 drag로 U를 수행하고 대표 셀 몇 개만 확인 (전수 mapping은 native test가 담당합니다)
- [ ] e2e: Both mode에서 3D drag 중 전개도 픽셀이 함께 변하는지
- [ ] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- 정지 상태(`nullopt`)의 전개도 렌더링은 기존 코드 경로를 그대로 호출하며 기존 contract가 수정 없이 통과합니다.
- 3D drag 중 전개도가 축별 전략대로 함께 움직이고, 0°와 90°에서 논리 상태와 정확히 일치합니다.
- Y축은 wrap을 포함한 슬라이드로, F layer는 링 회전으로, 나머지는 축약 표현으로 렌더링됩니다.
- 90°를 넘는 drag에서 전개도의 진행은 한 칸에서 포화합니다.
- 링 회전이 net rect를 벗어나는 quad는 이탈 정도에 비례해 투명해지고, clipping 코드는 존재하지 않습니다.
- 전개도의 셀을 drag하면 3D와 같은 snap/commit 경로로 move가 확정되고, 부호가 손가락 방향을 따릅니다.
- `Net` mode에서도 전개도 조작이 가능하고, 빈 곳 drag는 아무 일도 하지 않습니다.
- ThorVG 경계와 renderer는 변경되지 않습니다.
- Native, WASM, TypeScript unit, browser e2e와 production build가 모두 통과합니다.

## Verification commands

```bash
meson test -C build/native --print-errorlogs
source /path/to/emsdk/emsdk_env.sh && ./build_wasm.sh
npm --prefix web run test:unit
npm --prefix web run test:e2e
npm --prefix web run build
```

## Completion

모든 acceptance criteria와 verification command를 통과한 뒤 다음 작업을 수행합니다.

- 이 문서의 status를 `Completed`로 변경합니다.
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 8을 완료 처리합니다.
- 실제 구현과 차이가 생긴 결정을 이 문서에 기록합니다.
