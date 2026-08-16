# Phase 15: N×N Cube Support

## Status

`In progress`

## Objective

3×3×3에 고정된 cube 크기를 **런타임에 선택할 수 있게** 합니다. 지원 범위는 **2 ~ 9**입니다.

Geometry, picking, net 렌더링, snap animation은 이미 `CubeState`의 size로 파라미터화되어 있고 고정 크기는 `Application.cpp`의 `kCubeSize` 하나에만 남아 있으므로, 실제 작업은 **크기 변경 시의 상태 재구성과 그 주변부**입니다. 주변부가 이 phase의 대부분입니다: 안쪽 slice를 포함하는 scramble, 바깥 면 외의 layer를 돌릴 UI, Phase 12가 실패 경로로 남겨 둔 numbered 표기, 크기를 싣는 공유 payload, 3×3에서 손으로 유도된 상수 두 개(관람 패턴의 `M'`, 고리 다이어그램의 slot 간격), 그리고 cubie 수가 세제곱으로 늘어날 때의 software rendering 성능입니다.

부하가 크기로 조절되므로 **renderer의 한계를 보여 주는 자리**이기도 합니다. 9×9는 729 cubie와 486 sticker이고, 세 view를 함께 켜면 한 frame이 그리는 도형이 3×3의 아홉 배가 됩니다.

## Scope

- 크기 선택 ABI (`set_cube_size` / `cube_size`)와 크기 변경 시의 상태 재구성
- 안쪽 layer를 포함하는 scramble 생성 (big cube 관례대로 wide move)
- 임의의 layer 구간을 돌리는 turn ABI와, 그것을 쓰는 depth/wide UI
- Numbered 표기 (`2R`, `Rw`, `3Rw`, `2-3Rw`)와 3×3 전용 `M`/`E`/`S`의 공존
- 공유 payload version 2: 크기를 싣고, 크기에 맞춰 mask를 검증
- 관람 패턴을 크기와 무관한 표로 교체하고 모든 지원 크기에서 복귀 상한 검증
- 고리 다이어그램의 slot 간격을 손으로 유도한 상수 대신 배치에서 계산
- 세션 기록에 크기를 함께 남기고 최고 기록은 크기별로 비교
- 최대 크기에서의 rendering 성능 측정과 문서화

## Out of scope

- **N×N solver** — Phase 16의 몫입니다. 이 phase의 solve는 Phase 11의 되감기 그대로이고, 되감기는 크기를 모르므로 모든 크기에서 이미 동작합니다(아래 "Solve와 Phase 16" 참조).
- **회전 표기(`x`/`y`/`z`)와 전폭 move** — 전체 layer를 한 번에 돌리는 move는 만들 수도, 표기할 수도, 링크로 들여올 수도 없습니다. 회전은 cube를 푸는 일과 무관하고, 표기 체계를 하나 더 끌고 옵니다.
- **크기별 solver·기록의 영속화** — 기록은 Phase 14대로 세션 메모리에만 남습니다.
- **Drag로 wide move 만들기** — 손가락 하나가 붙잡는 것은 언제나 layer 하나입니다. Wide는 명시적 설정이 있는 버튼·키보드의 것입니다.
- **크기별 scramble 규격(WCA)** — 여기서 만드는 것은 "충분히 섞인 큐브"이지 대회 규격이 아닙니다.

## Architecture decisions

### 크기는 한 곳에 산다

`kCubeSize` 상수를 없애고, 크기의 유일한 원본을 `state->cube_state.size()`로 둡니다. `CubeState`가 이미 크기를 들고 있으므로 `ApplicationState`에 필드를 하나 더 두면 두 값이 어긋날 수 있는 자리가 생깁니다 — cube를 새로 만들면서 필드를 갱신하지 않는 실수가 표현 가능해집니다. 파생값 하나면 그 실수가 표현 불가능해집니다.

`InteractionController`만은 생성자에서 크기를 받아 들고 있습니다. 이것은 복제가 아니라 **다른 객체의 불변식**입니다(controller는 자기 크기 밖 layer를 거절해야 하고, cube를 참조하지 않습니다). 그래서 크기가 바뀌면 controller를 **다시 만듭니다** — setter를 두면 "gesture가 진행 중인데 크기가 바뀐" 상태가 표현 가능해지는데, 재생성은 그 상태를 만들 수 없습니다.

### 크기 변경은 새 세션이다

```text
set_cube_size(size) -> bool
cube_size() -> int
```

- 범위 밖(2 미만, 9 초과)은 **거절**하고 아무것도 바꾸지 않습니다. 현재 크기와 같은 값은 **성공이되 no-op**입니다 — `set_view_mode`가 이미 같은 관례입니다. 같은 크기를 다시 고른 것은 명령이 아니라 확인이고, 그것으로 판이 리셋되면 놀랍니다.
- 바뀌는 것: playback 폐기(`discard_playback`), `cube_state` 재생성, `interaction` 재생성, `timeline.clear()`. 즉 **`reset_cube()`가 하는 일 + 크기**입니다. 크기가 다른 cube에 예전 기록을 물릴 방법이 없으므로 기록을 옮기는 선택지는 없습니다.
- 바뀌지 않는 것: 시점(orbit), view mode, flat style, palette, 속도 배율. **보는 방법은 언제나 사용자의 것**이라는 규칙이 Phase 10·11에서 이미 여러 번 쓰였고, 크기 변경도 예외가 아닙니다.
- Web에서는 cube command로 다룹니다: 관람 중이면 먼저 관람을 끝내고, 크기를 바꾸고, 한 frame 그리고, session을 `restart()` 합니다. Reset과 같은 경로이고 다른 것은 크기 하나입니다.

### Scramble은 big cube 관례를 따른다

`make_scramble`이 layer를 고르는 방식이 `layer 0 또는 size-1` 두 갈래였습니다. 이대로 5×5를 섞으면 **안쪽 slice가 solved인 채로 남아** 섞였다고 할 수 없습니다.

새 규칙은 실제 big cube scramble의 관례 그대로입니다: 축을 고르고(직전 축 금지, 기존과 동일), 양 끝 면 중 하나를 고르고, **깊이 `d ∈ [1, ⌊N/2⌋]`를 골라 `d = 1`이면 그 면 한 겹, `d > 1`이면 그 면에서 `d`겹을 함께(wide)** 돌립니다.

- **3×3에서는 `⌊3/2⌋ = 1`이라 결과가 지금과 똑같습니다** — 바깥 면 단일 layer만 나오고, 표준 3×3 scramble에 슬라이스가 섞이지 않습니다. 선택지가 하나뿐일 때는 난수를 뽑지 않으므로 3×3의 seed→수순 대응도 글자 그대로 보존됩니다.
- Wide move를 쓰면 안쪽 slice가 실제로 섞이고, `2R` 같은 단독 안쪽 slice를 따로 뽑지 않아도 됩니다(`Rw R'`가 그것이므로 도달 가능한 상태 집합은 같습니다).
- Seed는 공유 payload에 실리지 않으므로(Phase 14) 이 변경은 **어떤 기존 링크도 깨지 않습니다.** 그것이 seed를 싣지 않기로 한 결정이 산 것입니다.

### Numbered 표기: 구간을 찾고, 가까운 면에서 센다

Phase 12는 3×3의 9칸 표에서 멈췄고 그 밖의 mask를 전부 실패 경로로 두었습니다. 소비자가 여기서 처음 생기므로 여기서 채웁니다.

읽는 순서는 하나입니다.

1. `mask`가 이 cube의 **연속 구간** `[lo, hi]`인지 본다. 아니면 표기 없음(null).
2. 구간의 폭이 `N`이면(전폭) 표기 없음. 그것은 회전이고, 이 phase의 밖입니다.
3. `N == 3`이고 단일 layer이면 **Phase 12의 9칸 표**(`R`/`M`/`L`, `U`/`E`/`D`, `F`/`S`/`B`)를 그대로 씁니다.
4. 그 밖에는 numbered 규칙으로 조립합니다.

기준 면은 **구간에 가까운 쪽**입니다. 축 X의 양의 끝이 `R`, 음의 끝이 `L`이고, layer `i`의 깊이는 양의 면에서 `N - i`, 음의 면에서 `i + 1`입니다. 두 깊이가 같으면(구간이 정확히 가운데) 양의 면(`R`/`U`/`F`)을 씁니다.

기준 면에서 잰 깊이 구간을 `[a, b]`라 하면 조립은 다섯 칸입니다.

| 구간 | 표기 | 예 |
| --- | --- | --- |
| `a = 1, b = 1` | `R` | 바깥 한 겹 |
| `a = 1, b = 2` | `Rw` | 바깥 두 겹 |
| `a = 1, b ≥ 3` | `{b}Rw` | `3Rw` |
| `a = b ≥ 2` | `{a}R` | `2R` |
| `a ≥ 2, b > a` | `{a}-{b}Rw` | `2-3Rw` |

부호 규약은 표기 문자를 고르는 곳에 그대로 남습니다: 음의 면(`L`/`D`/`B`)에서 센 표기는 회전 부호가 뒤집힙니다. Phase 12의 표에 있던 `inverted` 칸과 같은 규칙이고, `M`/`E`/`S`의 방향 규약도 그 표에 남아 있으므로 이 phase가 건드리지 않습니다.

**`M`/`E`/`S`를 일반 규칙으로 통합하지 않습니다.** 5×5의 가운데 layer를 `M`으로 쓰는 관행은 없고, 3×3의 `M`을 `2R`로 쓰는 관행도 없습니다. 두 표기는 서로 다른 크기의 관행이므로 크기로 갈리는 것이 맞습니다.

**`2-3Rw`는 버튼이 만들지 못합니다.** 아래의 depth/wide UI가 만드는 것은 `[d, d]`와 `[1, d]` 두 모양뿐이고, `[2, 3]`은 turn ABI와 공유 링크로만 도달합니다. 그런데도 규칙에 넣는 이유는, 구간을 찾아 놓고 "시작이 1이 아니면 null"을 따로 쓰는 편이 더 많은 코드이기 때문입니다 — 일반 규칙이 특수 규칙보다 짧은 드문 경우입니다.

### 안쪽 layer를 돌리는 UI: depth와 wide

Drag는 이미 어느 layer든 돌립니다. 3D cube에서든 전개도에서든 고리에서든, 집은 cubie가 속한 layer가 도는 것이 Phase 5·8·8.5의 규칙이고 거기에 "바깥이어야 한다"는 조건이 없었습니다. **그래서 이 phase가 UI에서 새로 만드는 것은 버튼과 키보드뿐입니다.**

Turn ABI를 layer 구간으로 일반화합니다.

```text
turn_face(face, first_depth, last_depth, turns) -> bool
```

`face`에서 잰 깊이 `first_depth ..= last_depth`(1부터)를 함께 돌립니다. `turn_face(Right, 1, 1, 1)`이 예전의 `R`이고, `(Right, 1, 2, 1)`이 `Rw`, `(Right, 2, 2, 1)`이 `2R`입니다. 깊이를 mask로 바꾸는 계산이 **면에서 센 깊이 → layer index** 한 곳에 모이고, 표기가 읽는 방향과 정확히 역이라 두 쪽을 같은 표로 검토할 수 있습니다.

- 구간이 cube를 벗어나거나(`last_depth > N`), 뒤집혔거나(`first > last`), 전폭(`last - first + 1 == N`)이면 거절합니다. 전폭 거절은 표기와 payload의 규칙과 같은 값이라, "만들 수 있는 move는 전부 표기할 수 있다"가 세 곳에서 같은 말로 유지됩니다.
- UI는 move 패널에 **Depth 숫자 입력(1 ~ N-1)**과 **Wide 토글**을 둡니다. Wide가 켜지면 `[1, depth]`, 꺼지면 `[depth, depth]`입니다. 여섯 개의 면 버튼과 여섯 개의 키보드 문자가 모두 이 설정을 그대로 씁니다 — 설정이 화면에 보이므로 "지금 누르면 무엇이 돌아가는지"가 눌러 보지 않아도 읽힙니다.
- 크기가 바뀌면 Depth의 상한이 따라 바뀌고 현재 값은 상한으로 clamp됩니다. 3×3에서 depth 2로 두고 2×2로 내리면 1이 됩니다.
- `Depth = 1, Wide = off`가 기본값이고, 그 상태의 동작은 지금과 완전히 같습니다.

키보드 숫자 접두(`2` → `R`)는 기각했습니다. 화면에 상태가 남지 않아 다음 키가 무엇을 할지 볼 수 없고, "접두가 살아 있는 동안"이라는 시한부 상태를 하나 더 만듭니다.

### 공유 payload version 2

Phase 14가 예고한 대로 version을 올립니다. 크기가 payload에 없으면 받는 쪽이 어떤 cube에 mask를 얹어야 할지 알 수 없습니다.

```text
payload = version(1) ‖ size(1) ‖ scramble_count(4) ‖ packed(4) × scramble_count
          ‖ user_count(4) ‖ packed(4) × user_count
```

- `SHARE_VERSION = 2`. **v1 링크는 조용히 버려지고 새 cube로 엽니다** — Phase 14가 "알 수 없는 version은 버린다, 크기가 달라지면 version을 올린다"를 미리 정해 두었으므로 migration 코드가 없습니다.
- 크기는 1바이트입니다. 지원 범위가 2 ~ 9이고 packed mask가 28 layer까지밖에 담지 못하므로, 이 필드가 넓어질 이유는 영원히 없습니다.
- Mask 규칙이 "단일 layer"에서 **"이 크기의 연속 구간, 전폭 제외"**로 바뀝니다. 즉 **표기 가능한 mask**와 같은 집합입니다. 규칙이 표기·turn ABI·payload 세 곳에 있는 것은 Phase 14와 같은 구조(TS decode와 engine이 같은 규칙을 각자 검사)이고, 규칙의 내용이 하나로 정리된 것이 이 phase의 개선입니다.
- 복원 ABI는 크기를 함께 받습니다: `restore_apply(size, scramble_count, user_count)`. 크기를 따로 `set_cube_size`로 먼저 세우면 "크기는 바뀌었는데 기록은 거절된" 중간 상태가 생기는데, 한 호출로 받으면 Phase 14의 "전부 검증한 뒤에 전부 적용" 성질이 크기까지 덮습니다. 거절되면 크기도 그대로입니다.
- 왕복 test는 Phase 14의 범위 그대로이되 크기가 추가됩니다: cube 상태 동치 + 적용 구간 동치 + **크기 동치**.

### 관람 패턴은 크기를 타지 않는 표로 바꾼다

Phase 10의 표에는 `R U M' F`가 있습니다. `M'`은 `⌊N/2⌋` 번째 layer라 **짝수 크기에는 존재하지 않고**, 홀수 크기에서도 3×3의 `M`과 같은 것이 아닙니다. 그리고 나머지 세 패턴도 크기가 커지면 위수가 달라집니다 — 바깥 면만 도는 수순이라도 큰 cube에서는 wing과 center piece가 더 긴 궤도를 만들기 때문입니다.

표를 **바깥 면 네 수**로만 이루어진 것으로 갈고, 모든 지원 크기에서 위수를 계산해 골랐습니다.

| 패턴 | 2×2 | 3×3 | 4×4 이상 |
| --- | --- | --- | --- |
| `R U' D' F` | 18 | 72 | 72 |
| `R U F D` | 45 | 90 | 180 |
| `R B D' L2` | 30 | 60 | 60 |
| `R L' U F2` | 36 | 180 | 180 |

(4×4 이상에서 값이 일정한 것은 궤도가 그때 이미 포화되기 때문입니다. 9×9도 같습니다.)

기존 표의 `R U F' D`는 3×3에서 77이지만 4×4 이상에서 308이라 탈락했습니다 — `kAmbientMaxPeriod = 200`을 넘고, 300ms/수로 6분이 넘어 "관람 한 번 안에 처음 상태를 지나간다"는 Phase 10의 약속을 지키지 못합니다.

`kAmbientMaxPeriod = 200`은 그대로 두되, 그 성질을 **모든 지원 크기 × 모든 패턴**에서 검사합니다. 상한이 3×3만의 사실이었던 것이 이 phase에서 여덟 크기의 사실이 됩니다. `ambient_pattern`은 크기를 인자로 받아 값으로 돌려줍니다 — 크기마다 다른 표를 static으로 들고 있을 이유가 없고, 관람 시작은 frame 경로가 아닙니다.

### 고리 다이어그램의 간격은 배치에서 계산한다

`kRingsSlotSpacing = 0.248`은 "54개 slot 중 가장 가까운 두 개의 거리"를 **3×3에서 손으로 구해 적어 둔 값**입니다. Sticker 크기, 선 두께, press 반경이 전부 이 값에 비례하므로, 크기가 바뀌면 sticker가 서로를 덮거나 반대로 헐거워집니다.

layer별 반지름은 이미 크기로 파라미터화되어 있습니다(`layer_radius`: 중앙 layer가 1, 양끝이 `1 ± 0.24`). 크기가 커지면 같은 띠 안에 반지름이 더 촘촘히 들어가므로 최소 간격은 실제로 줄어듭니다. 그래서 상수를 지우고 **크기별로 실제 최소 slot 거리를 계산**합니다.

- 계산은 그 크기의 모든 slot 위치를 놓고 가장 가까운 쌍을 찾는 것입니다. 9×9면 486개 slot이라 쌍이 12만 개 남짓이고, **크기당 한 번만** 계산해 캐시합니다(지원 크기가 여덟 개뿐이라 캐시는 배열 하나입니다). Frame 경로에는 조회만 남습니다.
- 3×3에서의 계산 결과가 기존 상수 `0.248`과 일치한다는 것을 test로 고정합니다. 손으로 유도한 값이 계산과 맞는다는 확인이자, 이 phase가 3×3의 그림을 바꾸지 않았다는 증거입니다.
- 반지름 띠(`kRingRadiusOffset = 0.24`)는 그대로입니다. 그 값을 정한 조건은 "한 축의 바깥 원이 다른 축의 안쪽 원 안으로 들어가지 않을 것"이라 **양 끝 반지름만의 문제**이고, 그 사이에 원이 몇 개 더 들어가는지와는 무관합니다.
- 큰 cube에서 고리 그림이 빽빽해지는 것은 그대로 받아들입니다. 고리 그림은 "한 수가 세 칸 이동"이라는 구조를 보여 주는 그림이고, 9×9에서 그 구조는 실제로 빽빽합니다.

### 기록은 크기를 함께 적는다

크기가 다른 두 판의 시간을 견주는 것은 의미가 없습니다. `SolveRecord`에 `cubeSize`를 더하고, 목록 항목에 `5×5`를 함께 적고, **최고 기록은 크기별로** 봅니다(크기 → 기록의 map). 크기를 바꿔도 목록은 지워지지 않습니다 — 그 사람이 이 자리에서 푼 판들이라는 사실은 크기와 무관하게 남습니다.

### Solve와 Phase 16

계획서가 이 phase에서 정하라고 한 것: **solve는 모든 크기에서 Phase 11의 되감기 그대로입니다.**

되감기는 timeline에 적힌 move를 거꾸로 재생하는 일이고 cube를 들여다보지 않으므로 크기를 모릅니다. 그래서 9×9에서도 지금 그대로 동작하고, 이 phase는 solve에 손대지 않습니다. Phase 16의 solver는 되감기의 대체가 아니라 **기록이 없는 cube(직접 섞은 것, 링크로 받은 것)를 위한 두 번째 생산자**이고, 지원 크기를 스스로 밝히므로 5×5에서 "이 크기를 푸는 구현이 없다"가 그대로 드러납니다. Solve 버튼의 의미는 그때도 바뀌지 않습니다.

### 성능

최대 크기(9×9)에서 세 view를 모두 켠 상태의 frame 시간을 측정해 Completion에 적습니다. 측정 결과가 상호작용을 해칠 정도면 지원 상한을 낮추는 것으로 대응합니다 — 상한은 숫자 하나이고 그것을 읽는 곳이 검증 함수 하나뿐이므로, 되돌리는 비용이 거의 없는 결정입니다.

## ABI

```text
thorvg_rubiks_set_cube_size(size: int) -> int          // 0 = 거절 (범위 밖)
thorvg_rubiks_cube_size() -> int
thorvg_rubiks_turn_face(face: int, first_depth: int, last_depth: int, turns: int) -> int
thorvg_rubiks_restore_apply(size: int, scramble_count: uint32, user_count: uint32) -> int
```

## Implementation steps

### 1. 크기를 런타임 값으로

- [ ] `cube` 도메인: `make_scramble`이 깊이와 wide를 뽑도록 확장 (3×3 결과 불변)
- [ ] `Application`: `kCubeSize` 제거, 크기를 `cube_state.size()`에서 파생, `set_cube_size`/`cube_size` 추가
- [ ] `turn_face`를 layer 구간 형태로 확장하고 거절 규칙(범위·역순·전폭)을 붙임
- [ ] 관람 패턴 표 교체와 `ambient_pattern(choice, size)`
- [ ] Native test: 크기 변경이 cube·기록·playback을 재구성하고 시점·view·palette·속도를 보존하는지, 범위 밖 거절이 상태를 바꾸지 않는지, 같은 크기가 no-op인지
- [ ] Native test: scramble이 모든 지원 크기에서 안쪽 slice를 섞고 3×3에서는 예전과 같은 수순인지, 역수순으로 복원되는지
- [ ] Native test: 모든 패턴 × 모든 지원 크기에서 `kAmbientMaxPeriod` 안에 복귀
- [ ] Native test: 크기 2와 9에서 drag·snap·commit이 3×3과 같은 계약을 지키는지 (picking, net, rings 각각 한 번씩)

### 2. 표기와 그림

- [ ] 고리 간격을 배치에서 계산하고 캐시, `kRingsSlotSpacing` 제거
- [ ] Native test: 계산된 3×3 간격이 예전 상수와 일치, 각 크기에서 sticker가 서로 겹치지 않음
- [ ] TS `moveNotation`: 구간 판정 + numbered 조립, 3×3 단일 layer는 기존 표
- [ ] TS unit test: 표기 표 전체(`R`/`2R`/`Rw`/`3Rw`/`2-3Rw`/`M`·`E`·`S`), 부호 규약, 비연속·전폭·범위 밖 mask가 null

### 3. 공유와 UI

- [ ] Payload v2(크기 필드)와 mask 규칙 확장, `restore_apply(size, ...)`
- [ ] TS/Native test: 크기를 포함한 왕복, v1 payload 거절, 크기 밖 mask 거절, 비연속·전폭 mask 거절, v2 known-answer fixture
- [ ] Web: 크기 입력, depth 입력, wide 토글, 크기 변경 시 depth clamp와 session restart
- [ ] Web: `CUBE_SIZE` 상수 제거하고 `engine.cubeSize()` 사용 (move log, 공유 검증)
- [ ] 기록에 크기 추가와 크기별 최고 기록
- [ ] TS unit test: 크기 변경 명령의 경로(관람 종료 → 크기 → 렌더 → restart), 거절된 입력의 되돌림, 기록의 크기별 비교
- [ ] e2e: 크기를 바꿔 섞고 돌리고 되감기, 큰 cube의 공유 왕복, scene contract의 크기 파라미터화
- [ ] 9×9 frame 시간 측정과 문서화
- [ ] Native, WASM, TypeScript unit, e2e, production build 전체 실행

## Acceptance criteria

- 2 ~ 9의 모든 크기를 런타임에 고를 수 있고, 크기를 바꾸면 cube와 기록이 새로 시작하며 시점·view mode·flat style·palette·속도는 보존됩니다. 범위 밖 값은 거절되고 상태를 바꾸지 않습니다.
- 크기는 `CubeState` 하나에만 있습니다. `kCubeSize`도, 크기를 복제한 필드도 남지 않습니다.
- Scramble은 모든 크기에서 안쪽 slice까지 섞고, 3×3에서는 이 phase 이전과 같은 수순을 냅니다.
- 버튼·키보드로 임의의 깊이와 wide를 돌릴 수 있고, drag는 모든 크기에서 어느 layer든 돌립니다. 전폭 move는 어느 경로로도 만들 수 없습니다.
- 만들 수 있는 move는 전부 표기할 수 있습니다: turn ABI가 받아들이는 mask 집합, 표기가 문자열을 내는 mask 집합, payload가 싣는 mask 집합이 같습니다.
- 3×3의 표기는 `M`/`E`/`S`를 포함해 이 phase 이전과 완전히 같고, 다른 크기는 numbered 표기를 씁니다.
- 공유 링크가 크기를 함께 나르고, 받는 쪽이 같은 크기의 같은 cube를 얻습니다. v1 링크는 새 cube로 열립니다.
- 관람 패턴은 모든 지원 크기에서 `kAmbientMaxPeriod` 안에 처음 상태로 돌아오며, 그것이 test로 고정되어 있습니다.
- 고리 다이어그램의 sticker 크기와 press 반경이 크기에 따라 계산되며, 3×3의 그림은 이 phase 이전과 같습니다.
- 기록 항목은 크기를 밝히고, 최고 기록은 같은 크기끼리만 견줍니다.
- 최대 크기에서의 frame 시간이 측정되어 문서에 남아 있습니다.
- Native, WASM, TypeScript unit, browser e2e와 production build가 모두 통과합니다.

## Verification commands

```bash
meson test -C build/native --print-errorlogs
source /path/to/emsdk/emsdk_env.sh && ./build_wasm.sh
npm --prefix web run test:unit
npm --prefix web run test:e2e
npm --prefix web run build
```
