# Phase 14: Sharing and Records

## Status

`Not started`

## Objective

현재 cube 상태를 URL 하나로 공유할 수 있게 하고, 세션 안에서만 유지되는 가벼운 기록(PB/최근)을 보여 줍니다.

이 phase는 의도적으로 **아무것도 저장하지 않습니다.** 원래 계획은 localStorage 저장 계층(세션 복원, 기록 영속화, 옵션 저장)을 포함했지만 뺐습니다. Timer를 직렬화하지 않는 이상 복원된 세션은 어차피 `idle`로 돌아와 측정도 기록 도전도 이어지지 않으므로, reload를 넘어 지켜지는 것은 만지던 배열 하나뿐입니다. 그 하나를 위해 commit마다의 자동 저장, redo tail을 보존하는 payload, 시작 순서 계약이 전부 필요해지므로 비용이 가치를 넘습니다. 기록도 엄밀하게 지킬 자산이 아니라 재미로 보여 주는 값이면 충분합니다. **Reload는 언제나 깨끗한 새 시작입니다.**

상태 공유가 남는 이유는 정확히 그 반대입니다. 버튼 한 번의 명시적 행위라 저장 시점을 관찰하는 문제가 아예 없고, showcase에서 보여 줄 수 있는 기능이며, Phase 11의 timeline과 Phase 12의 packed 조회 ABI가 재료를 다 갖고 있어 새로 만들 것이 인코딩과 복원 ABI뿐입니다.

Sticker 배열을 직접 싣는 편이 짧지만 그렇게 하지 않습니다. 임의의 sticker 배열은 도달 가능한 cube 상태가 아닐 수 있어 받는 쪽에서 검증이 필요한 반면, move 목록은 합법적인 move만 재생하므로 **구성상 항상 합법**입니다.

## Scope

- 상태 직렬화: `version + scramble 수순 + 적용된 사용자 move 목록` → base64url 문자열
- Engine 복원 ABI: engine 소유 buffer에 packed move를 써 넣고 한 번에 적용하는 `restore_buffer`/`restore_apply`
- URL 공유: fragment에 직렬화 문자열, 공유 버튼(clipboard 복사), load 시 fragment 복원과 제거
- 세션 기록: `GameController`가 메모리에만 보관하는 PB와 최근 목록, HUD 표시
- TS unit test와 e2e

## Out of scope

- **모든 브라우저 저장소** — localStorage, sessionStorage, IndexedDB를 전혀 쓰지 않습니다. 세션 복원, 기록 영속화, 옵션(Phase 13) 저장이 전부 여기 포함됩니다
- 계정, leaderboard, network backend
- Replay 파일 공유 — 공유되는 것은 현재 상태 하나입니다
- 구버전 payload의 migration — 알 수 없는 version은 조용히 버리고 초기 상태에서 시작합니다
- N ≠ 3 직렬화 (Phase 15에서 version을 올려 확장)

## Architecture decisions

### 세션 기록

- 기록은 `GameController`의 메모리에만 있습니다: PB 하나와 최근 목록(고정 개수 5), 항목은 `{elapsed_ms, scramble_length, user_move_count}`. Reload하면 사라지고, 그것이 의도입니다.
- 자격 규칙은 새로 만들지 않습니다. Phase 6의 `completed`는 scramble로 시작한 세션에서만 도달하므로 기록은 자연히 완주에서만 생기고, Phase 11의 판별(완주 시점에 `timeline_cursor() == 0 && timeline_scramble_end() > 0`이면 solve가 푼 판)에 걸리는 판만 제외하면 됩니다. Undo가 섞인 판은 그대로 기록합니다.
- 새 PB는 completion 안내에 함께 알립니다.

### 상태 직렬화

```text
payload = version(1) ‖ scramble_count(4) ‖ packed_move(4) × scramble_count
          ‖ user_count(4) ‖ packed_move(4) × user_count
encoded = base64url(payload)

모든 uint32 필드와 packed move는 little-endian입니다. TS 구현은 `DataView`의 명시적 little-endian 인자를 씁니다 — `Uint32Array`는 platform byte order를 따르므로 영구 포맷의 근거가 되지 못합니다.
```

Byte order는 영구 포맷의 계약이므로 known-answer test에 맡기지 않고 명시합니다. Little-endian은 WASM heap의 `Uint32Array` 관례와 같아 변환 코드가 없습니다.

두 구간은 Phase 11의 timeline을 `timeline_scramble_end`에서 잘라 파생합니다. **두 구간 모두 실제 move 목록을 싣습니다.**

- **Seed를 싣지 않는 것이 이 설계의 핵심입니다.** Seed만 실으면 PRNG, `make_scramble`의 생성 규칙, native/WASM 결과 일치가 전부 **기존 공유 URL의 영구 호환 계약**이 됩니다. 나중에 WCA 규격을 넣거나 생성기를 고치는 순간, 예전에 공유된 URL이 조용히 다른 큐브를 복원합니다. 실제 수순을 실으면 그 결합이 통째로 사라져 scramble 알고리즘을 언제든 바꿀 수 있습니다.
- 비용은 크지 않습니다. 20수 scramble이면 80바이트가 늘어 base64url로 약 107자이고, 상한인 100수에서도 약 534자입니다. URL이 감당하는 길이 안에 충분히 들어옵니다.
- 덕분에 TS가 seed를 따로 보관할 이유도, Phase 9의 seed → sequence 함수를 이 phase가 재사용할 이유도 없어집니다. 재료는 전부 engine 조회입니다: `timeline_scramble_end()`, `timeline_cursor()`, `timeline_move(i)`.
- 사용자 목록에는 **적용된 move만** 싣습니다(`user_count = timeline_cursor() - timeline_scramble_end()`). **Redo tail은 싣지 않습니다** — 공유는 명시적 1회 행위이고, 받는 쪽에 보내는 사람이 무른 수까지 전달할 이유가 없습니다.
- **공유 조건은 `!busy && cursor > 0 && cursor ≥ scramble_end`입니다.** `cursor ≥ scramble_end`는 solve를 scramble 구간에서 멈춘 중간 상태를 빼는 것이고 — 허용하면 cursor 필드와 검증 규칙이 따라붙습니다 — `cursor > 0`은 빈 timeline을 뺍니다. **Scramble 없는 세션은 정상적인 공유 대상입니다**: idle에서 직접 돌린 판이나 solve 완주 뒤 새 수를 둔 판은 `scramble_end == 0, cursor > 0`이며, `scramble_count = 0, user_count > 0`으로 문제없이 복원됩니다. Decode는 `scramble_count + user_count > 0`을 검사하므로 복원 경로에 개수 0이 존재하지 않고, `restore_buffer(0)`의 "빈 버퍼인가 거절인가" 모호성도 생기지 않습니다. 버튼은 busy 중에도 비활성이라 재생 중간 상태도 실리지 않습니다.
- **개수 필드는 4바이트입니다.** 2바이트로 두면 상한을 넘는 경우를 거절하는 규칙과 안내와 test가 따라붙는데, 그 값에 닿으려면 초당 한 수로 18시간을 둬야 합니다. 도달할 수 없는 경우를 처리하는 대신 바이트를 더 써서 그 경우를 없앱니다. 남는 상한은 decode의 payload 길이 상한 하나입니다.
- TS는 조립뿐 아니라 decode 시에도 payload 전체를 검증합니다: **encoded fragment 문자열의 길이 상한(base64 decode 전에 먼저)**, trailing bytes 없음, version, `scramble_count + user_count > 0`, 각 packed의 **axis code(`0`~`2`, `3`은 무효)**·turns code·mask입니다. 길이를 decode 뒤에 재면 거대한 fragment가 먼저 메모리로 펼쳐집니다. Axis는 2비트라 packed 값 자체가 만들 수 있는 무효 코드가 하나 있으므로, 조작된 URL이 그것을 들여오지 못하게 세 필드를 같은 자리에서 함께 봅니다.
- **v1 payload의 mask는 단일 layer 셋(`layer(0)`, `layer(1)`, `layer(2)`) 중 하나로 제한하고, TS decode와 `restore_apply()`가 같은 규칙을 검사합니다.** `CubeMove` 자체는 multi-layer mask를 지원하므로 단순 범위 검증만 하면 조작된 URL이 wide move를 들여올 수 있는데, 그 move는 Phase 12의 표기에서 null이고 UI에 null을 그리는 경로는 "도달 불가"라는 근거로 두지 않았습니다. 입구를 제한해 그 전제를 지킵니다. Phase 15가 N×N을 직렬화할 때 version과 함께 확장합니다.
- v1 payload의 known-answer fixture(고정 세션 → 고정 문자열)를 영구 test로 남겨, 이후 어떤 리팩터링도 기존 공유 URL을 깨뜨리면 test가 잡게 합니다.

### 복원은 실패하면 되돌리지 않고 멈춥니다

전달은 pixel buffer와 같은 무늬입니다. Engine이 buffer를 소유하고 주소를 돌려주면 TS가 heap view로 접근합니다 — `pixel_buffer()`가 이미 `uintptr_t`를 반환하는 확립된 boundary 관례라, move 목록이라고 호출을 N번으로 쪼갤 이유가 없습니다. 그리고 **트랜잭션이 아닙니다.**

```text
restore_buffer(total_count)                 // 개수 상한 검증, 버퍼 확보, 주소 반환 (0 = 거절)
restore_apply(scramble_count, user_count)   // 전체 검증 → timeline 구성 → cube 적용
```

TS는 반환된 주소에 `Uint32Array` view를 만들어 packed move를 scramble 수순, 사용자 move 순으로 써 넣습니다. Buffer가 `std::vector<std::uint32_t>`라 4바이트 정렬이 보장되고, 두 호출 사이에 다른 engine 호출이 없으므로 memory growth로 view가 낡을 일도 없습니다.

- **기존 상태를 보존하지 않습니다.** 복원 시점은 startup이고 그때의 "기존 상태"는 갓 초기화된 solved cube뿐이라, 지킬 값이 없습니다. Staging에 쌓았다가 교체하는 구조는 그 없는 값을 지키려고 swap과 회복 계약을 들이는 일이었습니다.
- **실패하면 회복하지 않고 초기화를 중단합니다.** `restore_apply()`가 거절하면 TS는 복원을 다시 시도하지 않고 `reset_cube()`로 깨끗한 초기 상태에서 시작하며, `role="status"`로 "공유 링크를 불러오지 못했습니다"를 한 번 알립니다. 이 안내가 startup 마지막의 "Ready" 메시지에 덮이지 않도록, 복원 결과는 `startApp`의 상태 메시지를 만드는 쪽으로 전달되어 ready 문구에 합쳐집니다 — 순서에 기대는 별도 announce를 두지 않습니다. 손상된 링크에 새 세션을 주는 것이 사용자가 할 수 있는 유일한 일이므로 갈래를 하나로 둡니다. Rollback도 "같은 instance에서 재시도"라는 계약도 필요 없습니다.
- 그래서 `restore_allowed_` gate도 두지 않습니다. 복원은 startup에서 한 번 호출된다는 **호출 순서 계약**으로 충분합니다: `engine 생성 → fragment decode → restore → 최초 render → controller/listener 연결`. Gate는 우리가 직접 쓰는 호출 순서를 우리 코드로부터 지키는 장치였고, 사용자가 닿는 경로가 아닙니다.
- **최초 render가 restore 뒤인 것이 순서의 요점입니다.** 지금 `startApp`은 engine 생성 직후 solved cube를 render하는데, 그 뒤에 복원만 하고 render하지 않으면 frame loop가 도는 계기가 없어(정지 화면은 frame을 쓰지 않는 설계) **논리 상태는 복원됐지만 canvas에는 solved cube가 남습니다.** Restore를 최초 render 앞으로 두면 별도의 재렌더 호출도 필요 없습니다.
- **쓰는 동안의 검증은 없습니다.** 검증은 `restore_apply`가 한 번에 합니다: 두 구간의 합이 `restore_buffer`에 선언한 개수와 일치하는지, 그리고 모든 packed의 형식(axis code, turns code, mask)입니다. **개수 비교는 `std::uint64_t`로 승격해서 합니다** — wasm32에서는 `size_t`가 32비트라 `scramble_count + user_count`가 wrap하면 합이 선언 개수와 같아 보이면서 한 구간의 loop가 buffer 밖을 읽습니다. TS와 같은 세 필드를 봅니다 — 무효 axis는 `CubeMove`가 표현할 수 없는 값이라 engine 쪽에서도 막아야 합니다. C++ 쪽 검증의 목적은 상태 보존이 아니라 **범위 밖 접근과 UB를 막는 것**이고, 의미 수준의 검증은 TS가 decode에서 이미 합니다.
- `restore_apply()`가 하는 일: scramble 구간으로 `begin_scramble()`한 뒤 각 수를 cube에 적용하며 `step(Advance)`, 이어 사용자 구간을 cube에 적용하며 `record()`합니다. 일반 timeline 연산의 재사용이라 복원 전용 API가 없고, cursor는 자연히 끝에 옵니다.
- `restore_apply`는 `restore_buffer` 없이 호출되면 거절합니다. 이건 상태 기계가 아니라 null 검사이고, 정상 앱 경로에 없으므로 전용 test 대신 ABI smoke test에서 함께 확인합니다.
- **사용자 수를 따로 세우는 코드가 없습니다.** Phase 11이 저장 counter를 은퇴시키고 `cursor - scramble_end`로 파생하므로, timeline을 구성하고 나면 값이 이미 맞습니다. 복원된 세션의 count는 payload에 실린 사용자 구간의 길이입니다.
- 초안은 이 자리에서 `user_move_count = 0`으로 초기화하며 "복원은 commit이 아니므로 0이 정의상 맞다"고 했지만, 파생 이후에는 0으로 만들려면 오히려 예외 규칙이 필요해집니다. 그리고 "그 큐브가 지금까지 몇 수를 거쳤는가"는 복원된 판에서도 정확한 값이라, 기록에 남기기에 0보다 낫습니다. 화면에 live counter가 없다는 점(HUD는 timer와 status뿐)은 그대로여서 표시를 구분할 규칙은 어느 쪽이든 필요 없습니다.
- 복원은 **애니메이션 없이 즉시** 적용됩니다.
- 잘못된 base64url, 잘린 payload, 길이 상한 초과, 알 수 없는 version은 모두 decode 단계에서 거절되어 초기 상태로 강등됩니다.

### 공유가 보장하는 범위

Redo tail을 싣지 않기로 했으므로 "원본과 똑같이 동작한다"고 말할 수 없습니다. 보장 범위를 정확히 적습니다.

- 받는 쪽의 **cube 상태는 보낸 쪽과 동일**합니다.
- 전달된 사용자 move 범위 안에서 **undo와 solve가 동일하게** 동작합니다.
- Scramble 수순이 그대로 전달되므로 받는 쪽에서도 **solve가 scramble 구간까지 되감고 redo로 복구**할 수 있습니다.
- **보낸 쪽이 무른 사용자 redo tail은 전달되지 않습니다.** 받는 쪽에서 그 수들을 redo할 수 없습니다.

왕복 test도 이 범위로 검사합니다: 전체 기록 동치가 아니라 **cube 상태 동치 + 적용된 구간의 동치**입니다.

### URL 공유

- URL은 fragment(`#s=...`)를 씁니다. Fragment는 서버로 전송되지 않고, GitHub Pages 배포에서 라우팅에도 걸리지 않습니다.
- Load 시 fragment에 상태가 있으면 복원하고, 없으면 새 시작입니다. **Fragment를 발견하면 성공·실패와 무관하게 history API로 한 번 지웁니다** — decode나 `restore_apply()`가 거절했을 때 fragment가 남으면 reload할 때마다 같은 오류를 반복하게 되고, "reload는 언제나 새 시작"이라는 규칙과도 어긋납니다.
- **복원은 commit으로 관찰되지 않습니다.** Commit 관찰은 `timeline_cursor` 변화인데(Phase 11) 복원은 `GameController` 부착 전에 끝나므로, 부착 시점의 기준값이 이미 복원된 cursor입니다. 완료 판정과 효과음(Phase 13)이 복원을 commit으로 오인할 창이 없습니다. 공유로 연 세션은 `idle`이므로 그 판을 완주해도 기록이 생기지 않습니다 — Phase 6의 완주 조건이 그대로 문지기입니다.
- 공유 버튼은 현재 상태의 URL을 만들어 clipboard에 복사하고 `role="status"`로 알립니다. **URL은 `location.hash`를 바꾸지 않고 `new URL(location.href)`에 fragment를 붙여 분리 생성합니다** — 주소창을 건드리면 history entry와 scroll 부작용이 생기고, "내 화면의 fragment는 언제나 비어 있다"는 위 규칙과도 어긋납니다. Clipboard API가 없거나(비보안 컨텍스트) write promise가 거절되면 같은 `role="status"`로 복사 실패를 알립니다 — 공유는 부가 기능이라 실패가 앱 오류로 승격되지 않습니다. Ambient 관람 중의 버튼 press는 먼저 관람을 끝내므로(Phase 10) 공유되는 것은 언제나 실제 세션 상태이고, busy 중에는 비활성이라 재생 중간 상태가 실릴 일이 없습니다.

```text
thorvg_rubiks_restore_buffer(total_count: uint32) -> uintptr   // 0 = 거절 (상한 초과 등)
thorvg_rubiks_restore_apply(scramble_count: uint32, user_count: uint32) -> int
```

## Implementation steps

### 1. 직렬화와 복원 ABI

- [ ] Packed payload 인코딩/디코딩과 전체 선검증 (TS): 길이 상한, trailing bytes, version, packed 형식
- [ ] `restore_buffer`/`restore_apply` 구현: 상한 검증과 buffer 확보, apply의 일괄 검증, `begin_scramble` + `step(Advance)`/`record`로 timeline 구성과 cube 적용 (사용자 수는 timeline에서 파생되므로 따로 세울 counter가 없습니다)
- [ ] 왕복 test: 세션 직렬화 → 복원 → cube 상태 동치 + 적용된 두 구간 동치 (전체 기록 동치가 아님)
- [ ] **통합 test: 부분 solve → 사용자 move로 scramble 절단 → 공유 → 복원** (정상 조작으로 닿는 경로이고, 절단된 scramble이 그대로 실려 복원되는지 확인)
- [ ] Redo tail이 있는 세션을 공유하면 받는 쪽에 tail이 없고 undo/solve는 정상인 test
- [ ] `restore_apply`의 일괄 검증 test: 두 구간 합과 선언 개수의 불일치, 그리고 개수는 맞지만 그중 하나가 무효인 payload가 **전체** 거절되는지
- [ ] 거절 시 TS가 `reset_cube()`로 초기 상태에서 시작하고 알림을 한 번 내는 test (같은 instance 재시도 계약 없음)
- [ ] 복원 직후의 사용자 수가 payload의 사용자 구간 길이와 일치하는 test (파생값이라 초기화 코드가 없음)
- [ ] **Scramble 없는 세션(user-only)의 공유 왕복 test**: `scramble_count = 0, user_count > 0` payload가 정상 복원되는지
- [ ] 빈 timeline(`cursor == 0`)에서 공유 버튼이 비활성이고, `scramble_count + user_count == 0` payload가 decode에서 거절되는 test
- [ ] **Multi-layer mask 거절 test**: 단일 layer가 아닌 mask를 담은 payload가 TS decode와 `restore_apply()` 양쪽에서 거절되는지 (Phase 12의 "표기 null은 도달 불가" 전제 보호)
- [ ] **무효 axis code(`3`) 거절 test**: TS decode와 `restore_apply()` 양쪽 (2비트 필드가 만들 수 있는 유일한 무효 값)
- [ ] Decode 거절 test: 잘린 payload, trailing bytes, 잘못된 base64url, 알 수 없는 version, 길이 상한 초과
- [ ] v1 payload known-answer fixture 영구 test — 인코딩 문자열만이 아니라 `version → scramble_count → scramble moves → user_count → user moves` byte layout을 함께 드러내, seed가 다시 들어오는 회귀를 직접 잡게 합니다
- [ ] Scramble이 미완성인 상태에서 공유 버튼이 비활성인 test

### 2. URL과 기록

- [ ] Fragment 파싱, load 시 복원, 성공·실패와 무관한 fragment 제거
- [ ] 손상된 fragment로 열었다가 reload하면 오류가 반복되지 않고 새 시작인 test
- [ ] 공유 버튼과 clipboard 복사, `role="status"` 알림, busy 중 비활성
- [ ] 세션 기록 메모리 보관과 HUD 표시, 새 PB 안내
- [ ] TS unit test: 기록 자격(solve가 푼 판 제외, undo 포함), 공유로 연 세션은 완주해도 기록 없음
- [ ] 복원이 효과음·completion·timer 전이를 발생시키지 않는 test (부착 시 기준 cursor 초기화와 Phase 13의 무음 판정을 여기서 함께 검증)
- [ ] 복원된 세션의 move log가 live 세션과 같은 두 구간 표시를 내는 test (Phase 12의 목록 렌더링)
- [ ] e2e: 몇 수 두고 공유 URL을 새 컨텍스트에서 열어 같은 전개도와 undo/redo/solve 동작, 손상 URL이 초기 상태로 강등, reload 후 깨끗한 새 시작
- [ ] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- 공유 URL 하나로 받는 쪽이 같은 cube 상태를 얻고, 전달된 범위 안에서 undo와 solve가 동작하며, 받은 쪽에서 solve로 되감은 scramble 구간을 redo로 되돌릴 수 있습니다. 보낸 쪽이 무른 사용자 redo tail은 전달되지 않으며, 이 범위가 문서와 test에 같은 말로 적혀 있습니다.
- Payload는 scramble 수순과 적용된 사용자 move뿐입니다. Seed, sticker 배열, redo tail은 싣지 않습니다.
- **Seed가 실리지 않으므로 scramble 생성 알고리즘과 PRNG는 공유 URL의 호환 계약에 들어가지 않으며, 이후 자유롭게 바꿀 수 있습니다.**
- 공유는 `!busy && cursor > 0 && cursor ≥ scramble_end`에서만 가능하므로, 재생 중간·미완성 scramble·빈 timeline의 payload가 존재하지 않습니다. Scramble 없이 직접 돌린 세션은 정상적으로 공유되고 복원됩니다.
- Payload의 모든 uint32는 little-endian이고, mask는 단일 layer로 제한되며, 그 제한을 TS decode와 engine이 같은 규칙으로 검사합니다.
- 잘못된 payload는 `restore_apply`에서 전체가 거절되며, 거절 시 복원은 적용되지 않고 초기 상태로 시작합니다. Engine에 rollback도 오염 상태도 재시도 계약도 존재하지 않습니다.
- 복원 전달은 pixel buffer와 같은 engine 소유 buffer 관례를 쓰며, move를 한 개씩 넘기는 호출 경로가 존재하지 않습니다.
- 복원된 세션의 사용자 수는 timeline에서 파생되어 payload의 사용자 구간과 일치하며, 이 값을 세우거나 되돌리는 코드가 없습니다.
- 최초 render는 restore 뒤에 일어나므로, 복원된 상태가 첫 화면부터 canvas에 보입니다.
- 복원은 startup 호출 순서 계약 안에서만 일어나므로, 복원 후에 살아남은 재생·snap·snapshot이 상태를 덮는 경로가 없습니다.
- 복원은 commit으로 관찰되지 않습니다: 복원 직후 효과음, completion, timer 전이가 발생하지 않습니다.
- 앱은 브라우저 저장소를 전혀 사용하지 않고, reload는 언제나 초기 상태에서 시작합니다.
- 기록은 세션 메모리에만 있으며, solve가 푼 판은 제외되고, 공유로 연 판의 완주는 기록이 되지 않습니다.
- 개인 데이터는 URL에 실리지 않습니다 — fragment의 내용은 cube 상태뿐입니다.
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
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 14를 완료 처리합니다.
- 실제 구현과 차이가 생긴 결정을 이 문서에 기록합니다.
