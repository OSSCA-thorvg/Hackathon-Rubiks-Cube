# Phase 12: Move Notation and Move Log

## Status

`Completed`

## Objective

Phase 11의 timeline을 표준 표기법으로 렌더링해 수순 목록으로 보여 줍니다.

별도의 기록 자료구조를 만들지 않고 Phase 11의 timeline을 그대로 읽으므로, 이 phase의 engine 작업은 move를 인덱스로 조회하는 ABI와 pack 시점의 turns 정규화에 한정됩니다. C ABI는 primitive type만 사용하므로 engine은 move를 packed integer로 넘기고, 표기 문자열 조립은 TypeScript가 담당합니다. 표기는 move의 성질이 아니라 표현이므로, 표기 스타일이 바뀌어도 engine에 닿지 않습니다.

## Scope

- C ABI: `timeline_move(index)` — move 한 개를 packed `uint32`로 반환
- Packed 포맷의 명시적 정의와 양쪽(C++/TS) 상수 고정, pack 시점의 turns 정규화
- TS `notation` module: packed → `CubeMove` 구조 → 3×3 표기 문자열(`R`, `R'`, `R2`)
- 3×3 가운데 layer의 M/E/S 표기
- Move log UI: timeline 전체 목록, cursor 위치 강조, commit마다 갱신
- Native/TS unit test와 e2e

## Out of scope

- 문자열 notation parser와 algorithm 입력 — 표기는 출력 전용입니다
- **N×N numbered 표기(`Rw`, `3Rw`, `2R`, `2-3Rw`)와 그 유도에 필요한 mask 구간 판정** — 제품이 3×3뿐인 동안은 만들 수 없는 표기이고, N×N 자체가 Phase 15입니다. 구간 판정도 함께 Phase 15로 미룹니다. 소비자가 그때 처음 생기고, 3×3의 변환은 9칸 표로 끝나기 때문입니다
- 수순 목록의 클릭 탐색(cursor jump) — 되감기 UI는 Phase 11의 undo/redo/solve로 충분합니다
- History 직렬화 (Phase 14)

## Architecture decisions

### Packed move 포맷

```text
bits 0-1  axis          (0 = X, 1 = Y, 2 = Z)
bits 2-3  turns code    (0 = -1, 1 = +1, 2 = +2)
bits 4-31 layer mask    (bit 4 = layer 0, ... 최대 28 layers)
```

- Quarter turn은 **pack 시점에** `{-1, +1, +2}`로 정규화합니다(mod 4, `±2`는 `+2`, `+3`은 `-1`). Timeline 자체는 Phase 11의 계약대로 raw 수순을 보관합니다 — 기록 시점에 정규화하면 `-180°`로 돈 수가 `+2`로 저장되어 redo 재생이 반대 방향으로 돌고, "재진행은 되감기의 정확한 역순"이라는 Phase 11의 성질이 깨집니다. 표기와 공유에는 회전 방향이 무의미하므로(같은 결과 = 같은 표기), 방향을 지우는 일은 방향을 쓰는 소비자(재생)가 지나가지 않는 pack에서만 합니다.
- Layer mask는 항상 0이 아니므로 packed 값도 0이 될 수 없습니다. 따라서 **0이 곧 invalid index sentinel**입니다.
- 포맷 상수는 C++와 TS 양쪽에 정의하고, 같은 값을 쓰는지 fixed known-answer(예: `R` = axis X, turns +1, mask layer 2)를 양쪽 test로 고정합니다.

```text
thorvg_rubiks_timeline_move(index: uint32) -> uint32   // 0 = invalid index
```

Phase 11의 기록이 timeline 하나이므로 조회도 하나입니다. 목록은 인덱스 순서 그대로이고, scramble 구간과 사용자 구간의 구분은 `timeline_scramble_end` 하나에서 파생됩니다 — 이어 붙이는 코드도, 경계를 계산하는 코드도 없습니다.

### TS notation module

**3×3에서 만들어질 수 있는 mask는 단일 layer 세 개뿐입니다.** 그래서 변환은 구간을 찾는 알고리즘이 아니라 **9칸짜리 표 하나**입니다: `(axis, layer) → {문자, 부호}`.

| axis | layer 0 | layer 1 | layer 2 |
|---|---|---|---|
| X | `L`, 부호 반전 | `M`, 부호 반전 | `R`, 그대로 |
| Y | `D`, 부호 반전 | `E`, 부호 반전 | `U`, 그대로 |
| Z | `B`, 부호 반전 | `S`, 그대로 | `F`, 그대로 |

- **음의 face 부호 반전과 M/E/S 방향 규약이 전부 이 표의 부호 칸에 들어갑니다.** 별도의 분기도, "축의 어느 끝에 붙었는지"를 판정하는 코드도 없습니다. 표기는 문자 + turns 접미사(`''`, `'`, `2`)로 끝납니다.
- **표에 없는 mask는 예외 없이 null입니다.** 여러 layer, 불연속, 범위 밖이 모두 같은 경로입니다. 다만 3×3의 입력 경로가 단일 layer만 만들므로 **이 null을 화면에 그리는 코드는 두지 않습니다** — 도달할 수 없는 표시를 위해 UI에 분기를 만들 이유가 없고, 표기가 필요해지는 것은 Phase 15에서입니다. 함수의 반환 타입에만 남깁니다.
- **연속 구간 `[first, last]`를 찾는 뼈대는 이 phase에서 만들지 않습니다.** 그 코드의 유일한 소비자는 numbered 표기(`Rw`, `2-3Rw`)이고 그것은 Phase 15의 일인데, Phase 15는 계획의 마지막 구간에 있어 오지 않을 수도 있습니다. 3×3만 있는 동안 구간 탐색은 입력이 세 가지뿐인 함수를 일반화한 것에 지나지 않습니다. 함수가 크기를 인자로 받아 두므로, Phase 15는 위의 실패 경로에 구간 판정과 numbered 조립을 함께 얹으면 됩니다.
- 순수 함수 module로 두고 DOM을 만지지 않습니다. 표가 9칸이라 3축 × 3layer × 3turns = 27개를 전수 test로 고정할 수 있습니다.

### Move log UI

- `GameController`가 frame 후 관찰에서 세 query(`timeline_length`/`timeline_cursor`/`timeline_scramble_end`)의 변화를 감지하면 목록을 다시 그립니다. 목록 항목은 index → `timeline_move` → notation의 파이프라인입니다.
- 적용된 위치(`timeline_cursor`)를 강조해 undo/redo/solve가 어디까지 되감았는지 보여 줍니다. 그 뒤(재진행 가능 구간)는 흐리게 표시하며, scramble 구간이 반쯤 되감긴 경우도 같은 방식으로 자연히 표현됩니다.
- Scramble 구간과 사용자 구간은 `timeline_scramble_end` 앞뒤로 시각적으로 구분합니다. `GameController`가 scramble 직후의 길이를 기억할 필요가 없고, 공유 URL로 복원된 세션(Phase 14)도 같은 query를 읽으므로 live와 표시가 같습니다.
- 목록은 `aria-live`로 읽지 않습니다. 스크린 리더에는 마지막 move 하나가 아니라 목록 자체가 탐색 가능한 semantic list로 제공됩니다.

## Implementation steps

### 1. Engine ABI

- [x] Packed 포맷 상수와 pack 함수, pack 시점 turns 정규화
- [x] `-2`로 기록된 수가 packed로는 `+2`, redo 재생으로는 `-180°` 그대로인 test (정규화가 timeline에 새지 않는지)
- [x] `timeline_move(index)` 구현: 범위 밖·초기화 전 0 반환
- [x] Pack known-answer test와 정규화 test (native)

### 2. TS notation

- [x] Unpack과 포맷 상수 known-answer test (C++ 결과와 대조)
- [x] `(axis, layer) → {문자, 부호}` 9칸 표와 turns 접미사 조립
- [x] 표에 없는 mask(여러 layer, 불연속, 범위 밖)의 null 경로
- [x] 전수 test: 3축 × 3layer × 3turns = 27개 표기와, 표 밖 mask의 null 경로

### 3. Move log UI

- [x] Timeline 목록 렌더링과 적용 위치 강조, `timeline_scramble_end` 기준의 구간 구분 표시
- [x] Scramble이 반쯤 되감긴 상태의 표시 test
- [x] Commit/undo/redo 후 갱신 test (TS unit)
- [x] e2e: scramble 후 목록에 scramble 수순이 보이고, drag move가 표기로 추가되고, undo 시 cursor 강조가 이동
- [x] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- Engine의 추가 작업은 packed 조회 ABI와 pack 시점의 turns 정규화뿐이고, Phase 11의 timeline 외에 새 자료구조가 없습니다. Timeline의 raw 보관 계약은 이 phase가 건드리지 않습니다.
- 두 구간의 구분은 `timeline_scramble_end`에서 파생되며, 이어 붙이거나 경계를 계산하는 코드가 어디에도 없습니다.
- C ABI는 계속 primitive type만 사용하며, 문자열은 boundary를 넘지 않습니다.
- 같은 packed 값은 C++와 TS에서 같은 move로 해석되고, known-answer test가 양쪽에서 고정합니다.
- Drag로 만든 가운데 layer move가 M/E/S로, 바깥 face move가 표준 표기로 정확히 표시됩니다.
- 9칸 표에 없는 mask는 예외 없이 null을 반환하며, 구간 판정 코드도 그 null을 그리는 UI 분기도 이 phase에 존재하지 않습니다.
- 수순 목록이 cursor 위치와 redo 구간을 구분해 보여 주고, 표기 스타일 변경이 engine 코드에 닿지 않습니다.
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
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 12를 완료 처리합니다.
- 실제 구현과 차이가 생긴 결정을 이 문서에 기록합니다.

## 개정 기록

### 1. 구현하며 달라진 것

**Pack은 `cube::pack` 자유 함수이고, 포맷 상수와 함께 `cube/PackedMove.hpp`에 있습니다.** 문서는 위치를 정하지 않았고 `CubeMove.hpp`에 얹을 수도 있었지만, 이 파일은 "move란 무엇인가"를 말하는 자리이고 packed 포맷은 **경계를 건너는 방법**입니다. 별도 header로 두면 비트 배치·정규화 규칙·`0 = invalid` 약속이 한 곳에 모여, 반대편 TS 상수가 마주 보는 대상이 파일 하나가 됩니다. Header-only `constexpr`이라 known-answer 하나는 `static_assert`로 고정되어 test를 돌리기 전에 컴파일이 막습니다.

**"`-2`가 redo에서 `-180°` 그대로"는 cube 레벨에서 고정했습니다.** ±180°는 **같은 permutation**이라 상태로도 그림으로도 구분되지 않습니다(구분되는 것은 애니메이션 중간 frame뿐입니다). 그래서 app 레벨에서 재생 결과를 비교하는 대신 `tests/cube/PackedMoveTest.cpp`에서 **Player에게 건네지는 계획 자체**를 봅니다: `redo_plan`이 돌려주는 move의 `quarter_turns`가 여전히 `-2`이고 `pack`은 같은 것을 `+2`로 적습니다. 재생이 어느 쪽으로 도는지는 그 계획이 정하므로, 이것이 "정규화가 기록에 새지 않는다"의 관찰 가능한 전부입니다. App 레벨에는 `turn_face(Left, 2)`가 `0x18`로 읽히는 것만 남겼습니다.

**`timelineMove`는 `countFrom`을 쓰지 않습니다.** Phase 11이 uint32 query 검증을 helper 하나로 모았지만, 그 helper는 **음수를 boundary 오류로 거절**합니다. Packed move는 개수가 아니라 비트 패턴이고 최상위 bit도 layer이므로, i32 경계가 음수로 넘긴 값을 거절하는 대신 `>>> 0`으로 되읽는 것이 맞습니다. 정수 여부만 확인하고 unsigned로 복원합니다.

**목록은 세 수 중 하나라도 바뀌면 통째로 다시 그립니다.** "cursor만 움직였으면 표시만 옮기면 된다"는 더 싼 읽기는 틀립니다: undo 뒤에 새 move를 두면 redo tail이 잘리고 그 자리에 하나가 들어와, **길이가 그대로인 채 내용이 달라집니다**. 대신 세 수가 모두 같은 frame에서는 아무것도 하지 않으므로, 관람과 시점 sweep이 60fps로 목록을 재생성하지 않습니다.

### 2. 계획에 없던 것

**`CUBE_SIZE`를 `CubeEngine.ts`에 미러링했습니다.** `moveNotation`이 크기를 인자로 받는데 engine은 크기를 ABI로 넘기지 않습니다(3×3 고정 빌드입니다). `MAX_SCRAMBLE_MOVES`와 같은 성격의 미러 상수로 두었고, 크기가 선택이 되는 phase에서는 이 상수 대신 boundary가 답하게 됩니다.

**빈 기록의 안내는 CSS `:empty::after`입니다.** Placeholder 요소를 넣었다 뺐다 하면 목록이 "항목 없음"이라는 항목을 갖게 되고, 그 항목은 index → move → 표기 파이프라인 밖에 있습니다. 표시만 CSS로 옮기면 `<ol>`의 자식은 언제나 기록의 move뿐입니다.

**표기가 없는 move는 분기 없이 그대로 대입합니다.** `item.textContent = moveNotation(...)`이고, `null`을 검사하는 자리가 없습니다 — `textContent`가 `null`을 빈 문자열로 받으므로 "도달할 수 없는 표시를 위해 UI에 분기를 만들지 않는다"가 코드에서도 분기 0줄입니다.

**목록은 stage 아래 자체 section입니다.** 좌우 rail은 한 칸 폭의 control 목록이고, 기록은 페이지에서 유일하게 한없이 길어지는 것입니다. 네 줄 높이에서 스크롤하고, cursor 항목이 늘 보이도록 panel을 따라 스크롤합니다 — 100수 scramble을 되감을 때 움직이는 끝이 화면 밖에 있으면 강조 표시가 무의미하기 때문입니다.

### 3. 확인한 것

**Drag로 만든 가운데 layer가 `E'`로 적히는지**는 e2e에서 net의 front block 가운데 줄을 끌어 확인합니다. 윗줄을 끌면 U가 도는 layer이므로, 그 아래 줄은 같은 방향으로 도는 E의 반대 표기입니다 — 9칸 표의 부호 칸이 브라우저까지 살아 있다는 뜻이고, 이 표기를 만들 수 있는 입력은 drag뿐이라 버튼이나 키로는 대신 확인할 수 없습니다.

### 4. 사용자 요청으로 바꾼 것

**목록에 scramble 수순을 그리지 않습니다.** 문서는 "timeline 전체 목록"에 두 구간을 시각적으로 구분해 보여 주기로 했지만, 20수 scramble 사이에서 자기 수 둘을 찾는 것이 그 둘만 보는 것보다 나쁘다는 판단으로 사용자 수 구간만 그립니다.

**감추는 코드는 없습니다.** 기록이 배열 하나이고 경계가 `timeline_scramble_end` 하나이므로, 필터링이 아니라 **그리는 구간의 시작점이 0에서 `scrambleEnd`로 바뀐 것**입니다. 감춰진 move는 조회조차 되지 않습니다(`timeline_move`가 그 index로 불리지 않습니다). "이어 붙이는 코드도 경계를 계산하는 코드도 없다"는 성질이 이 변경으로 오히려 그대로 쓰였습니다 — 두 구간이 따로 보관돼 있었다면 목록의 재료를 고르는 코드가 필요했을 것입니다.

이에 따라 정리된 것들:

- `data-part` 속성이 사라졌습니다. 그려지는 항목은 전부 사용자 move라 구분할 것이 없습니다. Scramble의 점선 표시는 **되감긴 항목**의 표시로 옮겼습니다 — 지금 큐브에 없는 수를 move의 윤곽으로 그리는 편이 원래 그 표시가 뜻하던 바에 더 맞습니다.
- Cursor가 scramble 구간으로 내려가면(solve 도중·직후) 강조가 **아무 항목에도 붙지 않습니다**. 목록에 그려진 어떤 move도 큐브에 없기 때문이고, `items[cursor - 1 - scrambleEnd]`가 음수 index로 자연히 그렇게 됩니다.
- 새 move가 scramble 경계를 끌어내리면(solve 뒤에 둔 수) 방금 전까지 scramble이던 수가 사용자 구간이 되어 목록에 나타납니다. 경계가 파생값이라 목록이 저절로 따라갑니다.
- Panel 제목이 `Your moves`, 빈 목록 문구가 `No moves of your own yet.`입니다.
