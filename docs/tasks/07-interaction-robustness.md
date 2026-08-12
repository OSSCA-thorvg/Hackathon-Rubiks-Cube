# Phase 7: Interaction Robustness

## Status

`Completed`

## Objective

빠른 연속 drag에서 회전 입력이 사라지는 문제를 해결합니다. 원인은 서로 독립적인 두 가지입니다.

1. **Snap이 새 gesture를 막습니다.** `InteractionController::pointer_down()`은 snap이 진행 중이면 false를 반환하고, `PointerController.ts`는 거절된 press에 pointer capture를 걸지 않으므로 이후의 move/up까지 stroke 전체가 버려집니다. Snap은 90°당 200ms이므로 리듬을 타고 연속으로 돌리면 두 번에 한 번꼴로 입력이 사라집니다.
2. **짧은 flick이 제자리로 돌아갑니다.** Snap 목표가 `round(angle / 90°)`라는 반올림뿐이라, 튕기듯 돌려도 각도가 45°를 넘지 못하면 0으로 되돌아갑니다. 사용자의 의도(그 방향으로 돌리기)와 판정(놓은 위치)이 어긋나는 경우입니다.

해결은 각각 **snap 즉시 확정**과 **snap 경계의 이동**입니다. 둘 다 `InteractionController`와 `Application` 안에서 끝나며, C ABI와 web 코드는 이 phase에서 바뀌지 않습니다.

## Scope

- `InteractionController::finish_snap()`: 진행 중인 snap의 목표를 즉시 확정하고 그 move를 반환
- `Application::commit_move(move)`: cube 적용과 count 증가가 모이는 단일 함수
- `Application::pointer_down()`이 확정된 move를 그 함수로 적용한 뒤 새 gesture 시작
- Release 시 snap 경계 이동: 배수 사이의 경계를 중간(45°)에서 낮은 배수 + `kCommitDegrees`(30° 내외)로, 모든 배수에서 동일하게
- Native unit test와 연속 drag / 짧은 drag e2e

## Out of scope

- 관성(momentum) 모델 — release 후에도 계속 도는 물리는 없고, snap 목표만 조정합니다
- 각속도(flick 속도) 판정 — 아래 결정에서 기각합니다
- C ABI 변경과 `PointerController.ts` 변경 — 이 phase는 engine 내부에서 끝납니다
- 전개도 interaction (Phase 8)
- Move player (Phase 9) — 즉시 확정은 재생이 아닙니다
- 감도, 임계값의 사용자 설정 UI
- Orbit gesture의 관성 — orbit은 commit이 없어 씹힘 문제 자체가 없습니다

## Architecture decisions

### Snap 즉시 확정

`pointer_down()`에서 snap이 진행 중이면 거절하는 대신, 그 snap의 목표를 확정하고 새 gesture를 시작합니다.

```cpp
/** Ends a snap at its target, returning the move it had already decided. */
[[nodiscard]] std::optional<cube::CubeMove> finish_snap() noexcept;
```

```cpp
// Application::pointer_down
if (!std::isfinite(x) || !std::isfinite(y)) return false;
if (const auto move = interaction.finish_snap()) {
    commit_move(*move);
}
return interaction.pointer_down(x, y, current_camera(), placement.cube);
```

- **좌표 검증이 확정보다 앞입니다.** 순서를 바꾸면 non-finite 좌표의 press가 새 gesture는 거절되면서 진행 중인 snap만 확정합니다 — 아무것도 시작하지 못하는 press가 상태를 바꾸는 일은 없어야 합니다. Controller의 finite 검사와 겹치지만, 이 중복은 검사의 반복이 아니라 확정의 전제 조건입니다.

- Controller는 목표 quarter turn을 계산해 `snap_`을 비우고 move를 **반환**하며, 목표가 0(snap-back)이면 `nullopt`입니다. 확정된 move를 controller 안에 쌓아 두지 않으므로 `committed_`는 지금처럼 `std::optional` 하나로 남고 `advance()`의 commit 경로도 변하지 않습니다.
- 이 구조를 고른 이유는 순서 추론을 없애기 위해서입니다. 확정된 move를 보관했다가 다음 frame에 꺼내면 "새 gesture가 먼저 끝나면 어느 쪽이 먼저 적용되는가"를 저장소가 답해야 하지만, 호출 지점에서 즉시 적용하면 그런 상태가 애초에 생기지 않습니다.
- **진입점은 둘이 되지만 적용 함수는 하나입니다.** `commit_move(move)`가 cube 적용과 count 증가를 담당하고, 이후 phase가 여기에 timeline 기록을 붙입니다. 이 phase만 놓고 보면 `cube_state.apply` + `++user_move_count` 두 줄을 함수로 감싼 것에 불과하지만, 인라인으로 두면 Phase 11에서 빠른 연속 drag의 확정 move가 history와 효과음에서 통째로 누락됩니다. 지금 감싸 두면 그 결함이 생기지 않습니다.
- **출처를 나타내는 인자나 enum은 두지 않습니다.** Phase 9가 재생을 도입하면 "지금 재생 중인가"는 Application이 소유한 재생 상태(`playback_`) 하나로 답해지고, 그것은 `commit_move` 안에서 읽을 수 있는 값이라 호출자가 나를 필요가 없습니다. 초안은 `MoveOrigin` enum을 여기서 `User` 하나로 도입해 이후 phase가 값을 더하는 방식이었으나, 그러면 값의 개수만큼 이름→성질 변환표(tempo, count, timeline 반영, 정지 가능 여부)가 phase마다 늘어납니다. 이 phase의 두 호출 지점은 어차피 같은 값을 넘기게 되므로, 인자를 두지 않는 편이 이 phase에서도 더 단순합니다.
- 확정은 시각적으로 남은 각도(구조상 90° − `kCommitDegrees` 이하, 감속 구간이라 실제로는 더 작음)를 건너뛰는 jump이지만, 사용자가 이미 다음 drag를 시작한 순간이라 눈에 띄지 않습니다.
- Picking은 정지 상태의 축 정렬 cube 기하만 대상으로 하고 `cube_state`를 읽지 않으므로, 확정과 pick의 순서는 결과에 영향을 주지 않습니다.
- 빈 공간을 눌러 orbit을 시작하는 경우에도 똑같이 확정합니다. 누른 곳이 어디든 snap은 끝난 것으로 취급하는 편이 규칙이 하나로 유지됩니다.

### Snap 경계를 배수 + kCommitDegrees로 옮깁니다

Snap 목표의 경계를 옮깁니다. 분기도, 시간도, 속도도, 새 상태도 없습니다.

```text
target = round((|angle| + 45° − kCommitDegrees) / 90°) × sign(angle)
```

- **`kCommitDegrees`는 30°로 확정합니다.** 감도가 "viewport 절반 = 90°"(`kQuarterTurnFraction`)이므로 30°는 화면 폭의 1/6 — mobile 360px에서 약 60px로, 엄지 flick이 자연스럽게 닿는 거리입니다. 값을 열어 두면 아래 두 경계(30°, 120°)를 straddle하는 known-answer test가 상수와 함께 흔들려, 조정할 때마다 test 표를 다시 계산해야 합니다. 손맛 조정이 필요해지면 그때 상수와 표를 한 번에 바꿉니다.
- **경계는 모든 배수 사이에서 같은 지점입니다: 낮은 배수에서 `kCommitDegrees` 지난 곳.** "면 하나를 30° 넘기면 다음 칸"이 어느 칸에서나 참인 한 문장이 되고, 어느 경계든 전진 1 대 후퇴 2의 같은 비율입니다. 90°를 넘겨 계속 끌고 있는 손가락은 더 돌릴 의도이므로, 첫 칸은 30°에 열어 주면서 둘째 칸에 45°를 요구하는 것이 오히려 마찰의 비일관입니다.
- 처음 안은 반올림이 0일 때만 문턱을 보는 첫 칸 한정 분기였으나 개정했습니다. 그 안은 첫 칸 1:2, 나머지 1:1의 혼합 규칙이라 한 문장으로 서술되지 않습니다. 오류의 방향도 근거입니다: 이 phase가 고치려는 것이 "돌렸는데 무시됐다"(사용자가 앱 탓으로 느끼는 false negative)이므로, 덜 돌려도 동작하는 쪽으로 모든 경계를 일관되게 기울입니다. 과회전(false positive)은 transient 회전으로 release 전에 보이고, 자기 행동의 결과로 읽히며, 한 gesture로 되돌릴 수 있습니다.
- **이것은 Phase 5의 "가장 가까운 90° 배수" 계약의 개정입니다.** [Phase 5 문서](./05-pointer-interaction-and-animation.md)의 개정 기록에 남기며, 그 phase의 snap known-answer 표, canonical 120° drag 상수, "남은 각도 ≤ 45°" 유도 성질이 재고정 대상입니다.
- **한 바퀴로 끝난 snap(quarter turns mod 4 == 0)은 commit을 만들지 않습니다.** Viewport 폭 두 배를 끄는 drag는 목표가 360°에 닿을 수 있는데, 지금 commit 조건은 `quarter_turns != 0`이라 그 수가 `CubeState`는 불변이면서 count와 (Phase 11부터는) timeline에 한 수로 남습니다. 논리적 no-op이므로 snap animation은 360°까지 그대로 돌고 commit 단계에서만 폐기합니다.
- **각속도 판정은 검토 후 기각했습니다.** 속도를 재려면 시간이 필요한데, 시간의 유일한 입구인 `advance()`는 rAF 주기로만 흘러 마지막 `pointer_move → pointer_up` 구간이 분모에서 빠지고, 그 오차는 주사율에 비례해(120Hz ≤8ms, 30Hz ≤33ms) 같은 gesture의 판정이 기기마다 달라집니다. 정확히 하려면 pointer event timestamp를 ABI로 주입해야 하는데 — pointer 함수 셋 변경, `PointerController` 수정, 임계값·최소 시간 상수와 강등 규칙 — 경계 이동 하나가 같은 문제를 해결하므로 그 비용을 들이지 않습니다.

받아들이는 손실은 셋이고 모두 좁습니다.

- `kCommitDegrees` 미만의 극단적으로 짧은 flick은 여전히 제자리로 돌아갑니다. 경계가 유일한 레버이고, 더 낮추면 오조작 commit이 늘어납니다.
- 한 칸 의도의 drag가 배수를 `kCommitDegrees` 이상 지나친 채 놓이면 두 칸이 됩니다. 놓기 전에 transient 회전이 면을 지나친 것이 보이므로, 보이는 대로 확정되는 동작입니다. **이 근거는 회전이 각도 그대로 보이는 3D에서만 성립합니다** — Phase 8이 더하는 전개도는 표시가 한 칸에서 포화하므로, 그 phase가 net gesture의 각도를 `[-90°, +90°]`로 제한해 두 칸이 나오지 않게 합니다.
- 규칙이 방향을 보지 않으므로, 마음을 바꿔 되돌리는 gesture는 직전 배수의 `kCommitDegrees` 안까지 와야 복귀합니다. 가장 가까운 배수 규칙보다 15°(화면 폭 기준 약 30px) 더 돌아와야 하지만, 되돌리는 사람은 보통 배수 근처까지 돌아옵니다.

## Implementation steps

### 1. Snap 즉시 확정

- [x] `finish_snap()` 구현: 목표 quarter turn 산출, `snap_` 해제, move 반환 (목표 0이면 `nullopt`)
- [x] `commit_move(move)` 도입과 `advance()`의 기존 commit 경로를 이 함수로 이관
- [x] `Application::pointer_down()`에서 확정 move를 `commit_move`로 적용
- [x] Snap 중 두 번째 drag 시작 → 두 move가 모두 정확히 한 번씩 적용되는 test
- [x] 확정된 상태 위에서 새 gesture의 pick이 올바른 cell을 잡는 test
- [x] Snap-back(목표 0) 확정이 `CubeState`를 바꾸지 않는 test
- [x] 빈 공간 press로 orbit을 시작할 때도 확정이 일어나는 test
- [x] Non-finite 좌표의 pointer down이 snap을 확정하지 않는 test (좌표 검증이 확정보다 앞)
- [x] **기존 controller test의 유지 확인**: "input during the snap is ignored"는 controller 수준 계약이라 그대로 유효합니다 — `finish_snap()`은 Application이 snap을 먼저 비우고 나서 `pointer_down`을 부르는 상위 계층의 일이므로, 이 test는 교체 대상이 아니라 그 분업의 근거로 유지하고 의도를 주석으로 명시합니다

### 2. Snap 경계

- [x] `kCommitDegrees` 상수와 경계 이동 규칙 구현 — 분기 없는 편향 식 하나
- [x] 경계 known-answer test: 첫 경계(`kCommitDegrees`)와 둘째 경계(90° + `kCommitDegrees`)를 straddle하는 쌍, 음의 방향 대칭
- [x] **기존 "nearest quarter turn" known-answer 표의 교체**: `37° → 0`과 `±45°` 동점 straddle 쌍은 새 규칙에서 결과가 바뀌고, `143° → 2`와 `270° → 3`은 그대로입니다. 표를 새 경계의 straddle 쌍으로 재고정하고 test 이름의 "nearest"도 규칙 서술로 바꿉니다
- [x] **Canonical drag 상수 이동**: test 전반이 "명백한 한 칸"으로 쓰는 120°가 정확히 새 경계에 얹히므로(`round(1.5)` 동점이 부동소수 노이즈에 걸림), 한 칸 구간의 중앙(예: 75°)으로 일괄 이동
- [x] Snap duration 유계 test 갱신: 남은 각도의 유도 상한이 45°에서 90° − `kCommitDegrees`로 커진 것을 반영
- [x] Commit의 mod 4 폐기: 360° 목표로 끝난 drag가 상태도 count도 바꾸지 않는 test

### 3. Verification

- [x] e2e: 빠른 연속 drag 두 번이 모두 반영되는지
- [x] e2e: 짧은 drag가 한 칸 도는지 — 각도는 경계에 붙이지 않고 여유를 둔 값(예: 45°)을 씁니다. e2e는 CSS 크기와 pointer 좌표 반올림을 거치므로 경계 ±1° 판정은 환경에 따라 뒤집힙니다. 경계 자체는 native known-answer test가 고정합니다
- [x] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- Snap 진행 중의 pointer down이 stroke를 잃지 않고 새 gesture를 시작합니다.
- 확정은 진행 중이던 snap의 목표 move를 정확히 한 번 적용하며, 연속 drag에서 move가 사라지거나 중복되지 않습니다.
- 두 진입점(animation 종료, 즉시 확정)이 모두 `commit_move()` 하나를 통과하므로, 이후 phase가 commit에 붙이는 처리에서 어느 쪽도 누락되지 않습니다.
- Controller의 commit 보관은 여전히 `std::optional` 하나이고, `advance()`의 commit 경로는 변하지 않습니다.
- Snap 경계는 모든 90° 배수에서 `kCommitDegrees` 지난 지점에 있습니다. 정지에서 `kCommitDegrees` 미만의 drag는 제자리로 돌아가고, 이상이면 진행 방향 한 칸으로 확정되며, 같은 규칙이 둘째 칸 이후에도 적용됩니다.
- 기존 snap known-answer 표는 새 경계의 straddle 쌍으로 재고정되고, canonical drag 상수는 경계에서 떨어진 값으로 이동합니다. 이 개정은 Phase 5 문서의 개정 기록에 남습니다.
- 되돌리는 gesture는 직전 배수의 `kCommitDegrees` 안까지 와야 복귀합니다. 규칙이 방향을 보지 않아 의도적으로 받아들인 손실입니다.
- Snap 판정은 각도만 보며, 시간·속도·gesture의 추가 상태가 존재하지 않습니다.
- `pointer_up()`은 기존처럼 그 자리에서 snap을 시작하며, release를 미루는 중간 상태가 없습니다.
- `commit_move()`는 출처 인자를 받지 않으며, 출처를 나타내는 enum이 이 phase에 존재하지 않습니다.
- Release 판정의 부호 규칙은 기존과 동일합니다. 기존 snap/부호 test 중 결과가 바뀌는 것은 경계 straddle 케이스뿐이고, 나머지는 canonical drag 상수의 일괄 이동 외에 수정 없이 통과합니다.
- 시간은 여전히 `advance(elapsed_ms)` 하나로만 engine에 들어옵니다.
- C ABI와 `PointerController.ts`는 이 phase에서 변경되지 않습니다.
- Commit에 이르는 경로는 정상 release와 즉시 확정뿐이고, `cancel()`은 어떤 상태에서도 `CubeState`를 변경하지 않습니다.
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

모든 acceptance criteria와 verification command를 통과했습니다. Native 13/13, WASM
build, TypeScript unit 81/81, e2e 15/15(신규 2건 포함), production build.

### 구현에서 달라진 것

- **Mod 4 폐기는 commit 단계가 아니라 `settled_move()` 한 곳에 있습니다.** 계획은
  "commit 단계에서만 폐기"였으나, 확정 경로가 둘(`advance()`, `finish_snap()`)이 된
  이상 두 곳이 각각 검사하면 한쪽만 고쳐질 수 있습니다. 진행 중인 snap이 만들 move를
  계산하는 private helper `settled_move()`를 두고 양쪽이 그것을 부릅니다 —
  `finish_snap()`은 그 결과를 반환하고 `advance()`는 `committed_`에 넣습니다. 폐기
  규칙도 목표 각도를 아는 그 한 곳에 있습니다.
- **좌표 검증 앞에 view mode 검증도 있습니다.** 계획이 명시한 것은 non-finite
  좌표뿐이지만, net 전용 view의 press도 거절되면서 아무것도 시작하지 못합니다.
  거절 사유 전부가 확정보다 앞이라는 한 문장이 되도록 순서를 맞추고 test를 하나
  더 두었습니다(`set_view_mode`는 snap을 죽이지 않으므로 도달 가능한 조합입니다).
- **확정은 pointer snap만이 아니라 programmatic turn에도 걸립니다.** `start_move()`가
  같은 `snap_`을 쓰므로, keyboard/DOM 버튼으로 시작된 회전 중에 press가 오면 그것도
  즉시 확정되고 새 gesture가 시작됩니다. 계획 본문은 release만 이야기했지만 이쪽이
  같은 규칙의 자연스러운 범위이고("무엇이 돌고 있든 press가 오면 끝난 것으로 친다"),
  Phase 9의 재생 중 사용자 개입도 이 경로를 그대로 씁니다. Test를 하나 두었습니다.
- **Canonical drag 상수는 75°이고 이름이 test마다 다릅니다.** `InteractionTest`는
  `kOneTurnDrag`, `PointerInteractionTest`는 기존 이름 `kQuarterTurnDrag`를 유지한 채
  값만 옮겼습니다. e2e는 각도 대신 quarter turn 분수 `SHORT_TURN = 0.5`(45°)를 쓰고,
  snap-back e2e는 0.3(27°)이 새 경계에서 3°밖에 떨어져 있지 않아 0.15(13.5°)로
  내렸습니다.
- **"확정 위에서 새 gesture가 올바른 cell을 잡는다" test는 왼쪽 열이 아니라 가운데
  열을 씁니다.** 전면 왼쪽 열에서 화면 수직 drag는 X축이 아니라 Y축으로 resolve
  됩니다 — 그 지점에서 X 회전의 화면 방향이 심하게 단축되어 두 후보의 점수가
  1.0 대 0.94로 붙기 때문입니다. 이 phase와 무관한 기존 pick 동작이라 건드리지 않고,
  판정이 명확한 가운데 열로 test를 세웠습니다.
- **빠른 연속 drag e2e는 실패할 수는 없지만 항상 확정 경로를 타지는 않습니다.**
  남은 각도의 상한이 90° − `kCommitDegrees`이므로 snap이 도는 시간은 최대 약
  133ms이고, 45° drag는 100ms입니다. Playwright의 up → move → down이 그보다 느리면
  두 move가 그냥 순서대로 commit되어 test는 통과합니다. 확정 경로 자체는 native
  test가 프레임 단위로 고정하고, e2e는 stroke가 유실되지 않는다는 것만 봅니다.
