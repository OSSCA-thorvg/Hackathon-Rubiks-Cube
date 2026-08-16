# Phase 16: Cube Solver

## Status

`In progress`

## Objective

Timeline과 무관하게 **현재 `CubeState`만으로 해법을 계산하는 solver**를 추가하되, 구현을 교체할 수 있는 형태로 둡니다. 첫 구현은 **초보자 layer-by-layer(LBL)**이고 **3×3만** 지원합니다.

Solver는 `cube` 아래의 별도 하위 target에 격리합니다. `cube`가 아무 dependency도 갖지 않는 것과 같은 이유이되 한 겹 더 안쪽입니다 — solver는 `cube`를 읽지만, `cube`는 solver를 모릅니다. 이 저장소에서 가장 큰 단일 코드 덩어리가 되면서 rendering과 무관하므로, 도메인을 읽는 사람이 solver를 지나치지 않고도 도메인을 다 읽을 수 있어야 합니다.

### 되감기가 닿지 못하는 곳에 대한 정정

계획서와 Phase 15는 solver의 자리를 "되감을 기록이 없는 cube(직접 섞은 것, 링크로 받은 것)"라고 적었습니다. **소스를 확인한 결과 그런 cube는 존재하지 않습니다.** 이 application에서 cube를 바꾸는 경로는 전부 timeline을 지나고(`commit_move`의 두 갈래), 공유받은 상태도 `restore_apply`가 기록을 함께 세웁니다. 그래서 되감기는 언제나 가능합니다.

Solver의 자리는 기록의 유무가 아니라 **방향**입니다.

- 되감기는 **뒤로** 갑니다. 사용자가 백 수를 두어 거의 다 풀어 놓았어도 되감기는 그 백 수를 전부 거꾸로 돌려 스크램블 이전으로 데려갑니다. 지금 상태에서 solved까지 가는 길이 아닙니다.
- 공유받은 cube를 되감는 것은 **남의 세션을 거꾸로 재생하는 일**입니다. 받은 사람이 보게 되는 것은 해법이 아니라 그 사람이 무엇을 했는지입니다.
- Phase 17의 힌트는 현재 상태에서 계산되어야 의미가 있으므로 되감기로 대체할 수 없습니다.

계획서의 해당 문장은 이 문서로 개정합니다. 되감기는 그대로 남고(그것대로 옳은 명령입니다), solver는 그 옆의 두 번째 생산자가 됩니다.

## Scope

- `Solver` interface와 `cube/solver` 하위 target
- 초보자 LBL 구현(3×3), 조각 조회와 단계별 수순
- 해법 후처리: 같은 layer의 연속 turn 병합과 상쇄 제거
- Solver의 수가 timeline에서 갖는 의미 — Phase 11이 이 phase로 넘긴 결정
- Solve 명령과 ABI, 그리고 되감기와의 UI 분리
- 완주 판정: solver가 도운 판은 기록으로 남지 않습니다

## Out of scope

- **2×2 코너 BFS와 N×N reduction** — 별도 phase입니다. 이 phase는 자리(interface)와 첫 구현을 만들고, 지원 크기가 늘어나는 것은 그 자리에 구현을 더하는 일입니다. 아래 "다음 구현이 들어올 자리"가 LBL이 지켜야 할 제약을 미리 적어 둡니다.
- **실행 중 solver를 고르는 UI** — 구현이 하나뿐인 동안 고를 것이 없습니다. 선택 ABI는 interface를 바꾸지 않고 나중에 얹을 수 있습니다.
- **최적 해법** — LBL은 백 수 남짓을 냅니다. 짧은 해법은 탐색 계열의 몫이고, 그것을 더하는 것이 interface가 있는 이유입니다.
- **해법의 표시와 한 수씩 진행** — Phase 17입니다. 이 phase의 Solve는 해법 전체를 재생합니다.
- **비동기 계산** — LBL은 밀리초 단위라 명령 시점에 동기로 계산합니다. `solve()`가 frame 경로가 아니라 명령 경로에서 한 번만 불린다는 것이, 나중에 오래 걸리는 solver를 다른 곳으로 옮길 수 있게 남겨 두는 성질입니다.

## Architecture decisions

### Interface: 무엇을 푸는지 스스로 밝힌다

```cpp
namespace rubiks::cube::solver {

class Solver {
public:
    virtual ~Solver() = default;

    /** 이 solver가 푸는 cube의 크기인지. */
    [[nodiscard]] virtual bool supports(int size) const noexcept = 0;

    /** `state`를 solved로 데려가는 수순. 이미 풀린 cube에서는 빈 수순. */
    [[nodiscard]] virtual std::vector<CubeMove> solve(
        const CubeState& state) const = 0;
};

}  // namespace rubiks::cube::solver
```

자유 함수가 아니라 객체인 이유는 계획서 그대로입니다: 알고리즘마다 지원 크기가 다르고, 탐색 계열은 pruning table 같은 준비 상태를 생성자에서 만들어 들고 있어야 합니다. LBL은 준비 상태가 없지만, 그 자리가 비어 있다는 것과 그 자리가 없다는 것은 다릅니다.

**실패 경로가 없습니다.** `solve()`는 `supports(state.size())`가 참인 모든 `CubeState`에 대해 수순을 냅니다. `CubeState`는 `apply` 외에 상태를 바꾸는 방법을 노출하지 않으므로 **도달 불가능한 cube를 만들 수 없고**, 도달 가능한 cube는 정의상 풀립니다. 크기가 맞지 않는 호출은 precondition 위반이라 `supports()`로 거르는 것이 호출자의 몫입니다 — 두 질문에 두 답을 두면 "지원하지 않는 크기가 빈 수순을 냈다"와 "이미 풀려 있어서 빈 수순을 냈다"가 같은 값이 됩니다.

빈 수순 하나만은 남습니다: **이미 풀린 cube**입니다. 그것은 실패가 아니라 사실이고, 명령 쪽에서 "할 일이 없는 명령은 거절"이라는 기존 규칙(`play_rewind`의 빈 plan 거절)이 그대로 받습니다.

### 색이 아니라 면으로 쓴다

3×3은 센터가 고정이라 색과 면이 일대일입니다. 그런데도 LBL의 모든 단계를 **면**으로 씁니다 — 첫 층은 `Down`이지 노랑이 아닙니다.

- Palette는 rendering의 것이라 도메인이 볼 이유가 없고, `solved_color()`가 `Face` 순서와 같다는 사실에 기대면 그 대응이 바뀔 때 solver가 조용히 틀립니다.
- 조각을 찾을 때만 색을 씁니다: 코너는 노출된 세 색의 집합, 엣지는 두 색의 집합으로 유일하게 식별됩니다. **노출된 면만 봅니다** — `Cubie`는 안 보이는 면에도 색을 들고 있고 그 색도 회전을 따라 돌기 때문에, 여섯 칸을 다 보면 같은 색이 여러 조각에서 나옵니다.

### LBL은 자기 사본 위에서 실제로 돌려 본다

`solve()`는 `state`를 복사하고, 단계마다 "지금 케이스를 판정 → 수순을 이어 붙이고 사본에 `apply`"를 반복한 뒤 쌓인 수순을 돌려줍니다.

이 형태의 값은 **틀릴 수 있는 방법이 하나로 줄어든다**는 것입니다. 수순과 상태가 어긋나는 종류의 버그가 존재할 수 없고 — 낸 수순이 곧 사본에 적용된 수순이므로 — 남는 실패는 "단계가 끝나지 않는다" 하나입니다. 각 단계는 자기 술어가 참이 될 때까지 반복하고, 반복 상한은 **회복 경로가 아니라 버그를 잡는 자리**이므로 debug assertion으로 둡니다.

일곱 단계입니다. 각 단계는 앞 단계가 맞춘 것을 보존하는 수순만 쓰므로 종료가 구조적으로 보장됩니다.

| 단계 | 목표 | 수순 |
| --- | --- | --- |
| 1 | D면 십자 (옆면 색까지) | setup만, 고정 수순 없음 |
| 2 | 첫 층 코너 4개 | 코너 삽입 하나를 반복 |
| 3 | 둘째 층 엣지 4개 | 좌·우 삽입 둘 |
| 4 | U면 십자 (엣지 방향) | 엣지 뒤집기 하나를 1~3회 |
| 5 | U면 완성 (코너 방향) | 코너 돌리기 하나를 반복 |
| 6 | 마지막 층 코너 위치 | 코너 순환 하나 + U 정렬 |
| 7 | 마지막 층 엣지 위치 | 엣지 순환 하나 + U 정렬 |

**수순의 가짓수는 여섯 남짓이고 나머지는 전부 setup입니다.** 케이스마다 수순을 두면 케이스가 곱셈으로 늘어나는데, "조각을 정해진 슬롯으로 가져다 놓고 하나의 삽입 수순을 방향이 맞을 때까지 반복"으로 쓰면 덧셈이 됩니다. CFOP의 F2L·OLL·PLL 전체 표(41 + 57 + 21 케이스)는 수순을 절반으로 줄이는 대신 코드의 대부분이 표가 되므로 첫 구현으로 두지 않습니다.

**문자열 표기를 쓰지 않습니다.** `CubeMove`가 "R"과 "3Rw"는 layer 집합의 직렬화이고 파서를 두지 않는다고 명시하고 있으므로([CubeMove.hpp:57](../../engine/src/cube/CubeMove.hpp:57)), 수순은 solver 안의 `constexpr` 헬퍼로 적습니다. 문헌의 수순을 옮겨 적을 때 사람이 읽을 수 있어야 하므로 헬퍼 이름은 표기를 따릅니다(`R()`, `Rp()`, `R2()`).

### 다음 구현이 들어올 자리

N×N은 LBL을 확장하는 것이 아니라 **감쌉니다**. 표준 reduction은 센터를 맞추고, 엣지 wing을 묶고, 그 다음부터 바깥 면만 돌려 3×3처럼 푸는 세 단계이고, 마지막 단계가 이 phase의 LBL 그대로입니다. 그때 필요한 것은 순수 함수 둘 — 축약된 N×N에서 3×3을 뽑는 projection과, 3×3의 수를 N×N의 layer mask로 옮기는 lift — 이고, 둘 다 이 phase의 밖입니다.

이 phase가 지금 지불하는 것은 **LBL이 스스로에게 거는 제약 하나**입니다.

> **LBL의 출력은 바깥 면 단일 layer 회전만입니다.** 전체 큐브 회전도, slice도 내지 않습니다.

케이스를 줄이려면 전체 회전(`layers_through(0, N-1)`)으로 "R면 기준 수순 하나"만 구현하는 방법이 있고 실제로 흔합니다. 쓰더라도 **내보내기 전에 흡수(conjugation)해서 지웁니다.** 미루면 나중에 lift가 가운데를 넓혀야 하는지 따져야 하는데, 지금 지키면 lift는 `layer(0) → layer(0)`, `layer(2) → layer(N-1)` 두 줄입니다.

이 제약은 편의가 아니라 **이 application의 기존 계약이기도 합니다.** Solver의 수는 timeline에 적히고 공유 링크에 실리므로, `restore_apply`가 받아들이는 mask(한 크기의 연속 구간, 전폭 제외)와 표기가 문자열을 내는 mask 집합 안에 있어야 합니다. 전체 회전은 세 곳 모두에서 거절됩니다. 그래서 이것은 test로 고정할 수 있는 성질입니다.

### Timeline에서의 의미: 네 번째 값 대신 기록을 먼저 쓴다

Phase 11이 이 phase로 넘긴 결정입니다. `TimelineEffect`의 세 값 중 어느 것도 solver의 수에 맞지 않습니다 — `Advance`는 이미 기록된 다음 수를 적용한다는 뜻이라 cursor를 기록 끝 너머로 밀고, `Rewind`는 기록된 수를 되감는다는 뜻이며, `None`은 cube만 바뀌어 기록과 어긋납니다.

**네 번째 값을 더하지 않습니다. 대신 재생하기 전에 기록에 씁니다.**

```cpp
/** 커서 위에 수순을 적되, 커서는 두고 온다. */
void record_ahead(std::vector<CubeMove> moves);
```

`record()`가 한 수에 대해 하는 일 — 커서 뒤를 자르고, 커서 위에 있던 `scramble_end`를 커서까지 끌어내리고, 적는다 — 을 수순 전체에 대해 한 번 합니다. 그 다음 `TimelineEffect::Advance`로 재생하면, **scramble이 이미 쓰는 경로와 글자 그대로 같습니다**(`begin_scramble` 후 `Advance`). 

`TimelineEffect::Record`(commit 때마다 적는 값)를 기각한 이유입니다.

- `MoveTimeline`이 문서로 약속한 "재생되는 수는 이미 기록에 있다"가 유지됩니다. 네 번째 값은 그 약속을 깨고, 깨진 자리를 주석으로 메워야 합니다.
- 중단했을 때 남는 것이 **redo tail**이고, 그것은 되감기가 이미 남기는 것과 같은 모양입니다. 남은 해법을 `redo()`로 한 수씩 이어 갈 수 있게 되는데, 이것이 Phase 17의 step-through에 그대로 쓰입니다 — `redo()`는 이미 "커서를 정확히 하나 전진"입니다([Application.cpp:1168](../../engine/src/app/Application.cpp:1168)).
- `commit_move`가 그대로입니다. 두 갈래로 남습니다.

대가는 **해법이 재생되기 전에 기록에 먼저 나타난다**는 것입니다. 되감기가 남기는 tail과 같은 것이라 move log가 이미 다룰 줄 아는 상태이고, 그것이 대가의 전부입니다.

Solver의 수는 `scramble_end` 위에 적히므로 **사용자의 수로 셉니다.** 그 수들은 실제로 cube 위에 있고, `committed_move_count()`가 "지금 cube에 올라와 있는 스크램블 이후의 수"라는 뜻을 유지합니다. 그 수를 누가 골랐는지는 기록이 아니라 아래의 완주 판정이 답합니다.

### 완주 판정: 도움을 받은 판은 시간을 재지 않는다

Phase 11의 규칙은 `cursor == 0 && scramble_end > 0`이었습니다 — 되감기로 풀린 cube는 아무것도 적용되지 않은 상태로 끝나므로 파생값 하나로 구분됩니다. **Solver로 풀린 cube는 이 모양이 아닙니다.** 커서는 기록의 끝에 있고 수는 전부 올라와 있어, 손으로 푼 판과 파생값으로 구분되지 않습니다.

그래서 이번에는 **세션의 사실**로 둡니다. `GameSession`은 solve 명령이 받아들여진 순간 그 판을 도움받은 판으로 표시하고, 판이 새로 시작하는 곳(scramble, reset, 크기 변경, 링크 복원)에서 지웁니다.

- **표시되는 순간 timer가 멈추고 다시 시작하지 않습니다.** 이 규칙 하나가 두 가지를 함께 처리합니다: solver의 수가 파생 사용자 수를 늘려 `ready` 상태의 timer를 시작시키는 문제(Phase 11이 되감기에 대해 막아 둔 것과 같은 자리)와, 잰 시간을 쓰지 않을 판을 계속 재는 어색함입니다.
- **중단하고 손으로 마무리해도 도움받은 판입니다.** Phase 11은 되감기를 중단하고 직접 완성한 판을 정상 기록으로 남겼는데, 그것과 다르게 정합니다. 되감기는 자기가 둔 수를 도로 가져가는 일이라 절반을 봐도 새로 알게 되는 것이 없고, solver는 답을 보여 주는 일이라 **절반만 봐도 본 것**입니다.
- 완주 안내는 되감기와 따로 씁니다. Phase 11의 "Rewound to solved … Not a solve of your own."이 그대로 남고, solver의 판은 자기 문장을 갖습니다.

### Solve와 Rewind는 다른 버튼이다

Phase 11의 `Solve` 버튼은 되감기입니다(ABI 이름은 이미 `thorvg_rubiks_solve_rewind`라 정확합니다). 여기서 진짜로 푸는 명령이 생기므로 **화면의 이름을 개정합니다.**

- 기존 버튼: **Rewind** — 하는 일이 되감기이고, 지금까지 `Solve`라고 적혀 있던 것이 이 phase 이전까지는 유일한 명령이라 문제가 되지 않았을 뿐입니다.
- 새 버튼: **Solve** — solver의 해법을 재생합니다.

두 버튼 모두 재생 중에는 `Stop`으로 끊을 수 있고, 재생 경로·중단·안내·busy 규칙은 Phase 11의 것을 그대로 씁니다. Phase 11 문서에 이 개정을 기록합니다.

**지원하지 않는 크기에서 Solve는 비활성이고, 그 이유가 화면에 있습니다.** 3×3이 아닌 cube에서는 "이 크기를 푸는 solver가 아직 없습니다"가 읽히고, Rewind는 크기와 무관하게 그대로 동작합니다. 계획서가 "지원 크기를 solver 자신이 밝히므로 지원 여부가 그대로 드러난다"고 한 것이 화면에서 이 모양입니다.

### 후처리: 같은 layer의 연속 turn을 합친다

LBL의 원 수순은 이어 붙인 자리마다 `R' R` 같은 상쇄와 `R R` 같은 중복을 남깁니다. 내보내기 직전에 한 번 훑어 **같은 축·같은 mask가 이어지면 `quarter_turns`를 더하고, 4의 배수가 되면 지웁니다.**

- 15줄 남짓에 수순이 보통 20~30% 짧아집니다. 재생 시간에 그대로 반영되고, 기록에 남는 수도 그만큼 줄어듭니다.
- mask를 바꾸지 않으므로 위의 "바깥 면 단일 layer" 성질과 표기·payload 규칙을 깨지 않습니다.
- 되감기가 상쇄를 일부러 하지 않는 것([MoveTimeline.hpp](../../engine/src/cube/MoveTimeline.hpp))과 반대인데, 이유가 반대이기 때문입니다: 되감기는 **자기가 둔 수가 순서대로 돌아오는 것**이 목적이고, 해법은 **푸는 것**이 목적이라 오지 않아도 되는 수가 오면 그냥 긴 재생입니다.

### 결정성

같은 `CubeState`는 언제나 같은 수순을 냅니다. Solver 안에 난수가 없고, 조각을 고르는 순서도 고정입니다. Test가 known-answer로 고정할 수 있고, e2e가 재생 길이를 예측할 수 있습니다.

## ABI

```text
thorvg_rubiks_can_solve() -> int   // 지금 크기를 푸는 solver를 들고 있는가
thorvg_rubiks_solve() -> int       // 해법을 계산해 재생 시작. 0 = 거절
```

`can_solve()`는 **크기만** 봅니다. 재생 중·gesture 중의 가부는 Phase 11이 이미 `is_busy()`로 정해 둔 것이고, 버튼의 활성은 그 둘을 controller가 함께 읽어 정합니다.

`solve()`의 거절: 초기화 전, 다른 것이 cube를 들고 있을 때, 지원하지 않는 크기, 그리고 **이미 풀린 cube**(빈 수순은 할 일이 없는 명령입니다). 거절은 기록도 cube도 건드리지 않습니다 — 수순을 먼저 계산하고, 비어 있지 않을 때에만 `record_ahead`와 재생이 일어납니다.

## Implementation steps

### 1. Solver target과 LBL

- [ ] `engine/src/cube/solver/` 하위 target(`cube_solver_dep`)과 `Solver` interface
- [ ] 조각 조회: 노출된 색으로 코너·엣지의 현재 위치와 방향을 찾는 함수들
- [ ] LBL 일곱 단계와 수순 헬퍼, 단계별 반복 상한 assertion
- [ ] 수순 후처리(연속 병합·상쇄 제거)
- [ ] Native test: 무작위 seed 수천 개에 대해 scramble → solve → `is_solved()`
- [ ] Native test: 이미 풀린 cube가 빈 수순, `supports()`가 3에만 참
- [ ] Native test: **출력이 바깥 면 단일 layer 회전만**이고 전부 표기 가능한 mask
- [ ] Native test: 단계별 불변식 — 각 단계가 끝난 뒤 앞 단계의 술어가 그대로 참
- [ ] Native test: 같은 상태가 같은 수순(결정성), 후처리가 상태를 바꾸지 않음
- [ ] Native test: 수순 길이의 상한과 평균(회귀용)

### 2. Timeline과 명령

- [ ] `MoveTimeline::record_ahead(moves)`와 그 절단 규칙
- [ ] `Application`: solver 소유, `can_solve()`, `solve()`
- [ ] Native test: `record_ahead`가 tail을 자르고 `scramble_end`를 끌어내리는지, 커서를 두고 오는지
- [ ] Native test: `solve()`의 네 거절 경로가 기록과 cube를 그대로 두는지
- [ ] Native test: 재생 완료 후 cube가 solved이고 커서가 기록 끝, 사용자 수가 해법 길이
- [ ] Native test: 재생을 중단하면 기록과 cube가 일치하고 남은 수순이 redo tail로 남는지
- [ ] Native test: solver로 푼 뒤 공유 payload 왕복이 그대로 성립하는지
- [ ] WASM bindings와 TS boundary

### 3. 화면

- [ ] 기존 Solve 버튼을 Rewind로 개정하고, Solve 버튼을 새로 둠 (Phase 11 문서에 개정 기록)
- [ ] 지원하지 않는 크기에서 Solve 비활성과 그 이유 표시
- [ ] `GameSession`: 도움받은 판 표시, timer 정지, 완주 안내 분리, 기록 제외
- [ ] TS unit test: 도움받은 판이 기록되지 않고 timer가 다시 시작하지 않는지, 판이 새로 시작할 때 표시가 지워지는지
- [ ] TS unit test: 크기별 Solve 버튼 상태
- [ ] e2e: 섞고 → Solve → solved 전개도와 "도움받은 판" 안내
- [ ] e2e: Solve 중단 → 남은 수순을 redo로 이어 가기 → 손으로 완성
- [ ] Native, WASM, TypeScript unit, e2e, production build 전체 실행

## Acceptance criteria

- 3×3의 어떤 도달 가능한 상태에서도 Solve가 solved로 데려가며, 그것이 무작위 seed 수천 개로 고정되어 있습니다.
- Solver는 `cube`만 보고, `cube`는 solver를 모릅니다. Target 선언이 그것을 강제합니다.
- Solver가 내는 수는 전부 바깥 면 단일 layer 회전이고, `restore_apply`가 받아들이고 표기가 문자열을 낼 수 있는 mask입니다. 전체 회전도 slice도 나오지 않습니다.
- 해법은 재생 전에 기록에 적히고 `Advance`로 재생됩니다. `TimelineEffect`는 세 값 그대로입니다.
- 재생을 어디서 끊어도 기록과 cube가 같은 것을 말하고, 남은 해법은 redo로 이어 갈 수 있습니다.
- Solver가 도운 판은 시간이 기록되지 않고, 그 사실이 완주 안내에 드러납니다. 도중에 끊고 손으로 마무리한 판도 같습니다.
- 판이 새로 시작하면(scramble, reset, 크기 변경, 링크 복원) 도움받은 표시가 사라집니다.
- 3×3이 아닌 크기에서 Solve는 비활성이고 그 이유가 읽히며, Rewind는 모든 크기에서 이 phase 이전과 같이 동작합니다.
- 같은 상태는 같은 수순을 냅니다.
- Native, WASM, TypeScript unit, browser e2e와 production build가 모두 통과합니다.

## Verification commands

```bash
meson test -C build/native --print-errorlogs
source /path/to/emsdk/emsdk_env.sh && ./build_wasm.sh
npm --prefix web run test:unit
npm --prefix web run test:e2e
npm --prefix web run build
```
