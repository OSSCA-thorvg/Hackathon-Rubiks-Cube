# Phase 11: Move History, Solve, and Undo

## Status

`Completed`

## Objective

Cube에 적용된 move를 되감을 수 있게 기록하고, 그 위에 solve와 undo를 함께 올립니다.

기록은 **하나의 선형 timeline**입니다. Scramble 수순과 사용자 move가 한 배열에 이어지고, 그 위를 두 인덱스가 가리킵니다: `cursor`(현재 적용된 개수)와 `scramble_end`(scramble 구간의 끝). 큐브에 실제로 일어나는 일이 언제나 하나의 선형 수순이므로, 자료구조도 그 모양을 그대로 따릅니다.

이 모양이 핵심 불변식을 자료구조로 만듭니다. "적용된 사용자 수는 완성된 scramble 위에서만 존재한다"를 두 기록으로 나눠 담으면 두 cursor의 조합 규칙으로 지켜야 하지만, cursor가 하나인 선형 배열에서는 **뒤의 수가 적용됐는데 앞의 수가 안 된 상태를 적을 방법 자체가 없습니다.** 지킬 규칙이 아니라 표현 불가능한 상태입니다.

화면에서 구분되어 보여야 하는 두 구간(Phase 12)과 공유 payload의 두 구간(Phase 14)은 `scramble_end` 하나로 파생됩니다. 표현이 두 구간을 원한다고 저장까지 둘일 필요는 없습니다.

기록이 C++에 있는 이유는 drag로 commit되는 move가 engine 안에서만 존재하기 때문입니다. TypeScript는 count만 관찰하므로 TS 쪽 기록은 재구성이 불가능합니다.

## Scope

- `cube::MoveTimeline`: 한 배열과 두 인덱스(cursor, scramble_end)를 가진 순수 컨테이너
- Application의 timeline 연결: Player에 더하는 `timeline_effect`와 `stoppable` 두 필드
- 저장하던 `user_move_count`의 은퇴 — 적용된 사용자 수는 timeline에서 파생됩니다
- Undo/redo: 한 칸 되감기/재진행 (재생 중에는 거절)
- Solve: cursor가 0이 될 때까지 되감는 수순을 Player로 재생, 중단 가능
- Phase 9의 폐기 helper와 Player를 그대로 재사용 (이 phase가 더하는 재생 경로 없음)
- C ABI: `undo`, `redo`, `solve_rewind`, `stop_playback`, `timeline_length`, `timeline_cursor`, `timeline_scramble_end`
- `GameController`의 undo/redo/solve UI와 상태 전이
- Native/TS unit test와 e2e

## Out of scope

- 되감을 move의 상쇄 축약 — 자기가 둔 수순이 그대로 되짚어지는 편이 보기 좋고, 축약은 필요해지면 나중에 얹을 최적화입니다
- Notation 표시와 수순 목록 UI (Phase 12) — 이 phase의 ABI는 길이와 두 인덱스까지입니다
- 기록의 직렬화와 URL 공유 (Phase 14)
- History 없는 큐브를 푸는 solver (Phase 16)
- Undo가 섞인 기록의 무효 처리 — undo에 드는 시간이 이미 손해이므로 두지 않습니다
- Solved까지 되감은 뒤 순방향으로 다시 보는 replay와 재생 일시정지 — replay 자체는 redo의 반복이라 거의 공짜지만, 일시정지는 busy로 구분되지 않는 별도 상태(paused)와 그 상태의 mutation 거절 규칙, ABI 셋을 끌고 옵니다. 관람 가치에 비해 구조가 커서 두지 않고, 필요해지면 그때 붙입니다

## Architecture decisions

### MoveTimeline은 cube의 순수 컨테이너

```cpp
namespace rubiks::cube {

/** What one committed move does to the timeline cursor. */
enum class TimelineEffect { Advance, Rewind, None };

/** One linear record of the moves a cube session is made of. */
class MoveTimeline {
public:
    /** Replaces everything with a new, not-yet-applied scramble sequence. */
    void begin_scramble(std::vector<CubeMove> moves);

    /** Records a user move at the cursor, discarding everything after it. */
    void record(const CubeMove& move);

    /** The recorded move at an index, as it was played. */
    [[nodiscard]] const CubeMove& at(std::size_t index) const noexcept;

    /** Moves the cursor after a planned move was actually committed. */
    void step(TimelineEffect effect) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t cursor() const noexcept;
    [[nodiscard]] std::size_t scramble_end() const noexcept;
    void clear() noexcept;
};

}  // namespace rubiks::cube
```

- `begin_scramble()`은 전체를 새 수순으로 교체하고 `cursor = 0`, `scramble_end = size`로 둡니다. 재생은 commit마다 `step(Advance)`로 cursor만 전진합니다.
- `record()`는 cursor 뒤를 전부 버리고(`resize(cursor)`, `scramble_end = min(scramble_end, cursor)`) 덧붙인 뒤 cursor를 전진합니다. **"새 수가 redo 구간을 버린다"는 한 규칙이 사용자 redo tail과 미적용 scramble 구간에 똑같이 적용되는 것이, 두 개의 절단 규칙이 아니라 `min` 한 번입니다.**
- `cursor() < scramble_end()`는 되감기 도중의 정상 상태입니다(redo를 기다리는 구간). Solve의 정상 종료가 정확히 이 모양(`cursor = 0 < scramble_end`)이므로, 그 위의 사용자 move는 거절이 아니라 `record()`의 절단으로 받아들입니다 — 거절하면 "방금 푼 큐브를 만질 수 없다"가 됩니다.
- **접근자는 `at(index)` 하나이고 뒤집지 않습니다.** Cursor는 commit에만 움직이므로 "cursor 위치의 한 수"를 주는 접근자는 계획을 만드는 동안 같은 수만 반복해 돌려줍니다. 뒤집는 일은 plan을 만드는 곳 한 군데로 모아, 이중 inverse로 원래 수가 재실행될 자리를 없앱니다.
- **복원 전용 API는 없습니다.** Phase 14의 복원은 `begin_scramble()` 후 scramble 구간을 cube에 적용하며 `step(Advance)`, 이어 사용자 구간을 적용하며 `record()` — 일반 연산의 재사용이고, cursor는 자연히 끝에 옵니다.
- 다른 target에 의존하지 않는 cube 안의 순수 자료구조라 native test만으로 전부 검증됩니다.

### 기록 규칙은 두 갈래와 정수 하나

**모든 적용이 Phase 7의 `commit_move(move)` 하나를 통과하고, 그 안의 분기는 재생 여부 둘뿐입니다.**

```cpp
// Application::commit_move
if (playback_) timeline_.step(playback_->timeline_effect);
else           timeline_.record(move);  // 뒤 구간 절단 포함
```

- Player에 `TimelineEffect timeline_effect`를 더합니다 — **기본값 없이**, 모든 생산자가 명시합니다. Scramble과 redo가 `Advance`, 되감기가 `Rewind`, ambient가 `None`입니다. 정수 `-1/0/+1` 대신 이름 있는 값을 쓰는 이유는 오용의 결과가 크기 때문입니다: ambient가 `Advance`를 받으면 관람 첫 commit부터 cursor가 timeline 길이를 넘어 기록이 즉시 손상됩니다. 값 이름이 효과를 그대로 말하므로 산술 오용의 자리가 없고, `step`의 "delta는 ±1 아니면 0" docstring 계약이 타입으로 대체됩니다.
- 기본값이 없어도 컴파일러가 명시를 강제하지는 못하므로(빠진 초기화는 첫 enumerator로 조용히 값-초기화됩니다), 각 생산자의 timeline 효과는 test가 고정합니다 — scramble 재생의 전진, 되감기의 후진, ambient의 불변이 모두 이 phase의 test 목록에 있습니다.
- **Scramble 재생과 redo가 같은 값인 것은 우연이 아닙니다** — 둘 다 "timeline의 다음 수를 적용"하는 같은 일입니다. 되감기 commit이 어느 기록을 움직일지 고르는 규칙도 없습니다. Cursor가 하나이기 때문입니다.
- `step_back()`/`step_forward()` 대신 `step(effect)` 하나를 두면 위 분기가 부호 선택 없이 값 전달로 끝나고, ambient의 "기록하지 않음"도 별도 행이 아니라 `None`이라는 값이 됩니다.
- Cursor가 `[0, size()]`를 벗어나지 않을 것은 debug assertion으로만 고정합니다. 호출자가 Application 하나뿐이라 런타임 거절 경로나 회복 구조를 두면 검증할 수 없는 코드가 남고, 위반은 개발 중 assertion으로 잡히는 편이 정확합니다.
- 진입점은 `advance()`의 animation 종료 지점과 Phase 7의 `finish_snap()` 확정 지점 둘이지만, 둘 다 이 함수를 부릅니다.
- Reset은 `clear()`, scramble은 `begin_scramble()`입니다.

### `user_move_count`는 저장하지 않고 파생합니다

**이 phase에서 저장 counter를 은퇴시키고, 적용된 사용자 수를 timeline에서 계산합니다**: `cursor > scramble_end ? cursor - scramble_end : 0`.

- 그대로 두면 counter와 timeline이 어긋납니다. Undo는 재생이라 count에서 빠지므로(Phase 9의 규칙) 5수 뒤 두 번 undo한 판은 counter가 5인 채 실제 적용된 사용자 수는 3이 됩니다. 감소 규칙을 더해 맞추는 방법도 있지만, 그러면 절단(`record`가 cursor 뒤를 버릴 때)과 solve와 복원에서도 같은 보정이 각각 필요해집니다.
- 파생값은 그 보정들이 전부 필요 없습니다. 이 phase의 세 연산(`record`의 절단, `step`, `begin_scramble`)이 이미 timeline을 정확히 유지하므로, 파생값은 정의상 언제나 맞습니다.
- 기존 count ABI는 이름과 시그니처를 유지한 채 파생값을 반환합니다. Web은 바뀌지 않고, `commit_move`에서 count를 관리하는 코드가 사라집니다.
- Phase 9까지는 저장 counter가 정확합니다(재생과 사용자 commit만 있고 되감기가 없으므로). 그래서 이 은퇴는 Phase 9의 결정을 되돌리는 것이 아니라, 되감기가 생기는 이 phase에서 파생으로 옮기는 것입니다.

### Commit의 관찰은 cursor 변화입니다

**별도의 commit counter를 두지 않습니다.** 모든 commit이 `timeline_cursor`를 정확히 ±1 움직이므로(User·Scramble·Redo는 +1, Rewind는 −1, Ambient는 불변), `GameController`가 frame마다 cursor를 읽어 이전 값과 다르면 commit이 있었던 것입니다. 완료 판정과 Phase 13의 효과음이 모두 이 관찰 하나를 씁니다.

- Cursor를 통째로 바꾸는 명령(reset, scramble)은 **GameController 자신이 부르는 것**이라, 그 handler에서 기준값을 갱신하면 됩니다 — 지금 코드의 `onScramble`이 `previousMoveCount = 0`으로 이미 쓰는 패턴입니다. Phase 14의 복원은 controller 부착 전에 끝나므로 부착 시점의 초기화로 충분합니다.
- Ambient는 cursor를 움직이지 않습니다. 덕분에 "관람은 무음"이 자료구조의 결과로 굳고, web이 ambient 여부를 조회해 분기할 필요가 없습니다.
- 재생 중에는 pointer가 차단되어 +1과 −1이 한 frame에 섞일 수 없으므로, 관찰은 "달라졌는가" 판정만으로 안전합니다. 한 관찰 창에 commit이 둘 들어와도(빠른 연속 입력에서 `finish_snap()` 확정이 frame 사이에 끼는 경우) 그 창은 16ms라 소리 하나로 들리는 것이 자연스럽고, 개수까지 세는 계약은 두지 않습니다.
- **예외는 timer의 시작 하나입니다 — cursor가 아니라 파생 사용자 수의 증가를 봅니다.** Cursor 변화만 보면 ready 상태에서 Solve를 눌렀을 때 scripted rewind의 첫 commit이 timer를 시작합니다. 적용된 사용자 수(`cursor − scramble_end` 파생)는 사용자 move에서만 늘어나므로 timer 시작의 관찰값은 그것이고, solved 판정·수순 목록·효과음은 계속 cursor 변화를 봅니다. 관찰 소스는 둘 다 이미 노출된 query라 새 상태는 없습니다.

### 되감기는 수순을 만들어 Player에 넘깁니다

**명령은 재생할 수순을 그 자리에서 계산해 Phase 9의 Player로 만듭니다.** 재생 자체는 그 소비이므로 이 phase가 만드는 재생 경로는 없습니다.

```cpp
/** The moves that take the cursor to the given applied count. */
[[nodiscard]] std::vector<cube::CubeMove> rewind_plan(std::size_t to) const;
```

적용된 수는 `cursor()` 그 자체입니다. Undo가 한 수이고 solve가 전부인 것은 **인자 하나의 차이**입니다.

| 명령 | 수락 조건 (`!is_busy()` 공통) | 넣는 수순 |
|---|---|---|
| Undo | `cursor() > scramble_end()` | `rewind_plan(cursor() - 1)` |
| Redo | `cursor() < size()` | `redo_plan(cursor() + 1)` |
| Solve | `cursor() > 0` | `rewind_plan(0)` |

- Undo의 하한 `scramble_end()`가 **"undo는 자기 수 안에서만"이라는 정책 그 자체**입니다. Scramble만 적용된 판이나 solve가 scramble 구간까지 내려간 판에서는 `cursor() ≤ scramble_end()`라 undo가 같은 표에서 자동으로 거절됩니다.

```cpp
// 되감기(cursor → to)
for (std::size_t i = timeline_.cursor(); i > to; --i) {
    plan.push_back(inverse(timeline_.at(i - 1)));
}

// 재진행(cursor → to)
for (std::size_t i = timeline_.cursor(); i < to; ++i) {
    plan.push_back(timeline_.at(i));
}
```

- Loop가 `to`에서 멈추므로 plan의 길이는 언제나 정확히 `|cursor() - to|`이고, `at(i - 1)`은 `i > to ≥ 0`에서만 불려 `std::size_t` underflow가 생길 자리가 없습니다.
- **Timeline은 raw 수순을 보관하고 되감기에서만 `inverse()`를 씌웁니다.** 재진행은 보관된 수를 그대로 다시 재생하므로, 양방향이 정확한 역순입니다.
- **Cursor는 계획이 아니라 commit이 움직입니다.** 그래서 중간에 멈춰도 cursor와 cube 상태가 정확히 일치합니다.
- **계획이 Player에 있는 동안 timeline은 바뀌지 않습니다.** 실제로 timeline을 바꾸는 명령(scramble, reset, ambient 진입)은 폐기 helper가 Player를 함께 버리고, 사용자 move는 busy라 거절되므로 이미 성립합니다. 그래서 Player가 든 수순이 timeline의 사본이어도 둘이 어긋나지 않습니다.
- **`rewind_target_`도, commit마다 다음 수를 고르는 공급 규칙도, 방향을 대소로 유도하는 규칙도 두지 않습니다.** 수순을 미리 만들면 undo와 solve의 차이가 벡터 길이로만 남습니다.
- **Phase 16이 재사용하는 것은 재생 경로까지입니다.** Solve는 이미 "`vector<CubeMove>`를 만들어 Player에 넘기는" 모양이므로 solver도 같은 자리에 수순을 넣을 수 있습니다. 다만 **solver move가 timeline에서 갖는 의미는 이 phase가 정하지 않습니다** — `TimelineEffect`의 세 값 중 어느 것도 맞지 않기 때문입니다: `Advance`는 timeline에 이미 있는 다음 수를 적용한다는 뜻이고, `Rewind`는 기록된 수를 되감는다는 뜻이며, `None`은 cube만 바뀌어 timeline과 상태가 어긋납니다. Solver의 수는 timeline에 없는 새 수라 네 번째 답이 필요한데, 그 답(기록할지, 완주로 칠지, 기록한다면 어느 구간인지)은 소비자가 생기는 Phase 16에서 정합니다. 이 phase에서 미리 정책을 만들면 검증할 소비자 없이 규칙만 남습니다.
- **중단**(`stop_playback`)은 남은 수순을 버리고 진행 중인 snap을 Phase 7의 `finish_snap()`으로 확정합니다. 확정 commit은 아직 살아 있는 `playback_`을 보고 `commit_move`를 타므로 cursor 이동까지 따라옵니다. Solve는 수십 초가 걸릴 수 있어 중단이 필요하고, 구현은 이미 있는 두 조각의 조합입니다.
- **`stop_playback`이 보는 것은 Player의 `stoppable`(기본 `false`)이고, 되감기·재진행만 그것을 `true`로 세웁니다.** 같은 Player를 scramble과 ambient도 쓰므로, 제한하지 않으면 UI 갱신이 한 frame 늦은 사이의 입력이 scramble을 반쯤에서 끊거나 ambient를 snapshot 복원 없이 멈춥니다. 기본값이 `false`라 Phase 9·10의 생산자는 손대지 않습니다.
- 재생 여부를 나타내는 별도 상태나 ABI는 두지 않습니다. 밖에서 보이는 것은 기존 busy뿐입니다.
- 재생 속도는 되감기용 tempo를 Player에 실어 정하고, UI control은 Phase 13이 하나로 묶습니다.

### 진행 중 재생의 원자적 폐기

`scramble`, `reset_cube`, ambient 진입처럼 새 상태를 만드는 명령은 진행 중인 재생을 먼저 끝냅니다. **Phase 9의 폐기 helper(`drain_orbit()` + `interaction.reset()` + `playback_.reset()`)를 그대로 부르고, 이 phase가 목록에 더하는 항목은 없습니다.** 되감기가 남기는 상태도 Player 하나뿐이기 때문입니다.

- `interaction.reset()`이 `CubeState`를 건드리지 않으므로, 폐기는 이미 commit된 수만 남기고 정확히 멈춥니다.
- `stop_playback`은 예외적으로 진행 중인 snap을 **확정**합니다(폐기가 아닙니다). 진행 중인 회전을 도중에 되돌리는 편이 더 어색하고, 확정은 아직 살아 있는 `playback_`을 타므로 timeline도 어긋나지 않습니다.

### 세션 의미

- Solve로 끝난 판은 기록으로 남는 완주가 아닙니다. Timer는 정지하되 completion 안내를 구분하고, Phase 14의 세션 기록은 이 판을 제외합니다.
- **판별은 보관하는 상태가 아니라 파생 값입니다.** 완주를 관찰한 순간 `cursor() == 0`이면서 `scramble_end() > 0`이면 그 판은 solve가 푼 것입니다. Cursor를 scramble 구간 아래로 내릴 수 있는 명령은 solve뿐이고(undo의 하한이 `scramble_end()`), 사용자가 직접 완성한 판은 반드시 `cursor() > scramble_end() > 0`이기 때문입니다. 이미 노출된 query만으로 완주 시점에 계산되므로, 수락 시 세우고 stop·reset·scramble에서 지우는 pending flag도 그 전이 규칙도 존재하지 않습니다.
- 이 판별은 경계 케이스를 규칙 없이 맞게 처리합니다. **Solve를 중단한 뒤 사용자가 직접 완성한 판**은 `cursor() > 0`이라 정상 기록이고, **마지막 수가 도는 중의 stop**으로 `finish_snap()`이 solved를 만든 판은 `cursor() == 0`이라 solve가 푼 판입니다 — 되감기를 전부 수행한 것이 실제로 solve이므로 분류가 사실과 일치합니다.
- Timer 측정 중의 undo는 그대로 허용합니다. Undo/redo는 사용자가 시작한 명령이지만 scripted 재생이라 cursor만 움직이므로, 파생 count는 되감은 만큼 자연히 줄어듭니다. 대신 `GameController`는 `timeline_cursor`의 변화를 commit으로 관찰해, undo만으로 solved에 도달한 경우에도 판정이 이루어집니다.
- Busy 중의 undo/redo/solve는 거절합니다. 쌓이는 명령 queue를 만들지 않고, 재생을 끊고 싶으면 `stop_playback`을 씁니다.
- **재생 중 pointer는 camera orbit만 합니다.** Layer를 잡는 pick과 `finish_snap()` 확정은 재생 중에 닿지 않으므로, 수십 초짜리 solve를 돌려 가며 볼 수 있으면서도 재생이 가로채이지 않습니다. 규칙은 Phase 9가 정의합니다.

### C ABI

```text
thorvg_rubiks_undo() -> int
thorvg_rubiks_redo() -> int
thorvg_rubiks_solve_rewind() -> int
thorvg_rubiks_stop_playback() -> void
thorvg_rubiks_timeline_length() -> uint32
thorvg_rubiks_timeline_cursor() -> uint32
thorvg_rubiks_timeline_scramble_end() -> uint32
```

- 세 query로 화면과 공유에 필요한 모든 파생이 됩니다: scramble 구간은 `[0, scramble_end)`, 사용자 구간은 `[scramble_end, length)`, 적용된 사용자 수는 `cursor - scramble_end`(cursor가 그 위일 때)입니다. Phase 12가 목록 표시에, Phase 14가 payload 구성에 같은 세 값을 읽습니다. 기존 count ABI도 이제 이 파생값을 돌려줍니다.
- 되감을 것이 없거나(위 표의 수락 조건 위반) 되감기 재생이 아닌 busy이면 0을 반환합니다. Query는 초기화 전 0을 반환합니다.
- Commit 관찰용 counter ABI는 없습니다. `timeline_cursor`의 변화가 곧 commit입니다.

## Implementation steps

### 1. MoveTimeline

- [x] `TimelineEffect`와 `MoveTimeline` 구현, `begin_scramble`/`record`/`at`/`step(effect)`/`clear` 전수 test
- [x] `record()`가 cursor 뒤를 버리고 `scramble_end`를 함께 내리는 test (`min` 한 번으로 두 절단이 처리되는지)
- [x] `at(index)`가 기록된 그대로의 수를 반환함을 고정하는 test (뒤집지 않음)
- [x] 표현 불가능성을 이용한 단순화 확인: 두-기록 모델의 cursor 조합 불변식 test에 해당하는 것이 **존재하지 않음**을 문서로 남김 (배열 하나 + cursor 하나로는 위반 상태를 만들 수 없음)

### 2. Timeline과 명령

- [x] Scramble이 `begin_scramble()`을, reset이 `clear()`를 부르는 연결
- [x] **Solve 완주 직후(`cursor 0 < scramble_end`) 사용자 move가 받아들여지고 timeline이 잘리는 test** (거절되지 않음)
- [x] Solve를 scramble 구간에서 중단한 뒤 사용자 move가 cursor까지 자르고 기록되는 test
- [x] Undo가 `scramble_end()` 아래로 내려가지 못하는 test
- [x] `cursor() == 0` = solved test: scramble 후 전부 되감으면 solved

### 3. Commit과 되감기 공급

- [x] Player에 `timeline_effect`(기본값 없음)와 `stoppable`(기본 false) 추가, `commit_move(move)`의 두 갈래 분기 구현
- [x] **모든 생산자의 `timeline_effect` 명시**: Phase 9의 scramble에 `Advance`, Phase 10의 ambient에 `None` (ambient가 전진하면 cursor가 길이를 넘어섬)
- [x] 관람을 길게 돌린 뒤에도 `timeline_cursor`와 `timeline_length`가 진입 시점 그대로인 test
- [x] **저장 `user_move_count` 제거와 파생값 전환**: 기존 count ABI가 `cursor - scramble_end`를 반환하도록 바꾸고, undo 두 번 뒤의 count가 실제 적용 수와 일치하는 test (저장 counter였다면 어긋나는 시나리오)
- [x] Phase 7의 `finish_snap()` 확정 경로가 같은 함수를 통과하는 test (즉시 확정된 move가 timeline에 남는지)
- [x] `rewind_plan(to)`/`redo_plan(to)` 구현과 test: undo와 solve가 같은 함수에 다른 인자를 준 결과임을 고정
- [x] `rewind_plan(0)` known-answer: timeline `S0,S1,U0,U1` → `[U1⁻¹, U0⁻¹, S1⁻¹, S0⁻¹]`
- [x] `redo_plan(size)` known-answer: 전부 되감긴 상태에서 → `[S0, S1, U0, U1]`
- [x] **한 칸 plan table test**: undo(`to = cursor-1`)와 redo(`to = cursor+1`) 두 행으로, plan 길이가 각각 1인지
- [x] **Scramble-only solve test**: 사용자 수 없이 solve → scramble 전체가 역순으로 되감기고 underflow가 없는지
- [x] 수락 조건 표 test: scramble만 적용된 판과 solve가 scramble 구간까지 내려간 판에서 undo 거절
- [x] Busy(재생, gesture, scramble 재생, ambient) 중 undo/redo/solve가 거절되는 test
- [x] **Undo 한 번이 사용자의 원래 수를 재실행하지 않고 정확히 되돌리는 test**
- [x] **재생 integration test**: plan을 Player에 넘기고 소진할 때까지 돌려 순서·exactly-once commit·최종 cursor·cube 상태를 검사하고, 되감기가 `scramble_end` 경계를 끊김 없이 지나는 것을 명시적으로 assert
- [x] Phase 9 helper 재사용 test: solve 재생 중 reset → 이후 `advance()`를 여러 번 돌려도 아무 move가 시작되지 않고 최종 cube·cursor가 reset 상태
- [x] `stop_playback`은 폐기가 아니라 확정임을 구분하는 test (중단 시점의 cursor와 cube 상태가 일치, 이후 redo/undo 재개 가능)
- [x] `stop_playback`이 scramble·ambient 재생 중에는 no-op인 test (`stoppable` 기본값이 그 둘을 보호하는지)
- [x] Ambient가 timeline에 흔적을 남기지 않는 test

### 4. Boundary와 UI

- [x] C ABI 추가와 generated 산출물, fake fixture 갱신
- [x] Undo/Redo/Solve 버튼과 재생 중 stop control
- [x] Engine-solved 판별: 완주 관찰 시 `cursor == 0 && scramble_end > 0`이면 solve가 푼 판으로 분류 (보관하는 flag 없음)
- [x] **판별 TS unit test**: solve 완주는 기록에서 제외, 사용자가 직접 완성한 판과 solve 중단 후 직접 완성한 판은 정상 기록
- [x] Completion 안내 구분
- [x] TS unit test: `timeline_cursor` 관찰 기반 상태 전이와 reset/scramble handler의 기준값 갱신, undo로 solved 도달
- [x] **Timer 시작 관찰 test**: ready에서 Solve 재생이 timer를 시작하지 않고, 파생 사용자 수를 늘리는 첫 사용자 move만 시작하는지
- [x] e2e: scramble → 몇 수 → undo 두 번 → redo → Solve로 완주 → solved 전개도 검증
- [x] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- 기록은 timeline 하나이며, `cursor() == 0`이면 언제나 solved입니다.
- "적용된 사용자 수는 완성된 scramble 위에서만 존재한다"는 성질은 규칙이 아니라 표현 불가능성으로 성립하며, 이를 지키는 별도 불변식 코드와 test가 존재하지 않습니다.
- 되감기 도중의 `cursor() < scramble_end()`는 위반이 아니라 정상 상태이고, solve를 끝까지 재생한 뒤에도 사용자는 큐브를 바로 조작할 수 있으며 그 move가 timeline을 자릅니다.
- 새 사용자 move는 cursor 뒤를 통째로 버립니다 — 사용자 redo tail과 미적용 scramble 구간이 한 규칙으로 함께 폐기됩니다.
- Timeline은 raw 수순을 보관하고 되감기에서만 뒤집으므로, undo 한 번이 원래 수를 재실행하는 일이 없고 재진행은 되감기의 정확한 역순입니다.
- 되감기·재진행은 plan 전체를 Phase 9의 Player에 넘기고, cursor는 실제 commit에서만 움직이며, 중단과 reset은 남은 수순을 폐기합니다.
- Undo와 solve는 `rewind_plan(to)` 한 함수에 다른 인자를 준 결과이며, plan의 길이는 언제나 `|cursor() - to|`입니다. `rewind_target_`도 commit마다의 공급 규칙도 존재하지 않습니다.
- 모든 move는 진입점과 무관하게 `commit_move(move)` 하나를 통과하므로, Phase 7의 즉시 확정도 기록·효과음에서 누락되지 않습니다. 그 안의 분기는 재생 여부 두 갈래이고, 출처를 이름 붙이는 enum이나 네 행짜리 표가 존재하지 않습니다.
- 적용된 사용자 수는 timeline에서 파생되며 저장 counter가 존재하지 않습니다. Undo·절단·solve·복원 어디에도 count 보정 코드가 없습니다.
- Commit의 관찰은 `timeline_cursor`의 변화이며 별도 counter ABI가 없습니다. Reset·scramble handler는 기준값을 스스로 갱신하고, ambient는 cursor를 움직이지 않아 무음이 유지됩니다. 복원이 관찰되지 않음은 Phase 14가 확인합니다.
- Solve가 푼 판의 판별은 완주 시점의 `cursor() == 0 && scramble_end() > 0` 파생 값이며, 보관되는 flag도 그 전이 규칙도 없습니다. 중단한 뒤 사용자가 직접 완성한 판은 정상 기록으로 남습니다.
- 재생은 언제든 중단할 수 있고, 중단 시점의 cursor와 cube 상태가 정확히 일치합니다.
- 새 상태를 만드는 명령은 Phase 9의 폐기 helper를 그대로 부르며, 이 phase가 그 목록에 더하는 항목이 없습니다. 이 phase가 Player에 더하는 것은 필드 둘이고, 그중 `stoppable`은 기본값이 기존 생산자를 그대로 두지만 `timeline_effect`는 기본값이 없어 기존 생산자(scramble, ambient)에 명시 한 줄씩을 함께 더합니다.
- Solve로 끝난 판은 timer가 정지하되 완주 기록으로 취급되지 않으며, undo가 섞인 판은 무효화되지 않습니다.
- Ambient 관람은 timeline에 흔적을 남기지 않습니다.
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

- Native `meson test -C build/native` **17개 전부 통과** (`move history` 하나가 늘었습니다)
- WASM 재빌드, TypeScript unit **105개**, browser e2e **30개**, production build 통과

## 개정 기록

### 1. 구현하며 달라진 것

**`rewind_plan`/`redo_plan`은 `MoveTimeline`의 멤버가 아니라 `cube`의 자유 함수입니다.** 문서의 선언은 뒤에 `const`가 붙어 멤버를 가리켰지만, 본문은 `timeline_.`을 쓰는 Application의 코드였습니다. 둘 중 어느 쪽으로 두어도 되지만 **known-answer test가 결정했습니다**: 이 phase는 `rewind_plan(0)`이 `[U1⁻¹, U0⁻¹, S1⁻¹, S0⁻¹]`임을, 즉 계획의 *내용*을 고정하라고 요구합니다. Application의 익명 namespace에 두면 그 내용은 engine을 띄우고 재생을 끝까지 돌린 결과로만 관찰되므로, 계획을 만드는 곳의 test가 아니라 재생의 test가 됩니다.

`MoveTimeline.hpp`의 자유 함수는 두 요구를 동시에 만족합니다. `tests/cube`에서 engine 없이 직접 부를 수 있고, 컨테이너는 public 접근자만으로 답할 수 있는 질문을 멤버로 흡수하지 않아 "배열 하나와 인덱스 둘"인 채로 남습니다. Application은 `cube::rewind_plan(timeline, to)`를 부르는 한 줄이고, undo·solve가 인자 하나 차이라는 성질은 그대로입니다.

**완주 판정이 `running`뿐 아니라 `ready`에서도 일어납니다.** 문서는 `running`에서의 판정만 다뤘는데, 그러면 아직 아무 수도 두지 않은 ready 상태에서 Solve를 누른 판이 **끝나지 않습니다** — 큐브는 solved인데 세션은 armed된 timer를 들고 첫 수를 기다리고, 그 다음 사용자 move가 이미 풀린 큐브 위에서 시계를 시작합니다. 그래서 완주를 보는 상태를 `ready`와 `running` 둘로 두었습니다. Timer의 시작 관찰은 문서대로 파생 사용자 수이므로, 이 확장이 "Solve가 timer를 시작하지 않는다"를 깨지 않습니다 — 오히려 그 시나리오를 끝까지 처리합니다.

**Stop control은 숨김과 비활성을 함께 씁니다.** `hidden`만으로는 bootstrap이 시작 시 모든 버튼을 disable해 둔 것을 되돌릴 자리가 없어 버튼이 영영 눌리지 않습니다(e2e가 이것을 잡았습니다). Controller의 `rewinding` 값 하나가 두 속성을 함께 정하므로, 보이는 것과 닿을 수 있는 것이 어긋나지 않습니다. 그 값은 화면에 올릴지만 정하고, 눌렀을 때 무엇이 일어나는지는 여전히 engine의 `stoppable`이 정합니다.

**한 frame의 engine 읽기를 `EngineFrame` 값 하나로 묶었습니다.** `observe(busy)`가 `observe(frame)`이 되었습니다. Session이 cursor·파생 사용자 수·scramble 경계를 모두 보게 되었는데, controller도 같은 값들로 세 버튼의 가부를 정하므로 각자 읽으면 한 frame 안에서 여섯 번의 boundary 호출이 생깁니다. 값 하나를 만들어 session과 controls가 나눠 쓰면 "둘이 다른 순간을 말할 수 없다"는 기존 주석의 약속이 필드가 늘어도 그대로 유지됩니다.

**`CubeEngine`의 uint32 query 검증을 `countFrom` 하나로 모았습니다.** 이 phase가 count 계열 query를 셋 더하면서 `committedMoveCount`의 검증 네 줄이 네 벌이 될 참이었습니다. 이름만 인자로 받는 private helper 하나로 두어 boundary가 거절하는 값의 정의가 한 곳에 있습니다.

### 2. 계획에 없던 것

**Undo·Redo·Solve는 한 줄에 셋, Stop은 그 아래 한 칸입니다.** Side panel은 한 칸 폭의 세로 목록이라 버튼 셋을 그대로 얹으면 Scramble·Reset과 같은 무게로 읽히고 세로도 길어집니다. 셋을 `.game-actions__group` 한 줄로 묶고 글자만 한 단계 줄였습니다(모바일에서는 폭이 남으므로 이웃과 같은 크기로 돌아갑니다). Stop은 그 줄에 넣지 않았습니다 — 기록 위의 어디로 갈지를 고르는 셋과 달리 **가는 일 자체에서 빠져나오는 control**이고, 넷째 칸에 들어가면 폭이 모자라 글자가 잘립니다.

**Ambient 중에도 Undo·Redo·Solve는 눌립니다.** Phase 10이 move 버튼에 대해 정한 것과 같은 이유입니다: 큐브를 바꾸는 명령은 관람을 먼저 끝내고 실행되므로, 버튼을 꺼 두면 "키는 듣는데 버튼은 안 눌리는" 어긋남이 생깁니다. 셋 다 `cubeCommand`를 지나므로 관람은 press 한 번에 끝나고 명령이 그 위에서 실행됩니다.

### 3. 문서로만 남긴 것

**두-기록 모델의 불변식 test가 없다는 사실**은 `tests/cube/MoveTimelineTest.cpp` 첫머리의 주석으로 남겼습니다. 없는 test를 목록에서 확인할 방법은 그것뿐이고, 다음에 이 파일을 여는 사람이 "왜 조합 규칙 test가 없지"를 묻는 자리가 바로 거기이기 때문입니다.
