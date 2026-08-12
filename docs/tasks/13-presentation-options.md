# Phase 13: Presentation Options

## Status

`Not started`

## Objective

들리는 방식과 재생 속도를 사용자가 고를 수 있게 합니다. 서로 독립적인 두 옵션입니다.

1. **회전 효과음**: 회전이 commit되는 순간의 짧은 소리를 더합니다.
2. **Animation 속도**: 90°당 시간을 슬라이더로 조절합니다. 사용자 snap, scramble, 되감기 재생이 하나의 control을 공유합니다.

색맹 palette는 원래 이 phase의 세 번째 옵션이었지만 [Phase 7.5](./07.5-colorblind-palette.md)로 옮겼습니다. 다른 두 옵션은 Phase 9의 tempo와 Phase 11의 timeline 관찰이 있어야 하지만 palette는 어디에도 의존하지 않는 접근성 항목이라, 계획의 뒤쪽에 둘 이유가 없었기 때문입니다.

## Scope

- Engine: tempo 배율 `set_speed_scale(float)` ABI — tempo를 `start_move`에 넘기는 두 지점에 공통 적용
- Web: WebAudio로 합성하는 효과음(외부 asset 없음), mute toggle, autoplay 정책 대응
- Web: 속도 슬라이더 UI
- Native/TS unit test와 e2e

## Out of scope

- 옵션의 저장과 복원 — 브라우저 저장소를 쓰지 않기로 했으므로(Phase 14) 설정은 세션 안에서만 유지됩니다
- 색맹 palette (Phase 7.5)
- Haptic feedback
- 효과음의 종류 선택 — 소리는 하나, on/off만 둡니다

## Architecture decisions

### 회전 효과음

- 관찰 대상은 Phase 11의 `timeline_cursor`입니다. 모든 commit이 cursor를 정확히 ±1 움직이므로, `GameController`가 frame마다 읽는 값이 이전과 다르면 그 frame에 commit이 있었던 것입니다 — 완료 판정이 쓰는 관찰과 같은 것이라 소리를 낼 지점을 새로 만들지 않습니다. **소리는 cursor가 변한 frame에 한 번입니다.** 한 관찰 창(≤16ms)에 commit이 둘 들어오는 극단적 입력에서도 클릭 하나로 들리는 것이 자연스러우므로, 개수를 세는 계약은 두지 않습니다.
- Reset·scramble은 GameController 자신이 부르는 명령이라 그 handler가 기준값을 갱신해 소리가 새지 않고(Phase 11), 복원은 controller 부착 전에 끝나 관찰 창 밖입니다.
- Scramble과 되감기 재생의 commit도 cursor를 움직이므로 재생 연출에 소리가 함께 붙습니다. Ambient만 cursor를 움직이지 않아 자연히 무음입니다 — 화면 보호기에 맞는 결과이고, web이 ambient 여부를 조회해 분기할 필요가 없습니다.
- 소리는 WebAudio oscillator + gain envelope로 합성한 수십 ms의 클릭입니다. 외부 asset이 없어 빌드와 배포가 바뀌지 않습니다.
- `AudioContext`는 첫 사용자 gesture에서 생성/resume합니다(autoplay 정책). Mute toggle은 기본 off(소리 남)로 두되, context를 만들 수 없는 환경에서는 조용히 무음으로 강등합니다.
- 빠른 재생에서 앞 소리의 envelope가 끝나기 전에 다음 소리가 시작될 수 있으므로, 겹쳐도 클리핑하지 않도록 gain을 제한합니다.

### Animation 속도

- Phase 9가 tempo를 `start_move`의 인자로 모아 두었으므로, 이 phase는 **하나의 배율**을 그 인자에 곱합니다. Tempo가 들어가는 자리는 둘뿐입니다: 재생 소비 loop(`playback_->tempo_ms`)와 사용자 drag의 `pointer_up`. Phase 9가 후자까지 배관해 두었으므로 슬라이더 하나가 전부를 다스립니다.
- 의미는 식으로 고정합니다: `duration_ms = base_duration_ms / scale`. 즉 `scale = 4.0`은 네 배 빠른(1/4 duration) 재생이고 `0.25`는 네 배 느린 재생입니다.
- 배율은 `[0.25, 4.0]`으로 clamp하고, non-finite는 거절합니다. 진행 중인 snap은 시작할 때의 duration을 유지하고 다음 snap부터 새 배율이 적용됩니다 — 재생 중 슬라이더를 움직여도 현재 frame이 튀지 않습니다.
- `prefers-reduced-motion`은 기존 DOM transition 제거에 더해 초기 배율을 빠른 쪽으로 제안하는 정도로만 반영하고, 강제하지 않습니다.

```text
thorvg_rubiks_set_speed_scale(scale: float) -> int
```

## Implementation steps

### 1. 효과음

- [ ] WebAudio 합성 클릭과 gain 제한 구현 (injectable audio seam)
- [ ] 첫 gesture에서의 context 생성/resume과 실패 시 무음 강등
- [ ] `timeline_cursor` 관찰 연결과 mute toggle
- [ ] TS unit test: cursor가 변한 frame마다 한 번 재생, reset·ambient에서 무음, mute 시 무음, context 부재 안전 (복원 무음은 복원이 생기는 Phase 14에서 검증)

### 2. 속도

- [ ] Engine 배율 상태와 두 tempo 지점 적용, clamp·검증
- [ ] 진행 중 snap의 duration 불변 test
- [ ] ABI 추가와 슬라이더 UI (값 표시 포함)
- [ ] Native 고정 dt test: `duration = base / scale` 식 자체와, 2× 배율이 1×보다 먼저 소진되는지 (정확한 frame 수 비례는 반올림에 걸리고, browser wall-clock은 더 불안정합니다)
- [ ] Native 고정 dt test: **사용자 drag release의 snap**도 2×에서 1×보다 먼저 끝나는지 (배율이 재생만이 아니라 pointer 경로에도 닿는 증거)
- [ ] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- 속도와 소리 설정은 cube, timeline, timer 상태에 영향을 주지 않습니다.
- Cursor가 변한 frame마다 효과음이 한 번 재생되고, reset에서는 울리지 않으며, ambient 관람은 무음이고, mute가 즉시 적용됩니다. 복원에서의 무음은 복원이 생기는 Phase 14가 확인합니다.
- 소리는 외부 asset 없이 합성되고, WebAudio가 없는 환경에서도 앱이 정상 동작합니다.
- 속도 슬라이더 하나가 사용자 snap, scramble, 되감기 재생의 tempo를 함께 바꾸고, 진행 중인 animation은 튀지 않습니다.
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
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 13을 완료 처리합니다.
- 실제 구현과 차이가 생긴 결정을 이 문서에 기록합니다.
