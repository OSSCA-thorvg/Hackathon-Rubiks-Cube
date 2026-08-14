# Phase 9: Move Player and Animated Scramble

## Status

`In progress`

## Objective

미리 만들어진 수순 하나를 순서대로 재생하는 **Player**를 Application에 도입하고, scramble을 그 첫 소비자로 만듭니다.

Scramble은 logical state를 한 번에 바꾸는 대신 수순을 Player에 넘겨 큐브가 실제로 돌아가면서 섞이는 모습을 보여 줍니다. 재생이 소비하는 move는 user move count와 timer에서 제외되고, 재생 중에는 layer를 돌리는 입력(cube·net·rings 세 picking 경로, `turn_face`, UI move)을 차단하되 3D cube가 화면에 있을 때의 pointer는 `start_orbit()`으로 보내 camera만 움직이게 합니다.

Scramble 수는 `make_scramble`이 이미 `move_count` 인자로 받고 있으므로 사용자가 정하는 정수 값으로 노출합니다. 난이도 단계로 포장하지 않고 수 자체를 보여 줍니다.

## Scope

- Application 소유의 `std::optional<Player>`: 수순과 그 재생 성질(진행 방향, tempo, 정지 가능 여부, 반복)을 함께 담는 값 하나
- 진행 중인 snap이 끝나면 Player의 다음 move를 `start_move`로 시작하는 소비 loop
- 재생 중 pointer를 orbit으로 라우팅하는 picking 없는 `start_orbit()` 추가와, 3D cube가 없는 view에서의 거절
- 재생 중 busy 유지: 개정된 `is_busy()`([Phase 6.5](./06.5-pre-enhancement-cleanup.md)가 선행 적용)에 재생 판정을 OR로 얹기
- Scramble의 animated 재생과 재생 move의 count/timer 제외
- Scramble 수를 C ABI와 UI 정수 입력으로 노출
- `GameController`의 `scrambling` 상태와 frame loop 시작 수정
- `GameSession` 분리: timer·게임 상태 전이·완주 판정을 `GameController`에서 모듈로 분리 — Phase 11~14가 관찰·log·sound·기록을 계속 얹으므로, web 코드가 쌓이기 시작하는 이 phase의 첫 단계로 절단합니다. DOM listener 배선과 frame 소유권은 지금처럼 각각 `GameController`와 `AppLifecycle`에 남습니다
- `scramble()` 호출 직후 큐브 상태를 읽는 기존 test 전체의 이관 (아래 "비동기가 된 scramble이 깨뜨리는 test")
- Native/TS unit test와 e2e

## Out of scope

- Step 모드(다음 한 수만 소비하고 정지) — 소비자가 Phase 17의 힌트 하나뿐이고 그 phase는 solver 뒤의 마지막 순번이라, 필요해지는 시점에 Player 필드 하나로 얹습니다
- History 기록 (Phase 11) — 이 phase의 재생은 기록을 남기지 않습니다
- 재생 속도 UI (Phase 13) — Player가 든 `tempo_ms`를 읽는 한 자리만 열어 둡니다
- Scramble notation 표시 (Phase 12)
- WCA 공식 scramble 규격

## Architecture decisions

### 재생은 Player 값 하나입니다

새 animation 경로를 만들지 않습니다. Player는 **데이터**이고, 소비는 `InteractionController::start_move()`를 순서대로 호출하는 기존 `advance()` 한 곳입니다.

```cpp
/** One scripted sequence being played back, with how to play it. */
struct Player {
    std::vector<cube::CubeMove> plan;
    std::size_t next = 0;
    double tempo_ms;   // 90°당
};

std::optional<Player> playback;   // ApplicationState의 필드
```

`double`인 것은 `snap_duration()`이 이미 `double`을 쓰기 때문이고, `Player`는 `Application.cpp`의 anonymous namespace에 둡니다 — engine 밖의 어떤 소비자도 이 struct를 이름으로 알 필요가 없습니다.

- **재생에 관한 상태가 값 하나입니다.** 초안은 FIFO와 `playback_origin_`을 나란히 둔 두 상태였지만, 둘은 언제나 함께 세워지고 함께 죽습니다 — 같은 수명을 가진 상태 둘은 객체 하나입니다. 폐기가 `playback.reset()` 한 줄이 되고, "queue는 비었는데 origin이 남았다" 같은 조합이 표현되지 않습니다.
- **`advance()`에서 `!interaction.is_busy()`이고 `playback`에 남은 수가 있으면** `plan[next++]`을 `start_move`로 시작합니다. Orbit이 `is_busy()`에서 빠지므로(아래 개정) 사용자가 재생을 보며 orbit을 잡고 있어도 다음 수가 시작됩니다.
- **`app::advance()`의 반환 의미를 "interaction이 더 그릴 frame이 있거나 `playback`이 남아 있음"으로 고정합니다.** `InteractionController::advance()`가 아니라 frame loop가 읽는 바깥쪽입니다. 새 move를 시작한 frame과 move 사이의 frame에도 true여야 하며, 그렇지 않으면 재생 중간에 frame loop가 멈춥니다. 이 한 줄로 native test의 `settle_application()`(`while (app::advance(16.0))`)이 수정 없이 재생까지 소진시킵니다.
- **소비 loop는 `start_move()`가 false를 반환하는 경우를 처리해야 합니다.** Controller는 layer mask가 범위를 벗어나거나 회전량이 4의 배수인 move를 거절하는데, 그때 `next`를 전진시키지 않으면 같은 move를 매 frame 다시 시도하며 `advance()`가 영원히 true를 반환해 frame loop가 멈추지 않습니다. `next`는 꺼낼 때 전진하고, 거절된 move는 재시도 없이 건너뜁니다 — scramble 수순에서는 도달하지 않는 경로지만, 잘못 만들어진 plan이 행 상태가 되어서는 안 됩니다.
- **Player가 받는 것은 언제나 완성된 수순입니다.** 이 phase의 scramble, Phase 10의 ambient 패턴, Phase 11의 undo/solve/redo가 모두 `std::vector<CubeMove>`를 만들어 넘깁니다. 한 수씩 흘려보내는 공급자는 어디에도 없으므로 재생 경로가 하나뿐입니다.
- **재생 성질은 이름이 아니라 값으로 듭니다.** 초안은 `MoveOrigin` enum(`User`/`Scramble`/`Ambient`/`Rewind`/`Redo`)으로 출처를 이름 붙이고, tempo·count·timeline 반영·정지 가능 여부를 그 이름에서 유도하는 표들을 두는 방식이었습니다. 그러나 그 성질들을 이름으로 요구하는 소비자가 하나도 없습니다 — 필요한 것은 "지금 재생 중인가"라는 bit 하나와, 재생 세션이 든 성질 값들뿐입니다. 이름을 두면 phase마다 값이 늘고 그때마다 이름→성질 변환표가 원격지에서 함께 자라므로, 성질을 Player가 직접 들게 합니다. 의미의 이름은 코드에서 사라지지 않고 원래 자리인 생산자 함수 이름(`scramble()`, `ambient_start()`, `undo()`)으로 돌아갑니다.
- **이후 phase는 값을 더하는 대신 필드를 더합니다.** Phase 10이 `loop`(기본 false)를, Phase 11이 `timeline_effect`(기본값 없음)와 `stoppable`(기본 false)을 더합니다. 각 필드는 그것을 처음 필요로 하는 생산자와 그것을 읽는 한 지점에서 함께 생기므로, 소비자가 없는 필드를 미리 두지 않습니다.
- 기본값의 유무는 오용의 결과로 정합니다. `stoppable`의 기본 false는 틀려도 "버튼이 안 먹는" 정도라 기본값을 두고 기존 생산자를 손대지 않지만, `timeline_effect`는 잘못 받으면 timeline이 손상되므로 기본값 없이 **Phase 11이 이 phase의 scramble 생산자에도 명시를 더합니다.** 상세는 Phase 11 문서에 있습니다.
- `start_move`가 tempo 인자를 받도록 확장하고(기본값은 기존 `kSnapMsPerQuarterTurn`) 소비 loop가 `playback->tempo_ms`를 넘깁니다. **인자의 단위는 duration이 아니라 90°당 ms입니다** — 실제 duration은 기존 `snap_duration()`처럼 각도에 비례해 유도하므로, half turn은 quarter turn의 두 배 시간으로 돕니다. 총 duration을 넘기면 그 비례가 조용히 사라집니다. Tempo를 정하는 것은 개별 move가 아니라 재생 세션이므로, Phase 13이 배율을 얹을 자리도 이 값을 읽는 한 지점입니다.
- **Drag release의 snap도 같은 자리를 지나게 합니다.** `pointer_up()`이 90°당 tempo 인자를 받고(기본값은 기존 상수) `Application::pointer_up()`이 사용자 tempo 상수를 넘깁니다. 이 phase에서는 같은 값이라 동작이 변하지 않지만, 이 배관이 없으면 Phase 13의 속도 배율이 재생에만 적용되고 사용자 snap에는 닿지 않습니다. `snap_duration()`을 부르는 것은 `pointer_up()` 자신이 아니라 private helper `start_snap()`이므로, 인자는 `pointer_up` → `start_snap` → `snap_duration` 세 자리를 함께 지납니다.
- **`kMinSnapMs` 바닥은 절대값이 아니라 `min(kMinSnapMs, tempo)`가 됩니다.** 바닥의 목적은 "아주 작은 보정도 움직임으로 보이게" 하는 것이지 최소 재생 시간이 아닙니다. 절대값으로 두면 tempo가 60ms/90° 아래로 내려가는 순간 바닥이 실질 pacing이 되어 5° 보정과 쿼터 턴이 같은 시간을 쓰게 됩니다. 지금의 두 tempo(200·120)에서는 값이 그대로 60이라 동작이 변하지 않고, 바뀌는 것은 Phase 13이 배율을 얹은 뒤입니다.
- **`start_move()`는 `opened_ms_`를 0으로 되돌립니다.** 리프트 높이는 `opened_ms_`(올라간 정도)와 snap의 남은 시간(내려가는 정도)의 곱인데, 재생은 move 사이에 쉬는 frame이 없어 `opened_ms_`가 상한에 머뭅니다. 그대로 두면 매 move가 최대 높이에서 시작해 0으로 내려갔다가 다시 튀어올라, 스무 수 내내 조각이 깜빡입니다. 되돌리면 재생의 각 move가 오늘 keyboard turn 하나와 정확히 같은 호를 그립니다 — 그리고 이는 `turn_face`가 직전 move의 정리 frame보다 먼저 도착하던 좁은 창의 같은 깜빡임도 함께 없앱니다.
- **Scramble tempo는 `kOpeningMs`(120ms)와 같은 120ms/90°입니다.** 리프트가 올라갔다 내려올 시간이 move보다 짧아야 호가 형태를 갖습니다. 스무 수(반 바퀴가 섞여 쿼터 턴 환산 24회 내외)가 3초 안쪽이라 "20수를 4초 안에"도 지킵니다.
- **재생 판정은 `playback.has_value()` 하나이고, count 규칙도 그것 하나입니다.** 재생 중의 commit은 `user_move_count`를 증가시키지 않습니다. `commit_move(move)`가 이 값을 스스로 읽으므로 출처 인자가 필요 없고, 재생 중에는 layer 입력이 막혀 사용자 commit이 생길 수 없으므로 판정이 어느 시점에도 맞습니다.
- **`playback`은 마지막 수의 commit까지 살아 있습니다.** 그래서 "마지막 수를 꺼내면 queue가 비어 판정이 뒤집힌다"는 구멍이 없습니다. 지워지는 곳은 둘뿐입니다: 재생의 마지막 commit(`next == plan.size()`이고 진행 중인 snap이 없어지는 그 지점)과 아래의 폐기 helper.
- Busy는 **개정된 `is_busy()`(gesture, snap, 미소비 commit — orbit 제외)에 `playback.has_value()`를 OR로 얹은 것**입니다. 재생의 처음부터 마지막 회전이 끝날 때까지 한 값이 계속 true이므로, 중간에 busy가 풀리는 구멍을 조건 조합으로 메울 일이 없습니다.
- **`turn_face`의 재생 거절은 Application 수준의 guard입니다.** Controller는 `playback`을 모르므로 controller의 거절만으로는 부족합니다 — 재생 시작 직후 첫 `advance()` 전에는 controller가 idle이라, guard가 없으면 그 창에서 keyboard turn이 재생 수순 위에 끼어듭니다. `Application::turn_face()`(와 UI move 경로)가 app 수준 busy를 먼저 봅니다.
- **재생 중 pointer는 camera orbit만 하거나 아무 일도 하지 않습니다.** `Application::pointer_down`은 재생 중이면 3D cube가 화면에 있을 때에 한해 `start_orbit()`으로 보내고, 없으면 press를 거절합니다 — 이는 Phase 8이 정한 "전개도 전용 view에는 orbit할 대상이 없다"의 연장입니다. 조건 없이 보내면 `ViewMode::Flat`에서 `placement.cube`가 기본 `Rect{0,0,0,0}`인 채로 넘어가 pixel당 각도가 무한대가 되고, `OrbitCamera::turn`의 `fmod(inf, 360)`이 yaw를 **영구히** NaN으로 만듭니다. 3D로 돌아와도 카메라가 돌아오지 않습니다.
- **이 분기는 `pointer_down`의 유한성 검사 바로 뒤, picking보다 앞에 놓입니다.** 지금의 순서는 유한성 → net pick → rings pick → 조기 반환 → `finish_snap()`(상태를 바꾸는 호출) → dispatch이므로, 뒤쪽에 두면 확정 경로와 세 picking을 모두 지난 뒤에야 막게 됩니다. 아래의 "확정 경로가 도달 불가능"은 이 위치에서만 참입니다. 차단해야 할 picking이 Phase 8·8.5를 거치며 cube 하나에서 cube·net·rings 셋이 되었으므로, 한 자리에서 막는 편이 세 곳에 조건을 두는 것보다 낫기도 합니다.
- 이 라우팅이 **snap의 소유자를 따지는 조건을 대신합니다.** Phase 7이 "누르면 진행 중인 snap을 확정한다"고 해 두었으므로 재생 중의 스치는 click이 재생을 가로채지 않게 막아야 하는데, "진행 중인 snap이 사용자 것인가"를 묻는 대신 재생 판정만 보면 됩니다. 조건이 pick보다 앞단에 있어 확정 경로 자체가 도달 불가능해집니다.
- **`is_busy()`는 orbit을 보지 않습니다 — [Phase 6.5](./06.5-pre-enhancement-cleanup.md)가 선행 적용한 개정입니다.** 이 성질이 없으면 사용자가 orbit을 잡고 있는 동안 `start_move()`가 거절되어 재생이 멈춥니다. 근거와 test는 그 문서에 있고, 이 phase는 개정된 술어 위에서 재생 소비 loop를 짓습니다. 술어는 하나로 남아 `start_move()`도 재생 소비도 같은 `is_busy()`를 봅니다.
- **`start_orbit(x, y, viewport)`를 더합니다** — picking 없이 orbit gesture만 시작하는 진입점입니다. 일반 `pointer_down()`은 좌표로 cube를 pick해 layer drag를 시작하므로, 거절 조건만 완화해서는 "좌표와 무관한 orbit"이 되지 않습니다. 재생 중 라우팅은 이 함수로 갑니다.
- **`start_orbit()`의 거절 조건은 `pointer_down()`의 것과 하나 다릅니다: 진행 중인 snap을 보지 않습니다.** Orbit은 지킬 commit이 없으므로 회전이 도는 중에도 시작할 수 있어야 하고, 재생 중에는 거의 언제나 snap이 돌고 있습니다. `pointer_down()`이 snap을 거절하는 것은 앞단에서 `finish_snap()`이 이미 그것을 비웠기 때문이지 orbit이 snap과 다투기 때문이 아니라, 그 검사가 여기로 따라올 이유가 없습니다. 퇴화된 viewport(폭·높이 0)는 `pointer_down()`과 같이 거절합니다 — 위의 NaN을 함수 자신이 한 번 더 막습니다.
- `pointer_down()`의 "cube를 빗나갔으니 둘러보기" 분기도 같은 함수를 부릅니다. 진입점이 둘이면 둘 중 하나만 고쳐지는 날이 옵니다.
- Orbit은 commit이 없고 camera만 바꾸므로(Phase 5.5의 "up == cancel") 재생 결과에 영향을 주지 않습니다. Scramble이든 solve든 **돌려 가며 볼 수 있는 편이 showcase에 맞고**, 특히 수십 초가 걸릴 수 있는 Phase 11의 solve 재생에서 차이가 큽니다.
- Ambient는 이 규칙을 따르지 않습니다. 관람은 자리를 비운 사이 도는 화면 보호기라 어떤 입력이든 빠져나오는 편이 맞고, 그 규칙은 Phase 10이 정의합니다.
- **Resize와 view 전환(`set_view_mode`·`set_flat_style` 둘 다)은 재생을 건드리지 않습니다.** 기존 정책의 구분(화면 좌표에 의존하는 gesture는 취소, 각도만 animation하는 snap은 계속)에서 재생 중인 move는 snap과 같은 부류입니다. 폐기하면 모바일 회전 한 번에 scramble이 반쯤 적용된 채 busy가 풀려 timer가 준비되고, Phase 11의 "사용자 수는 완성된 scramble 위에서만 존재한다"는 불변식도 함께 깨집니다.
- **폐기 helper는 "재생에 관련된 상태를 남김없이 버린다"로 정의하고, 목록은 둘입니다**: `playback` 전체와, 이미 시작되어 진행 중인 snap. Player만 버리면 그것을 떠난 snap이 몇 frame 뒤 새 상태 위에 commit되어, reset 직후 이전 scramble의 수가 적용됩니다. 둘을 함께 버리는 helper 하나로 두고 폐기가 필요한 모든 곳이 그것을 부릅니다.
- **폐기의 구현은 이미 있는 두 함수의 조합이고, controller에 새 함수를 만들지 않습니다.** `cancel()`은 Phase 5의 계약대로 `gesture_`와 `orbit_`만 비우고 `snap_`을 남기지만(resize에서 snap이 살아남는 근거), **`reset()`은 snap과 미소비 commit까지 전부 버립니다.** 지금의 `reset_cube()`와 `scramble()`이 정확히 이 자리에서 `drain_orbit()` + `interaction.reset()`을 이미 부르고 있으므로, 폐기 helper는 그 관례에 `playback.reset()` 한 줄을 더한 것입니다. `reset()`은 `CubeState`를 건드리지 않으므로 폐기는 이미 commit된 수만 남기고 정확히 멈춥니다.
- **재생 상태가 값 하나라 이 목록은 이후 phase에서도 늘어나지 않습니다.** Phase 11의 되감기와 solve는 수순을 Player에 넘기는 방식이라 남기는 것이 Player뿐이고, Phase 10의 ambient는 같은 helper를 부른 뒤 snapshot을 되돌립니다. 재생 상태를 helper 밖에 따로 두는 방식은 쓰지 않습니다 — 폐기 지점이 여럿이라 한 곳만 빠뜨려도 이전 재생이 되살아나고, 그 누락은 "폐기했는데 왜 다시 도는가"라는 형태로만 드러나 추적이 어렵기 때문입니다.
- 이 phase에서 폐기를 부르는 것은 `reset_cube`와 `scramble` 둘이고, Phase 10의 `ambient_stop`과 Phase 11의 재생 정리가 같은 helper를 재사용합니다. 이미 commit된 move는 어느 경우에도 되돌리지 않습니다.
- **`shutdown()`은 이 목록에 없습니다.** 초안은 셋으로 적었으나 [Phase 6.5](./06.5-pre-enhancement-cleanup.md) 2차가 전역들을 `unique_ptr<ApplicationState>` 하나로 묶은 뒤로 `shutdown()`은 그 상태를 통째로 파괴할 뿐이고, `playback`도 그 안에 있으므로 더할 것이 없습니다. 새 필드가 shutdown의 수동 초기화 목록에서 빠질 수 없게 만든 것이 그 정리의 목적이었고, 이 phase가 그 첫 수혜자입니다.

### Animated scramble

- `scramble(seed, move_count)`는 cube를 초기화한 뒤 sequence를 즉시 적용하는 대신 Player에 넘깁니다. Phase 11이 이 지점을 "완성된 수순으로 timeline을 초기화하고 재생은 cursor만 전진"으로 확장하므로, 이 phase에서도 **완성된 sequence를 한 번에 넘기는** 형태를 유지합니다. 한 수씩 만들어 흘리는 공급 방식을 쓰면 Phase 11에서 뒤집어야 합니다.
- **Solved 검증을 하지 않습니다.** 기존 코드의 검사와 R 덧붙임은 [Phase 6.5](./06.5-pre-enhancement-cleanup.md)가 이미 걷어냈고(근거도 그 문서에 있습니다), 이 phase의 재생 형태에서도 그 결정이 그대로 유지됩니다 — 재시도든 실패 반환이든 경로를 다시 들이지 않습니다.
- 우주적 우연으로 solved가 나오면 사용자가 Scramble을 한 번 더 누릅니다. 그 대가로 seed → 최종 sequence를 유도하는 함수와 그 결정성 계약이 통째로 사라집니다.
- `move_count`는 `1 ≤ count ≤ 상한`(상수, 예: 100)으로 검증하고 벗어나면 거절합니다. ABI가 `uint32`라 음수는 표현되지 않으므로 실제로 막는 것은 0(solved 상태의 "가짜 scramble")과, 입력창에 큰 수를 쳐서 재생이 끝나지 않게 만드는 경우입니다.
- Scramble 재생의 tempo는 Player에 사용자 move보다 빠른 상수(120ms/90°)를 실어 정합니다. 값의 근거는 위의 리프트 항목에 있습니다.

### 비동기가 된 scramble이 깨뜨리는 test

`scramble()`이 반환한 직후 큐브는 아직 solved입니다. 그래서 손볼 것은 known-answer 하나가 아니라 **호출 직후 큐브 상태를 읽는 test 전부**이고, 목록을 여기 둡니다.

- `tests/app/ApplicationTest.cpp` — "scramble and reset own only the cube solve session"과 "view modes preserve state…"가 `scramble()` 바로 뒤에서 `REQUIRE_FALSE(is_solved())`를 봅니다. `settle_application()`을 사이에 넣습니다.
- `tests/app/PointerInteractionTest.cpp` — "scramble and reset keep the viewpoint the drag left behind"도 같습니다. 이 test의 주제는 시점이 남는지이므로, 재생을 소진시킨 뒤에 보는 것이 오히려 원래 의도에 가깝습니다.
- `web/tests/e2e/gameplay-ui.spec.ts` — scramble click 직후 canvas를 찍어 solved와 다르길 기대하는 곳은 `data-game-state`가 `ready`가 되기를 먼저 기다립니다.
- **`seed 42 has a stable cross-platform known answer`는 삭제합니다.** 대체물을 새로 쓸 필요가 없습니다 — 바로 위의 `seeded scrambles are reproducible and structurally valid`가 이미 결정성과 축 제약·수 개수를 다 보고 있어, 남는 것은 20수 배열 하나뿐이고 그것이 곧 "생성 규칙을 바꿀 수 없다"는 계약입니다. `move_count` 인자 검증만 그 test에 더합니다.
- **키보드로 푸는 e2e는 버리지 않고 짧게 만듭니다.** 그 test는 실제 WASM에서 `running → completed`와 timer 정지를 확인하는 유일한 브라우저 경로입니다(unit test는 fake engine 위 상태 기계만 봅니다). 24키짜리 역재생이 부담스러운 것은 길이지 존재가 아니므로, `move_count`가 인자가 된 지금 3수 scramble로 줄여 3~5키로 같은 경로를 지납니다. 생성 규칙을 바꾸는 날 다시 뽑아야 하는 것은 그대로지만, 비용이 한 자릿수가 됩니다.

### C ABI와 GameController

```text
thorvg_rubiks_scramble(seed: uint32, move_count: uint32) -> int
```

- 기존 `scramble(seed)`의 signature 변경입니다. Generated declaration, exported list, fake fixture를 함께 갱신합니다.
- `GameController.onScramble`은 현재 `startFrameLoop()`를 호출하지 않습니다. Instant 적용이던 시절의 흔적이므로, 재생이 보이려면 **scramble 수락 시 frame loop를 시작**해야 합니다.
- `GameState`에 `scrambling`을 추가합니다: `Scramble click → scrambling → (재생 소진) → ready`. Timer는 재생이 소진된 frame에 arm됩니다. 판정은 `afterEngineFrame`에서 **`state == scrambling && !isBusy()`**입니다. `true → false` 전이를 기다리면 1수 scramble처럼 첫 관찰 frame에 이미 끝난 경우 `false → false`만 보게 되어 영원히 `scrambling`에 머뭅니다. Scramble 호출이 수락된 것 자체가 `scrambling` 진입의 증거이므로 true 표본을 따로 볼 이유가 없습니다.
- Scramble 수 입력은 숫자 `<input type="number">`로 두고, 유효 범위를 벗어난 입력은 boundary에서 clamp 대신 거절하고 이전 값을 유지합니다.
- 안내 문구도 재생을 따릅니다. 지금의 "Scramble ready. The timer starts after your first move."는 click 시점에 나가는데, 그 시점에 준비된 것은 아무것도 없습니다. 진입에서는 섞는 중임을 알리고 준비 문구는 `ready` 판정 자리로 옮깁니다.

## Implementation steps

### 0. GameSession 분리

- [ ] `web/src/game/GameSession.ts`: timer 소유, `GameState` 전이, 완주 판정을 `GameController`에서 옮김. DOM listener 배선은 `GameController`에, frame 소유권은 `AppLifecycle`에 그대로 남김
- [ ] `GameController`는 session을 만들고 명령(`scramble`/`reset`/move)과 `afterEngineFrame` 관찰을 그것에 넘기는 배선만 남김
- [ ] 기존 `GameController` unit test가 관찰하던 전이가 그대로 통과하는지 확인 (동작 변경 없음)

### 1. Player

- [ ] `start_move`에 90°당 tempo 인자 추가 — duration은 각도 비례 유도 (기본값 유지, 기존 test 무수정 통과)
- [ ] `snap_duration`의 바닥을 `min(kMinSnapMs, tempo)`로 바꾸고, 기존 두 tempo에서 값이 변하지 않음을 고정하는 test
- [ ] `start_move`가 `opened_ms_`를 되돌리는 구현과, 연속 두 move의 리프트가 두 번 모두 0에서 오르는 test
- [ ] Half turn이 quarter turn의 두 배 duration으로 도는 test
- [ ] 재생 중 매 frame(`start_move` 직후와 move 사이 포함) `app::advance()`가 true를 반환하는 test
- [ ] `pointer_up`에 tempo 인자 추가(`start_snap`을 함께 지남)와 `Application::pointer_up()`의 사용자 tempo 연결 (기본값 유지, 기존 test 무수정 통과)
- [ ] `Player` 정의(`plan`, `next`, `tempo_ms`)와 `ApplicationState::playback`, `advance()`의 소비 loop 구현
- [ ] 거절되는 move가 섞인 plan이 frame loop를 멈추게 하지 않고 끝까지 소진되는 test
- [ ] `commit_move(move)`가 `playback`을 스스로 읽어 count를 가르는 구현 (출처 인자 없음)
- [ ] 재생 중 commit이 `user_move_count`를 증가시키지 않는 test
- [ ] 재생 진행 중 `is_busy()` true, `turn_face`와 UI move 거절 test
- [ ] 재생 중 net·rings 영역의 press가 layer drag를 시작하지 못하는 test (Phase 8·8.5의 두 picking 경로)
- [ ] **Scramble 수락 직후, 첫 `advance()` 전의 `turn_face`가 거절되는 test** (controller가 idle인 창을 Application guard가 막는지)
- [ ] 마지막 수의 snap이 도는 동안(plan은 소진된 뒤) busy가 유지되는 test
- [ ] Picking 없는 `start_orbit()` 추가 — snap은 보지 않고, 퇴화된 viewport는 거절. `pointer_down()`의 빗나감 분기도 이 함수를 부르게 함
- [ ] 재생 중 pointer down이 cube 위에서도 orbit gesture를 시작하고 `finish_snap()`을 부르지 않는 test
- [ ] **`ViewMode::Flat` 재생 중의 press가 거절되고, 이후 3D로 돌아온 카메라가 유한한 값인 test** (NaN 회귀)
- [ ] **Orbit을 잡고 있는 동안에도 다음 move가 시작되어 재생이 끝까지 진행되는 test**
- [ ] `playback`이 마지막 수의 commit까지 유지되어 그동안 orbit 라우팅과 busy가 끊기지 않는 test
- [ ] 원자적 폐기 helper(`drain_orbit()` + `interaction.reset()` + `playback.reset()`) 구현
- [ ] `cancel()`이 진행 중인 snap을 버리지 않음을 고정하는 회귀 test
- [ ] Reset과 scramble의 폐기 test (`shutdown`은 `ApplicationState` 파괴로 자동)
- [ ] Player를 떠난 snap이 진행 중일 때 reset하면 그 move가 commit되지 않는 test
- [ ] Resize와 view 전환(`set_view_mode`·`set_flat_style`) 중에도 재생이 계속되어 scramble이 끝까지 적용되는 test

### 2. Animated scramble

- [ ] `scramble(seed, move_count)` 구현: `move_count` 검증(큐브를 건드리기 전에)과 Player 생성
- [ ] `move_count` 범위 검증 test — 거절된 호출이 큐브도 재생도 남기지 않음을 함께 확인
- [ ] `seed 42 has a stable cross-platform known answer` 삭제, 그 자리의 `move_count` 커버리지를 위 결정성 test에 흡수
- [ ] 고정 dt를 반복 주입해 재생 소진 후 상태가 즉시 적용과 동치인 test
- [ ] `scramble()` 직후 상태를 읽던 native test들에 `settle_application()` 삽입 (위 목록)

### 3. Boundary와 UI

- [ ] C ABI signature 변경과 hand-maintained `.d.ts`, exported list, fake fixture 갱신
- [ ] `GameSession`: `scrambling` 상태, busy 전이로 ready 판정, 안내 문구 이동
- [ ] `GameController`: scramble 수락 시 frame loop 시작
- [ ] Scramble 수 입력 UI와 검증
- [ ] TS unit test: scrambling 전이, timer arm 시점, 입력 검증
- [ ] 1수 scramble이 첫 관찰 frame에 끝나도 `ready`로 넘어가는 test
- [ ] e2e: Scramble click 후 큐브가 돌아가는 중간 frame 관찰, 소진 후 timer armed, 첫 user move에 timer 시작
- [ ] e2e: `ready`를 기다리도록 기존 scramble 관찰 test 수정, 키보드 solve test를 3수 scramble로 축소
- [ ] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- Scramble은 큐브가 실제로 회전하는 animation으로 재생되고, 소진 후 상태는 같은 seed의 즉시 적용과 동일합니다.
- 재생이 소비한 move는 committed user move count와 timer에 포함되지 않습니다.
- 재생에 관한 상태는 `std::optional<Player>` 하나이며, 그 값이 재생의 처음부터 마지막 commit까지 유지됩니다. 재생 판정도 count 규칙도 이 값의 존재 여부 하나를 봅니다.
- 출처를 이름 붙이는 enum이 존재하지 않으며, tempo·count 규칙은 Player가 든 값이거나 재생 여부 bit에서 직접 나옵니다. `commit_move()`는 출처 인자를 받지 않습니다.
- Orbit drag 중에도 재생이 진행됩니다 (Phase 6.5가 개정한 `is_busy()` 위에서). Busy 관련 술어는 `is_busy()` 하나뿐입니다.
- 폐기는 `playback`과 진행 중인 snap을 함께 버리므로, 폐기 이후에 이전 재생의 move가 commit되는 경로가 없습니다.
- 재생 중 layer를 돌리는 입력(cube·net·rings 세 picking, `turn_face`, UI move)은 차단되고, pointer는 3D cube가 보일 때 camera orbit만 하며 보이지 않으면 아무 일도 하지 않습니다. 소진 후 timer가 준비되고, 재생이 한 frame 안에 끝나는 짧은 scramble에서도 준비 판정이 이루어집니다.
- 어떤 view mode의 재생 중에 눌러도 카메라 각도는 유한한 값으로 남습니다.
- 재생 중 어떤 move도 리프트를 최대 높이에서 시작하지 않습니다 — 각 move가 0에서 올라 0으로 내려앉습니다.
- `turn_face`의 재생 거절은 Application 수준에서 이루어지며, 재생 시작 직후 첫 `advance()` 전에도 성립합니다.
- 사용자 drag의 snap duration도 `pointer_up`의 tempo 인자를 지나며, 이 phase에서 그 값은 기존 상수와 같습니다. `kMinSnapMs` 바닥도 tempo를 넘지 않습니다.
- Timer·게임 상태 전이·완주 판정이 `GameSession`에 있고, `GameController`에는 DOM 배선만 남습니다.
- 재생 중 소비 loop가 거절당하는 move를 만나도 frame loop가 끝납니다.
- 재생 중에는 진행 중인 snap의 소유자를 확인하는 조건 없이도 확정 경로가 도달 불가능합니다.
- Scramble 수는 정수로 노출되고, 표시된 수만큼의 move가 실제로 재생됩니다.
- Scramble 결과를 solved인지 검사하지 않으며, 재시도 경로도 다시 생기지 않습니다 (Phase 6.5의 결정 유지).
- 같은 build 안에서 같은 seed와 수는 항상 같은 sequence를 만듭니다. Seed가 공유 payload에서 빠졌으므로 native와 WASM의 결과가 일치해야 할 계약은 없고, scramble 생성 규칙은 언제든 바꿀 수 있습니다.
- Native, WASM, TypeScript unit, browser e2e와 production build가 모두 통과합니다.

## Verification commands

```bash
meson test -C build/native --print-errorlogs
source /path/to/emsdk/emsdk_env.sh && ./build_wasm.sh
npm --prefix web run test:unit
npm --prefix web run test:e2e
npm --prefix web run build
```

## 개정 기록

### 1. 시작 직전 대조

이 문서는 Phase 8·8.5 이전에 쓰였습니다. 시작 전에 주장을 소스와 맞춰 보고 아래를 고쳤습니다.

**틀린 주장 하나.** "`reset_cube()`/`scramble()`/`shutdown()`이 `drain_orbit()` + `interaction.reset()`을 부르고 있다" — `shutdown()`은 그러지 않습니다. Phase 6.5 2차가 전역을 `ApplicationState` 하나로 묶은 뒤로 상태를 통째로 파괴할 뿐입니다. 폐기 목록에서 뺐습니다.

**그 사이 늘어난 것 둘.** 차단해야 할 picking이 cube 하나에서 cube·net·rings 셋이 되었고, view 전환 진입점이 `set_view_mode` 하나에서 `set_flat_style`과 둘이 되었습니다. 전자는 guard를 `pointer_down` 맨 앞에 두는 것으로 한 자리에서 해결되지만, 그 위치가 "확정 경로가 도달 불가능"의 전제이므로 명시했습니다.

**설계 결함 하나.** "재생 중 press는 좌표와 무관하게 orbit"은 `ViewMode::Flat`에서 카메라를 NaN으로 만듭니다. 그 mode의 `placement.cube`는 기본 `Rect{0,0,0,0}`이고, 0으로 나눈 pixel당 각도가 `OrbitCamera::turn`의 `fmod`를 통과하면 yaw가 영구히 NaN이 됩니다. 3D cube가 보일 때만 orbit으로 보내고 아니면 거절하는 것으로 바꿨는데, 이는 Phase 8이 "전개도 전용 view에는 orbit할 대상이 없다"고 이미 정해 둔 것과 같은 규칙입니다.

**보이지 않던 상호작용 하나.** 재생은 move 사이에 쉬는 frame이 없어 리프트의 `opened_ms_`가 상한에 머뭅니다. 그대로 두면 매 move가 최대 높이에서 시작해 조각이 스무 번 깜빡입니다. `start_move()`가 그 값을 되돌리게 해, 재생의 각 move가 keyboard turn 하나와 같은 호를 그리도록 했습니다. Tempo 상수도 이 리프트 시간(`kOpeningMs`)에 맞춰 정했습니다.

**행 상태 하나.** 소비 loop가 `start_move()`의 거절을 처리하지 않으면 `next`가 멈춘 채 `advance()`가 영원히 true를 반환합니다.

**과소평가한 범위 하나.** 손볼 test는 known-answer 하나가 아니라 `scramble()` 직후 큐브를 읽는 전부입니다. 목록을 문서에 실었습니다. 대신 키보드 solve e2e는 버리지 않고 3수로 줄입니다 — 브라우저에서 `running → completed`를 보는 유일한 경로라, 문제는 그 존재가 아니라 24키라는 길이였고 `move_count`가 인자가 된 지금 길이는 고를 수 있습니다.

**빠져 있던 단계 하나.** `GameSession` 분리가 Scope에만 있고 구현 단계도 acceptance criterion도 없었습니다. 문서 자신의 근거("web 코드가 쌓이기 시작하는 이 phase의 첫 단계")를 따라 단계 0으로 두었습니다.

## Completion

모든 acceptance criteria와 verification command를 통과한 뒤 다음 작업을 수행합니다.

- 이 문서의 status를 `Completed`로 변경합니다.
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 9를 완료 처리합니다.
- 실제 구현과 차이가 생긴 결정을 이 문서에 기록합니다.
