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
- 세부 문서가 기존 코드의 계약에 대해 주장하는 내용은 작성 시점에 소스로 확인합니다. 기억 속의 계약으로 설계하면 존재하는 함수를 재발명하거나 없는 동작을 지키는 구조가 생깁니다.
- 새 phase가 기존 계약과 충돌하면 우회(병렬 API 추가)보다 개정을 먼저 검토합니다. 어느 쪽이든 결정과 근거를 해당 문서에 남기고, 개정이면 원래 phase 문서에 기록합니다.

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

## [x] Phase 5.5: [Camera orbit](./05.5-camera-orbit.md)

빈 공간 drag를 turntable camera orbit으로 해석해 큐브의 여섯 면을 모두 둘러볼 수 있게 합니다.
Picking과 drag 해석은 camera만 통하므로 수정되지 않고, 초기 시점의 rendered scene contract도 그대로 유지됩니다.

## [x] Phase 6: [Gameplay and UI](./06-gameplay-and-ui.md)

Scramble, reset, solved-state 판정, timer와 접근 가능한 UI를 구현해 최소 gameplay flow를 완성합니다.
Loading, error, unsupported state와 responsive layout을 포함하여 desktop과 mobile browser에서 사용할 수 있게 합니다.

---

# 고도화

Phase 6까지로 최소 gameplay flow가 완성되었습니다. 이후 phase는 초안에서 드러난 조작감 문제를 해결하고, 전개도를 조작 가능한 면으로 올리며, 큐브가 스스로 도는 연출과 solve를 추가합니다.

이 저장소는 ThorVG의 showcase로도 쓰일 예정입니다. 그래서 순서는 의존 관계만이 아니라 rendering이 드러나는 정도를 함께 보고 정했고, 읽는 사람이 구조를 따라갈 수 있는지를 각 phase의 판단 기준에 포함합니다. 마지막 세 phase는 그 기준에서 뒤로 밀린 것들입니다.

## [x] Phase 6.5: [Pre-enhancement cleanup](./06.5-pre-enhancement-cleanup.md)

고도화에 들어가기 전에 기존 코드를 정리합니다.
1차(완료): scramble의 solved 검사와 R 덧붙임 제거 — 축 제약으로 identity가 구조적으로 불가능해 도달하지 않는 분기였습니다 — 와 `is_busy()`의 orbit 제외 — busy가 막는 대상은 cube를 바꾸는 입력인데 orbit은 commit이 없습니다. 두 건 모두 Phase 9의 결정을 앞당긴 것이고, 고정하는 test가 없음을 확인했습니다.
2차: 이후 phase가 상태를 얹기 전에 그 자리를 안전하게 만듭니다. Application의 흩어진 전역을 `unique_ptr<ApplicationState>` 하나로 묶어 shutdown의 수동 초기화 목록을 없애고, 유일한 소비자(web)가 fail-stop으로만 쓰는 renderer resize rollback을 test 전용 fault seam과 함께 제거하고(Phase 1 계약 개정), timer의 스크린 리더 소음(`aria-live`)과 busy 주석을 정리합니다.

## [x] Phase 7: [Interaction robustness](./07-interaction-robustness.md)

빠른 연속 drag에서 회전 입력이 사라지는 문제를 해결합니다.
Snap animation이 도는 동안 새 gesture가 시작되지 못해 drag 전체가 무시되는 경로와, 짧고 빠른 flick이 90°의 절반을 넘기지 못해 제자리로 snap되는 두 가지 원인을 함께 다룹니다.
전자는 pointer down 시점에 진행 중인 snap을 즉시 확정하고 새 gesture를 시작하는 방식으로, 후자는 snap 경계를 배수 사이의 중간(45°)에서 낮은 배수 + 30° 내외 상수로 옮기는 것으로 해결합니다.
경계는 모든 배수에서 같은 지점이라 "면 하나를 넘기면 다음 칸"이 어느 칸에서나 참입니다 — 첫 칸에만 문턱을 두는 혼합 규칙 대신, 입력이 무시되는 오류(사용자가 앱 탓으로 느낍니다)보다 덜 돌려도 동작하는 오류가 낫다는 방향으로 전체를 일관되게 기울입니다. 이는 Phase 5의 "가장 가까운 배수" 계약 개정이며 원 문서에 기록합니다. 각속도 판정은 기각했습니다. 시간이 `advance`로만 들어오는 구조에서는 마지막 pointer 구간이 분모에서 빠져 판정이 주사율에 좌우되고, 정확히 하려면 pointer ABI에 timestamp를 더해야 하는데 경계 이동 하나가 같은 문제를 해결합니다. 시간도 속도도 새 상태도 없으므로 C ABI와 web 코드는 이 phase에서 바뀌지 않습니다.

## [x] Phase 8: [Net rotation and interaction](./08-net-rotation-and-interaction.md)

2D 전개도를 표시 전용에서 회전이 보이고 직접 조작할 수 있는 면으로 올립니다.
`build_net_scene`이 `build_cube_scene`과 대칭으로 ActiveRotation을 받는 overload를 갖고, application이 같은 회전을 모든 view에 함께 넘깁니다. 전개도 quad는 이미 화면 공간이라 필요한 것은 2D 변환뿐입니다.

전개도의 회전은 3D의 강체 운동이 아니라 순열이라는 점이 이 phase의 핵심입니다. 한 번의 layer 회전이 도는 면의 제자리 회전과 나머지 네 면에 걸친 셀 띠의 순환으로 쪼개지고, 그 띠가 전개도에서 이어져 있는지는 축마다 다릅니다. 1차 구현은 여기서 축별 전략(Y 슬라이드 / F 링 / 나머지 축약)으로 갈라졌으나, 축약된 축의 띠가 색만 바뀌고 도는 것으로 보이지 않아 **링 하나로 통일**하도록 개정했습니다.

어느 (axis, layer)든 그 띠 12칸은 **12개 resting slot을 지나는 닫힌 cubic loop** 위의 슬롯이고, 회전 진행도만큼 셀이 그 곡선을 따라 이동합니다(1/4 턴 = 3슬롯 = 링의 1/4바퀴). Z축은 F 블록을 감싸는 중첩된 둥근 고리, Y축은 가로줄을 좌우로 감아 도는 고리, X축은 U-F-D 열과 B 열을 cross의 빈 모서리로 잇는 세로 고리가 되어, 기존 세 전략이 전부 이 규칙의 특수한 경우로 흡수됩니다. 분기가 렌더링 로직이 아니라 곡선 데이터로 내려갑니다. 링은 슬롯을 꿰는 곡선이 아니라 **슬롯이 올라앉은 polyline의 모서리를 원호로 깎은 것**입니다. 직선 구간은 정확히 직선이라 그 위에서 셀이 기울지 않고, 모서리 반지름은 **링 하나당 하나** — 그 링의 가장 좁은 모서리가 허락하는 값입니다(모서리마다 최대치를 쓰면 가장 둥근 모양은 되어도 같은 모양은 되지 않아 L·R 궤적이 한쪽만 크게 둥글어집니다). control arm 한 식이 직선(현의 1/3)과 직각(0.5523 r)을 모두 덮습니다. 슬롯 순환은 화면을 모르는 domain(`cube/Surface`)이 답하는데, 이는 새 회전 구현이 아니라 `CubeState`가 이미 갖고 실물 대조로 검증해 둔 `rotated()`·`sticker_cycle()`의 노출입니다 — 스티커에 `SurfaceSticker` identity를 주는 것이 핵심이고(`CubeState`가 읽어 주는 여섯 색으로는 한 면 안의 순서 오류를 잡을 수 없습니다), 이 struct는 기존 `NetCell`과 같으므로 별칭으로 통합합니다. Phase 8.5의 링 다이어그램이 같은 순환을 그대로 씁니다. 셀의 기울기는 출발 슬롯 접선 대비 상대 회전이라 0°에서 axis-aligned가 보장되고 직선 구간에서는 돌지 않으며, 3칸 떨어진 슬롯의 접선 차가 90°의 배수라 0°·90° 픽셀 일치가 유지됩니다.

**Z축만은 예외**입니다. cross가 그 회전을 제자리에 그리기 때문입니다 — F 블록의 테두리가 곧 Z 밴드이고 둘레 네 블록이 그 중심에 4중 대칭이라, 한 점을 중심으로 한 번 돌리면 밴드가 전부 목적지에 내려앉고 그것이 F면이 이미 하는 회전과 같습니다. 도는 면과 밴드가 하나의 운동이 되어 떨어지지 않습니다. 도는 면에 억지로 링을 얹으면(모서리가 스티커 위라 직각으로 꺾여야 합니다) 오히려 부자연스럽다는 것을 구현 후 확인했습니다.

셀을 누르면 그 셀이 **실제로 지나갈 경로**가 축마다 다른 색(스티커가 비워 둔 세 구간에서 고른 cyan·magenta·lime)의 안내선으로 나타나고 — X·Y는 링, Z는 그 셀의 원 — 방향이 lock되면 하나만 남습니다. 링은 cubic 구간의 닫힌 목록 하나로 표현되어 셀 이동은 그것을 매개변수로 평가하고 안내선은 그대로 ThorVG stroke로 그립니다 — renderer 경계(`RenderScene`)에 순수 2D stroke 자료구조를 더하는 Phase 3 계약의 개정이며, ThorVG 쇼케이스에서 엔진의 stroke를 우회해 다각형으로 흉내내지 않기 위한 것입니다. 안내선은 net rect와 그 주변 배경 여백 안에만 머물러 Both mode의 3D cube를 침범하지 않고, 이를 cube rect 버퍼 비교 raster test로 고정합니다. 좁은 쪽은 좌우가 아니라 net 위쪽(cube 하단과의 여유가 약 0.75칸)이고, 굽이의 반지름을 셀 크기로 제한하는 것이 그 예산을 지키는 손잡이입니다.

밴드는 셀 12장이 아니라 **3장짜리 강체 조각 넷**으로 나릅니다 — 조각의 가운데에서만 링을 읽고 나머지를 그 옆에 붙이면 굽이에서도 한 줄이 흩어지지 않습니다. 조각이 지나는 자리는 **비켜서지 않습니다.** 대신 조각을 모든 정지 셀 다음에, 조금 크게, 자기 그림자 위에 그리는 **3층 호버**로 위아래를 말합니다(정지 전개도 / 도는 면 / 밴드 조각). 종이에 구멍을 내는 대신 손이 조각을 종이 위로 들고 있는 그림입니다. Quad를 잘라내는 clipping도, rect 이탈 fade도 없습니다 — 알파가 변하면 큐비가 *이동하는* 것이 아니라 *색이 변하는* 것으로 읽힙니다. 링은 감기므로 전개도 drag의 각도 제한도 없습니다.

Interaction은 3D보다 단순하고 1차 구현으로 완료되어 개정에서 그대로 유지됩니다. Ray cast가 아니라 점과 사각형 판정이고, 화면 방향이 고정이라 후보 축 투영도 필요 없습니다. 눌린 셀과 drag 방향으로 한 칸 옆 셀에 `net_cell`을 각각 적용하면 그 차이가 큐브 공간의 이동 방향이 되어 축과 부호가 유도되므로, 면별 부호 표를 두지 않습니다. 전개도 전용 view에는 orbit할 대상이 없으므로 빈 곳 drag는 아무 일도 하지 않습니다.

## [x] Phase 8.5: [Ring diagram view](./08.5-ring-diagram-view.md)

큐브를 (1,1,1) 대각선에서 본 투영을 편 세 번째 화면입니다. 세 축이 120° 간격의 세 중심이 되고 아홉 링이 전부 둥근 고리가 되며, 54개 스티커는 각각 서로 다른 두 축 링이 만나는 자리에 놓입니다(54 × 2 = 108 membership로 등식이 닫힙니다). 이 topology가 view의 실제 계약이고, 슬롯 순환은 Phase 8의 `cube/Surface`를 그대로 씁니다. 다만 슬롯 접선이 임의라 셀 기울기는 재사용하지 않고 스티커를 축 정렬로 유지합니다. 배치는 **위치가 링을 정합니다**: 54개 슬롯 좌표를 먼저 두고 아홉 링을 그 슬롯들을 지나는 loop로 그립니다. 반대 방향(원이 좌표를 유도)은 제약 충족 문제가 되어 폐기했습니다.

표시까지 완성한 뒤 **2차로 확장했습니다**: view가 네 번째 `ViewMode` 값이 아니라 **영역(`Cube3D`/`Both`/`Flat`)과 평면 스타일(`Net`/`Rings`/`Both`) 두 축**이 되고(`Both`가 토글을 따라가고, 스타일의 `Both`는 전개도 위·링 아래로 쌓아 아홉 조합이 전부 성립합니다), 이 view에서도 링을 잡아 돌릴 수 있게 되어 1차의 "조작은 UI 단계의 몫"과 "press는 부작용 없이 거절"이 함께 뒤집혔습니다. 스티커는 정확히 두 링 위에 있으므로 후보가 둘뿐이고, 접선이 곧 양의 회전 방향이라 부호표가 없으며, 각도를 화면 각도가 아니라 **슬롯**으로 재어(12슬롯이 균등하지 않습니다) 잡은 스티커가 손가락을 따라옵니다.

시작 직전 대조에서 세부 문서의 세 주장이 개정되었습니다. 재사용 대상은 "spline 생성기"가 아니라 **링 클래스**입니다 — Phase 8의 loop는 rounded polyline이고 그 생성기는 전개도 전용이라, 이 view는 슬롯마다 Knot을 원의 접선과 함께 두어 같은 Knot→cubic 기계로 정확한 원을 얻습니다(`NetRing` → `SlotRing` 개명). 배치는 자유로운 초기값이 아니라 **계약**입니다 — 교차점 배정이 어긋나면 loop가 자기를 가로지르고, 각 링의 12슬롯이 중심 기준 각도 순서여야 하는데 이는 반지름이 중심 간 거리 근처일 때만 성립합니다(0.75배 근처 1944개 구성에서 링 하나도 불가). 그리고 도는 면은 **제자리 회전일 수 없습니다** — 면의 9개 스티커가 곡선 격자라 4중 대칭이 없어 90° 강체 회전이 도착 슬롯에서 클러스터 반지름의 48%만큼 어긋나므로, 클러스터 중심 기준 극좌표 보간으로 대체합니다.

`ViewMode::Rings`(ABI 값 3) 추가는 값 하나가 아니라 전 계층 migration입니다: Application의 render가 지금 `mode != Net` 같은 부정 조건으로 분기해 값을 더하면 잘못된 경로가 실행되므로 mode별 명시 분기로 재구성하고, C ABI 검증·layout·TypeScript enum·view 버튼까지 함께 갑니다. 이 phase는 표시와 회전 동조까지이고, 이 view의 조작과 정교한 배치는 UI 단계의 몫입니다. Phase 8에 얹지 않고 분리한 이유와 경위는 Phase 8 문서의 개정 기록에 있습니다.

## [x] Phase 9: [Move player and animated scramble](./09-move-queue-and-animated-scramble.md)

미리 만들어진 수순 하나를 순서대로 재생하는 Player를 application lifecycle에 도입합니다.
Scramble은 logical state를 한 번에 바꾸는 대신 수순을 Player에 넘겨 실제로 돌아가는 모습을 보여주고, 재생이 소비하는 move는 user move count와 timer에서 제외됩니다.
재생에 관한 상태는 `optional<Player>` 값 하나입니다 — 수순과 그 재생 성질(tempo, 이후 phase가 더하는 반복·진행 방향·정지 가능 여부)을 함께 들고, 재생 판정도 count 규칙도 그 존재 여부 하나를 봅니다. 출처를 이름 붙이는 enum은 두지 않습니다: 그 이름을 이름으로 요구하는 소비자가 없고, 값이 늘 때마다 이름→성질 변환표가 원격지에서 함께 자라기 때문입니다. 의미의 이름은 생산자 함수 이름이 이미 갖고 있습니다.
남은 수순과 마지막 move의 animation까지 끝나야 busy가 풀리며, 그동안 layer를 돌리는 입력은 차단하고 pointer는 camera orbit만 하도록 보내 재생을 돌려 가며 볼 수 있게 합니다. Scramble animation이 끝나는 시점에 timer가 준비됩니다.
Scramble 수는 `make_scramble`이 이미 인자로 받고 있으므로 사용자가 정하는 정수 값으로 함께 노출합니다. 난이도 단계로 포장하지 않고 수 자체를 보여 줍니다.
Scramble의 solved 검사와 R 덧붙임은 Phase 6.5가 이미 걷어냈으므로, 이 phase는 그 위에서 재생 형태만 바꿉니다.
세부 문서가 Phase 8·8.5 이전에 쓰였으므로 시작 직전 대조에서 여섯 가지가 개정되었습니다. 가장 큰 것은 "재생 중 press는 좌표와 무관하게 orbit"이 `ViewMode::Flat`에서 카메라를 NaN으로 만든다는 것입니다 — 그 mode에는 cube 영역이 없어 pixel당 각도가 0으로 나뉘므로, 3D cube가 보일 때만 orbit으로 보내고 아니면 거절합니다(Phase 8이 "전개도 전용 view에는 orbit할 대상이 없다"고 이미 정한 것과 같은 규칙입니다). 차단해야 할 picking은 그 사이 cube 하나에서 cube·net·rings 셋이 되었고, 이는 guard를 `pointer_down` 맨 앞 — 세 picking과 Phase 7의 확정 호출보다 앞 — 에 두는 것으로 한 자리에서 해결됩니다. 그 위치가 "확정 경로가 도달 불가능"의 전제이기도 합니다. 재생은 move 사이에 쉬는 frame이 없어 리프트가 최대 높이에 머무는데, `start_move()`가 그 누적을 되돌려 각 move가 keyboard turn 하나와 같은 호를 그리게 합니다. 그 밖에 `shutdown()`이 폐기 목록에 잘못 올라 있었고(Phase 6.5가 상태를 통째로 파괴하게 만들어 둔 뒤로 더할 것이 없습니다), 소비 loop가 거절당한 move에서 멈추면 frame loop가 끝나지 않으며, 손볼 test는 known-answer 하나가 아니라 `scramble()` 직후 큐브를 읽는 전부입니다.

## [x] Phase 10: [Ambient mode](./10-ambient-mode.md)

입력이 없는 동안 큐브가 스스로 도는 관람 모드를 더합니다. Phase 9의 Player에 패턴 하나를 넘기고 `loop` 필드로 반복시키는 일이라, 새 animation 경로도 공급 장치도 만들지 않습니다.
방식은 하나입니다. 위수를 아는 짧은 수순 표에서 하나를 골라 반복합니다. 어떤 수순이든 반복하면 유한한 횟수 뒤에 solved로 돌아온다는 성질을 그대로 보여 주므로 관람은 주기적으로 처음 상태를 지나가고, cube group에서 원소의 최대 위수가 1260이라 반복 횟수의 상한도 근거를 갖습니다. 표의 수순(`R U F' D`류, 위수 60~90)은 그 복귀가 관람 한 번 안에 보이도록 위수를 계산해 고른 것들이고, 어느 것을 고를지는 browser가 넘긴 값의 나머지로 정하므로 engine은 여전히 난수원을 갖지 않습니다.
위수 자체는 선정 근거로 문서에만 남기고 상수 표에는 수순만 싣습니다. 공급 loop가 위수를 읽지 않으므로, 코드가 쓰지 않는 값을 test가 지키는 구조를 만들지 않고 "표의 모든 패턴이 상한 안에 복귀한다"는 성질 하나만 고정합니다.
무작위 move를 계속 흘려보내는 공급은 두지 않습니다. Seed 주입과 전용 결정성 test가 더 필요해지는 데 비해 보이는 것은 똑같이 그냥 도는 큐브이고, 복귀는 오히려 보이지 않게 되기 때문입니다.
관람에 들어갈 때 cube 상태를 snapshot하고, 큐브를 바꾸는 명령이 들어오면 즉시 멈추면서 그 상태로 되돌립니다. 관람 중의 move를 timeline에 남기면 자리를 비운 시간에 비례해 timeline이 무한히 자라고, 남기지 않으면 상태와 timeline이 어긋나 이후의 되감기와 복원이 전부 틀어집니다. 되돌리면 둘 다 피할 수 있습니다. 보는 방법을 바꾸는 것(canvas drag, 2D/3D, flat style, Home view)은 명령이 아니어서 관람을 끝내지 않고, scramble 재생이 같은 조작에 보이는 반응과 정확히 같습니다 — 시점과 view는 어느 순간에나 사용자의 것이고, 관람은 돌려 가며 볼 값어치가 있는 쪽입니다.
탭이 보이지 않는 동안에는 frame loop를 돌리지 않아, 쉬는 화면이 배터리를 쓰지 않게 합니다.

## [x] Phase 11: [Move history, solve, and undo](./11-move-history-solve-and-undo.md)

Cube에 적용된 move를 되감을 수 있게 기록하고, 그 위에 solve와 undo를 함께 올립니다.
기록은 하나의 선형 timeline입니다: scramble 수순과 사용자 move가 한 배열에 이어지고, cursor(적용된 개수)와 scramble_end(scramble 구간의 끝) 두 인덱스가 그 위를 가리킵니다. 큐브에 일어나는 일이 언제나 선형 수순이므로 자료구조도 그 모양을 따르고, "사용자 수는 완성된 scramble 위에서만 존재한다"는 불변식은 지킬 규칙이 아니라 **표현 불가능한 상태**가 됩니다 — cursor가 하나면 뒤의 수가 적용됐는데 앞의 수가 안 된 상태를 적을 방법이 없습니다. 화면과 공유의 두 구간 구분은 scramble_end 하나로 파생됩니다.
되감기는 cursor를 목표로 내리는 수순이고, undo(한 수)와 solve(0까지)는 같은 함수에 준 인자의 차이입니다. Undo의 하한이 scramble_end라 "undo는 자기 수 안에서만"이 조건문이 아니라 표의 한 칸이 됩니다. 새 move가 들어오면 cursor 뒤가 통째로 잘려, 사용자 redo tail과 미적용 scramble 구간이 한 규칙으로 함께 폐기됩니다.
명령은 재생할 수순을 그 자리에서 계산해 Phase 9의 Player에 넘깁니다. 목표값을 들고 commit마다 다음 수를 고르는 공급도 방향을 유도하는 규칙도 없습니다. 재생 경로가 하나로 통일되고, Phase 16의 solver는 같은 자리에 생산자를 하나 더 얹는 일이 됩니다.
Commit의 분기는 재생 여부 두 갈래이고, timeline 반영은 Player가 든 `TimelineEffect`(Advance 재생·재진행, Rewind 되감기, None 관람)가 정합니다. 적용된 사용자 수도 `cursor − scramble_end`로 파생해, 되감기가 생기면 어긋나는 저장 counter를 이 phase에서 은퇴시킵니다. Timer의 시작만은 cursor가 아니라 이 파생값의 증가를 봅니다 — 아니면 ready에서의 Solve 재생이 timer를 시작합니다.
Solve가 끝났거나 중단된 상태에서 사용자가 수를 두면 cursor 뒤가 잘리며 받아들여집니다. 거절하면 방금 푼 큐브를 만질 수 없게 됩니다.
재생 중 pointer는 camera orbit만 합니다. 수십 초짜리 solve를 돌려 가며 볼 수 있고, layer picking에 닿지 않으므로 진행 중인 snap의 소유자를 따지는 조건도 필요 없어집니다.
되감을 move는 상쇄 축약하지 않고 그대로 재생합니다. 자기가 둔 수순이 되짚어지는 편이 보기 좋고, 축약은 필요해지면 나중에 얹을 수 있는 최적화이지 이 phase의 요구사항이 아닙니다.
되감기를 순방향으로 다시 보는 replay와 재생 일시정지는 두지 않습니다. Replay 자체는 redo의 반복이라 거의 공짜지만, 일시정지는 busy로 구분되지 않는 별도의 정지 상태와 그 상태에서 조작을 막는 규칙, ABI 셋을 함께 끌고 옵니다. 재생은 끝까지 돌거나 중단하거나 둘뿐이라 기존 busy가 전부 커버합니다. 재생 속도는 Phase 13의 animation 속도 control이 함께 담당합니다.
Solve는 기록으로 남는 완주가 아니므로 timer는 정지하되 기록하지 않으며, 재생 중 중단할 수 있는 경로를 함께 제공합니다.
Timer 측정 중의 undo는 그대로 허용하고 기록을 무효로 처리하지 않습니다. Undo에 드는 시간이 이미 손해이므로 별도의 무효 상태를 두지 않습니다.

## [x] Phase 12: [Move notation and move log](./12-move-notation-and-move-log.md)

Phase 11의 기록을 표준 표기법으로 렌더링해 수순 목록으로 보여 줍니다.
별도의 기록 자료구조를 만들지 않고 Phase 11의 timeline을 인덱스로 조회하므로, 이 phase의 engine 작업은 조회 ABI 하나와 pack 시점의 turns 정규화에 한정됩니다. Timeline은 raw 수순을 보관합니다 — 기록 시점에 정규화하면 `-180°`로 돈 수의 redo가 반대 방향으로 돕니다. Scramble 구간과 사용자 구간의 구분은 timeline의 scramble 경계 query에서 파생되어, 이어 붙이거나 경계를 계산하는 코드가 없습니다.
C ABI는 primitive type만 사용하므로 engine은 layer mask와 회전량을 packed integer로 넘기고, 표기 문자열 조립은 TypeScript가 담당합니다. 표기는 move의 성질이 아니라 표현이므로 표기 스타일 변경이 engine에 닿지 않습니다.
3×3에서 만들어질 수 있는 mask는 단일 layer 세 개뿐이므로, 변환은 구간을 찾는 알고리즘이 아니라 `(axis, layer) → {문자, 부호}` 9칸 표입니다. 음의 face 부호 반전과 M/E/S 방향 규약이 전부 그 표의 부호 칸에 들어가므로 별도 분기가 없고, M/E/S는 N×N 이야기가 아니라 3×3에서도 가운데 줄을 drag하면 바로 나오므로 처음부터 필요합니다.
표에 없는 mask는 예외 없이 실패 경로입니다. 구간 판정과 numbered 표기는 소비자가 Phase 15에서 처음 생기므로 그때 함께 만듭니다.

## [ ] Phase 13: [Presentation options](./13-presentation-options.md)

보이고 들리는 방식을 사용자가 고를 수 있게 합니다. 서로 독립적인 세 옵션이 하나의 옵션 면에 함께 놓입니다.
적록색약에서 구분되는 대체 palette를 더하고 두 벌 중 하나를 고르게 합니다. 검증은 명도 대비가 아니라 deuteranopia 변환을 거친 뒤의 색 거리로 합니다. 명도만 보면 두 색이 색약에서 무너져도 test가 통과해, 목적 자체를 확인하지 못하기 때문입니다. Palette는 `to_color` 한 곳에 모여 있고 cube·interaction·history 어디에도 닿지 않아 나머지 둘과 달리 의존이 없습니다 — 한때 Phase 7.5로 앞당겨 두었으나, 그러면 toggle을 단독으로 붙였다가 이 phase의 옵션 면으로 옮기게 되므로 UI를 한 번만 만들도록 되돌렸습니다. 임의 색 지정은 기각했습니다: 색 거리 test가 성립하지 않게 되고, 저장 계층이 없어 매번 사라지며, 손으로 설정하는 접근성은 검증된 preset보다 나쁩니다.
회전이 commit되는 순간의 효과음을 더합니다. 모든 commit이 timeline cursor를 정확히 ±1 움직이므로, `GameSession`이 완료 판정에 쓰는 cursor 관찰이 그대로 소리를 낼 지점이고 별도의 commit counter도 새 기준값도 없습니다 — reset·scramble에서 소리가 새지 않는 것도 그 기준값 갱신이 이미 거기 있어서 공짜입니다. 소리는 cursor가 변한 frame에 한 번입니다. `AudioContext`는 첫 gesture의 콜스택 안에서 만들어야 하므로 one-shot 리스너로 세웁니다 — commit은 frame 안에서 관찰되어 gesture 밖이라, 첫 소리에서 lazy하게 만드는 방식은 autoplay 정책에 걸립니다.
Animation 속도를 조절할 수 있게 합니다. Phase 9가 tempo를 `start_move`의 인자로 모아 두므로 그 인자에 배율을 곱하는 일이고, 사용자 snap과 scramble, Phase 11의 되감기 재생이 슬라이더 하나를 함께 씁니다.

## [ ] Phase 13.5: [Turn scrape sound](./13.5-turn-scrape-sound.md)

레이어가 도는 동안 큐비가 스치는 소리를 더해, Phase 13의 클릭이 채우지 못하는 구간을 채웁니다.
값어치의 대부분은 drag에 있습니다 — 클릭은 손을 뗀 뒤에야 울리지만 스침은 레이어를 붙잡고 미는 동안 손의 속도를 따라가므로, 클릭이 원리적으로 줄 수 없는 감각입니다. 재생 연출에 붙는 소리는 그 부수 효과입니다.
Phase 13에서 떼어낸 이유는 비용의 종류가 다르기 때문입니다. 클릭은 engine에 한 줄도 닿지 않는 web 전용 항목인데, 스침은 회전 각도를 읽어야 해서 ABI와 generated 산출물과 fixture를 함께 끌고 옵니다.
각도 자체는 `InteractionController::active_rotation()`이 이미 frame마다 내놓고 있고 drag·snap·재생·관람이 전부 그 한 값으로 들어오므로, engine이 새로 계산하는 것은 없고 내보내기만 합니다. 각속도가 아니라 각도를 내보내는 것은 web이 이미 frame마다 관찰을 하고 있어 차분이 공짜인 반면, 속도는 engine에 dt 계약과 frame 간 상태를 새로 요구하기 때문입니다.
각도는 commit에서 0으로 돌아가므로 그 frame의 차분은 회전이 아니라 리셋인데, 차분 크기로는 둘을 가를 수 없습니다 — 되감기 tempo 120ms/quarter에 Phase 13의 4× 배율이 걸리면 정상 회전이 60Hz에서 frame당 약 50°라 리셋(최대 90°)과 겹칩니다. 그래서 Phase 13이 클릭을 위해 이미 보고 있는 `timeline_cursor` 변화를 그대로 써서 그 frame만 정확히 걸러냅니다. 누적 회전량을 engine이 들고 내보내는 대안은 같은 답을 위해 상태를 하나 더 만드는 일이라 기각했습니다.
소리는 oscillator가 아니라 루프하는 noise를 bandpass에 통과시키고 gain만 각속도로 움직이는 방식입니다. 회전마다 소스를 start/stop하면 그 지점마다 딱딱거려 클릭과 섞입니다.
Ambient 무음은 여기서 공짜가 아닙니다. 클릭은 관람이 timeline을 건드리지 않아 저절로 조용했지만 각도는 관람 중에도 움직이므로, `isAmbient()` 분기와 그것을 고정하는 test 하나가 이 phase의 비용입니다. 관람에 소리를 넣는 쪽은 Phase 10이 그것을 화면 보호기로 규정했고 on/off가 하나뿐이라 "볼 때만 조용히"를 표현할 수 없어 기각했습니다.
각속도를 frame에서만 갱신하므로 frame loop가 멈춘 자리에 gain이 얼어붙으면 배경에서 소리가 남습니다. Audio context의 suspend/resume을 `AppLifecycle`이 이미 들고 있는 loop 수명과 `visibilitychange`에 묶어, 쉬는 화면이 소리도 audio thread도 쓰지 않게 합니다.
Mute는 Phase 13의 것 하나를 공유합니다. 소리가 둘이 되어도 켜고 끄는 것은 하나입니다.

## [ ] Phase 14: [Sharing and records](./14-sharing-and-records.md)

현재 상태의 URL 공유와, 세션 안에서만 유지되는 가벼운 기록을 더합니다.
이 phase는 의도적으로 아무것도 저장하지 않습니다. 원래 계획했던 localStorage 저장 계층(세션 복원, 기록 영속화, 옵션 저장)은 뺐습니다. Timer를 직렬화하지 않는 이상 복원된 세션은 어차피 idle이라 지켜지는 것이 만지던 배열 하나뿐인데, 그 하나를 위해 자동 저장과 redo tail 보존, 시작 순서 계약이 전부 필요해지므로 비용이 가치를 넘기 때문입니다. Reload는 언제나 새 시작입니다.
공유는 현재 상태를 scramble 수순과 적용된 사용자 move 목록으로 직렬화해 URL fragment에 싣습니다. 두 구간은 Phase 11 timeline의 scramble 경계에서 파생됩니다. 버튼 한 번의 명시적 행위라 저장 시점 문제가 없고, Phase 11의 timeline과 Phase 12의 조회 ABI가 재료를 다 갖고 있으므로 새로 만들 것은 인코딩과 애니메이션 없이 즉시 적용하는 복원 ABI뿐입니다.
복원은 트랜잭션이 아닙니다. 복원 시점의 "기존 상태"는 갓 초기화된 solved cube라 지킬 값이 없으므로, 실패하면 되돌리는 대신 초기화를 중단하고 깨끗한 초기 상태로 시작합니다. Staging 교체와 회복 계약과 startup gate가 지키려던 것이 그 없는 값이었습니다. Move를 받는 자리에서는 검증하지 않고 마지막에 개수와 모든 값을 한 번에 보므로, 일부만 반영되는 경로도 생기지 않습니다.
Payload에는 seed 대신 실제 scramble 수순을 싣습니다. Seed를 실으면 PRNG와 생성 규칙이 기존 공유 URL의 영구 호환 계약이 되어 나중에 scramble 알고리즘을 고칠 수 없게 되는데, 20수 기준 80바이트를 더 쓰면 그 결합이 통째로 사라집니다.
Sticker 배열을 직접 싣는 편이 짧지만 그렇게 하지 않습니다. 임의의 sticker 배열은 도달 가능한 cube 상태가 아닐 수 있어 받는 쪽에서 검증이 필요한 반면, move 목록은 합법적인 move만 재생하므로 구성상 항상 합법입니다.
기록은 GameController의 메모리에만 두는 최고 기록과 최근 목록입니다. Solve로 끝난 판은 기록하지 않고, undo가 섞인 판은 그대로 기록합니다.

---

여기부터는 앞의 phase가 모두 끝난 뒤에 진행합니다.
ThorVG를 보여 주는 데 필요하지 않거나, 들어가는 코드의 양에 비해 rendering과의 관련이 옅은 항목들입니다. 특히 solver는 이 저장소에서 가장 큰 단일 코드 덩어리가 되면서 rendering과 무관하므로, 도메인의 읽기 좋음을 해치지 않도록 `cube` 안에서도 별도의 하위 target으로 격리합니다.

## [ ] Phase 15: N×N cube support

3×3×3에 고정된 cube 크기를 런타임에 선택할 수 있게 합니다.
Geometry, picking, net 렌더링은 이미 CubeState의 size로 파라미터화되어 있고 고정된 크기는 application lifecycle 한 곳에만 남아 있으므로, 실제 작업은 크기 변경 시의 상태 재구성과 그 주변부입니다.
안쪽 slice를 포함하는 scramble 생성, 바깥 면 외의 layer를 돌릴 UI, 그리고 cubie 수가 세제곱으로 늘어날 때의 software rendering 성능을 함께 확인합니다. 부하가 크기로 조절되므로 renderer의 한계를 보여 주는 자리이기도 합니다.
Phase 12가 실패 경로로 남겨 둔 numbered 표기(`Rw`, `3Rw`, `2R`, `2-3Rw`)를 여기서 채웁니다. Mask에서 구간을 찾는 판정과 조립 규칙을 함께 만들고 — 소비자가 여기서 처음 생기므로 Phase 12는 9칸 표에서 멈춰 있습니다 — M/E/S는 3×3에서만 쓰는 표시 방식으로 남깁니다. 5×5의 가운데 layer를 M으로 쓰는 관행은 없으므로 하나의 규칙으로 통합하지 않습니다.
Phase 16의 solver interface는 지원 크기를 스스로 밝히므로 다른 크기에서는 지원하는 구현이 없다는 사실이 그대로 드러납니다. 그때 solve를 Phase 11의 되감기로 되돌릴지 N×N solver를 더할지 이 phase에서 정합니다.

## [ ] Phase 16: Cube solver

Timeline과 무관하게 현재 CubeState만으로 해법을 계산하는 solver를 추가하되, 구현을 교체할 수 있는 형태로 둡니다.
Solver는 아무 dependency도 갖지 않는 cube target 아래 별도 하위 target으로 두어 rendering과 interaction은 물론 cube 도메인 자체와도 섞이지 않게 합니다. Phase 11이 solve를 "수순 벡터를 만들어 Player에 넘기기"로 구현해 두므로 재생 경로와 UI는 그대로 재사용합니다. 다만 solver의 수는 timeline에 없는 새 수라 `TimelineEffect`의 세 값 중 어느 것도 맞지 않습니다 — 기록 여부와 완주 판정에서 갖는 의미는 소비자가 처음 생기는 이 phase에서 정합니다.
사용자가 직접 섞은 큐브나 공유받은 상태처럼 되감을 기록이 없는 큐브까지 풀 수 있게 되어, Phase 11의 되감기가 닿지 못하는 곳을 메웁니다.

교체 가능한 형태는 renderer와 같은 방식입니다. CubeState를 받아 move 목록을 내는 interface를 정의하고 application이 하나를 소유합니다. 자유 함수 대신 객체인 이유는 알고리즘마다 지원하는 cube 크기가 다르고, 탐색 계열은 pruning table 같은 준비 상태를 생성자에서 만들어 들고 있어야 하기 때문입니다. 지원 크기를 solver 자신이 밝히므로 Phase 15에서 크기가 늘어날 때 지원 여부가 그대로 드러납니다.
첫 구현은 layer-by-layer이고 3×3×3만 지원합니다. Interface가 같으므로 이후 다른 알고리즘을 더할 때 application과 UI는 바뀌지 않고, 해법을 적용하면 solved가 된다는 검증도 구현마다 같은 test를 돌리는 형태가 됩니다.
실행 중 solver를 고르는 UI는 이 phase의 범위가 아닙니다. 선택 ABI는 interface를 바꾸지 않고 나중에 얹을 수 있습니다.

## [ ] Phase 17: Solve hint and step-through

Phase 16의 solver를 사용해 다음 한 수만 알려 주는 힌트와, 해법을 한 수씩 진행하는 모드를 제공합니다.
힌트는 현재 상태로부터 계산되어야 의미가 있으므로 timeline 되감기로는 대체할 수 없고, solver가 선행되어야 합니다.
Player는 다음 한 수만 소비하고 정지하는 step 모드를 지원해야 합니다. Phase 9에서 미리 열어 두지 않고 이 phase에서 함께 더합니다 — 유일한 소비자가 여기이고, Player에 필드 하나와 소비 loop에 조건 하나를 얹는 작은 변경이기 때문입니다.

---

## 세부 문서 관리

각 phase를 시작할 때 `NN-kebab-case.md` 형식의 세부 문서를 추가하고 이 계획에서 해당 phase 제목에 링크합니다.
세부 문서에는 최소한 scope, requirements, out of scope, implementation steps, acceptance criteria와 verification을 기록합니다.
