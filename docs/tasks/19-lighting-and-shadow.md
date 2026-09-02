# Phase 19: Lighting and shadow

## Status

`Planned` — 설계만 있고 코드는 아직 없습니다.

초안에 외부 리뷰를 받아 한 차례 개정했습니다. 개정된 자리는 지우지 않고 **"개정"** 표시와 함께 무엇이 왜 틀렸는지를 남겼습니다. 리뷰가 지적한 것은 방향이 아니라 계약의 빈 곳이었습니다 — 광원의 좌표계, 그림자 그룹의 속성이 다각형에 붙어 있던 것, 바닥 아래로 내려가는 카메라, 그리고 상수가 정해지기 전에는 v4 계약 숫자를 낼 수 없다는 순서입니다.

Phase 17(Solve hint and step-through)과 Phase 18(Paint your cube) 어느 쪽도 필요로 하지 않고, 어느 쪽에도 영향을 주지 않습니다. 번호는 순서가 아니라 자리입니다.

## Objective

**3D 큐브에 광원을 둡니다.** 면마다 빛을 받는 정도가 다르고, 광원을 마주 보는 면에는 하이라이트가 맺히고, 큐브 아래 바닥에는 광원 위치와 큐브 자세를 따르는 그림자가 눕습니다. 레이어를 돌리면 튀어나온 레이어의 그림자가 함께 돌아갑니다.

이 phase는 기능이라기보다 **ThorVG showcase**입니다. ThorVG는 셰이더를 노출하지 않는 2D 벡터 엔진인데, 그 위에서 Blinn–Phong 조명과 투영 그림자를 어떻게 내는지 보여 주는 것이 목적입니다. 그래서 구현 방식을 고를 때 "가장 짧은 코드"보다 **ThorVG의 합성 기능(Scene 합성, SceneEffect, blend, gradient, mask, clip)이 각각 제 역할을 하는 구성**을 택합니다.

계산 모델은 Blinn–Phong의 ambient + diffuse + specular 세 항입니다. 반사(reflection) 항은 Blinn–Phong에 없고, 이 phase에도 없습니다.

## 무엇이 이미 있고, 무엇이 없는가 (확인함)

### 파이프라인은 패스를 끼울 자리를 이미 갖고 있습니다

[`Pipeline.hpp`](../../engine/src/graphics/Pipeline.hpp)의 `operator|`는 왼쪽 scene에 오른쪽 callable을 적용하는 문법일 뿐이고, 왼쪽에 올 수 있는 타입은 `is_pipeline_value`로 WorldScene, ViewScene, ClipScene, RenderScene 넷이 열려 있습니다. 새 패스는 이 넷 중 하나를 받아 같은 타입을 돌려주면 등록 없이 체인에 들어갑니다. 현재 체인은 [`Application.cpp:1002`](../../engine/src/app/Application.cpp) 한 곳입니다.

### 기존 패스는 `faces`만 만집니다

[`TransformPass`](../../engine/src/graphics/Passes.cpp)는 `scene.faces`의 꼭짓점만 model 행렬로 변환하고, `ViewPass`, `ProjectPass`, `ViewportPass`도 마찬가지입니다. 새 필드(casters, shadows, highlight)는 **네 패스 모두**에 통과 규칙을 적어야 합니다. 초안은 view/project/viewport만 적고 transform을 빼먹었는데, model이 지금 identity라 숨겨질 뿐인 누락이었습니다(개정, 결정 1).

`ViewportPass`는 `rect`를 받아 좌표를 옮기고 **rect 자체는 결과에 남기지 않습니다.** 그림자를 뷰포트로 자르려면 clip 사각형을 결과에 실어 보내야 합니다(결정 7).

### 면에는 색과 꼭짓점만 있고 노멀이 없습니다

[`Scene.hpp`](../../engine/src/graphics/Scene.hpp)의 WorldFace, ViewFace, ClipFace와 [`RenderScene.hpp`](../../engine/src/graphics/RenderScene.hpp)의 RenderFace는 모두 `points`와 `color`뿐입니다. 노멀은 저장하지 않아도 됩니다 — 면이 평면이라 `cross(p1 - p0, p3 - p0)`로 매 프레임 구할 수 있고, 뷰 공간에서는 눈이 원점이라 시선 벡터도 `-p`로 공짜입니다. 방향 규약은 [`CullPass`](../../engine/src/graphics/Passes.cpp)가 이미 정해 두었습니다: NDC에서 반시계 방향이 앞면입니다. 노멀의 부호는 이 규약에서 유도하고 test로 고정합니다.

### 그려지는 것은 스티커이고, 큐비 몸체는 없습니다

[`build_cube_scene`](../../engine/src/graphics/CubeGeometry.hpp)은 바깥 스티커만 냅니다. 스티커는 셀의 `kStickerScale = 0.92`배라 사이에 틈이 있고, 회전 중 드러나는 단면은 `kBodyColor`로 채워집니다. **그려지는 면을 그대로 바닥에 투영하면 그림자에 격자 무늬가 납니다.** 그림자는 다른 재료가 필요합니다(결정 4).

틈은 렌더 계약이 "배경색"으로 검사하는 자리이기도 합니다. 실물이라면 틈 너머는 큐비 몸체이지 바닥이 아니므로, **바닥 그림자가 틈으로 비치면 그것은 물리가 아니라 스티커-only 모델의 artifact입니다**(개정, 결정 6·9).

### `LayerMask`는 비트마스크입니다

[`CubeMove.hpp`](../../engine/src/cube/CubeMove.hpp)의 `LayerMask`는 `uint32_t`라 타입은 비연속 층 집합을 허용합니다. 다만 지금 mask를 만드는 경로는 [`depth_layers(face, first, last, size)`](../../engine/src/cube/Surface.hpp) 하나이고 항상 연속 구간을 냅니다. caster 개수 상한은 이 둘을 구분해 적습니다(결정 4).

### 렌더러는 단색 fill만 씁니다

[`ThorVGSoftwareRenderer.cpp`](../../engine/src/render/ThorVGSoftwareRenderer.cpp)는 `SwCanvas`에 Shape를 바로 push하고 `fill(r, g, b, a)`와 stroke만 씁니다. Scene, gradient, blend, mask, SceneEffect는 한 번도 쓰이지 않았습니다. 배경색은 UI polish 작업에서 `Renderer` 경계의 값이 되어 Light(231, 235, 240)와 Dark(32, 32, 32) 두 값을 받습니다. **그림자 색은 이 두 배경 위에서 모두 성립해야 합니다.**

### ThorVG 1.0.0이 필요한 API를 전부 갖고 있습니다

vendored [`subprojects/thorvg`](../../subprojects/thorvg/meson.build)는 1.0.0이고 [공개 헤더](../../subprojects/thorvg/inc/thorvg.h)에 다음이 있습니다. 엔진 수정은 필요 없습니다.

| API | 이 phase에서의 역할 |
|---|---|
| `Scene::opacity()` | 겹치는 그림자 조각을 평탄화한 뒤 한 번만 반투명하게 |
| `Scene::add(SceneEffect::GaussianBlur, double sigma, int direction, int border, int quality)` | 반그림자(penumbra) |
| `Paint::blend(BlendMethod::Multiply / Screen)` | 그림자는 바닥을 어둡게, 하이라이트는 스티커를 밝게 |
| `RadialGradient::radial(cx, cy, r, fx, fy, fr)` | 스페큘러 하이라이트의 형태, 접촉 그림자, 퇴화한 fade |
| `LinearGradient` + `Paint::mask(MaskMethod::Alpha)` | 큐브에서 멀어질수록 옅어지는 그림자 |
| `Paint::mask(MaskMethod::InvAlpha)` | 큐브 실루엣 안쪽(틈 포함)에서 그림자를 지움 |
| `Paint::clip()` | 그림자가 큐브 뷰포트 밖 넷 뷰를 침범하지 않게 |

`SceneEffect::add`는 **variadic**이라 인자 개수와 타입이 헤더 주석과 정확히 같아야 합니다. GaussianBlur는 `(double, int, int, int)` 넷이고, `float`을 넘기면 undefined behavior입니다. 렌더러는 이 호출을 인자 타입이 고정된 함수 하나로 감쌉니다.

`SceneEffect::DropShadow`도 있지만 쓰지 않습니다(결정 6).

### 렌더 계약이 스티커 색을 byte 단위로 검사합니다 — 이 phase가 깨뜨립니다

[`RenderContractTest.cpp:151`](../../tests/app/RenderContractTest.cpp)은 solved 3×3의 보이는 세 면 중앙 픽셀이 palette 색과 **정확히 같다**고 단언하고, 세 seam 픽셀이 배경색과 정확히 같다고 단언합니다. 브라우저 e2e([`sceneContract.ts`](../../web/tests/e2e/sceneContract.ts))도 같은 지점을 같은 값으로 봅니다. 계약의 현재 판은 [Phase 4의 contract v3](./04-rubiks-cube-domain.md#rendered-scene-contract-v3)입니다.

조명은 그 세 면의 색을 바꾸는 것이 목적이므로 **v3 그대로는 통과할 수 없습니다.** 우회(test에서만 조명을 끄는 것)가 아니라 개정으로 갑니다(결정 9).

이 test가 실제 `SwCanvas` 픽셀 버퍼를 읽는다는 사실은 반대로 기회이기도 합니다. **렌더러의 합성 결과(평탄화, 블러, fade, clip, 하이라이트 순서)를 native test에서 픽셀로 검증할 수 있습니다.** 초안은 렌더러를 수동 스크린샷에만 맡겼는데 그럴 이유가 없었습니다(개정, Step 5).

### 전개도에는 이미 2D 그림자가 있습니다

[`NetGeometry.cpp:685`](../../engine/src/graphics/NetGeometry.cpp)는 들린 조각 아래에 어두운 사각형을 평행이동해 깔고, 농도는 `ActiveRotation::opening`을 따릅니다. ThorVG 효과 없이 RenderScene의 face로 표현한 선례이고, 이 phase는 그것을 바꾸지 않습니다([Phase 19.5](./19.5-net-lift-shadow.md)가 다룹니다).

### 카메라는 위에서 내려보지만, 바닥 아래까지 내려갈 수 있습니다

홈 시점은 `kHomeEye{3, 3, 3}`으로 고도각 약 35°이고 바닥 그림자가 잘 보이는 각도입니다. 그런데 [`OrbitCamera`](../../engine/src/graphics/OrbitCamera.hpp)의 pitch 한계는 `kPitchLimitDegrees = 80`이라 궤도 반지름 √27에서 눈은 `y ≈ −5.12`까지 내려갑니다. **바닥 `y = −1.5`는 pitch 약 −16.8°에서 이미 지나갑니다.** 초안은 "바닥 높이까지 내려가면 띠로 얇아진다"라고 적어 바닥 아래를 생각하지 않았습니다(개정, 결정 6).

## Architecture decisions

### 1. 조명은 패스이고, 렌더러는 시킨 것만 그린다

`light(light, camera)`는 `ViewScene -> ViewScene`, `shadow(light, ground)`는 `WorldScene -> WorldScene`입니다. 각자 계산이 가장 싼 좌표계에 놓입니다 — 바닥 평면은 월드에 정의되고, 시선 벡터는 뷰 공간에서 공짜입니다.

```cpp
scene = build_cube_scene(state, active, palette)
      | transform(model)          // faces와 casters 둘 다
      | shadow(light, ground)     // WorldScene: casters → shadow group (월드 광원, 월드 바닥)
      | view(camera)              // faces와 shadow group 둘 다
      | light(light, camera)      // ViewScene:  광원을 뷰 공간으로 옮겨 face.color와 highlight 계산
      | project(camera)
      | cull()
      | depth_sort()
      | viewport(rect);           // shadow group에 clip = rect를 적는다
```

**개정 — 광원의 좌표계.** 초안은 `light(light)`가 뷰 공간에서 돌면서 월드 좌표의 `Light::position`을 그대로 썼습니다. 그대로 구현하면 카메라를 돌릴 때 광원이 카메라에 붙어 따라 돌아, 그림자(월드)와 하이라이트(뷰)가 서로 다른 광원을 보게 됩니다. `light()`는 생성 시 `camera.view()`로 광원 위치를 뷰 공간으로 옮겨 들고 있습니다. `shadow()`는 월드 공간에서 돌므로 월드 좌표 그대로 받습니다. 두 패스가 **같은 `Light` 값**을 받고 각자 자기 좌표계로 옮긴다는 것이 계약이고, 궤도 카메라에서 하이라이트 반사점과 그림자 방향이 같은 광원에서 나온다는 test로 고정합니다.

**개정 — casters도 transform을 받습니다.** `TransformPass`가 `casters`의 여덟 꼭짓점도 변환합니다. model이 identity가 아닐 때 스티커와 그림자가 함께 움직인다는 test를 둡니다.

RenderScene은 여전히 ThorVG 타입을 모릅니다. 조명 결과는 `Color`와 기하(하이라이트 중심·반지름·세기, 그림자 그룹)로만 전달되고, 그것을 gradient와 blend로 옮기는 결정은 렌더러가 합니다. 이 경계는 [DESIGN.md](../DESIGN.md)의 "ThorVG는 파이프라인의 마지막 backend" 원칙 그대로입니다.

### 2. Diffuse는 면 하나에 값 하나다 — 근사가 아니라 평면의 성질

ThorVG의 `fill(r, g, b, a)`는 경로 안을 한 색으로 칠합니다. GLSL처럼 픽셀마다 다른 값을 낼 수 없습니다. 그런데 큐브의 면은 평면이라 N이 면 전체에서 같고, 확산항 N·L도 방향광이면 정확히 상수입니다. 점광원이면 L이 면 위에서 조금 달라지는데, 스티커 한 장의 크기에서 그 차이는 눈에 보이지 않으므로 **면 중심에서 한 번 계산**합니다.

```
I      = ambient + diffuse * max(dot(N, L), 0)
color' = (color * round(I * 255) + 127) / 255     // 채널별 정수 연산, 255로 clamp
```

정수 반올림을 공식에 박아 두는 이유는 렌더 계약 v4가 byte 단위 일치를 요구하기 때문입니다(결정 9). `I`는 `ambient + diffuse ≤ 1`이 되게 상수를 잡아 clamp가 실제로는 일어나지 않게 합니다.

**ambient는 낮출 수 없습니다.** 스티커 색은 상태를 나타내는 정체(빨강·주황·흰·노랑·초록·파랑)이고, 특히 Phase 18에서 사용자는 실물과 화면을 대조합니다. 어두운 쪽 면에서 빨강과 주황이 섞이면 조명이 아니라 결함입니다. `kAmbient`는 0.55 이상으로 두고, **여섯 색이 명암의 양 끝에서도 서로 구별된다는 것을 test로 고정합니다.**

**개정 — High contrast palette에서 조명을 끄는 값.** 초안은 `ambient = 1, specular = 0`을 권했는데, 그러면 diffuse 항이 남아 `I > 1`이 되어 오히려 더 밝아집니다. 조명을 무력화하는 값은 `ambient = 1, diffuse = 0, specular = 0`입니다. 이 값에서 `color' == color`가 byte 단위로 성립하는지도 test합니다.

### 3. Specular만 gradient를 쓴다 — 형태가 맞기 때문

하이라이트 (N·H)^n은 한 점에서 최대이고 사방으로 빠르게 떨어지는 봉우리라, radial gradient의 형태와 거의 같습니다. 면 전체에 상수로 곱하면 면이 균일하게 하얘져 하이라이트로 읽히지 않으므로, **면마다 `Highlight`를 데이터로 싣고** 렌더러가 같은 경로를 한 번 더 그려 흰색→투명 radial gradient를 `Screen`으로 얹습니다.

봉우리의 위치는 추측하지 않고 계산합니다. 점광원 L을 면의 평면에 대칭시킨 L'과 눈 E를 잇는 선분이 평면과 만나는 점이, 빛이 정확히 눈으로 반사되는 점(H = N)입니다. 그 점이 면 안이면 그대로 중심이고, 밖이면 면 위의 가장 가까운 점으로 당긴 뒤 **그 자리에서** (N·H)^n을 다시 계산해 alpha로 씁니다. 그러면 광원이 움직일 때 하이라이트가 면 위를 미끄러지다 가장자리에서 자연스럽게 사라집니다.

**개정 — 반지름의 단위와 원근.** 초안은 `center`만 변환하고 scalar `radius`를 그대로 들고 갔는데, 그 반지름이 어느 공간의 값인지, 원근에서 어떻게 줄어드는지가 없었습니다. 계약은 다음과 같습니다.

- 하이라이트 lobe의 크기는 스티커가 아니라 **평면의 성질**입니다(광원·눈 거리와 `shininess`가 정합니다). 그래서 반지름은 월드 단위 상수 `kHighlightRadius`(큐브 반폭 기준)이고 N에 무관합니다. 스티커는 그 lobe를 자기 경로로 잘라 보이는 것뿐입니다.
- `Highlight`는 `center`와 함께 **`rim` 한 점**을 싣습니다. `rim = center + kHighlightRadius * u` 이고 `u`는 면 평면 안의 단위 벡터(면의 한 변 방향)입니다. project와 viewport는 `rim`을 꼭짓점과 같은 식으로 투영하고, 렌더러는 `|rim - center|`를 화면 반지름으로 씁니다. 원근에 따라 반지름이 정확히 줄어듭니다.
- 기울어진 면 위의 원은 화면에서 타원이 되는데, **원으로 근사하는 것은 의도**입니다. 스티커 한 장 위의 하이라이트는 작아 타원과 원의 차이가 보이지 않고, 두 축을 실으면 세 단계 타입에 필드가 둘 더 늘어납니다. 근사임을 여기 적어 둡니다.

**개정 — 그리는 순서.** 초안은 스티커를 전부 그린 뒤 하이라이트를 전부 그리기로 했습니다. 회전 중 큐브는 볼록하지 않아, 뒤에 있는 면의 하이라이트가 앞면 위로 샙니다. 렌더러는 depth_sort된 순서로 **면 하나를 그리고 바로 그 면의 하이라이트를 그립니다.** 하이라이트는 `RenderFace::highlight`에 붙어 있으므로 순서를 따로 관리할 것이 없습니다.

alpha가 문턱값(`kHighlightMinAlpha`) 아래인 면은 Highlight를 달지 않습니다. 하이라이트를 받는 스티커 수는 lobe가 덮는 큐브 면의 비율에 비례하므로 **N²에 비례하되 기본 비용(스티커 6N²)에 대한 비율은 N과 무관**합니다. 이 주장은 Step 6에서 28×28로 잽니다.

### 4. 그림자를 드리우는 것은 스티커가 아니라 층 묶음 박스다

스티커는 틈이 있고 몸체가 없어 투영하면 격자가 됩니다. 큐비 하나씩 박스로 투영하는 안은 3×3에서는 되지만 **N×N에서 성립하지 않습니다** — 28×28은 큐비 21,952개입니다.

그래서 caster는 **층 묶음(slab) 박스**입니다. 세 축으로 나누는 것이 아닙니다 — **지금 도는 축 하나로만, 그것도 회전 중일 때만** 나눕니다.

- 쉬는 큐브는 박스 하나, 큐브 전체입니다.
- 회전 중이면 `ActiveRotation::axis` 방향으로 `layers`의 연속 구간마다 박스 하나(회전을 받음), 그 사이에 남는 정지 구간마다 박스 하나입니다. 다른 두 축의 층은 그 회전에서 움직이지 않으므로 나눌 이유가 없습니다. 회전은 한 번에 하나뿐입니다(`optional<ActiveRotation>`).

```text
3×3, X축 층을 [L M R]이라 할 때
R  : [L M] 정지 + [R] 회전            → 2개
Rw : [L] 정지   + [M R] 회전          → 2개
M  : [L] 정지   + [M] 회전 + [R] 정지 → 3개
```

각 박스는 회전축 방향 범위만 그 구간이고 나머지 두 방향은 큐브 전체 폭인 직육면체입니다. 볼록한 박스들의 합집합이 큐브(회전 중 모양 포함)이므로 그림자의 합집합도 큐브의 그림자와 같습니다.

**개정 — 개수 상한.** 박스 수는 `(mask의 연속 구간 수) × 2 + 1` 이하입니다. 지금 mask를 만드는 유일한 경로 `depth_layers`는 연속 구간 하나만 내므로 **현재 생성 경로에서는 3 이하**이고, 이것을 test로 고정합니다. `LayerMask`가 비트마스크라 비연속 mask가 들어올 수는 있고 그때도 규칙은 성립합니다 — 상한만 구간 수에 비례해 늘어납니다. 초안의 "한 자릿수"는 틀린 말은 아니었지만 계약으로는 느슨했습니다.

박스는 꼭짓점 8개로 충분합니다. 볼록한 박스의 그림자는 볼록 다각형 하나라서, `shadow()`는 여섯 면을 따로 투영하지 않고 **꼭짓점 8개를 투영한 뒤 2D convex hull**을 잡아 박스마다 다각형 하나를 냅니다. 박스 안에서는 겹침이 없고, 박스 사이의 겹침만 렌더러의 Scene 평탄화가 처리합니다.

박스는 `build_cube_scene`이 스티커와 같은 회전을 적용해 `WorldScene::casters`로 냅니다. 스티커를 회전시키는 코드가 이미 그 자리에 있어서, casters가 스티커와 어긋날 방법이 없습니다. casters는 그려지지 않습니다 — `shadow()`와 결정 6의 occluder만 읽습니다.

**개정 — 0°의 casters.** 회전 각도 0°에 `ActiveRotation`이 있으면 casters는 쉬는 큐브의 박스 하나가 아니라 나눠진 박스 둘 이상입니다. 초안의 test "0°의 casters가 쉬는 큐브와 같음"은 벡터가 아니라 **기하 합집합**이 같다는 뜻으로 고쳐 적습니다.

### 5. 광원은 점광원 하나이고, 조명과 그림자가 공유한다

방향광은 그림자를 평행이동만 시켜서 빛이 있다는 것이 잘 보이지 않습니다. 점광원은 광원이 움직이면 그림자가 늘어나고 기울어지고 하이라이트가 면 위를 이동하므로, showcase에는 점광원이 맞습니다. 투영은 다음과 같습니다.

```
t  = (ground_y - L.y) / (P.y - L.y)
P' = L + (P - L) * t          // 박스의 어느 꼭짓점이든 P.y >= L.y 이면 그 박스는 버린다
```

**개정 — 직상부 광원의 그림자는 발자국이 아닙니다.** 초안의 test "광원이 바로 위일 때 그림자는 발자국과 일치"는 이 공식과 모순됩니다. 유한 높이의 점광원은 직상부에서도 그림자를 확대합니다. 높이 `Py`의 꼭짓점은 수평으로 `(L.y − ground_y) / (L.y − Py)` 배 늘어나므로, `L.y = 4.5`, `ground_y = −1.5`, 윗면 `Py = 1`이면 배율은 `6 / 3.5 ≈ 1.71`입니다. test는 "중심과 축은 일치하고, 각 꼭짓점이 계산된 배율만큼 확대된다"로 씁니다. 발자국과 같아지는 것은 방향광의 극한뿐입니다.

광원 위치는 월드 좌표로 `ApplicationState`가 하나 들고, `light()`와 `shadow()`가 같은 값을 받습니다(결정 1). 기본값은 카메라 홈 시점 기준 위쪽 앞이라 첫 화면에서 그림자와 하이라이트가 둘 다 보입니다. 정확한 값은 Step 0에서 결정 9의 제약(계약 sample 지점이 하이라이트 밖)을 만족하도록 확정합니다.

### 6. 바닥은 보이지 않고, 큐브는 떠 있다

바닥은 `y = kGroundY` 상수 하나입니다. 그리지 않습니다 — 배경색이 곧 바닥이고, 제품 사진에서 흰 배경에 그림자만 깔린 것과 같이 읽힙니다.

**바닥을 큐브 밑면에 붙일 수 없습니다.** X축이나 Z축 레이어를 돌리면 아래 큐비의 모서리가 회전축에서 `h√2`까지 내려오므로, 밑면에 붙인 바닥을 뚫습니다.

| 항목 | 값 |
|---|---|
| `kCubeHalfExtent` | 1.0 |
| 회전 중 모서리 최저점 | −√2 ≈ −1.414 |
| `kGroundY` | −1.5 |

그래서 큐브는 바닥 위에 살짝 떠 있고, 이것은 어느 크기에서도 어느 레이어를 어떻게 돌려도 성립합니다. 큐브 바로 아래에 두는 **접촉 그림자**(결정 7)는 "놓여 있다"가 아니라 "가까이 떠 있는 물체가 아래에 드리우는 환경광 그림자"로 읽히므로 회전과 충돌하지 않습니다.

**개정 — 바닥 아래로 내려가는 카메라.** 궤도 카메라는 pitch −16.8° 아래에서 눈이 바닥 밑으로 갑니다. 바닥은 그려지지 않으므로 그 자체는 문제가 없지만, 바닥 평면 위의 그림자 다각형은 눈이 평면에 가까워질수록 한 줄로 얇아지다가 평면을 지나면 뒤집혀 큐브 위쪽에 나타나고, 눈이 정확히 평면에 있으면 near-plane과 교차합니다. 정책은 하나로 둡니다.

- `shadow()`가 그룹 `opacity`에 **눈 높이에 따른 계수**를 곱합니다. `eye.y ≥ kGroundY + kShadowFadeBand`에서 1, `eye.y ≤ kGroundY`에서 0, 사이는 선형입니다. 0이면 그룹을 비워 렌더러가 그리지 않습니다.
- 여섯 면 전체 궤도 계약(Phase 5.5)은 그대로입니다 — 바뀌는 것은 그림자가 사라지는 것뿐이고, 밑에서 올려다보는 물체의 바닥 그림자가 보이지 않는 것은 자연스럽습니다.

**개정 — 그림자는 큐브 실루엣 안쪽에 나타나면 안 됩니다.** 초안은 "그림자가 seam으로 비치면 물리적으로 맞으니 계약을 그 값으로 개정한다"고 했는데 틀렸습니다. 실물에서 틈 너머는 큐비 몸체이고 바닥이 아닙니다. 비치는 것은 스티커-only 모델의 artifact이므로 **막습니다.** casters를 화면에 투영한 convex hull(쉬는 큐브는 하나, 회전 중은 slab마다 하나)을 그룹의 `occluders`로 실어 보내고, 렌더러가 그것을 `MaskMethod::InvAlpha`로 그림자 Scene에 씌웁니다. 큐브 실루엣 안쪽(틈 포함)에서는 그림자가 0이 되고, seam 계약은 배경색 그대로 유지됩니다. casters가 이미 있으므로 재료가 새로 들지 않습니다.

`SceneEffect::DropShadow`를 쓰지 않는 이유가 여기서 나옵니다. 그것은 화면 공간에서 2D로 밀어 놓는 그림자라 바닥에 눕지 않고, 광원과 큐브 자세를 따르지도 않습니다.

### 7. 그림자는 그룹 하나이고, 렌더러가 ThorVG 합성으로 완성한다

**개정 — 그룹 수준의 계약.** 초안은 다각형마다 `anchor, tip`을 붙였는데, fade 방향·clip·opacity는 다각형이 아니라 **그림자 전체의 속성**입니다. slab이 여럿일 때 어느 다각형의 값을 Scene의 mask에 쓸지 정의되지 않았고, clip 사각형은 아예 없었습니다. 계약은 다음과 같습니다.

```cpp
struct RenderShadowGroup {                     // ThorVG 타입 없음
    std::vector<std::vector<math::Vec2>> polygons;   // slab마다 convex hull 하나
    std::vector<std::vector<math::Vec2>> occluders;  // slab의 화면 실루엣, InvAlpha mask용
    math::Vec2 fade_start;   // 큐브 중심의 바닥 수직 투영 (앵커)
    math::Vec2 fade_end;     // 큐브 중심이 광원으로부터 투영된 점 (그림자의 끝 방향)
    Rect clip;               // viewport pass가 적는 큐브 뷰포트
    std::uint8_t opacity;    // 결정 6의 눈 높이 계수를 곱한 값
    float blur_sigma;        // 화면 픽셀, viewport 한 변에 비례
    std::optional<Contact> contact;  // 앵커의 접촉 그림자 타원 (중심, 두 반지름)
};
```

`RenderScene::shadow`는 `std::optional<RenderShadowGroup>` 하나입니다. `fade_start`, `fade_end`, `contact`는 월드 점으로 시작해 view → project → viewport를 꼭짓점과 같은 식으로 지납니다. `clip`은 `ViewportPass`가 적습니다 — rect를 아는 유일한 자리입니다.

렌더러는 그룹을 다음 순서로 그림자답게 만들고, 각 단계가 ThorVG 기능 하나씩을 맡습니다.

1. **Scene 하나에 담고 `opacity()`.** `polygons`를 전부 불투명한 그림자색으로 push하고 Scene에 `opacity`를 겁니다. Scene은 자식을 먼저 평탄화한 뒤 opacity를 적용하므로, slab이 겹치는 곳이 두 번 어두워지지 않습니다.
2. **`GaussianBlur` SceneEffect.** 같은 Scene에 `(blur_sigma, 0, 0, kBlurQuality)`로 겁니다. sigma는 하나입니다 — 회전 중 올라간 레이어만 더 흐리게 하려면 Scene을 나눠야 하고, 그러면 1의 평탄화가 깨집니다.
3. **`Multiply` blend.** 그림자가 회색을 덧칠하는 것이 아니라 바닥을 어둡게 합니다. Light와 Dark 배경 어느 쪽에서도 같은 색 상수로 성립하는 이유입니다.
4. **Fade mask.** `fade_start`에서 불투명, `fade_end`에서 `kShadowFadeFloor`로 떨어지는 linear gradient 도형을 `mask(MaskMethod::Alpha)`로 씌웁니다. **개정 — 퇴화.** 광원이 직상부에 가까우면 `fade_end ≈ fade_start`라 방향이 정의되지 않습니다. `|fade_end − fade_start| < kFadeMinLength`(픽셀)이면 앵커 중심의 **radial gradient**로 대신합니다 — 사방으로 균등하게 옅어지는 것이 그 상황의 옳은 그림입니다.
5. **Occluder mask.** `occluders`를 Scene 하나에 담아 `mask(MaskMethod::InvAlpha)`로 씌웁니다(결정 6).
6. **`clip()`.** `clip` 사각형으로 잘라, 광원이 낮아져 그림자가 길게 뻗어도 옆의 넷 뷰를 침범하지 않습니다.

그리고 **접촉 그림자**: `contact` 타원에 radial gradient를 `Multiply`로 깝니다. 광원이 어디 있든 큐브 아래에 있어서, 투영 그림자가 극단적으로 늘어났을 때 큐브가 붕 떠 보이는 것을 잡아 줍니다. 이것도 occluder mask와 clip을 받습니다.

그리는 순서는 접촉 그림자 → 투영 그림자 Scene → (스티커, 그 스티커의 하이라이트)를 depth 순으로 → 축 기즈모입니다. 렌더러가 여기서 처음 갖게 되는 "면 여러 개를 `tvg::Scene` 하나로 묶어 효과를 거는" 기능은 [Phase 19.5](./19.5-net-lift-shadow.md)가 넷의 들림 그림자에 재사용합니다.

### 8. 그림자색은 배경에서 유도한다

회색 그림자는 싸 보입니다. 그림자색은 배경보다 어둡고 차가운 색을 `Multiply`로 얹어 만듭니다. Light 배경에서는 푸른 기가 도는 그림자, Dark 배경에서는 더 깊은 자리가 됩니다. Dark에서 농도가 모자라면 opacity를 theme별로 달리 받되, 값이 두 개 이상 필요해지기 전까지는 상수 하나로 시작합니다.

### 9. 렌더 계약은 v4로 개정하고, 조명이 계약을 더 세게 만든다

계약의 목적은 winding, culling, 채널 순서 실수가 "그럴싸한 그림"이 아니라 "틀린 색"으로 드러나게 하는 것입니다. 조명은 이 목적에 어긋나지 않고 오히려 보탭니다 — 보이는 세 면은 광원과의 각도가 다르므로 세 가지 다른 밝기를 갖고, **노멀 부호가 뒤집히면 색이 아니라 밝기가 틀리게 나옵니다.** 이것은 v3가 잡지 못하던 종류의 실수입니다.

그래서 test에서만 조명을 끄는 우회는 택하지 않습니다. 그것은 조명이 test에 한 번도 닿지 않게 하고, 브라우저 e2e까지 같은 스위치를 끌고 가야 합니다. 대신 **contract v4**로 갑니다.

- 세 면의 기대 색은 결정 2의 정수 공식으로 palette 색에 그 면의 밝기를 적용한 값이며, 밝기는 `Light` 기본값과 홈 카메라에서 한 번 계산해 **숫자로 Phase 4 문서에 적습니다.** Layout 비율을 엔진에서 읽지 않고 문서의 숫자로 test에 다시 쓰는 기존 방식과 같습니다 — 광원이나 ambient를 바꾸면 두 곳을 의도적으로 함께 바꿔야 합니다.
- **개정 — sample 지점은 하이라이트와 블러의 영향 밖이어야 합니다.** 중앙 sample이 하이라이트 lobe 안에 있으면 `palette × I`만으로는 기대값을 만들 수 없습니다(Screen 합성까지 재현해야 합니다). 그래서 Step 0에서 기본 광원을 정할 때 **세 sample 면의 하이라이트 alpha가 0(문턱값 아래)이라는 제약**을 걸고, 이것을 `light()` 패스 test로 고정합니다. 그림자는 결정 6의 occluder mask 덕에 큐브 실루엣 안쪽에 닿지 않으므로 면 sample과 seam sample 모두 영향이 없습니다.
- seam 세 지점은 배경색 그대로입니다(결정 6). 초안의 "비치면 실제 값으로 개정" 문장은 철회합니다.
- e2e의 sample도 같은 숫자로 갱신합니다.
- **개정 — 순서.** v4 숫자는 상수가 확정된 뒤에만 나올 수 있습니다. 광원 위치, ambient, diffuse, 반올림 공식 중 하나라도 바뀌면 숫자가 바뀝니다. 그러므로 Step 0(상수 확정) → Step 3(패스 test) → Step 7(v4 숫자 산출)의 순서는 바꿀 수 없고, Step 0의 값은 이 문서에 표로 적어 둡니다.

개정은 계획의 원칙대로 Phase 4 문서의 계약 절에 v4로 기록하고, 이 phase가 그 이유입니다.

## 상수 (Step 0에서 확정)

리뷰가 지적한 대로 값이 비어 있으면 v4 계약도, 성능·시각 acceptance도 검증할 수 없습니다. 아래는 **출발점**이고 Step 0에서 결정 9의 제약과 결정 2의 색 구별 test를 통과하는 값으로 확정한 뒤 이 표를 갱신합니다.

| 상수 | 출발값 | 뜻 |
|---|---|---|
| `Light::position` | `{2.0, 4.5, 3.5}` (월드) | 홈 시점에서 +Y가 가장 밝고 +Z, +X 순 — 세 면이 서로 다른 밝기 |
| `kAmbient` / `kDiffuse` | 0.60 / 0.40 | 합이 1이라 clamp가 일어나지 않음; I ∈ [0.60, 1.00] |
| `kSpecular` / `kShininess` | 0.60 / 32 | 하이라이트 최대 alpha 153, 반치폭 약 12° |
| `kHighlightRadius` | 0.45 (큐브 반폭 단위) | 월드 상수, N 무관 |
| `kHighlightMinAlpha` | 8 | 이 아래는 Highlight를 달지 않음 |
| `kGroundY` | −1.5 | 결정 6 |
| 그림자색 | `{40, 48, 64}` | Multiply용, 배경보다 어둡고 차가움 |
| `kShadowOpacity` | 96 | 눈 높이 계수를 곱하기 전 값 |
| `kShadowBlur` | 0.012 × 뷰포트 한 변 (픽셀) | sigma; 뷰포트 512px에서 약 6px |
| `kBlurQuality` | 60 | GaussianBlur quality |
| `kShadowFadeFloor` | 0.25 | fade_end에서의 alpha 배율 |
| `kFadeMinLength` | 4px | 이 아래면 radial fade로 대체 |
| `kShadowFadeBand` | 1.0 (월드) | 눈이 바닥 위 이 높이부터 그림자가 옅어짐 |
| 접촉 그림자 | 반지름 발자국의 0.55배, alpha 64 | 앵커 중심 radial, Multiply |

홈 시점의 세 면 밝기(출발값 기준, 면 중심에서 계산): +Y ≈ 0.86, +Z ≈ 0.78, +X ≈ 0.67. 정확한 byte 값은 Step 7에서 공식으로 냅니다.

## Scope

- `graphics`: `Light`, `kGroundY`, `Highlight{center, rim, alpha}`, `casters`, 그림자 그룹 타입, `light()`와 `shadow()` 패스, casters를 내는 builder 확장
- 기존 네 패스(transform, view, project, viewport)가 새 필드를 꼭짓점과 같은 규칙으로 통과시키는 것, viewport가 `clip`을 적는 것, cull과 depth_sort는 건드리지 않는 것
- `render`: 그림자 Scene 합성(평탄화, 블러, Multiply, fade mask, occluder mask, clip), 접촉 그림자, 면 직후의 하이라이트 도형, variadic 효과 호출의 타입 고정 래퍼
- 렌더러 픽셀 test(native, `SwCanvas` 버퍼)
- Rendered scene contract v4: Phase 4 문서의 계약 절 개정, native test와 e2e의 기대 색 갱신
- Application: 체인에 두 패스 삽입, 광원 상태, High contrast palette의 조명 무력화 값
- 28×28 회전 중 성능 측정

## Out of scope

- **광원을 조작하는 UI.** 이 phase는 광원을 상수로 둡니다. 슬라이더나 드래그는 UI polish의 정보 구조에 들어갈 자리를 정해야 해서 따로 봅니다. 다만 광원을 상태로 두어 나중에 얹기 쉽게 합니다.
- **전개도와 링 다이어그램의 조명.** 평면 표현은 색이 정체인 도식이라 조명이 정보를 해칩니다. 넷의 들림 그림자를 `DropShadow`로 바꾸는 것은 이 phase의 묶음 기능 위에 얹는 [Phase 19.5](./19.5-net-lift-shadow.md)입니다.
- **바닥 반사.** Blinn–Phong에 반사 항은 없습니다. 큐브를 바닥에 대칭시켜 옅게 그리는 트릭은 `reflect(ground)` 패스로 같은 자리에 들어갈 수 있으나, 그림자만으로 바닥의 존재는 충분히 읽힙니다.
- **하이라이트의 타원 투영.** 결정 3의 의도된 근사입니다.
- **픽셀 단위 조명.** `Picture::load(uint32_t*, ...)`로 CPU가 채운 라이트맵을 올리는 방법이 있지만, 평면 큐브에는 필요가 없습니다.
- **ThorVG 엔진 수정.** SVG의 `feDiffuseLighting`/`feSpecularLighting`을 SceneEffect로 추가하는 것은 upstream 기여 주제이고 이 저장소의 일이 아닙니다.
- **GL/WebGPU 백엔드.** 소프트웨어 엔진만 씁니다.

## Implementation steps

### 0. 상수 확정과 타입

- 위 표의 값을 놓고 (a) 홈 시점 세 sample 면의 하이라이트 alpha가 `kHighlightMinAlpha` 아래인지, (b) 여섯 palette 색이 `I = kAmbient`와 `I = 1`에서 pairwise 구별되는지(Classic, High contrast 둘 다)를 계산으로 확인해 확정합니다. 둘 중 하나라도 어긋나면 광원 위치나 ambient를 조정하고 표를 갱신합니다.
- `graphics/Light.hpp`: `struct Light { math::Vec3 position; float ambient, diffuse, specular, shininess; }`, `kGroundY`, 조명 무력화 값 `Light::unlit()`(`ambient = 1, diffuse = 0, specular = 0`).
- `Scene.hpp`: `Highlight{center, rim, alpha}`(월드/뷰는 `Vec3`, clip 이후는 `Vec2`), `WorldScene::casters`(박스 = 꼭짓점 8개), 각 단계 scene의 그림자 그룹.
- `RenderScene.hpp`: `RenderShadowGroup`, `RenderFace::highlight`.

### 1. Builder — casters

- `build_cube_scene`이 쉬는 큐브에서 박스 하나, 회전 중에는 `layers`의 연속 구간과 정지 구간마다 박스 하나를 냅니다. 스티커에 쓰는 회전을 그대로 적용합니다.
- Test: 쉬는 큐브의 casters는 정확히 큐브 껍질 하나. `R`·`Rw`·`2-3Rw` 회전 중 casters의 층 구간이 겹치지 않고 합치면 N층 전부이며 개수가 3 이하. 회전 0°의 casters의 **합집합**이 쉬는 큐브와 같음.

### 2. 통과 규칙

- `TransformPass`가 casters를, `ViewPass`·`ProjectPass`·`ViewportPass`가 그림자 그룹의 점들과 `highlight.center`·`rim`을 꼭짓점과 같은 식으로 변환합니다. `ViewportPass`는 `clip = rect`를 적습니다. `CullPass`, `DepthSortPass`는 그룹을 건드리지 않습니다.
- Test: 같은 점을 face 꼭짓점과 그룹의 점으로 넣었을 때 결과가 같음. identity가 아닌 model에서 스티커와 casters가 함께 움직임. cull이 그룹을 지우지 않음. viewport 결과의 `clip`이 입력 rect와 같음.

### 3. `light()` 패스

- 생성 시 광원을 `camera.view()`로 뷰 공간에 옮깁니다. 노멀 부호를 cull의 반시계 규약에서 유도합니다. 면 중심에서 L, V, H를 구해 결정 2의 정수 공식으로 색을, 결정 3의 반사점으로 highlight를 계산합니다.
- Test: 광원을 마주 보는 면이 등을 돌린 면보다 밝고, 등을 돌린 면은 정확히 ambient. 카메라를 궤도로 돌려도 월드에서 같은 면이 가장 밝음(광원이 카메라에 붙지 않음). 하이라이트 중심은 반사점 공식으로 면 안에 있거나 면 위로 당겨져 있고 `rim`이 `center`에서 `kHighlightRadius`만큼 떨어져 있음. 홈 시점의 세 sample 면은 highlight 없음. `Light::unlit()`에서 `color' == color`. **여섯 palette 색이 `I = kAmbient`와 `I = 1` 양쪽에서 pairwise로 구별됨**(Classic과 High contrast 둘 다).

### 4. `shadow()` 패스

- casters의 꼭짓점 8개를 결정 5의 식으로 투영하고 2D convex hull을 잡아 박스마다 다각형 하나를, 화면 실루엣용으로 박스마다 occluder 하나를 냅니다. `fade_start`(큐브 중심의 수직 투영), `fade_end`(큐브 중심의 광원 투영), `contact`, 눈 높이 계수를 곱한 `opacity`를 적습니다. 광원보다 높은 꼭짓점이 있는 박스는 버립니다.
- Test: 투영점이 전부 `y == kGroundY`. 쉬는 큐브의 그림자는 다각형 하나이고 볼록함. 직상부 광원에서 그림자는 발자국과 중심·축이 같고 각 꼭짓점이 `(L.y − ground_y)/(L.y − P.y)`배 확대됨. 광원을 옆으로 옮기면 `fade_end`가 반대쪽으로 이동. 광원보다 높은 큐브는 그림자가 없음. 눈이 `kGroundY + kShadowFadeBand` 위면 opacity 계수 1, 바닥 아래면 그룹 없음.

### 5. 렌더러와 픽셀 test

- 결정 7의 여섯 단계와 접촉 그림자, 결정 3의 하이라이트를 면 직후에 그리기. 효과 호출은 인자 타입을 고정한 함수 하나로 감쌉니다. 실패 경로는 기존과 같이 `false`를 돌려주는 fail-stop입니다.
- 렌더 계약 test와 같은 방식으로 `SwCanvas` 버퍼를 읽는 native test를 둡니다. 다음을 픽셀로 단언합니다.
  - 두 slab이 겹치는 자리와 slab 하나만 있는 자리의 RGB가 같음(평탄화).
  - 블러 가장자리와 fade 방향을 따라 luminance가 단조.
  - `clip` 밖은 정확히 배경색. 큐브 실루엣 안쪽(seam 포함)도 정확히 배경 또는 스티커 색(occluder).
  - 회전 중 뒤쪽 면의 하이라이트가 앞면 픽셀을 밝히지 않음.
  - Light/Dark 두 배경에서 그림자 자리가 배경보다 어두움.
- WASM 빌드 뒤 브라우저에서 DPR 1과 2, 두 theme, 홈 시점과 낮은 시점, 쉬는 큐브와 회전 중 큐브를 확인합니다. 이것은 픽셀 test의 보조입니다.

### 6. Application과 성능

- 체인에 `shadow`와 `light`를 삽입하고 `ApplicationState`에 `Light` 하나를 둡니다. High contrast palette가 선택되면 `Light::unlit()`을 넘깁니다(값만 바뀌고 분기는 없음).
- **28×28, 45° 회전 중** 프레임의 median과 p95를 조명 전후로 재고, 그 프레임의 하이라이트 도형 수를 기록합니다. 기준은 median 증가 15% 이하, 하이라이트 수가 전체 스티커의 15% 이하입니다. 어긋나면 `kHighlightMinAlpha`를 올립니다.

### 7. 계약 개정과 문서

- 세 면의 byte 값을 결정 2의 공식으로 계산해 [Phase 4 문서](./04-rubiks-cube-domain.md)에 **Rendered scene contract v4**로 적고, [`RenderContractTest.cpp`](../../tests/app/RenderContractTest.cpp)와 [`sceneContract.ts`](../../web/tests/e2e/sceneContract.ts)의 기대 색을 그 숫자로 바꿉니다. Step 3의 패스 test가 초록이 된 뒤에 합니다 — 계약 숫자를 구현에서 베끼는 것이 아니라 공식에서 내야 합니다.
- 이 문서의 Status와 상수 표, [IMPLEMENTATION_PLAN.md](./IMPLEMENTATION_PLAN.md), [DESIGN.md](../DESIGN.md)의 파이프라인 예시에 두 패스 추가.

## Acceptance criteria

- 광원을 마주 보는 면이 밝고 등을 돌린 면이 어두우며, 여섯 스티커 색은 `I ∈ [kAmbient, 1]` 전 구간에서 pairwise 구별됩니다(test).
- 카메라를 궤도로 돌려도 가장 밝은 면은 월드에서 같은 면입니다(test).
- 하이라이트는 문턱값을 넘는 면에만 맺히고, 면 직후에 그려져 회전 중 뒤쪽 면의 하이라이트가 앞면 픽셀을 바꾸지 않습니다(픽셀 test).
- 바닥 그림자는 큐브 아래에 눕고, 레이어를 돌리면 튀어나온 slab의 그림자가 함께 돌아가며, 어떤 레이어 회전에서도 큐브가 바닥을 뚫지 않습니다.
- 겹친 slab과 단일 slab 자리의 그림자 RGB가 같고, 블러 가장자리와 fade 방향의 luminance가 단조이며, `clip` 밖과 큐브 실루엣 안쪽은 정확히 배경 또는 스티커 색입니다(픽셀 test).
- 눈이 바닥 아래로 내려가면 그림자가 사라지고, 다시 올라오면 돌아옵니다.
- Light와 Dark theme 모두에서 그림자 자리가 배경보다 어둡습니다(픽셀 test).
- 현재 mask 생성 경로에서 caster는 3개 이하이고, 비연속 mask에서도 `구간 수 × 2 + 1` 이하입니다(test).
- 28×28 45° 회전 중 median 프레임 시간 증가 15% 이하, 하이라이트 도형 수는 스티커의 15% 이하입니다(측정 기록).
- 전개도와 링 다이어그램은 픽셀 하나도 바뀌지 않습니다.
- Rendered scene contract가 v4로 개정되어 Phase 4 문서, native test, e2e가 같은 숫자를 보고, 노멀 부호를 뒤집으면 v4 test가 실패하며, seam 세 지점은 배경색 그대로입니다.
- Native test, WASM 빌드, TypeScript unit, e2e, production build가 모두 통과합니다.

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

## 먼저 답이 필요한 것

- **High contrast palette에서 조명을 끌 것인가.** 그 palette는 색 구별이 목적이라 명암을 얹으면 취지에 어긋납니다. 추천은 `Light::unlit()`(`ambient = 1, diffuse = 0, specular = 0`)을 넘기는 것입니다 — 패스는 그대로 돌고 값만 바뀌므로 분기가 생기지 않습니다. 이 경우 v4 계약은 Classic palette 기준입니다.
- **Ambient mode에서 광원을 천천히 돌릴 것인가.** 화면 보호기 성격의 관람 모드라 광원이 움직이면 그림자와 하이라이트가 살아 보입니다. 다만 Phase 10은 관람을 "큐브가 스스로 도는 것"으로 규정하고 카메라 자동 궤도도 out of scope로 두었으므로, 광원 움직임을 더하는 것은 그 규정의 확장입니다.
- **광원 기본 위치의 좌우.** 표의 출발값은 홈 시점 기준 왼쪽 위입니다. 오른쪽 위로 바꾸면 +X와 +Z의 밝기가 뒤바뀝니다. 어느 쪽이든 결정 9의 제약(세 sample 면에 하이라이트 없음)은 Step 0에서 다시 확인합니다.
