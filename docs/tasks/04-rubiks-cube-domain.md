# Phase 4: Rubik's Cube Domain

## Status

`Completed`

## Objective

Rendering과 독립적인 Rubik's Cube 도메인을 구현합니다.
면별로 독립적인 색을 갖는 1×1×1 cubie를 N×N×N 격자에 저장하고, axis와 layer 기반 `CubeMove`, quarter turn 적용, inverse, sequence를 지원합니다.
Graphics pipeline의 입력을 Phase 3의 하드코딩된 단일 cube에서 `CubeState`로 교체하고, 같은 상태를 **전개도(net)** 로도 함께 렌더링하여 큐브 전체를 한눈에 볼 수 있게 합니다.

```text
                 CubeState (N³ cubies, 면별 색)
                    │
        ┌───────────┴────────────┐
        ▼                        ▼
  build_cube_scene()       build_net_scene()
  world-space quads         screen-space quads
        │                        │
        ▼                        │
  Phase 3 pipeline               │  (파이프라인 불필요:
  transform│view│project│        │   이미 2D, 가릴 면 없음)
  cull│depth_sort│viewport       │
        │                        │
        └───────────┬────────────┘
                    ▼
              RenderScene (합쳐진 quad 목록)
                    ▼
           ThorVGSoftwareRenderer
```

Phase 4가 끝나면 화면 위쪽에는 seam이 보이는 3×3×3 cube가, 아래쪽에는 6면 전개도가 표시되고, Phase 5의 interaction은 "어느 layer를 얼마나 돌릴지"만 결정하면 됩니다.

## Scope

- `engine/src/cube/`: `CubeState`, `Cubie`, `CubeMove`
- N×N×N 격자 저장과 face-color 표현 (orientation 수학 없이 색 순열로)
- Axis + layer + quarter turn 기반 move 적용, inverse, sequence
- 이름 있는 표준 move를 만드는 factory (`moves::R(size)` 등) — 문자열 파싱이 아니라 생성자
- Graphics의 `build_cube_scene(const CubeState&)` 교체 (surface sticker만 방출)
- 전개도 `build_net_scene(const CubeState&, Rect)` 추가
- `ViewportPass`가 전체 buffer가 아니라 sub-rectangle로 매핑하도록 확장
- Cube 도메인을 완전 독립 Meson target으로 분리
- Rendered scene contract v3: split layout, seam 검증, 전개도 54칸 전수 검증

## Out of scope

- Pointer interaction, picking, drag (Phase 5)
- Drag 중 transient rotation과 `ActiveRotation` (Phase 5) — 도메인은 90° 단위 discrete 상태만 가집니다
- Layer 회전 중의 cubie 옆면(body) 렌더링 (Phase 5)
- View mode 전환 UI(3D만 / 전개도만 / 둘 다) (Phase 6) — 이 phase는 고정 split layout입니다
- Scramble, reset, solved 판정, timer (Phase 6)
- N ≠ 3의 실제 출시. 저장과 move 엔진은 N-generic이지만 engine이 만드는 것은 N = 3뿐입니다
- 문자열 notation parsing (`"R U R' U'"` → `CubeMove[]`). 아래 참고
- C ABI와 TypeScript boundary 변경
- 성능 최적화 (sticker 수는 N = 3에서 최대 54 + 전개도 54)

## Architecture decisions

### Domain purity and build structure

DESIGN.md의 원칙대로 cube 도메인은 graphics, math, ThorVG를 알지 못합니다.

- `engine/src/cube`는 **아무 dependency도 없는** Meson target입니다. `math_dep`조차 쓰지 않습니다 — 도메인 좌표가 정수라 float vector가 필요 없습니다.
- `tests/cube`는 cube target만 link합니다. 도메인에 graphics나 linalg include가 생기면 빌드가 깨집니다.
- 의존 방향은 단방향입니다: `graphics → cube`, `cube → 없음`.

### N×N×N storage with index coordinates

Cubie는 N³ 격자에 저장하고 각 축 좌표는 **`0 … N−1`의 index**입니다.

```cpp
enum class Face { Right, Left, Up, Down, Front, Back };  // +X -X +Y -Y +Z -Z
enum class FaceColor { Red, Orange, White, Yellow, Green, Blue };

struct Cubie {
    std::array<FaceColor, 6> stickers;  // Face로 index
};

class CubeState {
public:
    explicit CubeState(int size = 3);            // solved
    [[nodiscard]] int size() const noexcept;
    [[nodiscard]] const Cubie& at(int x, int y, int z) const noexcept;
    void apply(const CubeMove& move) noexcept;
    // ...
private:
    int size_;
    std::vector<Cubie> cubies_;  // size_³, index (x*size_ + y)*size_ + z
};
```

Phase 3 스펙 초안은 `{−1, 0, +1}` 부호 좌표에 `std::array<Cubie, 27>`를 쓰려 했지만, index 좌표로 바꾸면 **짝수 N에서도 같은 식이 성립합니다**. 부호 좌표는 N = 3처럼 홀수일 때만 정수로 떨어지고 N = 2·4에서는 반정수가 되어 갈라집니다.

- 저장은 `std::vector<Cubie>`이고 크기는 runtime N입니다. Template `CubeState<N>`도 가능하지만, 그러면 `build_cube_scene`을 포함한 모든 소비자가 template이 되고 헤더로 올라가야 합니다. Runtime N은 소비자를 단순하게 두고, Phase 6에서 UI로 크기를 바꾸는 길도 열어 둡니다.
- 중앙 cubie와 내부 cubie도 격자에 자리를 차지합니다. 빈 칸을 특별 취급하는 것보다 uniform indexing이 단순하고, N = 3에서 낭비는 cubie 1개입니다.
- `at(x, y, z)`가 3차원 접근의 공개 인터페이스이며, 내부 flat 저장은 구현 세부입니다.

**이 phase가 만드는 것은 N = 3뿐입니다.** N-generic은 구조로만 확보하고, 실제로 다른 N이 성립하는지는 N = 2 smoke test 하나로 고정합니다. 2×2×2와 4×4×4를 제품으로 지원하려면 UI와 크기별 layout 조정이 더 필요하므로 그것은 별도 작업입니다.

### Cubie representation: 색 순열, orientation 수학 없음

Quarter turn은 (a) layer에 속한 cubie들의 **위치 순열**과 (b) 각 cubie 내부의 **sticker 색 순열**로 구현합니다.
Quaternion이나 회전 행렬로 orientation을 추적하지 않는 이유는 두 가지입니다.

- 도메인이 math 모듈을 알면 안 되고, 90° 단위 회전에서 색 순열은 회전과 정확히 동치입니다.
- 연속 각도(drag 중의 `23.7°`)는 DESIGN.md 8절의 visual state이며 Phase 5에서 graphics가 소유합니다. Logical state는 항상 discrete합니다.

전개도 렌더링이 이 선택을 한 번 더 정당화합니다. 전개도는 "위치 P의 cubie에서 면 F가 무슨 색인가"만 필요한데, 그것이 바로 이 표현이 직접 답하는 질문입니다. Orientation을 quaternion으로 들고 있었다면 전개도를 그릴 때마다 색을 역산해야 합니다.

Solved 상태는 모든 cubie가 `stickers[f] == color_of(f)`인 상태입니다 (`Right`=Red, `Left`=Orange, `Up`=White, `Down`=Yellow, `Front`=Green, `Back`=Blue — Phase 3 contract와 같은 배색).

### CubeMove semantics

```cpp
enum class Axis { X, Y, Z };

struct CubeMove {
    Axis axis;
    LayerMask layers;   // 0 … N−1 layer index에 대한 bitmask
    int quarter_turns;  // 양수 = 축의 양의 끝에서 원점을 바라볼 때 시계 방향
};
```

`quarter_turns`는 mod 4로 정규화하고 0이면 no-op입니다. `inverse(move)`는 부호만 뒤집습니다.

+1 quarter turn의 위치 매핑과, 같은 회전을 face normal에 적용한 sticker 순열:

| Axis | 위치 매핑 (index 좌표) | Sticker cycle (축 face는 고정) |
| --- | --- | --- |
| X | `(x, y, z) → (x, z, N−1−y)` | Up → Back → Down → Front → Up |
| Y | `(x, y, z) → (N−1−z, y, x)` | Front → Left → Back → Right → Front |
| Z | `(x, y, z) → (y, N−1−x, z)` | Up → Right → Down → Left → Up |

이 표는 프로토타입으로 검증했습니다. N = 3에서 `R`(`{X, {N−1}, +1}`)을 적용하면 UFR 자리의 cubie가 UBR 자리 `(2, 2, 0)`로 이동하고, 그 자리의 Up sticker는 Green(원래 Front), Back sticker는 White(원래 Up), Right sticker는 Red입니다. 실물 큐브의 R과 일치합니다.

### Named move factories, and why there is no parser

문자열 notation parsing은 이 phase에서 구현하지 않습니다.

`CubeMove` 자체가 이미 일반화된 표현이기 때문입니다. `{axis, layers, quarter_turns}`는 층을 숫자로 지목하는 구조체이고, `R`이나 `3Rw` 같은 표기는 그것을 사람이 읽고 쓰기 위한 직렬화 형식일 뿐입니다. 큐브 크기가 커지면 표기 체계도 실제로 갈라지지만(`M E S`는 가운데 층이 있는 홀수 N에서만 존재하고, 큰 큐브는 WCA의 `2R`·`3Rw`처럼 층 번호를 쓰는 숫자 표기가 필요합니다) `CubeMove`는 그 차이를 이미 흡수합니다.

그리고 파서를 필요로 하는 소비자가 없습니다. Phase 5의 interaction은 drag에서 곧바로 `CubeMove`를 만들고, Phase 6의 scramble도 랜덤 move를 직접 생성하면 됩니다. 문자열이 필요해지는 것은 solve 기록이나 알고리즘 입력 UI를 붙일 때이고, 그때 이 표현 위에 얹으면 됩니다.

대신 읽기 쉬운 **factory**를 둡니다. 파싱이 아니라 생성자이므로 실패 경로가 없습니다.

```cpp
namespace moves {
CubeMove R(int size);  CubeMove L(int size);
CubeMove U(int size);  CubeMove D(int size);
CubeMove F(int size);  CubeMove B(int size);
}
```

| Factory | Axis | Layers | quarter_turns |
| --- | --- | --- | --- |
| `R` / `L` | X | {N−1} / {0} | +1 / −1 |
| `U` / `D` | Y | {N−1} / {0} | +1 / −1 |
| `F` / `B` | Z | {N−1} / {0} | +1 / −1 |

음의 face(`L`, `D`, `B`)는 자기 face 기준 시계 방향이 축 기준으로는 반대라서 `quarter_turns`가 −1입니다.
Wide, slice, 전체 회전은 factory 없이 `CubeMove`를 직접 구성합니다 — layer 집합을 지정하는 것이 전부입니다.

### Canvas layout

Canvas를 두 영역으로 나눕니다. 좌표는 canvas의 짧은 변에 대한 비율이며 두 영역 모두 가로 중앙 정렬입니다.

```text
┌──────────────────────────┐
│      3D cube viewport    │  정사각형, 한 변 0.58, 위쪽 여백 0.01
│         (square)         │
├──────────────────────────┤
│    net: 4×3 faces        │  face 한 칸 0.12 → 블록 0.48 × 0.36
│                          │  아래쪽 여백 0.02
└──────────────────────────┘
```

**Cube viewport는 항상 정사각형**입니다. 그래서 camera aspect가 canvas 비율과 무관하게 1로 고정되고, Phase 3 contract의 sample 비율이 "정사각형 canvas에서만 유효"하다는 v2의 제약이 사라집니다. Sample은 이제 canvas가 아니라 **cube viewport 기준 비율**로 정의됩니다.

이를 위해 `ViewportPass`가 `width`/`height` 대신 sub-rectangle을 받습니다.

```text
x_screen = rect.x + (ndc_x + 1) / 2 * rect.width
y_screen = rect.y + (1 − ndc_y) / 2 * rect.height
```

### Net view

전개도는 `CubeState`에서 곧바로 screen-space quad를 만들며 **파이프라인을 거치지 않습니다**. 이미 2D이고, 가려지는 면도 원근도 없기 때문입니다. `RenderScene`이 순수한 2D quad 목록이라 두 출처의 scene을 이어 붙이기만 하면 되는데, 이것 자체가 Phase 3에서 정한 renderer 경계가 옳았다는 근거입니다.

배치는 표준 cross net이며 위 이미지와 같습니다: 위 White(U), 가운데 줄 왼쪽부터 Orange(L) · Green(F) · Red(R) · Blue(B), 아래 Yellow(D).

각 면의 net 칸 `(col, row)`(둘 다 `0 … N−1`)가 어느 cubie의 어느 face인지는 다음과 같습니다. F를 정면으로 두고 펼친 표준 전개이며, 인접 변이 실제로 맞닿도록 정해집니다.

| Net face | Cubie 위치 | Face |
| --- | --- | --- |
| U | `(col, N−1, row)` | Up |
| L | `(0, N−1−row, col)` | Left |
| F | `(col, N−1−row, N−1)` | Front |
| R | `(N−1, N−1−row, N−1−col)` | Right |
| B | `(N−1−col, N−1−row, 0)` | Back |
| D | `(col, 0, N−1−row)` | Down |

검산: F의 오른쪽 끝(`col = N−1`)은 `x = N−1`이고 R의 왼쪽 끝(`col = 0`)은 `z = N−1`이라, 두 칸은 실제 큐브에서 같은 모서리를 공유합니다.

Sticker는 칸 크기의 `0.88`로 축소해 그리며, 남는 부분이 seam으로 배경색이 보입니다. 3D 쪽 seam과 목적이 같습니다.

### Render connection

`graphics`가 `cube`에 의존하게 되고 `FaceColor → graphics::Color` 매핑도 graphics 쪽에 둡니다.

3D 기하는 Phase 3 계약을 보존하도록 정합니다.

- 전체 cube는 여전히 edge 2, 원점 중심입니다. Cubie pitch는 `2/N`, 중심은 `(2i/N) − (N−1)/N × ... ` 즉 index `i`에 대해 `(2i − (N−1)) / N`입니다.
- 각 cubie는 `sticker_scale = 0.92`로 축소되어 half extent가 `(1/N) × 0.92`입니다. N = 3에서 seam 폭은 world 기준 약 `0.053`입니다.
- **Surface sticker만 방출합니다**: cubie의 face 중 위치가 그 face 방향의 바깥 layer와 일치하는 것만 quad가 됩니다. N = 3에서 54개이고 고정 camera에서 culling 후 27개가 남습니다.
- Cubie의 안쪽 면과 옆면은 방출하지 않습니다. 그래서 seam으로 보이는 것은 항상 배경색이며 seam pixel 검증이 결정적입니다. Layer 분리 시 옆면이 필요한 것은 Phase 5에서 body face를 추가하며 해결합니다.

Application은 `CubeState`를 소유하고, 매 frame 두 scene을 만들어 이어 붙입니다. 전개도 quad를 뒤에 붙여 3D 위에 그리지만 두 영역은 겹치지 않으므로 순서는 결과에 영향을 주지 않습니다.

## Rendered scene contract v3

배경색과 면 색은 v2와 같고, 검증 지점이 세 종류로 늘어납니다. Browser 출력은 solved cube입니다.

**1. 3D cube sample** — cube viewport 기준 비율이며 v2 값을 그대로 씁니다. 각 면 중앙 cubie의 sticker 안에 들어가는 것을 projection으로 확인했습니다.

```text
white (+Y) : (0.50, 0.29)   green (+Z) : (0.31, 0.61)   red (+X) : (0.69, 0.61)
```

정사각형 canvas에서 canvas 절대 비율로는 `(0.500, 0.178)`, `(0.390, 0.364)`, `(0.610, 0.364)`입니다.

**2. Seam sample** — sticker 사이 틈이므로 배경색과 정확히 일치해야 합니다. Cube viewport 기준 비율이고, seam이 좁아 소수 3자리로 고정합니다.

```text
(0.377, 0.645)  +Z face, 중앙과 오른쪽 sticker 사이
(0.313, 0.534)  +Z face, 중앙과 위쪽 sticker 사이
(0.564, 0.321)  +Y face, 중앙과 앞오른쪽 사이
```

Seam 폭은 canvas 512px에서 약 2.8px, 1008px에서 약 5.5px입니다. Anti-aliasing이 sample을 오염시키지 않도록 **seam 검증은 canvas 1024 이상에서만** 수행하고, 작은 크기와 resize 이후에는 3D sample과 모서리만 재검증합니다.

Browser에서는 canvas가 `min(80vw, 70vh)`라 기본 viewport에서 504px에 그칩니다. E2E는 seam을 검증하기 전에 viewport를 키우고 **canvas 크기가 실제로 임계값을 넘었는지 먼저 단언**합니다. 그렇게 하지 않으면 stylesheet가 바뀌었을 때 seam 검증이 조용히 건너뛰어집니다.

**3. 전개도 전수 검증** — N = 3에서 54칸의 중심 pixel을 모두 읽어 기대 색 격자와 비교합니다. Sample 3개보다 훨씬 강한 검증이며, 도메인 → 렌더 경로 전체를 덮습니다. 칸 하나가 canvas 1008px에서 약 40px이라 중심 pixel은 anti-aliasing에서 안전합니다.

모서리 검증은 v2와 같습니다. Camera, cube 크기, `sticker_scale`, layout 상수 중 무엇이든 바꾸면 위 좌표를 다시 유도해야 합니다.

Native pipeline 검증 (pixel이 아니라 `RenderScene` 수준):

- Solved 상태에서 3D face 27개, 색별 정확히 9개씩
- 전개도 face 54개, 색별 정확히 9개씩
- 3D sample은 각 면 중앙 sticker 다각형 내부, seam sample은 어떤 sticker에도 속하지 않음
- Move 적용 후 두 scene 모두에 색 변화가 반영됨

## Rendered scene contract v4

[Phase 19](./19-lighting-and-shadow.md)가 3D 뷰에 조명을 넣으면서 개정되었습니다. **검증 지점과 배경색, 전개도 54칸, 모서리는 v3 그대로**이고, 바뀐 것은 3D sample 세 지점의 기대 색입니다. 전개도는 도식이라 조명을 받지 않으므로 palette 색 그대로입니다.

**3D sample의 기대 색은 palette 색의 채도를 올린 뒤 그 면이 놓인 평면의 밝기를 곱한 값**입니다. 밝기는 Blinn–Phong의 ambient + diffuse 항을 평면마다 하나씩 계산한 byte이고, 두 연산 모두 정수로 고정되어 있습니다. 밝기는 255를 넘을 수 있고(palette 색은 "빛을 잘 받는 면"의 색이라 그보다 밝은 면이 있습니다) 채널은 255에서 멈춥니다.

```text
gray            = (2126 r + 7152 g + 722 b + 5000) / 10000
saturate(c)     = clamp(gray + round_away((c − gray) × 125 / 100))   채널마다 (5차 개정)
lit(c, b)       = min(255, (saturate(c) × b + 127) / 255)            채널마다, 정수 나눗셈
```

채도는 스티커에만 적용되고 몸체색 `kBodyColor`에는 적용되지 않습니다. 흰색은 3D에서 종이 흰색 `(216, 216, 216)`으로 놓이며(넷은 255 그대로) gray = 216이라 채도는 그대로이고, 초록 `(0, 155, 72)`은 `(0, 165, 61)`, 빨강 `(183, 18, 52)`은 `(215, 8, 51)`이 된 뒤 밝기를 받습니다.

밝기 `b`는 **키 라이트** `(2.6, 7.0, 4.0)`, ambient 0.75, diffuse 0.38, 거리 감쇠 지수 1에서 각 평면의 "큐브 중심에 가장 가까운 점" P에서 계산한 `round(255 * (0.75 + 0.38 * max(N·L, 0) * |L − C| / |L − P|))`입니다(C는 큐브 중심; 3차 개정 이전에는 광원 `(2.0, 8.3, 4.2)`, 0.60 + 0.40, 감쇠 없음). 램프는 키 하나입니다(5차 개정에서 킥커 제거). 키의 specular(0.60, shininess 12; 6차 개정 전 0.40 / 24)는 홈 시점의 sample 스티커에 닿지 않아 byte에 관여하지 않습니다. 스티커 평면은 큐브 면에서 seam만큼 안쪽(`N = 3`에서 0.9733)에 있습니다. +Z의 값은 반올림 경계에 가깝지만(229.52) 아래 ±1 허용으로 흡수됩니다.

| 평면 | 밝기 `b` | solved cube의 sample 색 |
|---|---|---|
| +Y (위) | 275 | white, 3D에서는 `kPaperWhite (216, 216, 216)` → `(233, 233, 233)` (7차 개정 전 흰색은 255라 255로 clamp; 넷의 흰색은 255) |
| +Z (앞) | 230 | green → saturate `(0, 165, 61)` → `(0, 149, 55)` |
| +X (오른쪽) | 211 | red → saturate `(215, 8, 51)` → `(178, 7, 42)` |
| −Y, −X, −Z | 191 | ambient만; 키를 등지고 있음 |

회전 중 드러나는 단면(`kBodyColor`)은 한 층 안쪽 평면에 있어 자기 밝기를 갖습니다. `N = 3`에서 `±1/3` 평면의 값입니다.

| 단면 평면 | 밝기 | 어느 회전에서 보이나 |
|---|---|---|
| x = +1/3, +X | 218 | R |
| x = −1/3, +X | 224 | L |
| y = +1/3, +Y | 273 | U |
| y = −1/3, +Y | 270 | D |
| z = +1/3, +Z | 235 | F |
| z = −1/3, +Z | 239 | B |

단면도 다른 면과 같이 양 끝으로 갈수록 밝기가 변합니다. 회전 중 단면이 보이는지는 색을 sweep으로 찾지 않고 **자리로 찍습니다**(5차 개정): 홈 시점에서 U를 45° 돌렸을 때 정지한 가운데 층의 윗 단면 위의 점 `(0.70, 1/3, 0.85)`, R이면 `(1/3, 0.70, 0.85)`를 화면에 투영해 그 픽셀이 `kBodyColor`의 어느 밝기(190~300, ±2)인지 봅니다. 쉬는 큐브에서 그 시선은 앞면 스티커 한가운데(seam에서 0.13 이상 떨어진 곳)에 떨어져 스티커 색입니다. seam도 `kBodyColor`이므로 색만으로는 단면과 seam을 가를 수 없어서입니다. 브라우저 e2e는 좌표를 투영하지 못하므로 같은 두 점을 홈 시점에서 투영한 cube 영역 비율 `(0.3927, 0.474)`(R), `(0.4688, 0.6059)`(U)를 probe에 두고 같은 판정을 합니다. 카메라나 배치가 바뀌면 두 비율도 다시 유도해야 합니다. 3D sample 세 지점의 허용 오차는 **채널당 ±2**입니다(4차 개정): gradient가 면 전체에 걸쳐 px당 약 1/15 byte로 변하고 sample 지점이 스티커 중앙에서 몇 px 벗어나 있습니다. **한 변이 256px 미만인 버퍼에서는 ±4**입니다 — 면 하나의 stop 다섯 gradient가 수십 px 안에 눌려 들어가 보간이 거칠어지고, 129px 버퍼에서 채도를 올린 빨강이 3 벗어나는 것을 확인했습니다.

**하이라이트와 그림자는 sample에 닿지 않습니다.** 홈 시점에서는 보이는 세 면 어디에도 스페큘러 반사점이 놓이지 않으므로 세 sample은 순수한 `lit()` 값이고, 이것은 Phase 19의 패스 test가 고정합니다. 그림자는 큐브 실루엣을 InvAlpha mask로 지워 큐브 안쪽(seam 포함)에 나타나지 않으므로 seam sample은 배경색 그대로입니다. **다른 시점**(궤도 카메라)에서는 면에 하이라이트가 맺힐 수 있어 byte 일치가 성립하지 않습니다. 시점을 돌린 뒤 "어느 면이 어디 있나"를 묻는 test는 색 위에 흰색 Screen이 같은 비율로 얹힌 것을 허용하는 비교(`require_pixel_reads`)를 씁니다.

High contrast palette는 `Light::unlit()`(ambient 1, diffuse 0, specular 0) 아래 그려지므로 v3와 같이 palette 색 그대로입니다.

**seam은 배경이 아니라 몸체입니다 (4차·5차 개정).** Phase 19로 큐비 몸체가 그려집니다 — slab의 화면 실루엣을 `kBodyColor` (70, 74, 82)로 스티커 아래에 칠한 것이라, 스티커 사이 틈에는 배경이 아니라 회전 중 단면과 같은 플라스틱이 보입니다. **seam sample 세 지점은 `kBodyColor`와 정확히 일치**해야 합니다(조명을 받지 않는 단색). 4차에서 잠시 더 어두운 `kSeamColor`를 썼던 것은 단면을 색으로 찾는 test 때문이었고, 5차에서 그 test를 자리로 찍는 방식으로 바꾸면서 틈과 단면을 같은 색으로 되돌렸습니다. 몸체색은 여섯 스티커색·배경과 뚜렷이 다르므로 검증의 결정력은 그대로입니다. 위 v3 절의 "seam은 배경색"은 이 판부터 적용되지 않습니다.

**허용 오차 ±1 (2차 개정).** Phase 19의 2차 개정으로 스티커가 단색이 아니라 평면 위 밝기 함수를 따르는 linear gradient로 채워집니다. 중앙 sample 스티커의 중앙 픽셀은 이론상 위 표의 값 그대로이지만(함수를 선분으로 근사한 오차는 byte 0.05), gradient 보간의 반올림이 ±1을 만들 수 있습니다. 그래서 **3D sample 세 지점과 단면 색 탐색은 채널당 ±1을 허용**합니다. 전개도, seam, 모서리, 배경은 여전히 정확히 일치해야 합니다. 노멀 부호 실수는 20~40 단위로 드러나므로 이 완화가 계약의 목적을 흔들지 않습니다.

이 계약의 숫자는 test가 엔진에서 읽는 것이 아니라 여기 적힌 것을 옮겨 쓴 것입니다. 광원 위치, ambient, diffuse, 반올림 공식, `kStickerScale` 중 하나라도 바꾸면 이 표를 다시 유도하고 [`RenderContractTest.cpp`](../../tests/app/RenderContractTest.cpp), [`PointerInteractionTest.cpp`](../../tests/app/PointerInteractionTest.cpp), [`sceneContract.ts`](../../web/tests/e2e/sceneContract.ts)를 함께 고쳐야 합니다.

## Implementation steps

### 1. Cube domain

- [x] `Face`, `FaceColor`, `Cubie`, index 좌표와 `at(x, y, z)` 정의
- [x] Runtime N을 갖는 `CubeState` 생성자와 solved 초기화 구현
- [x] `CubeMove`, `LayerMask`, mod 4 정규화 구현
- [x] 축별 위치 순열과 sticker 순열로 `apply(CubeMove)` 구현
- [x] `inverse(CubeMove)`와 sequence 적용 helper 구현
- [x] `moves::R` 등 이름 있는 face move factory 구현
- [x] 동등 비교 연산자 구현
- [x] Cube를 무의존 Meson target으로 분리

### 2. Render connection

- [x] `ViewportPass`를 sub-rectangle 기반으로 확장
- [x] `FaceColor → graphics::Color` 매핑 구현
- [x] Surface sticker 방출 규칙으로 `build_cube_scene(const CubeState&)` 교체
- [x] Cubie pitch, `sticker_scale` 상수를 N 기준으로 정의
- [x] 전개도 배치표대로 `build_net_scene(const CubeState&, Rect)` 구현
- [x] Layout 계산(cube 정사각 viewport, net rect)을 한 곳에 정의
- [x] Graphics target에 cube dependency 추가
- [x] Application이 `CubeState`를 소유하고 두 scene을 합쳐 전달

### 3. Verification

- [x] 위치/sticker 매핑 known-answer test: `R` 후 `(2, 2, 0)` cubie의 Up=Green, Back=White, Right=Red
- [x] 모든 기본 move의 주기 4 test (`R⁴ = identity` 등)
- [x] `move + inverse = identity` test
- [x] `(R U R' U')⁶ = identity` test
- [x] 전체 회전 `x⁴ = y⁴ = z⁴ = identity` test
- [x] Scramble sequence 적용 후 역순 inverse로 solved 복귀 test
- [x] N = 2 smoke test: 저장 크기, `R⁴ = identity`, `R R' = identity`
- [x] Move factory가 표의 필드를 만드는지 test (특히 `L`·`D`·`B`의 부호)
- [x] Wide와 slice를 직접 구성한 `CubeMove`의 주기 4 test
- [x] Solved 상태의 face별 sticker 9개 동일 색 test
- [x] 전개도 배치표 test: 인접 변이 같은 모서리를 공유하는지, 54칸이 cubie face와 1:1인지
- [x] Native pipeline test: 3D 27개와 전개도 54개, 색 분포, sample/seam의 다각형 포함 관계
- [x] Move 적용 상태의 RenderScene 색 변화 test
- [x] Native contract test: 3D sample, seam(1024 이상), 전개도 54칸 전수, 모서리
- [x] Browser e2e에 전개도 54칸 전수 검증과 seam sample 추가
- [x] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- Cube target은 어떤 dependency도 없이 빌드되고, cube test는 cube target만 link합니다.
- `CubeState`는 90° 단위 discrete 상태만 가지며 float, quaternion, graphics type을 포함하지 않습니다.
- 저장과 move 엔진이 N-generic이고 N = 2에서도 동작함이 test로 확인됩니다.
- 표의 위치/sticker 매핑이 known-answer test와 property test(주기, inverse, 교환자 주기 6)로 검증됩니다.
- 이름 있는 move factory가 표대로 `CubeMove`를 만들고, wide와 slice는 layer 집합만 지정해 직접 구성됩니다.
- Solved `CubeState`가 seam이 보이는 3×3×3과 6면 전개도로 함께 렌더링되고 contract v3의 세 검증을 모두 통과합니다.
- 전개도 54칸이 pixel 수준에서 전수 검증됩니다.
- Cube viewport가 정사각형이므로 3D sample 비율이 canvas 비율과 무관하게 유효합니다.
- Camera, 전체 cube 크기, v2 sample 비율은 변하지 않습니다.
- C ABI와 TypeScript boundary는 변경되지 않습니다.
- Native unit test, TypeScript unit test, WASM build, browser e2e와 Vite production build가 모두 통과합니다.
- Pointer interaction, animation, scramble, view mode UI 코드는 이 phase에 포함되지 않습니다.

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
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 4를 완료 처리합니다.
- Phase 5 pointer interaction and animation 세부 문서를 작성합니다.
