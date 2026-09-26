# Scenes and surfaces

## Status

`Completed`

Phase 번호가 없는 횡단 작업입니다. [Stage and panels](./stage-and-panels.md)가 만든 무대에서 시작했습니다. 그 무대는 정사각형 캔버스 하나였고, Split은 그 안에서 큐브 아래에 전개도를 쌓았습니다. 그래서 넓은 화면에서도 양옆이 비었습니다.

요청은 "엔진은 장면을 따로 주고, 화면의 크기와 배치는 UI만 정한다"였습니다. 두 안을 비교했습니다.

- ① 캔버스는 하나로 두고, UI가 영역만 지정합니다(`set_regions`).
- ② 장면마다 캔버스를 따로 둡니다.

더 근본적인 ②를 골랐습니다. 이 작업으로 [Stage and panels](./stage-and-panels.md)의 "엔진은 한 줄도 바뀌지 않는다"는 전제는 끝납니다.

## Objective

- **Surfaces.** 엔진은 최대 네 surface에 그립니다. surface마다 크기와 장면 집합(cube, net, rings, axes)이 있고, 한 장면만 가진 surface는 그 장면을 가득 채워 그립니다.
- **The page lays out.** 페이지는 view마다 캔버스를 하나씩 둡니다. 어디에 얼마나 크게 둘지는 stylesheet가 정합니다. Split은 무대가 넓으면 좌우로, 좁으면 위아래로 놓입니다.
- **Axes badge.** 축 표시는 큐브 영역 구석에 박힌 그림이 아닙니다. 큐브 view 위에 뜬 둥근 배지입니다.
- **Cost.** 바뀐 surface만 그리고, 다시 그린 캔버스만 복사합니다. 무대 전체의 픽셀 수에는 상한을 둡니다.

## 결정

### 왜 ②인가

①은 엔진 변경이 작습니다. 대신 캔버스가 무대 전체라서 view 사이의 빈 여백까지 매 프레임 지우고 복사해야 합니다. 영역도 여전히 버퍼 하나 안에 있어서, 축 배지처럼 무대 위에 뜬 요소의 모양, 그림자, 위치를 CSS로 다룰 수 없습니다.

②에서는 view 하나가 DOM element 하나입니다. 그래서 배치, 크기, 꾸밈이 모두 CSS의 일이 되고, 다시 그릴지도 view마다 따로 정할 수 있습니다.

"한 번 그리던 것을 여러 번 그리게 되지 않느냐"는 우려는 측정으로 확인했습니다.

- native Split 한 프레임은 3×3에서 2.1ms, 28×28에서 12–15ms입니다.
- `putImageData`는 픽셀당 약 0.6ns에 캔버스당 약 0.007ms가 더해집니다.

장면을 나눠도 그리는 도형의 총량은 같습니다. 캔버스를 나눈 고정비는 캔버스당 몇 µs입니다. 오히려 surface마다 변화를 따지면, 시점을 돌리는 동안 전개도를, 층이 도는 동안 축을 다시 그리지 않아도 됩니다. 리팩터 뒤 render bench는 그대로입니다(3×3에서 1.99/2.68ms, 28×28에서 11.4/14.5ms, 중앙값/p95).

### Surface 0은 원래의 캔버스입니다

`initialize()`가 만드는 surface 0은 모든 장면(`kAllScenes`)을 가집니다. 그래서 surface를 지정하지 않는 기존 ABI 호출(`resize`, `pixel_buffer`, `pointer_down` …)은 뜻이 그대로이고, 기존 native test는 수정 없이 통과했습니다.

다른 surface는 크기 0으로 시작합니다. 크기를 0×0으로 주면 surface를 치워 둡니다. 치운 surface는 버퍼가 없고 그리지도 않습니다. surface 0도 치울 수 있고, `resize()`로 돌아옵니다.

### 한 장면만 가진 surface는 그 장면을 가득 채웁니다

surface가 그리는 것은 `scenes & shown_scenes()`입니다. 무엇이 보이는지는 여전히 view mode와 flat style이 정합니다. surface는 그중 자기 몫만 그리고, 축은 큐브와 함께 보입니다.

배치는 기존 `graphics::layout()`에 "이 surface가 그리는 장면"을 물어서 정합니다. 새 배치 규칙은 만들지 않았습니다.

- 큐브만 있으면 Cube3D 배치입니다. 짧은 변의 0.84인 정사각형이 가운데 놓입니다.
- 넷만 있으면 넷 단독 배치입니다. 가로 0.88과 세로 0.78 중 먼저 닿는 쪽까지 커집니다.
- 링만 있으면 링 단독 배치입니다. 짧은 변의 0.92입니다.
- 모든 장면을 가진 surface는 예전의 한 캔버스 그대로입니다.

### 축은 배지입니다

축만 가진 surface에는 `build_axis_badge(camera, rect)`가 그립니다. 가운데에서 세 팔을 뻗고, 팔 길이는 짧은 변의 0.34(`kAxisBadgeArmShare`), 굵기는 0.06(`kAxisBadgeWidthShare`)입니다. 큐브와 같은 surface에 있을 때는 이전처럼 큐브 영역 구석의 gizmo(`build_axis_gizmo`)입니다. 둘은 팔을 그리는 `draw_arms`를 공유합니다.

웹은 배지 캔버스를 큐브 view의 왼쪽 위에 둥글게 띄웁니다.

- **크기**는 `clamp(44px, 11cqmin, 84px)`입니다.
- **둥근 이유.** 캔버스 바탕은 페이지 색입니다. 사각형 그대로 큐브 그림자 위에 얹으면 구멍처럼 보였습니다.
- **누를 수 없습니다**(`pointer-events: none`). 배지 위를 눌러도 아래의 큐브에 닿습니다.
- **왼쪽 위인 이유.** 처음에는 왼쪽 아래였는데, 좁은 화면에서 첫 방문 힌트와 겹쳤습니다.

### 누름은 surface를 말합니다

`pointer_down_on(id, x, y)`는 그 surface의 픽셀로 받고, 그 surface에 보이는 것에만 닿습니다. 큐브가 있는 surface에서는 큐브 밖을 누르면 버퍼 밖이라도 시점을 돌립니다. 큐브가 없는 surface에서 아무것도 잡지 못한 누름은 거절합니다.

제스처는 시작한 surface를 기억합니다(`gesture_surface`). surface의 크기나 장면이 바뀌면 그 surface에서 시작한 제스처만 취소합니다. 패널이 열리며 넷 캔버스가 줄어도 큐브를 끄는 드래그는 살아남습니다. 칠하기 획도 시작한 surface의 픽셀로 이어집니다.

페이지 쪽에서는 한 걸음 더 나아가, 제스처가 진행되는 동안에는 어떤 캔버스의 크기도 바꾸지 않습니다. ResizeObserver나 창 크기 변경이 와도 lock이 잡혀 있으면 크기를 밀려 있는 것으로 적어 두고, 제스처가 끝나는 순간(`onGestureEnd`) 한꺼번에 적용합니다.

웹의 규칙은 다음과 같습니다.

- **빈 곳은 시점입니다.** 넷이나 링 캔버스에서 엔진이 거절한 누름(넷의 빈 곳)은 큐브 캔버스에 다시 건넵니다. 좌표는 큐브 캔버스의 픽셀로 바꿉니다(`PointerFallback`). 이어지는 이동도 넘겨받은 캔버스 기준으로 잽니다. 그래서 캔버스가 하나이던 때처럼 "빈 곳을 끌면 시점이 돈다"가 어디서나 참입니다.
- **제스처는 한 번에 하나.** 모든 캔버스가 `GestureLock` 하나를 공유합니다. 엔진이 제스처 하나만 따라가기 때문입니다.
- **힌트.** 첫 방문 힌트는 캔버스가 아니라 무대에서 누름을 들으므로, 어느 view를 눌러도 사라집니다.

### 바뀐 것만 그립니다

surface마다 마지막 프레임을 그린 입력(`graphics::FrameInputs`)을 두고, `render()`가 다음 프레임의 입력과 비교해 같으면 건너뜁니다.

`FrameInputs`는 크기, 배치, 그리고 그 surface가 그리는 그림마다 값 하나로 이루어집니다. 그림의 값에는 그 그림이 읽는 것이 전부 들어 있고, 읽지 않는 것은 들어 있지 않습니다.

| 그림 | 읽는 것 |
| --- | --- |
| `CubeDrawing` | 큐브, 진행 중인 회전, palette, 조명, 시점 |
| `NetDrawing` | 큐브, 진행 중인 회전, palette, 가이드, 칠하는 중인 draft |
| `RingsDrawing` | 큐브, 진행 중인 회전, palette, 가이드 |
| `AxesDrawing` | 시점 |

넷의 값에는 시점이 없으므로 시점을 돌려도 넷의 프레임은 그대로이고, 축의 값에는 큐브가 없으므로 층을 돌려도 축의 프레임은 그대로입니다. 건너뛰기는 이 표가 전부입니다.

그림은 `graphics::compose(frame)`가 만듭니다. graphics 라이브러리에 있어 앱의 상태에 닿을 수 없고, 받은 값만 읽습니다. 그래서 그리는 것과 비교하는 것이 어긋날 수 없습니다. 그림이 새로 읽을 것이 생기면 그 값에 필드를 더할 수밖에 없고, 기억할 곳은 바로 아래에 있는 그 값의 `operator==` 하나입니다. 회전, 가이드, 조명, 시점 같은 값의 비교도 각자의 타입 옆에 있습니다.

큐브만 복사하지 않습니다. 가장 큰 큐브는 cubie가 2만 개가 넘어 캔버스마다 매 프레임 복사하기에는 크기 때문입니다. 대신 주소와 revision(`CubeRevision`)으로 들고, revision이 같으면 같은 큐브입니다. revision은 큐브가 바뀌는 곳마다 올리지 않습니다. `render()`가 마지막으로 그린 큐브와 값으로 비교해 올립니다(`note_cube_change()`). 올리기를 빠뜨려 그림이 멈추는 종류의 버그가 생길 수 없게 하기 위해서입니다.

배경색은 장면이 아니라 renderer가 들고 있습니다. 그래서 프레임의 입력에 없고, theme이 바뀌면 모든 surface의 마지막 프레임을 잊습니다. 크기가 바뀌었을 때와 같습니다.

`surface_frame(id)`는 실제로 그린 횟수입니다. 웹은 그 값이 움직인 캔버스만 `putImageData`합니다. `invalidate()`를 부르면 다음 render가 모두 그립니다. render bench는 측정하는 프레임마다 앞에서 이를 부릅니다. 건너뛴 프레임은 아무것도 재지 않기 때문입니다.

브라우저에서 캔버스별 복사 횟수를 세어 확인했습니다.

- 넷의 빈 곳에서 시점을 돌리면 큐브와 배지만 복사되고, 넷은 복사되지 않습니다.
- 넷에서 층을 돌리면 큐브와 넷만 복사되고, 배지는 복사되지 않습니다.

### 픽셀 상한은 무대 전체에 한 번 둡니다

`MAX_STAGE_PIXELS`는 2,600,000입니다. 모든 캔버스를 같은 밀도로 그리고, 합이 상한을 넘으면 모두 함께 밀도를 낮춥니다(`stageDensity`). view 하나만 흐려지지 않게 하기 위해서입니다. 1024×768, dpr 2의 Split은 합이 약 150만 픽셀이라 기기 밀도 그대로 그립니다.

### 크기와 배치는 stylesheet가 정합니다

- `.game-stage`가 size container이고, 그 안의 `.stage-grid`가 grid입니다. container query는 자기 container를 꾸밀 수 없어서 한 겹을 더 두었습니다.
- 모든 캔버스는 자기 셀을 absolute로 채웁니다. 드로잉 버퍼의 크기가 grid의 track 계산에 들어가면 버퍼와 박스가 서로를 키우는 고리가 생깁니다. 버퍼 크기가 layout에 새어 들어가지 않게 막은 것입니다.
- 보이지 않는 view는 `display: none`이라 박스가 없습니다. 박스가 없는 캔버스의 surface는 치워 둡니다(`resizeSurface(id, null)`).
- 큐브 캔버스도 셀을 채웁니다. 큐브는 가운데 정사각형에 그려지고, 그림자와 시점을 돌릴 빈 곳은 셀 끝까지 닿습니다.

| 모드 | 무대 비율 6:5 미만 | 6:5 이상 | 11:5 이상 |
| --- | --- | --- | --- |
| 3D | 큐브 | 큐브 | 큐브 |
| Split, 도식 하나 | 큐브 위(11), 도식 아래(9) | 큐브, 도식 좌우 1:1 | 같음 |
| Split, Both | 큐브 위(3), 넷·링 아래(2) | 큐브 왼쪽(3), 넷 위·링 아래 오른쪽(2) | 큐브, 넷, 링 세 열 |
| 2D, 도식 하나 | 도식 | 도식 | 도식 |
| 2D, Both | 넷 위, 링 아래 | 넷, 링 좌우(13:11) | 같음 |

6:5는 계산으로 정했습니다. Split에서 큐브와 도식을 좌우로 놓는 것은 무대 비율이 약 1.1을 넘으면 위아래보다 둘 다 크게 그립니다. 거기에 조금 여유를 두었습니다. 11:5는 오른쪽 열의 넷과 링이 높이에 막혀 열 너비의 절반쯤을 비우기 시작하는 지점입니다.

### 표시 속성은 무대로 옮겼습니다

`data-view-mode`와 `data-flat-style`는 `#view`가 아니라 `#stage`에 씁니다. CSS가 이 두 값으로 캔버스를 배치하기 때문입니다. `GameUi.canvas`는 `GameUi.stage`로 바뀌었고, 캔버스는 셸의 `views`(`cube`, `net`, `rings`, `axes`)에 있습니다.

## C ABI

| 함수 | 뜻 |
| --- | --- |
| `thorvg_rubiks_resize_surface(id, w, h)` | surface의 크기를 정합니다. 0×0이면 치워 둡니다(surface 0도). `resize()`는 surface 0에 대한 이 호출이지만 0×0은 받지 않습니다. |
| `thorvg_rubiks_set_surface_scenes(id, scenes)` | 장면 비트(cube 1, net 2, rings 4, axes 8)를 정합니다. 없는 비트는 거절합니다. |
| `thorvg_rubiks_surface_scenes(id)` | 현재 장면 비트를 돌려줍니다. |
| `thorvg_rubiks_surface_pixel_buffer(id)` / `_pixel_byte_length(id)` | surface의 픽셀 버퍼입니다. 치운 surface는 0이고, 그 surface의 크기가 바뀔 때까지 유효합니다. |
| `thorvg_rubiks_surface_frame(id)` | 실제로 그린 횟수입니다. |
| `thorvg_rubiks_pointer_down_on(id, x, y)` | 그 surface의 픽셀에서 제스처를 시작합니다. |

모두 초기화 전에는 0을 돌려주고, 없는 surface id도 거절합니다. `CubeEngine`은 `presentSurface`, `setSurfaceScenes`, `resizeSurface`, `pointerDownOn`으로 이들을 감쌉니다. `render()`는 frame 수가 움직인 캔버스만 복사합니다.

큐브 캔버스(surface 0)도 같은 호출로 다룹니다. 처음에는 surface 0만 예전 한 캔버스 호출(`resize`, `pixel_buffer`, `pointer_down`)로 따로 다뤘는데, 치웠다가 되돌릴 때 크기를 0으로 만들어 호출을 억지로 다시 일으켜야 했고 고칠 때마다 두 곳을 고쳐야 했습니다. 엔진은 그 호출들을 여전히 내보내지만 페이지는 쓰지 않습니다. `resizeSurface`는 크기가 바뀌었는지를 돌려주고, AppLifecycle은 크기를 따로 들고 있지 않습니다. 크기는 엔진이 들고 있는 것 하나뿐입니다.

## Source organization

```text
engine/src/
├── app/Application.{hpp,cpp}     Surface, 장면별 배치, surface별 누름, frame_for와 건너뛰기
├── graphics/FrameInputs.{hpp,cpp} 그림마다 읽는 값과 그 비교, compose
├── graphics/AxisGizmo.{hpp,cpp}  draw_arms, build_axis_badge
└── platform/web/bindings.cpp     surface ABI

web/src/
├── wasm/CubeEngine.ts            CubeSurface, CubeScene, 모든 캔버스(surface 0 포함)를 한 경로로 표시와 복사
├── AppLifecycle.ts               StageView, stageDensity와 픽셀 상한, 한 밀도로 맞추기, 캔버스별 포인터
├── input/PointerController.ts    PointerFallback, GestureLock
├── ui/GameShell.ts               무대의 캔버스 넷
└── style.css                     stage grid, container query, 축 배지
```

## Verification

- **Native**: `tests/app/SurfaceTest.cpp`(suite `surfaces`)가 다음을 확인합니다.
  - surface 0은 모든 장면을 가지고, 나머지는 빈 채로 시작합니다.
  - 장면 하나는 surface를 채웁니다.
  - view mode가 보이는 것만 그립니다.
  - 누름은 누른 surface에 닿습니다.
  - 큐브 surface 밖의 누름은 시점을 돌립니다.
  - 그림이 같으면 frame이 그대로입니다. 두 프레임 사이에 돌렸다가 되돌린 큐브도 마지막으로 그린 큐브와 같으므로 다시 그리지 않습니다.
  - theme이 모든 surface에 닿습니다.
  - 치운 surface가 돌아옵니다.
  - 다른 surface의 resize에도 드래그가 살아남습니다.
  - 칠하기 획은 시작한 surface에 남습니다.
  - 없는 surface와 장면은 거절하고, 초기화 전 호출도 거절합니다.
  - native suite 29개가 모두 통과합니다.
- **WASM**: emsdk로 다시 빌드했습니다.
- **TypeScript unit**:
  - CubeEngine: fake module의 surface, 복사를 건너뛰는 frame(surface마다), 치우고 되돌리기, 크기가 바뀐 뒤의 복사, 메모리 증가 뒤의 view, 버퍼 없는 surface, 거절.
  - PointerController: 거절된 누름의 fallback, 공유 lock.
  - AppLifecycle: view마다의 장면, 첫 프레임 전 크기, 한 밀도, 치우고 되돌리기, 모든 캔버스 관찰, 누름 라우팅, teardown, `stageDensity`.
  - GameController: 무대의 속성.
  - 모두 317개가 통과합니다.
- **Browser e2e**:
  - scene contract는 큐브를 `#view`에서, 전개도를 `#view-net`에서 읽습니다. 큐브 영역은 단독 배치(한 변 0.84, 가운데)입니다.
  - probe는 읽기 전에 두 프레임을 기다립니다. 방금 박스가 바뀐 캔버스는 다음 rendering 단계에서 새 버퍼를 받기 때문입니다.
  - seam 검증 임계는 캔버스가 아니라 큐브 영역의 크기(594px)로 옮겼습니다.
  - flat view spec은 캔버스마다 보이고 숨는 것과 각 캔버스의 스티커를 봅니다.
  - 65개가 모두 통과합니다.
- **Visual QA**:
  - 1024×768, dpr 2에서 Split(Net, Both), 3D, 2D Both를 봤습니다.
  - 375×812와 1600×680(세 열)도 봤습니다.
  - 라이트 theme을 봤습니다.
  - 넷의 빈 곳으로 시점을 돌리는 것과 넷 드래그로 층을 돌리는 것을 확인했습니다.

```bash
meson test -C build/native --print-errorlogs
./build_wasm.sh
npm --prefix web run test:unit
npm --prefix web run test:e2e
```

## Out of scope

- 좁은 화면에서 무대는 최소 높이 `min(64vw, 300px)` 근처에 머뭅니다(375×812에서 259px). 두 줄의 rail과 dock이 세로 공간을 먼저 가져가기 때문이고, 이 작업 전부터의 배치입니다. 그 안에서는 Split이 좌우로 놓여 예전의 한 캔버스보다 두 view가 모두 큽니다.
- 캔버스 사이의 12px 틈은 누름을 받지 않습니다.

## 개정 기록

- **건너뛰기의 네 목록을 하나로 모았습니다.** 처음에는 한 프레임이 무엇으로 그려졌는지를 네 곳이 따로 적었습니다. 입력을 담는 구조체(`DrawnInputs`), 장면마다 무엇을 채울지(`inputs_for`), 필드마다 비교(`same_inputs`), 그리고 앱의 상태를 직접 읽는 그리기(`scene_for`)입니다. 그리기가 새로 읽는 것을 나머지 셋 중 하나에 빠뜨리면, 그림이 바뀌었는데도 프레임을 건너뛰어 옛 그림이 남습니다. 게다가 조용히 남습니다. 지금은 그림마다 값 하나가 그 그림이 읽는 것 전부이고, `compose()`는 graphics 라이브러리에서 그 값만 받습니다. 빠뜨리는 방향이 바뀌었습니다. 값에 없는 것은 그릴 수 없으므로, 빠뜨리면 조용히 남는 옛 그림이 아니라 눈에 보이는 틀린 그림이 됩니다. 조명과 draft도 이제 값으로 비교하므로 `lighting_revision`, `paint_revision`, `drawn_draft`는 없어졌습니다. `tests/graphics/FrameTest.cpp`(suite `frame inputs`)는 모든 입력이 비교에 들어가는지 보고, `surfaces`에는 palette, 조명, 칠하기, theme이 각각 그것을 보여 주는 surface만 다시 그리는지 보는 test를 더했습니다. 조명 비교를 빼거나 theme에서 프레임을 잊지 않게 바꾸면 이 test들이 실패하는 것을 확인했습니다. native suite는 30개가 되었습니다.
