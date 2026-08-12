# Phase 10: Ambient Mode

## Status

`Not started`

## Objective

입력이 없는 동안 큐브가 스스로 도는 관람 모드를 더합니다. Phase 9의 Player에 패턴 하나를 넘기고 반복시키는 일이므로 새 animation 경로도 공급 장치도 만들지 않습니다.

방식은 하나입니다. 위수를 아는 짧은 수순 표에서 하나를 골라 반복합니다. 어떤 수순이든 반복하면 유한한 횟수 뒤에 solved로 돌아온다는 성질을 그대로 보여 주므로 관람은 주기적으로 처음 상태를 지나가고, cube group에서 원소의 최대 위수가 1260이라 반복 횟수의 상한도 근거를 갖습니다. 표의 수순은 그 복귀가 관람 한 번 안에 보이도록(주기 1~2분대) 위수를 계산해 고른 것들입니다.

관람에 들어갈 때 cube 상태를 snapshot하고, 나갈 때 그 상태로 되돌립니다. 관람 중의 move를 timeline에 남기면 자리를 비운 시간에 비례해 timeline이 무한히 자라고, 남기지 않으면 상태와 timeline이 어긋나 이후의 되감기와 복원이 전부 틀어집니다. 되돌리면 둘 다 피할 수 있고, 화면 보호기가 원래 그렇게 동작하므로 부자연스럽지도 않습니다.

## Scope

- Application의 ambient 상태: 진입 시 `CubeState` snapshot, 이탈 시 즉시 복원
- `kAmbientPatterns` 상수 표(수순만)에서 고른 패턴 하나를 Player에 넘기기
- Player에 `loop` 필드 추가 — 소진 시 `next`를 0으로 되돌리는 한 줄
- C ABI: `ambient_start(choice)`, `ambient_stop()`, `is_ambient()`
- Web: 관람 toggle 버튼, 아무 입력(canvas pointer, keyboard, 버튼)으로 즉시 이탈
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
- 관람 중 어떤 입력이든(canvas pointer down, face key, 아무 버튼) 먼저 `ambient_stop`을 호출해 복원한 뒤 원래 동작을 수행합니다. Pointer down 자체는 이탈로만 쓰고 gesture를 시작하지 않습니다(복원 직후의 첫 press는 버려지는 편이 예측 가능합니다).
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

- [ ] Snapshot 보관과 즉시 복원 구현
- [ ] `kAmbientPatterns` 표와 choice modulo 선택, 고른 패턴을 Player로 넘기는 `ambient_start`
- [ ] Player에 `loop` 필드(기본 false) 추가와 소비 loop의 되감기 한 줄, ambient tempo
- [ ] `loop` 재생이 소진되지 않고 패턴을 계속 반복하는 test (기존 생산자는 `loop = false`로 그대로 끝남)
- [ ] `ambient_stop` 후 상태·count가 진입 전과 동일한 test
- [ ] Table-driven test: 표의 모든 패턴이 `kAmbientMaxPeriod` 이내에 solved로 복귀
- [ ] Reset/scramble/shutdown이 ambient를 끝내는 test

### 2. Boundary와 UI

- [ ] C ABI 세 함수 추가와 generated 산출물, fake fixture 갱신
- [ ] 관람 toggle 버튼과 `idle`/`completed` 활성 조건, 진입마다 새 `choice` 주입 (test는 injectable source)
- [ ] 모든 입력 경로의 선-이탈 처리 (pointer, keyboard, 버튼)와 toggle 버튼의 stop-후-종료 예외
- [ ] Toggle을 ambient 중에 눌러도 다시 시작되지 않는 test
- [ ] `visibilitychange` 연결과 teardown 정리
- [ ] TS unit test: 진입 조건, 이탈 경로, visibility 정지
- [ ] e2e: 관람 시작 → 픽셀 변화 관찰 → 아무 입력 → 진입 전 전개도와 일치
- [ ] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- 관람 중 큐브는 Phase 9의 Player와 같은 animation 경로로 계속 회전하며, 반복은 `loop` 필드 하나로 이루어져 별도의 공급 loop나 패턴 위치 상태가 존재하지 않습니다.
- 어떤 입력이든 관람을 즉시 끝내며, cube 상태와 move count는 진입 시점과 정확히 일치합니다.
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

모든 acceptance criteria와 verification command를 통과한 뒤 다음 작업을 수행합니다.

- 이 문서의 status를 `Completed`로 변경합니다.
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 10을 완료 처리합니다.
- 실제 구현과 차이가 생긴 결정을 이 문서에 기록합니다.
