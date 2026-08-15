# Phase 10: Ambient Mode

## Status

`Completed`

## Objective

입력이 없는 동안 큐브가 스스로 도는 관람 모드를 더합니다. Phase 9의 Player에 패턴 하나를 넘기고 반복시키는 일이므로 새 animation 경로도 공급 장치도 만들지 않습니다.

방식은 하나입니다. 위수를 아는 짧은 수순 표에서 하나를 골라 반복합니다. 어떤 수순이든 반복하면 유한한 횟수 뒤에 solved로 돌아온다는 성질을 그대로 보여 주므로 관람은 주기적으로 처음 상태를 지나가고, cube group에서 원소의 최대 위수가 1260이라 반복 횟수의 상한도 근거를 갖습니다. 표의 수순은 그 복귀가 관람 한 번 안에 보이도록(주기 1~2분대) 위수를 계산해 고른 것들입니다.

관람에 들어갈 때 cube 상태를 snapshot하고, 나갈 때 그 상태로 되돌립니다. 관람 중의 move를 timeline에 남기면 자리를 비운 시간에 비례해 timeline이 무한히 자라고, 남기지 않으면 상태와 timeline이 어긋나 이후의 되감기와 복원이 전부 틀어집니다. 되돌리면 둘 다 피할 수 있고, 화면 보호기가 원래 그렇게 동작하므로 부자연스럽지도 않습니다.

## Scope

- Application의 ambient 상태: 진입 시 `CubeState` snapshot, 이탈 시 즉시 복원
- `kAmbientPatterns` 상수 표(수순만)에서 고른 패턴 하나를 Player에 넘기기
- Player에 `loop` 필드 추가 — 소진 시 `next`를 0으로 되돌리는 한 줄
- C ABI: `ambient_start(choice)`, `ambient_stop()`, `is_ambient()`
- Web: 관람 toggle 버튼, 큐브를 바꾸는 명령(Scramble, Reset, face 버튼과 키)으로 즉시 이탈
- 탭이 보이지 않는 동안 frame loop 정지 (`visibilitychange`)
- Native/TS unit test와 e2e

## Out of scope

- 유휴 시간 감지로 자동 진입 — 진입은 명시적 버튼입니다
- 관람 중의 camera 자동 orbit 연출
- 관람 move의 기록 (Phase 11의 timeline과는 snapshot 복원으로 무관해집니다)
- 사용자 정의 반복 수순 — 수순은 상수 표에서만 나옵니다
- 무작위 move의 연속 공급 — 이유는 아래 공급 방식 결정에 있습니다

## Architecture decisions

### Snapshot과 복원

- `ambient_start`는 현재 `CubeState`를 복사해 보관하고 공급을 시작합니다.
- `ambient_stop`은 Phase 9의 원자적 폐기 helper(`drain_orbit()` + `interaction.reset()` + `playback_.reset()`)를 부른 뒤 snapshot을 **애니메이션 없이 즉시** 되돌립니다. 이탈은 원상 복귀이므로 재생할 이유가 없습니다.
- 이탈 시 `user_move_count`는 진입 전 값 그대로입니다. 관람은 세션에 아무 흔적을 남기지 않습니다.
- `reset_cube`, `scramble`, `shutdown`은 ambient를 먼저 끝냅니다(복원 없이 폐기 — 명령이 만드는 새 상태가 우선입니다).

### 공급 방식

`kAmbientPatterns`는 **수순만** 적은 상수 표입니다. 위수는 표에 싣지 않습니다 — 공급 loop는 수순을 순서대로 반복할 뿐이라 런타임에서 위수를 읽는 곳이 없고, 상수로 두면 코드가 쓰지 않는 값을 test가 지키는 구조가 됩니다.

아래 위수는 스크립트로 계산한 **정확한 값**이며, 네 개를 왜 골랐는지를 문서에 남기기 위한 것입니다. 네 면·세 축 이상을 쓰는 수순 중 주기가 관람 한 번 안에 보이는 것들이고, 이 표는 패턴을 더하거나 바꿀 때 같은 기준을 다시 적용할 수 있게 합니다. 코드와 test에는 들어가지 않습니다.

| 패턴 | 위수 | 한 주기 (tempo 300ms) |
|---|---|---|
| `R U M' F` | 60 | 1분 12초 |
| `R U' D' F` | 72 | 1분 26초 |
| `R U F' D` | 77 | 1분 32초 |
| `R U F D` | 90 | 1분 48초 |

- `ambient_start(choice)`는 받은 값을 `choice % 표 크기`로 인덱스화합니다. 다양성은 browser가 아무 uint32나 만들어 넘기는 것으로 얻고, engine은 여전히 스스로 난수원을 갖지 않습니다. Modulo라서 거절 경로도 표 크기를 알려 주는 query도 필요 없고, 같은 choice는 언제나 같은 패턴이라 test가 결정적입니다.
- **공급 장치를 두지 않습니다.** `ambient_start`는 고른 패턴을 그대로 Player의 `plan`으로 넘기고 `loop = true`를 세웁니다. Phase 9의 소비 loop가 `next`를 전진시키다 `plan.size()`에 닿으면 0으로 되돌리는 것이 반복의 전부입니다 — 초안의 "queue가 비면 다음 move를 채우는" 보충 loop, 그 loop가 읽어야 하던 패턴 위치 상태, 이탈 시 남는 잔량 규칙이 모두 없어집니다. Player가 소진되지 않으므로 재생이 스스로 끝나지 않는 것도 관람의 의미와 맞습니다.
- `loop`는 이 phase에서 Player에 추가되는 필드이고 기본값이 `false`라, 기존 생산자(scramble)는 손대지 않습니다.
- 관람 move가 count와 (이후 phase의) timeline을 건드리지 않는 근거는 Phase 9의 재생 판정 그대로입니다. 관람도 재생이므로 `playback_.has_value()`가 true이고, 그것만으로 count에서 빠집니다.
- 반복은 언젠가 snapshot 시점의 상태를 다시 지나갑니다. 이를 **상한 하나로** 고정합니다: 표의 모든 패턴이 `kAmbientMaxPeriod`(상수, 예: 200) 회 이내에 solved로 복귀합니다. 정확한 위수를 적어 두고 대조하면 test가 검증하는 것은 코드가 아니라 산술이고, 표에 패턴을 더할 때마다 손으로 계산한 숫자를 함께 넣어야 합니다. 상한 test는 표가 편집되어도 그대로 문지기 노릇을 하면서, 관람이 주기적으로 처음 상태를 지나간다는 성질만 정확히 지킵니다.
- 무작위 move를 계속 흘려보내는 공급은 두지 않습니다. Seed 주입과 ambient 전용 결정성 test가 더 필요해지는 데 비해, 보이는 것은 똑같이 "그냥 도는 큐브"이고 복귀는 오히려 보이지 않게 되기 때문입니다.
- Ambient tempo는 Player에 관람용으로 여유 있는 상수(90°당 300ms 내외)를 실어 정합니다.

### 진입 조건과 web 연결

- `GameController`는 `idle`과 `completed`에서만 관람 버튼을 활성화합니다. Timer가 준비되었거나 도는 중(`ready`, `running`)의 관람은 세션 의미를 흐리므로 막습니다.
- 관람 중 **큐브를 바꾸는 명령**이면(Scramble, Reset, face 버튼과 키) 먼저 `ambient_stop`을 호출해 복원한 뒤 원래 동작을 수행합니다. **보는 방법을 바꾸는 것**(canvas drag, 2D/3D, flat style, Home view)은 관람을 끝내지 않고, scramble 재생 때와 똑같이 그대로 이어집니다 — 아래 개정 기록 2를 보십시오.
- **관람 toggle 버튼만은 예외로, ambient 중이면 stop하고 거기서 끝냅니다.** "stop 후 원래 동작"을 그대로 적용하면 toggle의 원래 동작이 `ambient_start`라서 관람이 꺼졌다가 곧바로 다시 켜집니다.
- 관람 중 frame loop는 계속 돌아야 하므로 `advance`가 ambient 동안 true를 유지합니다.
- `document.visibilitychange`에서 hidden이면 frame loop를 멈추고 visible이면 재개합니다. **이 listener와 멈춤·재개는 rAF를 실제로 소유한 `AppLifecycle`의 일이고**, GameController나 engine에 새 상태가 생기지 않습니다. Engine은 시계를 읽지 않으므로 멈춤은 그저 `advance`가 호출되지 않는 것이고, 재개 첫 frame의 elapsed는 기존 `startFrameLoop`가 `previousTimestamp`를 비우므로 0입니다 — 자리 비운 시간이 dt로 들어오지 않고, 혹시 모를 큰 dt도 `kMaxFrameMs` clamp가 받습니다.

### C ABI

```text
thorvg_rubiks_ambient_start(choice: uint32) -> int
thorvg_rubiks_ambient_stop() -> void
thorvg_rubiks_is_ambient() -> int
```

- 이미 ambient면 `ambient_start`는 거절합니다.
- `ambient_stop`은 ambient가 아니면 no-op입니다.

## Implementation steps

### 1. Engine

- [x] Snapshot 보관과 즉시 복원 구현
- [x] `kAmbientPatterns` 표와 choice modulo 선택, 고른 패턴을 Player로 넘기는 `ambient_start`
- [x] Player에 `loop` 필드(기본 false) 추가와 소비 loop의 되감기 한 줄, ambient tempo
- [x] `loop` 재생이 소진되지 않고 패턴을 계속 반복하는 test (기존 생산자는 `loop = false`로 그대로 끝남)
- [x] `ambient_stop` 후 상태·count가 진입 전과 동일한 test
- [x] Table-driven test: 표의 모든 패턴이 `kAmbientMaxPeriod` 이내에 solved로 복귀
- [x] Reset/scramble/shutdown이 ambient를 끝내는 test

### 2. Boundary와 UI

- [x] C ABI 세 함수 추가와 generated 산출물, fake fixture 갱신
- [x] 관람 toggle 버튼과 `idle`/`completed` 활성 조건, 진입마다 새 `choice` 주입 (test는 injectable source)
- [x] 큐브를 바꾸는 명령 경로의 선-이탈 처리 (keyboard, 버튼)와 toggle 버튼의 stop-후-종료 예외
- [x] Toggle을 ambient 중에 눌러도 다시 시작되지 않는 test
- [x] `visibilitychange` 연결과 teardown 정리
- [x] TS unit test: 진입 조건, 이탈 경로, visibility 정지
- [x] e2e: 관람 시작 → 픽셀 변화 관찰 → 명령 → 진입 전 전개도와 일치, 그리고 drag는 시점만 돌리고 관람이 이어짐
- [x] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- 관람 중 큐브는 Phase 9의 Player와 같은 animation 경로로 계속 회전하며, 반복은 `loop` 필드 하나로 이루어져 별도의 공급 loop나 패턴 위치 상태가 존재하지 않습니다.
- 큐브를 바꾸는 명령이면 관람을 즉시 끝내며, cube 상태와 move count는 진입 시점과 정확히 일치합니다. 보는 방법을 바꾸는 것(drag, view mode, flat style, Home view)은 관람을 끝내지 않으며, 이는 scramble 재생이 같은 조작에 보이는 반응과 정확히 같습니다.
- 어떤 패턴이 선택되어도 반복 중 주기적으로 진입 시점의 상태를 다시 지나가며, 그 성질은 표의 모든 패턴이 상한 안에 복귀한다는 test 하나로 고정됩니다. 위수는 상수 표에도 test에도 들어가지 않습니다.
- 관람 move는 user move count와 timer 어디에도 흔적을 남기지 않습니다. History와의 관계는 기록이 생기는 Phase 11에서 함께 검증합니다.
- 탭이 보이지 않는 동안 frame loop가 돌지 않습니다.
- 관람할 때마다 표의 패턴 중 하나가 선택되며, 어느 것이 선택되어도 engine은 난수원과 시계를 스스로 갖지 않습니다.
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

모든 acceptance criteria와 verification command를 통과했습니다.

- Native `meson test -C build/native` **16개 전부 통과** (`ambient watching` 하나가 늘었습니다)
- WASM 재빌드, TypeScript unit **97개**, browser e2e **28개**, production build 통과

## 개정 기록

### 1. 구현하며 달라진 것

**Snapshot을 Player 안에 넣었습니다.** 계획은 Application의 상태로 따로 두는 것이었지만, snapshot과 반복 재생은 수명이 정확히 하나입니다 — 관람이 시작될 때 함께 생기고 끝날 때 함께 없어집니다. Player의 주석이 "수명이 같은 두 상태는 한 객체"라고 스스로 정해 둔 것을 그대로 따랐고, 그 덕에 "scramble·reset·shutdown은 복원 없이 ambient를 끝낸다"는 규칙이 **코드 한 줄도 없이** 성립합니다. 그 셋은 이미 `discard_playback()`을 부르고, snapshot이 Player와 함께 버려지는 것이 곧 그 규칙이기 때문입니다. `is_ambient()`도 별도 flag가 아니라 "되돌릴 곳이 있는 재생인가"입니다.

필드 이름은 `loop`이 아니라 `repeats`로 했습니다. `loop`은 재생 loop 자체를 가리키는 이름으로 이미 쓰이고 있어서, 필드와 순회가 같은 낱말이 됩니다.

**표를 public으로 열었습니다.** 계획은 표를 `.cpp` 안에 두는 것이었는데, 그러면 "표의 모든 패턴이 상한 안에 복귀한다"는 table-driven test가 표에 닿을 수 없습니다. Application을 통해 확인하려면 패턴 하나에 수천 frame을 돌려야 하고, 그러고도 "몇 회전째인지"를 셀 방법이 없습니다. `ambient_pattern(choice)`와 `kAmbientPatternCount`를 header에 두어 test가 산술을 산술로 확인하게 했습니다 — 4개 패턴 × 최대 200회 적용이 즉시 끝납니다. C ABI에는 나가지 않으므로 "표 크기를 묻는 query가 필요 없다"는 결정은 browser 쪽에서 그대로입니다. 배열 길이를 `kAmbientPatternCount`로 선언해 두 값이 어긋날 수 없게 했습니다.

**M 슬라이스는 domain에 넣지 않았습니다.** `cube::moves`는 크기가 얼마든 성립하는 여섯 외곽 면이고, N × N × N의 슬라이스는 N − 2개 중 어느 층인지를 말해야 합니다. 이 application이 만드는 한 가지 크기에서는 말할 것이 없으므로, Application.cpp의 지역 함수로 두었습니다. 표의 나머지는 전부 `cube::moves`에서 조립해, 어느 면이 어느 층이고 어느 쪽으로 도는지는 domain에 한 벌만 남습니다.

문서의 위수 표(60·72·77·90)는 구현 전에 실제 domain으로 다시 계산해 네 값이 모두 일치하는 것을 확인했습니다. `M = {X, layer(1), -1}`(L과 같은 방향) 해석이 맞다는 뜻이기도 합니다 — 반대로 잡으면 `R U M' F`가 120이 됩니다.

### 2. 무엇이 관람을 끝내는가 — 경계를 두 번에 걸쳐 바로잡았습니다

Spec은 "아무 입력이든 즉시 이탈"이라고 적었고, 처음에는 그대로 구현했습니다. **글자대로는 맞지만 뜻으로는 틀린 규칙이었고, 리뷰에서 두 번 지적받아 고쳤습니다.**

먼저 canvas의 drag가, 이어서 2D/3D·flat style·Home view가 관람을 끊었습니다. 둘 다 scramble 재생 중에는 멀쩡히 허용되는 조작입니다. **관람만 다르게 굴 이유가 없습니다** — 오히려 관람은 돌려 가며 볼 값어치가 있는 쪽입니다. 화면 보호기와의 비유가 이 대목을 잘못 이끌었습니다: 화면 보호기는 볼 것이 없어서 아무 입력에나 사라지지만, 이것은 보라고 켜는 것입니다.

**최종 규칙은 하나입니다. 큐브를 바꾸면 명령이고, 보는 방법을 바꾸면 명령이 아닙니다.**

| | 관람을 끝냄 | 끝내지 않음 |
|---|---|---|
| | Scramble, Reset, face 버튼·키, 관람 toggle | canvas drag, 2D/3D, flat style, Home view |

**Engine에는 고칠 것이 없었습니다.** 관람은 재생이므로 `pointer_down`의 재생 분기(cube가 화면에 없으면 거절, 있으면 `start_orbit`)와 `set_view_mode`/`set_flat_style`의 playback 무간섭이 이미 원하는 그대로였고, 「watching outlives a resize and a change of view」 test가 그것을 처음부터 지키고 있었습니다. 어긋난 것은 web의 wrapper 하나였습니다 — 모든 control이 그것을 지나며 `ambient_stop()`을 먼저 부르는 바람에 **engine이 호출을 받아 보기도 전에** 관람이 끝나 있었고, scramble이 멀쩡했던 것도 관람 중이 아닐 때는 그 호출이 no-op이기 때문이었습니다. 그래서 wrapper를 `cubeCommand()`와 평범한 `run()`으로 갈라, 위 표의 경계가 코드에 한 곳으로만 존재하게 했습니다.

**"iterator로 만들어 기존 로직을 그대로 쓰는 방안"에 대해서는** — engine은 이미 그렇습니다. 관람과 scramble은 같은 `Player`, 같은 `advance_playback`, 같은 `pointer_down` 분기, 같은 view 처리를 씁니다. `repeats` 필드가 곧 iterator를 최소로 줄인 것이고(`next`가 cursor, 끝에 닿으면 0으로), 별도의 iterator 객체를 두어도 이 한 줄이 하는 일을 대신할 뿐입니다. 이번 어긋남은 재생 장치가 아니라 그 위 web layer의 규칙에 있었으므로, iterator를 도입했더라도 똑같이 났을 것입니다. 막아 주는 것은 구조가 아니라 경계를 한 곳에만 두는 것입니다.

**시점은 복원하지 않습니다.** 관람 중에 돌려 본 각도는 이탈 뒤에도 그대로입니다. Snapshot은 큐브의 것이고 시점은 사용자의 것이라는 Phase 9의 폐기 규칙("The sweep the pointer has made is kept")을 그대로 따릅니다.

### 3. 계획에 없던 것

**Move 버튼은 관람 중에도 살려 두었습니다.** 관람은 재생이라 engine이 busy이고, 그대로 두면 move 버튼이 꺼집니다. 그런데 같은 글자의 **키보드**는 이탈 후 회전으로 이어집니다 — 버튼은 눌리지도 않는데 키는 듣는 상태가 됩니다. 그래서 move 버튼의 비활성 조건을 `busy && !watching`으로 했습니다. 관람 중의 press는 이탈이자 회전이고, 키와 같아집니다.

### 4. 관람과 무관하게 고친 것

`button:hover:not(:disabled)`가 `button[aria-pressed="true"]`보다 specificity가 높아, **선택된 버튼에 포인터를 올리면 어두운 글자가 어두운 배경에 얹혀** 읽을 수 없었습니다. 뷰 전환 버튼에도 있던 문제인데, 누르고 나서 포인터가 그대로 머무는 관람 toggle에서 정면으로 드러났습니다. `button[aria-pressed="true"]:hover:not(:disabled)` 규칙을 더해 선택된 버튼은 더 밝아지도록 했습니다.

### 5. 하지 않은 것

**Scramble 길이 입력창의 편집은 이탈로 치지 않습니다.** 그 상자는 큐브에게 명령하는 것이 아니라 *다음* 명령이 무엇일지를 바꿉니다. 명령은 Scramble 버튼이고, 그 버튼이 이탈시킵니다. `change`가 blur에서야 발생하는 것도 이탈 신호로 삼기에 어색합니다.
