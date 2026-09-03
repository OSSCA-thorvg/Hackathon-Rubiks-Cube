# Phase 19: Lighting and shadow

## Status

`Completed` — 2026-09-04. 초안에 외부 리뷰를 받아 한 차례 개정하고, 구현 뒤 화면을 보며 일곱 차례 손봤습니다. 아래 결정은 **최종 상태**를 적은 것이고, 거쳐 온 길과 그 과정에서 배운 것은 끝의 [개정 기록](#개정-기록)에 모았습니다.

Phase 17(Solve hint and step-through)과 Phase 18(Paint your cube) 어느 쪽도 필요로 하지 않고, 어느 쪽에도 영향을 주지 않습니다. 번호는 순서가 아니라 자리입니다.

## Objective

**3D 큐브에 광원을 둡니다.** 면마다 빛을 받는 정도가 다르고, 한 면 안에서도 광원 쪽이 더 밝고, 광원을 눈으로 반사하는 면에는 광택이 맺히고, 큐브 아래 바닥에는 광원 위치와 큐브 자세를 따르는 그림자가 눕습니다. 레이어를 돌리면 튀어나온 레이어의 그림자가 함께 돌아가고, 기울어진 면 위로 광택이 스쳐 지나갑니다.

이 phase는 기능이라기보다 **ThorVG showcase**입니다. ThorVG는 셰이더를 노출하지 않는 2D 벡터 엔진인데, 그 위에서 Blinn–Phong 조명과 투영 그림자를 어떻게 내는지 보여 주는 것이 목적입니다. 구현 방식을 고를 때 "가장 짧은 코드"보다 **ThorVG의 기능(gradient fill, fill rule, Scene 합성, SceneEffect, clip)이 각각 제 역할을 하는 구성**을 택하되, 합성 레이어는 측정으로 정당화되는 하나만 둡니다.

계산 모델은 Blinn–Phong의 ambient + diffuse + specular 세 항에 점광원의 거리 감쇠를 더한 것입니다. 반사(reflection) 항은 없습니다.

## 무엇이 이미 있고, 무엇이 없는가 (확인함)

### 파이프라인은 패스를 끼울 자리를 이미 갖고 있습니다

[`Pipeline.hpp`](../../engine/src/graphics/Pipeline.hpp)의 `operator|`는 왼쪽 scene에 오른쪽 callable을 적용하는 문법일 뿐이고, 왼쪽에 올 수 있는 타입은 `is_pipeline_value`로 WorldScene, ViewScene, ClipScene, RenderScene 넷이 열려 있습니다. 새 패스는 이 넷 중 하나를 받아 같은 타입을 돌려주면 등록 없이 체인에 들어갑니다. 체인은 [`Application.cpp`](../../engine/src/app/Application.cpp) 한 곳입니다.

### 기존 패스는 `faces`만 만집니다

`TransformPass`, `ViewPass`, `ProjectPass`, `ViewportPass`는 `scene.faces`의 꼭짓점만 변환합니다. 새 필드(casters, 그림자 그룹, 음영, 광택)는 **네 패스 모두**에 통과 규칙이 있어야 하고, `ViewportPass`는 rect를 아는 유일한 자리라 그림자의 clip을 적는 곳이 됩니다.

### 면에는 색과 꼭짓점만 있고 노멀이 없습니다

노멀은 저장하지 않습니다 — 면이 평면이라 `cross(p1 − p0, p3 − p0)`로 매 프레임 구할 수 있고, 뷰 공간에서는 눈이 원점이라 시선 벡터도 `−p`로 공짜입니다. 방향 규약은 `CullPass`가 이미 정해 두었습니다: NDC에서 반시계 방향이 앞면입니다. 노멀의 부호는 이 규약에서 유도하고 test로 고정합니다.

### 그려지는 것은 스티커이고, 큐비 몸체는 없습니다

`build_cube_scene`은 바깥 스티커만 냅니다. 스티커는 셀의 `kStickerScale = 0.92`배라 사이에 틈이 있고, 회전 중 드러나는 단면은 `kBodyColor`로 채워집니다. 그려지는 면을 그대로 바닥에 투영하면 그림자에 격자가 나고, 틈으로는 배경이 비칩니다. 그림자에는 다른 재료가 필요하고(결정 4), 틈에는 몸체가 필요합니다(결정 6).

### `LayerMask`는 비트마스크입니다

타입은 비연속 층 집합을 허용하지만, 지금 mask를 만드는 경로 `depth_layers`는 항상 연속 구간 하나를 냅니다. caster 개수 상한은 이 둘을 구분해 적습니다(결정 4).

### 렌더러는 단색 fill만 썼습니다

`ThorVGSoftwareRenderer`는 Shape를 `SwCanvas`에 바로 push하고 `fill(r, g, b, a)`와 stroke만 썼습니다. 배경색은 `Renderer` 경계의 값이라 Light(231, 235, 240)와 Dark(32, 32, 32) 둘을 받습니다. 그림자색은 두 배경 위에서 모두 성립해야 합니다.

### ThorVG 1.0.0이 필요한 API를 전부 갖고 있습니다

vendored [`subprojects/thorvg`](../../subprojects/thorvg/meson.build)는 1.0.0이고 엔진 수정은 필요 없습니다.

| API | 이 phase에서의 역할 |
|---|---|
| `LinearGradient` (fill, stop 다섯) | 평면의 밝기 함수로 스티커를 채움 |
| `RadialGradient` + `Fill::transform` | 광택 — 단위원 falloff를 화면 타원으로 |
| `Shape::fillRule(FillRule::NonZero)` | 여러 caster의 그림자 다각형을 한 Shape의 subpath로 넣어 합집합으로 |
| `LinearGradient`/`RadialGradient` (fill) | 그림자의 fade — 큐브 아래에서 끝까지 옅어짐 |
| `Scene::add(SceneEffect::GaussianBlur, double, int, int, int)` | 반그림자 — 이 phase의 **유일한 합성 레이어** |
| `Paint::clip()` | 그림자를 캔버스로 |

`SceneEffect::add`는 **variadic**이라 인자 개수와 타입이 헤더 주석과 정확히 같아야 합니다(`float`을 넘기면 undefined behavior). 렌더러는 이 호출을 인자 타입이 고정된 함수 하나로 감쌉니다. `Scene::opacity()`, `mask()`, `blend(Multiply/Screen)`, `SceneEffect::DropShadow`는 쓰지 않습니다 — 이유는 결정 7과 개정 기록에 있습니다.

### 렌더 계약이 스티커 색을 byte 단위로 검사합니다

[`RenderContractTest.cpp`](../../tests/app/RenderContractTest.cpp)와 e2e의 [`sceneContract.ts`](../../web/tests/e2e/sceneContract.ts)는 solved 3×3의 보이는 세 면 중앙 픽셀과 세 seam 픽셀을 정확한 값으로 단언합니다. 조명은 그 값을 바꾸는 것이 목적이므로 계약은 v4로 개정합니다(결정 9). 이 test가 실제 `SwCanvas` 버퍼를 읽는다는 것은 반대로 기회입니다 — 렌더러의 합성 결과를 native에서 픽셀로 검증할 수 있습니다.

### 카메라는 바닥 아래까지 내려갈 수 있습니다

홈 시점은 `kHomeEye{3, 3, 3}`(고도각 약 35°)이지만, 궤도 카메라의 pitch 한계 80°에서 눈은 `y ≈ −5.12`까지 내려갑니다. 바닥 `y = −1.5`는 pitch 약 −16.8°에서 지나갑니다(결정 6).

## Architecture decisions

### 1. 조명은 패스이고, 렌더러는 시킨 것만 그린다

```cpp
scene = build_cube_scene(state, active, palette)   // faces(to_lit_color) + casters
      | transform(model)                           // faces와 casters 둘 다
      | shadow(lighting, camera)                   // WorldScene: casters → 그림자 그룹 (월드 바닥)
      | view(camera)                               // faces, casters, 그림자 그룹
      | light(lighting, camera)                    // ViewScene: 음영·광택 계산
      | project(camera)                            // 음영 stop과 광택 타원을 화면으로
      | cull()
      | depth_sort()
      | viewport(rect, stage);                     // bodies, 그림자 clip = stage (캔버스 전체)
```

`shadow()`는 바닥 평면이 정의된 월드 공간에서, `light()`는 시선 벡터가 공짜인 뷰 공간에서 돕니다. 두 패스는 **같은 `Lighting` 값**을 받고 각자 자기 좌표계로 옮깁니다 — `light()`는 생성 시 `camera.view()`로 램프를 뷰 공간에 옮겨 들고 있습니다. 궤도 카메라에서 광택의 반사점과 그림자 방향이 같은 광원에서 나온다는 것이 test로 고정되어 있습니다.

RenderScene은 여전히 ThorVG를 모릅니다. 조명 결과는 `Color`와 기하(음영 축과 stop 색, 광택 타원과 stop, 그림자 그룹, 몸체 실루엣)로만 전달되고, 그것을 gradient와 Scene으로 옮기는 결정은 렌더러가 합니다. [DESIGN.md](../DESIGN.md)의 "ThorVG는 파이프라인의 마지막 backend" 원칙 그대로입니다.

### 2. 밝기는 평면의 함수이고, 스티커는 그 함수를 자기 색으로 채운다

ThorVG의 fill은 경로 안을 한 색 또는 한 gradient로 칠하고, 픽셀마다 다른 값을 낼 수는 없습니다. 하지만 큐브의 면은 평면이고, 평면 위의 확산항은 광원의 발(평면에 내린 수선의 발)로부터의 거리만의 함수입니다. 점광원이라 면 안에서 광원 쪽이 실제로 더 밝습니다.

```
b(P) = 255 × (ambient + Σ diffuse_i × max(N·L_i, 0) × (|L_i − C| / |L_i − P|)^attenuation)
```

`C`는 큐브 중심입니다. 밝기는 255를 넘을 수 있습니다 — palette 색은 "빛을 잘 받는 면의 색"이고, 그보다 밝은 면이 있습니다. 채널은 255에서 멈춥니다.

**밝기 함수는 평면(노멀, 오프셋)에 하나입니다.** `light()`는 평면마다 한 번, 키의 발에서 평면이 큐브 중심에 가장 가까운 점(큐브 면의 한가운데, anchor)으로 향하는 축을 잡고, 그 평면 위 모든 면의 꼭짓점을 축에 투영한 구간을 등간격 stop 다섯 개로 나눠 각 stop에서 참값 `b₀ … b₄`를 계산해 캐시합니다. **스티커는 축과 stop을 그대로 쓰고 stop 색만 자기 색 `c`에 밝기를 곱한 `shade(c, bₖ)`로 채웁니다.** 섞인 큐브에서 빨강 옆의 초록은 각자 자기 색이되 같은 자리에서 같은 밝기를 받으므로, 값도 기울기도 seam을 넘어 이어집니다. 회전 중 단면(`kBodyColor`)도 자기 평면의 함수를 받습니다.

- `project()`는 stop마다 3D 위치를 투영해 화면 축 위의 실제 fraction(`ShadingOf::offsets`)을 적습니다. 평면 위의 등간격은 perspective 아래 화면에서 등간격이 아닙니다.
- 스티커 양 끝에서 읽은 색이 같은 스티커는 그 자리의 단색으로 그립니다 — 같은 그림이고 28×28에서 4ms 싸집니다.
- `Face::color`는 anchor에서의 값입니다. 계약이 이름 붙이는 byte입니다.

**채도.** RGB에 밝기를 곱하면 명도가 깎여 채도가 빠진 것처럼 읽히므로, 스티커의 base 색에 조명 전에 `Lighting::saturation`(1.25)을 적용합니다: `gray = (2126 r + 7152 g + 722 b + 5000) / 10000`, `c' = clamp(gray + round_away((c − gray) × 125 / 100))`. 몸체색에는 적용하지 않습니다.

**ambient는 낮출 수 없습니다.** 스티커 색은 상태를 나타내는 정체이고, 특히 Phase 18에서 사용자는 실물과 화면을 대조합니다. 여섯 색이 명암의 양 끝에서도 서로 구별된다는 것을 test로 고정합니다.

**High contrast palette는 unlit입니다.** `Lighting::unlit()`은 ambient 1, saturation 1, 램프의 diffuse·specular 0이고 키의 위치는 남겨 그림자는 유지합니다. 그 palette는 측정으로 고른 여섯 색이라 byte 그대로 돌아와야 합니다. 값만 바뀌고 분기는 없습니다.

### 3. 광택은 평면 위의 타원 발자국이고, 붙이는 판정과 그리는 도형이 같다

(N·H)^n은 한 점에서 최대이고 사방으로 떨어지는 봉우리라 radial gradient의 형태입니다. 봉우리의 위치는 계산합니다 — 램프를 평면에 대칭시킨 점과 눈을 잇는 선분이 평면과 만나는 점(H = N)입니다. 봉우리는 **평면의 것**이라 스티커 밖에 있을 수 있고, 스티커는 같은 함수를 자기 경로로 잘라 보이기만 합니다.

- **발자국은 타원입니다.** 봉우리에서 평면의 `±u`, `±v` 네 방향으로 lobe가 문턱 `kHighlightMinAlpha`(8) 아래로 떨어지는 거리를 이분법으로 재고(평면당 램프당 네 번, 캐시), 네 끝점을 지나는 타원을 만듭니다. 기울어져 보이는 평면 위의 lobe는 눈 쪽으로 길어서 원이 아닙니다.
- **falloff는 lobe에서 샘플합니다.** 중심에서 rim까지 다섯 자리(`kGlintStops`)에서 네 축 방향의 세기를 평균한 값이 stop이고, 바깥으로 비증가, rim은 0입니다(gradient는 마지막 stop을 반지름 밖으로 이어 그리므로).
- **스티커는 타원의 좌표계에서 판정합니다.** 네 꼭짓점을 타원의 두 축에 투영해 반축으로 나눠 단위원 좌표의 상자로 만들고, 상자에서 원점에 가장 가까운 점이 반지름 1.1 안이면 그 스티커에 광택 도형을 붙입니다. 붙이는 판정과 그리는 gradient가 같은 타원이라 경계가 보이지 않습니다.
- **화면 좌표는 anchor의 투영 접선으로 만듭니다.** 발자국은 면보다 넓고(반지름 ~2.7, 면은 2) 눈에 가까워 perspective 아래 화면 타원이 아닙니다. `HighlightOf`는 평면의 anchor와 두 단위 축을 들고 가고, `project()`는 anchor와 축 방향 0.25 떨어진 두 점만 투영해 그 자리의 affine 접선을 만든 뒤 중심과 rim 네 점을 그 접선으로 옮깁니다. 광택이 보이는 자리에서 정확하고, 눈 뒤로 넘어간 점을 투영할 일이 없습니다(anchor가 near plane 뒤면 그 광택은 버립니다).
- **렌더러**는 네 rim으로 화면 타원을 만들어(중심 = 평균, 반축 = 마주보는 두 점 차의 절반) 단위 radial gradient에 `Fill::transform`으로 affine 행렬을 걸어 흰색을 얹습니다. 흰색 overlay는 Screen blend와 같은 식(`c + a(255 − c)`)이라 blend 레이어가 필요 없습니다.
- 광택은 **면을 그린 직후** 그립니다. 회전 중 큐브는 볼록하지 않아, 뒤에 있는 면의 광택이 앞면 위로 새면 안 됩니다.

**홈 시점에는 광택이 없습니다.** 눈이 (3, 3, 3) 대각선에 있을 때 보이는 세 면은 시선에서 54.7°씩 기울어 있어, 쉬는 앞·오른 면에 반사점이 놓이려면 램프가 큐브 아래에, 윗면에 놓이려면 큐브 뒤 위에 있어야 합니다. 키 하나로는 불가능하고, 이것이 계약(결정 9)에는 좋은 소식입니다. 광택은 **카메라를 돌려 면이 램프와 눈 사이에 놓일 때**와 **R·L·F·B 회전에서 도는 층의 윗면이 눈 쪽으로 기울 때** 나타납니다(R 45°에서 peak alpha 59).

### 4. 그림자를 드리우는 것은 스티커가 아니라 층 묶음 박스다

스티커는 틈이 있고 몸체가 없어 투영하면 격자가 됩니다. 큐비 하나씩 박스로 투영하는 안은 28×28(큐비 21,952개)에서 성립하지 않습니다. 그래서 caster는 **층 묶음(slab) 박스**이고, 지금 도는 축 하나로만, 회전 중일 때만 나눕니다.

- 쉬는 큐브는 박스 하나, 큐브 전체입니다.
- 회전 중이면 `ActiveRotation::axis` 방향으로 `layers`의 연속 구간마다 박스 하나(회전을 받음), 그 사이의 정지 구간마다 박스 하나입니다.

```text
3×3, X축 층을 [L M R]이라 할 때
R  : [L M] 정지 + [R] 회전            → 2개
Rw : [L] 정지   + [M R] 회전          → 2개
M  : [L] 정지   + [M] 회전 + [R] 정지 → 3개
```

박스 수는 `(mask의 연속 구간 수) × 2 + 1` 이하이고, 현재 생성 경로에서는 3 이하입니다(test). 박스는 꼭짓점 8개로 충분합니다 — 볼록한 박스의 그림자는 볼록 다각형 하나라서, `shadow()`는 꼭짓점 8개를 투영한 뒤 2D convex hull([`ConvexHull.hpp`](../../engine/src/graphics/ConvexHull.hpp))을 잡습니다. casters는 `build_cube_scene`이 스티커와 같은 회전을 적용해 냅니다. 그려지지 않고, `shadow()`와 결정 6의 몸체만 읽습니다.

### 5. 광원은 키 라이트 하나이고, 조명과 그림자가 공유한다

```
t  = (ground_y − L.y) / (P.y − L.y)
P' = L + (P − L) × t          // 박스의 어느 꼭짓점이든 P.y ≥ L.y 이면 그 박스는 버린다
```

점광원은 광원이 움직이면 그림자가 늘어나고 기울어지고 광택이 면 위를 이동하므로 showcase에는 점광원이 맞습니다. 유한 높이의 점광원은 직상부에서도 그림자를 확대합니다 — 배율 `(L.y − ground_y) / (L.y − P.y)`.

`Lighting { ambient, attenuation, saturation, lamps[] }`, `Light { position, diffuse, specular, shininess }`. 램프는 1~4개 가변이고 기본은 **키 하나**입니다. diffuse는 램프의 합, 광택은 램프마다 평면당 하나(`highlights` 목록), **그림자는 `lamps[0]`만** 드리웁니다. 키는 홈 시점 위쪽 앞, 조금 왼쪽에 있어 +Y가 가장 밝고 +Z, +X 순입니다. 위치는 계약 byte가 반올림 경계에서 0.3 이상 떨어지도록 격자 탐색으로 골랐고, 덤으로 2×2~9×9의 세 면 byte가 같습니다.

### 6. 바닥은 보이지 않고, 큐브는 떠 있고, 몸체는 실루엣이다

바닥은 `y = kGroundY = −1.5` 상수 하나입니다. 그리지 않습니다 — 배경색이 곧 바닥이고, 제품 사진에서 흰 배경에 그림자만 깔린 것과 같이 읽힙니다. 밑면(`y = −1`)에 붙일 수 없는 이유는 X·Z축 레이어를 돌리면 모서리가 `−√2`까지 내려오기 때문입니다. 어느 크기에서도 어느 레이어를 돌려도 성립합니다.

**눈이 바닥으로 내려가면 그림자는 사라집니다.** `shadow()`가 그룹 `opacity`에 눈 높이 계수를 곱합니다 — `eye.y ≥ kGroundY + kShadowFadeBand`에서 1, `eye.y ≤ kGroundY`에서 0, 사이는 선형. 밑에서 올려다보는 물체의 바닥 그림자가 보이지 않는 것은 자연스럽습니다.

**몸체는 caster의 실루엣입니다.** `ViewportPass`가 caster의 화면 convex hull을 `RenderScene::bodies`로 내고, 렌더러가 그림자 다음, 스티커 전에 `kBodyColor`로 그립니다. 그림자 유무와 무관하게 늘 그립니다. 스티커 사이 틈은 배경이 아니라 어두운 플라스틱을 보이고, 회전 중 층 사이로 뒤가 비치는 일이 구조적으로 사라지며, 그림자는 큐브 안쪽에 나타나지 않습니다. 몸체는 조명을 받지 않는 단색입니다 — 틈은 몇 픽셀 폭이라 명암이 보이지 않고, 단색이면 계약이 정확한 byte로 남습니다. 회전 중 드러나는 단면은 조명을 받는 면입니다. `SceneEffect::DropShadow`를 쓰지 않는 이유가 여기서 나옵니다. 그것은 화면 공간에서 2D로 밀어 놓는 그림자라 바닥에 눕지 않습니다.

### 7. 그림자는 그룹 하나이고, 렌더러는 합성 레이어 하나로 완성한다

```cpp
struct RenderShadow {                                // ThorVG 타입 없음
    std::vector<std::vector<math::Vec2>> polygons;   // slab마다 convex hull 하나
    math::Vec2 fade_start;   // 큐브 중심의 바닥 수직 투영
    math::Vec2 fade_end;     // 그림자의 끝: fade 방향으로 가장 먼 꼭짓점
    std::uint8_t opacity;    // 결정 6의 눈 높이 계수를 곱한 값
    float blur_sigma;        // 화면 픽셀, 뷰포트 한 변에 비례
    Rect clip;               // viewport pass가 적는 무대 — 캔버스 전체
};
```

fade 방향·clip·opacity는 다각형이 아니라 **그림자 전체의 속성**입니다. `fade_start`, `fade_end`는 월드 점으로 시작해 꼭짓점과 같은 식으로 화면까지 옵니다. 렌더러는 이렇게 그립니다.

1. **평탄화는 fill rule로.** 모든 다각형을 **한 Shape의 subpath**로 넣고 `FillRule::NonZero`로 채웁니다. 겹친 영역은 winding이 2여도 "안"이라 한 번만 칠해집니다.
2. **fade는 fill입니다.** 그 Shape의 fill이 `fade_start`(alpha = opacity)에서 절반 지점(35%)을 거쳐 `fade_end`(0)로 가는 linear gradient입니다. `|fade_end − fade_start| < kFadeMinLength`면 radial gradient로 대신합니다 — 직상부 광원의 옳은 그림입니다. 도형이 하나이기 때문에 가능한 일입니다.
3. **블러는 유일한 합성 레이어입니다.** 이 Shape를 담은 Scene에 `GaussianBlur(blur_sigma, 0, 0, 30)`을 겁니다. 블러는 외곽선 전체를 한 번에 봐야 하므로 없앨 수 없고, 품질 30(박스 1패스)은 60과 가장자리에서 구별되지 않습니다.
4. **Multiply는 미리 곱합니다.** 바닥은 단색이므로 `Multiply(바닥, 그림자색)`은 색 하나와 같습니다. 렌더러가 배경색과 `kShadowColor`를 프레임마다 한 번 곱해 Normal로 그립니다.
5. **clip은 캔버스 전체입니다.** Shape의 clip은 레이어를 만들지 않습니다. 넷은 그림자 뒤에 그려지므로 스티커는 그림자를 덮고, 스티커 사이로 바닥이 이어져 "넷이 놓인 바닥"으로 읽힙니다.

그리는 순서는 그림자 Scene → 몸체(`bodies`) → (스티커, 그 스티커의 광택)을 depth 순으로 → 축 기즈모입니다. "도형을 `tvg::Scene`에 묶어 효과를 거는" 기능은 [Phase 19.5](./19.5-net-lift-shadow.md)가 넷의 들림 그림자에 재사용합니다.

### 8. 그림자색은 배경에서 유도한다

회색 그림자는 싸 보입니다. 그림자색은 배경보다 어둡고 차가운 `kShadowColor{40, 48, 64}`를 배경에 **곱해서** 만듭니다. Light 배경에서는 푸른 기가 도는 그림자, Dark 배경에서는 더 깊은 자리가 됩니다. 두 theme에서 상수 하나로 성립합니다.

### 9. 렌더 계약은 v4로 개정하고, 조명이 계약을 더 세게 만든다

계약의 목적은 winding, culling, 채널 순서 실수가 "그럴싸한 그림"이 아니라 "틀린 색"으로 드러나게 하는 것입니다. 조명은 이 목적에 보탭니다 — 보이는 세 면이 서로 다른 밝기를 갖게 되어 **노멀 부호가 뒤집히면 밝기가 틀리게 나옵니다.** v3가 잡지 못하던 종류의 실수입니다. 그래서 test에서만 조명을 끄는 우회는 택하지 않습니다.

- 세 면의 기대 색은 `lit(saturate(c), b)`이고, `b`는 `Lighting::standard()`와 홈 카메라에서 계산해 **숫자로 [Phase 4 문서](./04-rubiks-cube-domain.md#rendered-scene-contract-v4)에 적습니다.** 광원이나 ambient를 바꾸면 두 곳을 의도적으로 함께 바꿉니다.
- 허용 오차는 **채널당 ±2**(한 변 256px 미만의 버퍼는 ±4)입니다. 평면 gradient는 px당 약 1/15 byte로 변하고 sample 지점은 스티커 정중앙에서 수십 px 벗어나 있습니다. 노멀 부호 실수는 여전히 수십 단위로 드러납니다.
- 홈 시점 sample 스티커에는 광택이 닿지 않습니다(`light()` test로 고정).
- seam 세 지점은 **`kBodyColor` (70, 74, 82) 정확히**입니다(결정 6).
- 회전 중 단면은 색으로 찾지 않고 **자리로 찍습니다.** 단면이 드러나는 월드 점 `(0.70, 1/3, 0.85)`(U)와 `(1/3, 0.70, 0.85)`(R)을 홈 시점에 투영한 픽셀이 회전 중에는 몸체, 쉴 때는 스티커입니다. 몸체는 **색조**로 알아봅니다 — `kBodyColor`는 어떤 밝기와 어떤 세기의 흰 광택 아래서도 `r ≤ g ≤ b`, `(b − g) = 2(g − r)`, 스프레드 2~16, `r` 40~220을 지킵니다(기울어진 단면은 램프를 정면으로 반사해 alpha 170의 광택을 받을 수 있습니다). e2e의 스크램블 test는 3D 영역의 몸체 픽셀 비율(`bodyShare`)이 쉬는 큐브보다 0.5% 넘게 오르면 회전 중으로 봅니다.
- 궤도로 시점을 돌린 뒤 "어느 면이 어디 있나"를 묻는 test는 색 위에 흰색이 같은 비율로 얹힌 것을 허용하는 비교(`require_pixel_reads`)를 씁니다. 물리적으로 맞는 광택을 없애지 않습니다.

### 10. 흰 스티커는 3D에서만 종이 흰색이다

Classic 흰색은 (255, 255, 255)입니다. 밝기 275인 윗면에서 255로 clamp되면 면이 평평해지고 흰 광택을 얹을 여지가 없습니다 — 실물 흰 플라스틱은 255가 아닙니다. 3D 빌더는 `to_lit_color()`를 씁니다: Classic 흰색만 `kPaperWhite (216, 216, 216)`, 나머지는 `to_color()`와 같습니다. 윗면이 다른 면처럼 gradient 음영을 받고(홈 시점 220~246), 광택이 얹힐 여지가 생깁니다. 넷과 링은 조명을 받지 않으므로 `to_color()`의 255를 그대로 그립니다. High contrast는 어느 쪽도 손대지 않습니다. 계약: 넷의 흰색 255, 3D의 +Y sample `lit(216, 275) = 233`.

### 11. 조명 값은 눈으로 정하고, 정해진 값은 코드에 굽는다

설정 패널에 **Lighting** 그룹이 있습니다. 슬라이더 아홉 개(Ambient, Falloff, Saturation, 키의 X·Y·Z, Diffuse, Specular, Shininess)와 Reset lights, Copy values, 그리고 현재 값을 `?lighting=` 형식으로 보여 주는 한 줄입니다. 슬라이더는 `input`마다 엔진에 쓰고 곧바로 한 프레임을 그립니다.

- **값의 출처는 엔진입니다.** ABI `thorvg_rubiks_lighting_count()`·`thorvg_rubiks_lighting_values(count)`로 현재 조명을 flat list(`ambient, attenuation, saturation, 램프마다 x, y, z, diffuse, specular, shininess`)로 읽습니다. TypeScript에 기본값을 복제하지 않습니다.
- 패널은 첫 램프만 편집하고, `?lighting=`으로 넘긴 다른 램프는 그대로 되돌려 씁니다. `?lighting=`은 두 램프 이상을 시험하는 길로 남고, 첫 렌더 전에 적용되므로 패널이 읽는 값이 곧 그 값입니다.
- Reset은 페이지가 열릴 때의 값으로, Copy는 `Lighting::standard()`에 그대로 굽는 문자열입니다. localStorage에 저장하지 않습니다 — 설정이 아니라 조율 도구이고, 정해진 값은 코드에 들어갑니다.
- 게임 컨트롤러가 아니라 별도 모듈([`LightingControls.ts`](../../web/src/ui/LightingControls.ts))입니다. 조명은 큐브에 대한 명령이 아니라 화면에 대한 것이라 회전 중에도 살아 있습니다.

## 상수 (확정)

[`Light.hpp`](../../engine/src/graphics/Light.hpp)와 [`Palette.hpp`](../../engine/src/graphics/Palette.hpp)의 값입니다.

| 상수 | 값 | 뜻 |
|---|---|---|
| `Lighting::ambient` | 0.75 | palette 색이 "빛을 잘 받는 면"의 색; 밝기는 1을 넘을 수 있음 |
| `Lighting::attenuation` | 1.0 | diffuse에 `(큐브 중심 거리 / 점까지 거리)^1`; 면 안의 gradient를 만드는 항 |
| `Lighting::saturation` | 1.25 | 스티커 채도를 조명 전에 올림; 몸체색과 High contrast에는 적용 안 함 |
| 키 `position` / `diffuse` | `{2.6, 7.0, 4.0}` / 0.38 | 홈 시점에서 +Y가 가장 밝고 +Z, +X 순; 그림자 배율 1.42 |
| 키 `specular` / `shininess` | 0.60 / 12 | 도는 층의 기울어진 윗면에 광택이 보이게; 홈 sample 스티커에는 닿지 않음 |
| `kHighlightMinAlpha` | 8 | 이 아래는 광택을 달지 않음; 타원 reach를 정하는 문턱 |
| `kGlintStops` | 5 | 광택 falloff의 stop 수 |
| `kShadingStops` | 5 | 평면 밝기 함수의 stop 수 |
| `kPaperWhite` | `{216, 216, 216}` | 3D의 Classic 흰색 |
| `kBodyColor` | `{70, 74, 82}` | 몸체 실루엣과 단면; seam 계약값 |
| `kGroundY` | −1.5 | 결정 6 |
| `kShadowColor` | `{40, 48, 64}` | 배경에 곱하는 그림자색 |
| `kShadowOpacity` | 80 | 눈 높이 계수를 곱하기 전 값 |
| `kShadowBlurShare` | 0.018 × 뷰포트 한 변 | GaussianBlur sigma(픽셀) |
| `kShadowBlurQuality` | 30 | 박스 1패스 |
| `kShadowFadeMidway` / `kShadowFadeFloor` | 0.35 / 0 | 끝까지의 절반 지점에 남는 비율 / 끝에서의 비율 |
| `kShadowFadeMinLength` | 4px | 이 아래면 radial fade로 대체 |
| `kShadowFadeBand` | 1.0 (월드) | 눈이 바닥 위 이 높이부터 그림자가 옅어짐 |

홈 시점의 세 평면 밝기는 byte로 **+Y 275, +Z 230, +X 211**이고 키를 등진 세 평면은 191(ambient)입니다. 면 안에서는 +Y 260~290, +Z 220~243, +X 206~219로 변합니다. 단면 평면 여섯 개를 포함한 표는 [Phase 4 문서의 contract v4](./04-rubiks-cube-domain.md#rendered-scene-contract-v4)에 있습니다.

**측정.** 1024×1024 native, 3D 뷰만, R을 45°까지 돌린 프레임, 40프레임의 `render()` median / p95([`RenderBench.cpp`](../../tests/app/RenderBench.cpp), 혼자 돌린 값 — 다른 빌드가 도는 동안 잰 값은 두 배 넘게 부풀려집니다).

| 크기 | 조명 전 | 최종 | 광택 도형 |
|---|---|---|---|
| 3×3 | 0.9 / 1.2 ms | 3.9 / 7.7 ms | 33면 중 3 |
| 9×9 | 1.8 / 2.0 ms | 5.0 / 5.2 ms | 315면 중 9 |
| 28×28 | 6.3 / 7.4 ms | 11.6 / 16.3 ms | 3,108면 중 28 (0.9%) |

그림자는 크기와 무관한 약 2.7ms의 고정 비용이고 그중 블러가 대부분입니다. 광택을 받는 스티커는 28×28에서도 1% 미만이라, 결정 3의 "비율은 N과 무관하게 작다"가 측정으로 확인됐습니다. 벤치는 광택 유무 프레임의 픽셀 차이(바뀐 픽셀 수, 최대 채널 차, `.glint.ppm` 덤프)도 냅니다.

## Scope

- `graphics`: `Lighting`/`Light`, `kGroundY`, `Caster`, `ShadowGroup`, `ShadingOf`(축·stop 색·투영 offset), `HighlightOf`(타원·stop·anchor), `light()`와 `shadow()` 패스, casters를 내는 builder, `ConvexHull`, `to_lit_color()`
- 기존 네 패스가 새 필드를 통과시키는 규칙, `project()`의 stop offset과 anchor 접선, `viewport(rect, stage)`의 bodies와 clip
- `render`: 그림자 Scene(NonZero, gradient fade, 블러, 미리 곱한 색, clip), 몸체, 평면 gradient fill, 면 직후의 타원 광택, variadic 효과 호출의 타입 고정 래퍼
- 렌더러 픽셀 test(native, `SwCanvas` 버퍼)와 벤치
- Rendered scene contract v4: Phase 4 문서, native test, e2e
- Application: 체인, `Lighting` 상태, High contrast의 unlit, 무대, 조명 읽기·쓰기 ABI
- 웹: 설정 패널 Lighting 그룹, `?lighting=`

## Out of scope

- **전개도와 링 다이어그램의 조명.** 평면 표현은 색이 정체인 도식이라 조명이 정보를 해칩니다. 넷의 들림 그림자를 `DropShadow`로 바꾸는 것은 [Phase 19.5](./19.5-net-lift-shadow.md)입니다.
- **바닥 반사.** Blinn–Phong에 반사 항은 없습니다. `reflect(ground)` 패스로 같은 자리에 들어갈 수 있으나, 그림자만으로 바닥의 존재는 충분히 읽힙니다.
- **Ambient mode에서 광원을 돌리는 것.** Phase 10은 관람을 "큐브가 스스로 도는 것"으로 규정했고 카메라 자동 궤도도 out of scope였습니다. 광원 움직임은 그 규정의 확장이라 따로 봅니다.
- **픽셀 단위 조명.** `Picture::load`로 CPU가 채운 라이트맵을 올리는 방법이 있지만 평면 큐브에는 필요가 없습니다.
- **ThorVG 엔진 수정.** SVG의 `feDiffuseLighting`/`feSpecularLighting`을 SceneEffect로 추가하는 것은 upstream 기여 주제입니다.
- **GL/WebGPU 백엔드.** 소프트웨어 엔진만 씁니다.

## Implementation steps

전부 끝났습니다. 각 단계가 무엇을 만들었고 무엇으로 고정되는지만 남깁니다.

### 0. 상수와 타입

`Light.hpp`의 `Lighting`/`Light`와 상수, `Scene.hpp`의 `Caster`·`ShadowGroup`·`ShadingOf`·`HighlightOf`, `RenderScene.hpp`의 `RenderShadow`·`RenderShading`·`RenderHighlight`·`bodies`. 상수는 홈 시점 sample 면의 광택 alpha가 문턱 아래인지, 여섯 palette 색이 명암 양 끝에서 pairwise 구별되는지를 계산으로 확인해 확정했습니다.

### 1. Builder — casters

`build_cube_scene`이 쉬는 큐브에서 박스 하나, 회전 중에는 연속 구간과 정지 구간마다 박스 하나를 냅니다. Test: 쉬는 큐브의 casters는 큐브 껍질 하나; `R`·`Rw`·`M` 회전 중 구간이 겹치지 않고 합치면 N층 전부이며 3개 이하; 0°의 casters의 합집합이 쉬는 큐브와 같음.

### 2. 통과 규칙

`TransformPass`가 casters를, 나머지 세 패스가 그림자 그룹·음영·광택을 변환합니다. `ViewportPass`가 `bodies`와 `clip`을 적습니다. Test: 같은 점을 face 꼭짓점과 그룹의 점으로 넣었을 때 결과가 같음; identity가 아닌 model에서 스티커와 casters가 함께 움직임; cull이 그룹을 지우지 않음; 광택이 anchor 접선으로 펴지고 눈 뒤의 rim이 장애가 되지 않음.

### 3. `light()` 패스

평면 캐시(밝기 stop, 광택 타원)와 스티커 채움. Test: 마주 보는 면이 밝고 등 돌린 면은 ambient; 궤도를 돌려도 월드에서 같은 면이 가장 밝음; 광택은 반사점에 놓이고 타원이 대칭이며 stop이 비증가; 홈 sample 스티커에는 광택 없음; 같은 평면의 스티커가 같은 축을 씀; `unlit()`에서 byte 그대로; 여섯 색이 양 끝에서 구별됨; 홈 시점 세 평면이 계약 byte로 음영짐.

### 4. `shadow()` 패스

꼭짓점 8개 투영 → convex hull, `fade_start`·`fade_end`, 눈 높이 계수. Test: 투영점이 전부 바닥 위; 쉬는 큐브의 그림자는 볼록 다각형 하나; 직상부 광원에서 계산된 배율만큼 확대; 광원보다 높은 큐브는 그림자 없음; 눈이 바닥 아래면 그룹 없음; 도는 층의 그림자가 함께 돎.

### 5. 렌더러와 픽셀 test

[`LightingRenderTest.cpp`](../../tests/app/LightingRenderTest.cpp): 겹친 slab 자리와 단일 slab 자리의 RGB가 같음; fade 방향의 luminance가 단조; 블러가 가장자리를 부드럽게; clip 밖은 배경색; 몸체가 틈으로 보이고 그림자는 큐브 안쪽에 없음; 광택은 자기 면만 밝히고 앞면을 밝히지 않음; 두 theme에서 그림자 자리가 배경보다 어두움; 세기 0의 그림자는 아무것도 그리지 않음.

### 6. Application, ABI, 웹

체인에 두 패스 삽입, `Lighting` 상태, High contrast의 `unlit()`, 무대, `lighting_buffer`/`set_lighting`/`lighting_count`/`lighting_values` ABI, `CubeEngine.setLighting`/`lighting`, `?lighting=` 파싱, 설정 패널 Lighting 그룹(`LightingControls.ts`, unit·e2e test).

### 7. 계약과 문서

Phase 4 문서에 contract v4, native와 e2e의 기대값, 이 문서와 [IMPLEMENTATION_PLAN.md](./IMPLEMENTATION_PLAN.md), [DESIGN.md](../DESIGN.md)의 파이프라인 예시.

## Acceptance criteria

- 광원을 마주 보는 면이 밝고 등을 돌린 면이 어두우며, 한 면 안에서 광원 쪽이 더 밝고, 여섯 스티커 색은 명암 전 구간에서 pairwise 구별됩니다(test).
- 카메라를 궤도로 돌려도 가장 밝은 면은 월드에서 같은 면입니다(test).
- 같은 평면의 스티커는 값도 기울기도 seam을 넘어 이어지고, 광택은 스티커 경계와 무관하게 하나의 타원으로 이어지며, 면 직후에 그려져 뒤쪽 면의 광택이 앞면 픽셀을 바꾸지 않습니다(test, 픽셀 test).
- 바닥 그림자는 큐브 아래에 눕고, 레이어를 돌리면 튀어나온 slab의 그림자가 함께 돌아가며, 어떤 레이어 회전에서도 큐브가 바닥을 뚫지 않습니다.
- 겹친 slab과 단일 slab 자리의 그림자 RGB가 같고, fade 방향의 luminance가 단조이며, 큐브 실루엣 안쪽은 몸체 또는 스티커 색입니다(픽셀 test).
- 눈이 바닥 아래로 내려가면 그림자가 사라지고, 다시 올라오면 돌아옵니다. 몸체는 어느 시점에서도 그려집니다.
- Light와 Dark theme 모두에서 그림자 자리가 배경보다 어둡습니다(픽셀 test).
- 현재 mask 생성 경로에서 caster는 3개 이하이고, 비연속 mask에서도 `구간 수 × 2 + 1` 이하입니다(test).
- 조명과 그림자의 비용이 1024² native에서 3×3 고정 비용 3ms 이하, 28×28 조명 전후 차이 6ms 이하이고, 광택 도형 수는 스티커의 15% 이하입니다(측정 기록).
- 전개도와 링 다이어그램은 픽셀 하나도 바뀌지 않습니다.
- Rendered scene contract가 v4로 개정되어 Phase 4 문서, native test, e2e가 같은 숫자를 보고, 노멀 부호를 뒤집으면 v4 test가 실패하며, seam 세 지점은 `kBodyColor`입니다.
- 설정 패널의 Lighting 슬라이더가 엔진의 현재 값을 보이고, 움직이면 쉬는 큐브가 곧바로 다시 그려지며, Reset이 열 때의 값으로 돌립니다(unit, e2e).
- Native test, WASM 빌드, TypeScript unit, e2e, production build가 모두 통과합니다.

## 개정 기록

계획대로 되지 않은 자리를 시간 순으로 적습니다. 결정 절은 최종 상태라 이 과정이 보이지 않기 때문입니다.

**초안 리뷰 (구현 전).** 외부 리뷰가 지적한 것은 방향이 아니라 계약의 빈 곳이었습니다. 광원의 좌표계(뷰 공간에서 월드 좌표를 그대로 쓰면 광원이 카메라에 붙어 돔), 그림자의 fade·clip·opacity가 다각형이 아니라 그룹의 속성이라는 것, casters도 transform을 받아야 한다는 것, 바닥 아래로 내려가는 카메라, 그리고 상수가 정해지기 전에는 v4 숫자를 낼 수 없다는 순서입니다.

**1차 구현 (2026-09-03).** 계획과 다르게 된 곳이 다섯입니다. (1) 스티커 중심에서 읽은 diffuse는 한 면을 여덟 가지 byte로 만들어, 평면이 큐브 중심에 가장 가까운 점에서 읽게 됐습니다. (2) 광원 출발값에서 +Z 밝기가 반올림 경계에 0.06 차이로 걸려 격자 탐색으로 옮겼습니다. (3) 홈 시점에는 기하학적으로 광택이 놓일 수 없음을 확인했습니다. (4) 시점을 돌린 뒤의 면 확인 test는 실제 광택을 만나 글린트 허용 비교로 바꿨습니다. (5) `Scene::opacity()`, fade mask, occluder mask, `Multiply` blend의 합성 레이어 넷이 3×3 프레임을 1.2ms에서 6.5ms로 만들어, NonZero fill rule·gradient fade·미리 곱한 색·배경색 실루엣으로 레이어 하나(블러)만 남겼습니다. 초안의 acceptance "median 증가 15% 이하"는 어떤 합성 레이어에도 성립할 수 없는 기준이라 절대 예산(3ms)으로 바꿨습니다.

**2차 — 첫 화면.** 그림자가 뷰포트 경계에서 잘렸고(clip을 무대로, 끝을 0으로, 광원을 올림), 광택이 스티커마다 따로 놓여 디지털처럼 보였고(반사점을 평면의 것으로, 반지름을 문턱까지 계산), 면 안에서 밝기가 변하지 않았습니다(스티커마다 linear gradient, 계약 ±1). 측정이 알려 준 것: 반지름 탐색은 평면당 한 번이어야 하고(면마다 하면 28×28이 13 → 19ms), 흰색의 Screen blend는 alpha 합성과 같은 식이라 레이어가 필요 없고, byte가 안 바뀌는 스티커는 단색으로 그려도 같은 그림입니다.

**3차 — 데모.** 광원을 올리자 면 안의 gradient가 3 byte 이내로 줄어 다시 평평해졌고, 밝기 최대가 1이라 모든 스티커가 palette보다 어두워 색이 빠져 보였고, 광택은 어디에도 없었습니다. 거리 감쇠를 넣었고, ambient 0.75에 밝기가 1을 넘을 수 있게 했고, 제품 사진처럼 키 + 킥커 두 램프를 두었고, 값을 눈으로 맞추도록 `?lighting=`을 만들었습니다. 계산이 알려 준 것: 먼 램프는 면을 통째로 광택으로 덮고 국소적인 반짝임은 가까운 램프만 만듭니다. gradient는 양 끝 두 stop이면 굽은 곡선을 현으로 잘라 계약에서 2 벗어나므로 중앙 stop이 필요합니다.

**4차.** 스티커 단위 gradient는 값은 이어져도 **기울기가 스티커마다 달라** 타일처럼 보였습니다 — 밝기 함수를 평면에 하나(축 하나, stop 다섯) 두고 스티커는 자기 색만 채우게 했습니다. 회전 중 틈으로 뒤가 비쳐, 그림자를 지우려던 배경색 실루엣을 몸체색으로 바꿔 몸체로 삼았습니다. 틈을 없애고 외곽선을 그리는 안은 원근이 없고 28×28에서 stroke 3천 개가 붙어 기각했습니다. 계약 허용은 ±2로 넓혔고, 틈을 잠시 `kSeamColor`로 어둡게 칠했습니다(단면을 색으로 찾는 test가 seam에 속아서).

**5차.** 몸체가 그림자 그룹에 묶여 있어 눈이 바닥 아래로 가면 함께 사라졌습니다 — `bodies`로 독립시켜 늘 그리게 했고, 색은 단면과 같은 `kBodyColor`로 돌렸습니다. 단면 검출은 색이 아니라 자리로 찍게 했고, probe 점은 홈 시점에서 광선을 쏘아 골랐습니다. 킥커는 어디에 두어도 어색해 뺐고(키의 specular는 남김), 대신 채도를 1.25배 올렸습니다. `unlit()`은 채도도 1로 돌려야 함을 e2e의 palette test가 알려 줬습니다.

**6차.** 큐브 밑에 그림자가 하나 더 보였습니다 — 접촉 그림자였고, 투영 그림자와 겹쳐 이중으로 읽혀 뺐습니다. 광택이 더 눈에 띄도록 specular를 0.60 / 12로 넓혔고(쉬는 홈 시점에는 여전히 없음, 도는 층의 윗면에서 peak 59), `?lighting=`은 불편해 설정 패널의 Lighting 그룹으로 옮겼습니다. 넓힌 lobe의 rim이 near plane 뒤로 넘어가 `project()`가 광택을 통째로 버리는 것을 벤치("하이라이트 3개, 바뀐 픽셀 0")가 알려 줬습니다.

**7차.** 높은 시점에서 광택이 가까운 네 스티커만 균일하게 덮고 바깥 다섯은 비어, 각도를 바꾸면 스티커 단위로 켜졌다 꺼졌습니다. 원인이 둘이었습니다. 붙이는 판정(3D 원)과 그리는 gradient(화면 원)가 다른 도형이었고, 발자국을 rim 네 점의 투영으로 affine 맞춤하면 perspective가 큐브 쪽을 압축해 lobe의 안쪽이 스티커에 그려졌습니다. 발자국을 네 방향 reach의 타원으로, 판정을 타원 좌표계로, 화면 사상을 anchor의 투영 접선으로 바꿨습니다. 흰 윗면은 255에서 clamp되어 광택을 얹을 수 없어 종이 흰색 216을 두었는데, 그러자 음영 gradient의 stop이 화면에서 등간격으로 놓여 있던 perspective 오차(윗면 sample이 5 어둡게)가 드러나 stop마다 투영된 offset을 쓰게 됐습니다. 광택이 닿는 단면을 몸체로 알아보려면 밝기 표가 아니라 색조가 필요했고, e2e의 "회전 중" 판정은 격자 셀 수 대신 픽셀 비율이 필요했습니다.

**8차 (2026-09-04).** 종이 흰색이 넷에서는 탁해 3D만 `to_lit_color`로 쓰게 했고, 그림자가 넷 윗변에서 직선으로 잘리는 것이 눈에 띄어 무대를 캔버스 전체로 했습니다.

**이 phase에서 배운 것.**
- ThorVG의 gradient는 화면 공간에서 affine입니다. 평면 위의 함수를 gradient로 옮길 때 perspective는 두 번 문제를 냈고(음영 stop의 위치, 광택 타원의 형태), 두 번 다 답은 "보이는 자리에서 투영을 선형화하라"였습니다. 넓은 도형을 네 점으로 맞추면 안 됩니다.
- 스티커 단위로 보이는 문제는 스티커 단위로 계산해서가 아니라, 판정과 그리기가 다른 도형이거나 함수가 스티커마다 다를 때 생겼습니다. 답은 늘 "평면에 하나".
- 합성 레이어는 각각 그림자 발자국 전체를 한 번 더 훑습니다. 같은 그림을 fill과 fill rule로 낼 수 있으면 그쪽이 맞고, 측정 없이는 알 수 없었습니다.
- test는 질문이 좁으면 물리적으로 맞는 결과를 실패로 보고합니다(광택이 닿은 단면, 회전 중의 seam). 그때 고칠 것은 그림이 아니라 질문이었습니다.
- 값은 눈으로 정해야 했습니다. 추정 → 빌드 → 계약 재계산의 반복은 손잡이(`?lighting=`, 그 뒤 슬라이더) 하나로 끝났습니다.
- 벤치는 혼자 돌려야 합니다. 백그라운드 빌드가 도는 동안의 측정은 두 배 넘게 부풀려졌습니다.

## Verification commands

```bash
meson test -C build/native --print-errorlogs
```

```bash
source /path/to/emsdk/emsdk_env.sh && ./build_wasm.sh
```

```bash
npm --prefix web run test:unit
```

```bash
npm --prefix web run test:e2e
```

```bash
npm --prefix web run build
```

```bash
build/native/tests/app/render-bench 3 40
```

## Completion

모든 acceptance criteria와 verification command를 통과했습니다.

- 이 문서의 status는 `Completed`입니다.
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 19가 완료 처리되어 있습니다.
- 실제 구현과 차이가 생긴 결정은 위 개정 기록에 있습니다.
