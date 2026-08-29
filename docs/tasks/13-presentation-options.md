# Phase 13: Presentation Options

## Status

`Completed`

## Objective

보이고 들리는 방식을 사용자가 고를 수 있게 합니다. 서로 독립적인 세 옵션이고, 하나의 옵션 면에 함께 놓입니다.

> **개정 (UI polish and showcase)** — 세 옵션이 한 면에 함께 놓인다는 계약은 유지되고, 그 면이 canvas 옆의 왼쪽 rail에서 Settings drawer(좁은 화면에서는 bottom sheet)로 옮겨졌습니다. Palette, 속도, turn sound는 stage 위에 상주할 만큼 자주 쓰이지 않으면서 primary action과 같은 무게로 보이고 있었습니다. Element id(`#mute`, `#speed`, `#speed-value`, `[data-palette]`)와 각 옵션의 의미는 그대로입니다.

1. **색맹 palette**: 세 이색형 색각 전부에서 구분되는 대체 palette를 더하고 두 벌 중 하나를 고르게 합니다.
2. **회전 효과음**: 회전이 commit되는 순간의 짧은 소리를 더합니다.
3. **Animation 속도**: 90°당 시간을 슬라이더로 조절합니다. 사용자 snap, scramble, 되감기 재생이 하나의 control을 공유합니다.

Palette는 나머지 둘과 달리 선행 phase에 의존하지 않아 한때 Phase 7.5로 분리되어 있었으나, 이 문서로 되돌렸습니다. 근거는 [개정 기록](#개정-기록)에 있습니다.

## Scope

- Engine: `Palette` enum(`Classic`, `HighContrast`)과 대체 palette 상수, `to_color(color, palette)` 시그니처 확장
- Engine: tempo 배율 `set_speed_scale(float)` ABI — tempo를 `start_move`에 넘기는 두 지점에 공통 적용
- C ABI: `set_palette`, `palette`, `set_speed_scale`
- Web: palette toggle과 `aria-pressed`
- Web: WebAudio로 합성하는 효과음(외부 asset 없음), mute toggle, autoplay 정책 대응
- Web: 속도 슬라이더 UI
- Native/TS unit test와 e2e

> **개정 (2026-08, [Phase 18](./18-paint-your-cube.md)).** 아래 out of scope의 "임의 색 지정"은 **여전히 유효합니다.** Phase 18이 더한 색 고르개는 색조를 고르는 것이 아니라 **스티커의 정체**(여섯 중 하나)를 고르는 것이고, 그 여섯이 각각 어떤 색으로 그려지는지는 지금도 palette의 일입니다. theme editor는 없습니다.

## Out of scope

- 옵션의 저장과 복원 — 브라우저 저장소를 쓰지 않기로 했으므로(Phase 14) 설정은 세션 안에서만 유지됩니다
- **Theme editor와 임의 색 지정** — 아래 개정 기록에서 기각합니다. Palette는 검증된 두 벌뿐입니다
- 배경·UI chrome의 테마 — palette가 바꾸는 것은 sticker 색뿐입니다
- 색 외의 구분 수단(패턴, 기호 오버레이) — sticker quad에 문양을 그리는 일이라 rendering 작업이 별개로 커집니다
- Haptic feedback
- 효과음의 종류 선택 — 소리는 하나, on/off만 둡니다
- **회전하는 동안의 스치는 소리** — [Phase 13.5](./13.5-turn-scrape-sound.md)로 분리했습니다. 이 phase의 효과음은 commit 순간의 클릭 하나이고 engine에 닿지 않는 web 전용 항목인데, 스침은 회전 각도를 읽어야 해서 ABI와 generated 산출물과 fixture를 함께 끌고 옵니다. 셋 중 유일하게 web 안에서 끝나는 항목에 그 무게를 얹지 않습니다. 그쪽이 mute toggle을 이 phase의 것 그대로 씁니다

## Architecture decisions

### 색맹 palette: 교체 지점은 한 곳

큐브는 색으로만 상태를 읽는 게임이라, 두 면이 섞이면 판을 읽는 것 자체가 불가능해집니다. 표준 큐브에서 그런 쌍이 실제로 어디인지는 **재서 확인했고, 직관과 달랐습니다** — 구현 기록은 아래 대체 palette 항목에 있습니다.

- `to_color`(`engine/src/graphics/Palette.cpp`)는 도메인의 `FaceColor`가 픽셀이 되는 유일한 지점입니다. `Palette` enum을 인자로 더하고 Application이 현재 값을 소유해 넘기면, 분기가 그 함수 안에서 끝납니다. Cube·interaction·history 어디에도 닿지 않습니다.
- Palette 변경은 다음 render부터 적용되는 **순수 상태 변경**입니다. Cube, timeline, timer, camera 어디에도 닿지 않으므로 busy 중에도 허용되고, 진행 중인 animation을 방해하지 않습니다.
- 3D cube와 전개도가 같은 함수를 쓰므로 두 view가 자동으로 함께 바뀝니다. Renderer는 변경되지 않습니다.

### 색맹 palette: 대체 palette의 기준

- **검증은 일반 명도 대비가 아니라 색각 변환을 거친 색 거리로 합니다.** 명도 대비만 보면 두 색이 색각에서 같은 색으로 무너져도 test가 통과합니다. 이 항목의 목적이 정확히 그 무너짐을 막는 것이므로, 목적을 검증하지 못하는 test를 두면 통과했다는 신호만 남습니다.
- **세 가지 이색형 색각(protanopia·deuteranopia·tritanopia)을 전부, 함께 봅니다.** 앞의 둘은 적록 축을 잃고 청황 축이 남는데 tritanopia는 정확히 반대입니다. 하나씩 보면 이 충돌이 드러나지 않습니다 — 각 변환은 저마다 만족스러운 답을 내놓기 때문입니다.
- HighContrast에 세 변환을 적용하고, **각 변환 후 여섯 색의 모든 쌍이 최소 색 거리(ΔE 30) 이상**임을 검사합니다. 변환 행렬은 결정적이고 세 개를 합쳐 열 줄 남짓입니다.
- **Classic보다 모든 유형에서 나은지도 함께 고정합니다.** 아래 기록한 1차 구현이 정확히 그 검사가 없어서 통과했습니다.
- **Classic에는 최소 거리 기준을 적용하지 않습니다.** Classic은 표준 큐브 색을 그대로 쓰는 것이 존재 이유이고, 통과할 수 없는 기준을 걸면 test가 "Classic은 예외"라는 조항을 달게 됩니다. Classic은 RGB known-answer로만 고정하고, 최소 거리 test는 "Classic은 이 기준을 통과하지 못한다"를 확인해 기준이 슬며시 낮아지는 것을 막는 데 씁니다.
- 결과 palette는 **명도의 사다리**입니다. 세 유형이 서로 직교하는 축을 잃으므로 어느 쪽도 빼앗지 않는 차원은 명도뿐이고, 색상은 그 위에 남는 구분을 더합니다. 대가는 정상 시각에서의 화려함이며(ΔE 54 → 49) 그 손해는 감수합니다.

```text
thorvg_rubiks_set_palette(palette: int) -> int   // 유효하지 않은 값 거절
thorvg_rubiks_palette() -> int
```

- 초기화 전에는 `set_palette`가 거절하고 `palette`는 기본값을 반환합니다.

### 회전 효과음

- 관찰 대상은 Phase 11의 `timeline_cursor`입니다. 모든 commit이 cursor를 정확히 ±1 움직이므로, frame마다 읽는 값이 이전과 다르면 그 frame에 commit이 있었던 것입니다 — 완료 판정이 쓰는 관찰과 같은 것이라 소리를 낼 지점을 새로 만들지 않습니다. **소리는 cursor가 변한 frame에 한 번입니다.** 한 관찰 창(≤16ms)에 commit이 둘 들어오는 극단적 입력에서도 클릭 하나로 들리는 것이 자연스러우므로, 개수를 세는 계약은 두지 않습니다.
- **그 관찰은 `GameController`가 아니라 `GameSession.observe()`에 이미 있습니다.** 완료 판정이 쓰는 `committed`가 정확히 이 값이므로, 소리는 그 자리에 붙는 한 줄이고 새 비교도 새 기준값도 생기지 않습니다. Session의 클래스 주석이 "the move log, the sound, the records and the history all watch a solve rather than a button"으로 이 자리를 이미 지목하고 있습니다. 초안은 이것을 controller의 관찰이라고 적었으나, 그대로 구현하면 같은 비교가 두 곳에 놓이고 아래의 기준값 갱신을 controller가 다시 만들게 됩니다.
- Reset·scramble에서 소리가 새지 않는 것도 그래서 공짜입니다. `restart()`와 `beginScramble()`이 이미 `takeBaseline()`으로 기준값을 갱신하므로(Phase 11), 이 phase가 더할 갱신이 없습니다. 복원은 controller 부착 전에 끝나 관찰 창 밖입니다.
- Scramble과 되감기 재생의 commit도 cursor를 움직이므로 재생 연출에 소리가 함께 붙습니다. Ambient만 cursor를 움직이지 않아 자연히 무음입니다 — 화면 보호기에 맞는 결과이고, web이 ambient 여부를 조회해 분기할 필요가 없습니다.
- 소리는 WebAudio oscillator + gain envelope로 합성한 수십 ms의 클릭입니다. 외부 asset이 없어 빌드와 배포가 바뀌지 않습니다.
- 소리를 내는 쪽은 `GameSessionOptions`에 주입하는 port 하나입니다 — `TimerEnvironment`가 이미 같은 모양으로 들어오고 있으므로 관례가 그대로 있습니다. jsdom에는 `AudioContext`가 없으므로, 이 seam이 unit test가 재생 횟수를 셀 수 있게 하는 유일한 수단이기도 합니다.
- **`AudioContext`는 window의 one-shot gesture 리스너에서 생성/resume하고 리스너는 스스로 떨어집니다.** 첫 commit에서 lazy하게 만드는 방식은 동작하지 않습니다: commit은 frame 안에서 관찰되므로 그 호출은 gesture의 콜스택 밖이고, autoplay 정책은 생성·resume이 gesture 콜스택 안에서 일어나기를 요구합니다. Mute toggle은 기본 off(소리 남)라 그 버튼을 첫 gesture로 삼을 수도 없습니다.
- Context를 만들 수 없거나 resume이 거절되면 조용히 무음으로 강등합니다 — 예외를 밖으로 내보내지 않습니다. 소리는 부가 기능이라 실패가 앱 오류로 승격되지 않습니다.
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

### 1. Palette

- [x] `Palette` enum과 대체 palette 상수, `to_color(color, palette)` 분기 구현
- [x] Application의 palette 상태와 두 scene builder 경로 연결
- [x] Classic의 여섯 색 RGB known-answer test
- [x] 세 이색형 색각 변환 구현
- [x] HighContrast의 변환별 여섯 색 상호 색 거리 test와, 모든 유형에서 Classic보다 낫다는 test
- [x] Palette 변경이 cube·camera·timeline·timer 상태를 바꾸지 않는 test
- [x] C ABI 추가와 유효성 검증, generated 산출물과 fake fixture 갱신
- [x] Palette toggle과 `aria-pressed`, TS unit test(전이와 초기 상태)
- [x] e2e: toggle 후 전개도 픽셀 색이 대체 palette와 일치하고, 3D cube와 전개도가 함께 바뀌는지

### 2. 효과음

- [x] WebAudio 합성 클릭과 gain 제한 구현, `GameSessionOptions`의 port로 주입
- [x] One-shot gesture 리스너에서의 context 생성/resume과 실패 시 무음 강등
- [x] `GameSession.observe()`의 기존 `committed`에 연결하고 mute toggle 추가
- [x] TS unit test: cursor가 변한 frame마다 한 번 재생, reset·ambient에서 무음, mute 시 무음, context 부재 안전 (복원 무음은 복원이 생기는 Phase 14에서 검증)

### 3. 속도

- [x] Engine 배율 상태와 두 tempo 지점 적용, clamp·검증
- [x] 진행 중 snap의 duration 불변 test
- [x] ABI 추가와 슬라이더 UI (값 표시 포함)
- [x] Native 고정 dt test: `duration = base / scale` 식 자체와, 2× 배율이 1×보다 먼저 소진되는지 (정확한 frame 수 비례는 반올림에 걸리고, browser wall-clock은 더 불안정합니다)
- [x] Native 고정 dt test: **사용자 drag release의 snap**도 2×에서 1×보다 먼저 끝나는지 (배율이 재생만이 아니라 pointer 경로에도 닿는 증거)
- [x] Native, WASM, TypeScript unit, e2e와 production build 전체 실행

## Acceptance criteria

- 색 매핑 분기는 `to_color` 한 곳에만 존재하며, Application과 scene builder는 palette 값을 전달만 합니다.
- 대체 palette의 여섯 색은 **세 이색형 색각 변환 각각을 거친 뒤에도** 모든 쌍이 최소 색 거리 이상 떨어져 있고, 모든 유형에서 Classic보다 낫습니다. 두 성질 모두 test로 고정됩니다. Classic은 RGB known-answer로만 고정되고 최소 거리 기준의 대상이 아닙니다.
- 3D cube와 전개도가 같은 palette로 함께 렌더링되고, palette 변경은 busy 중에도 허용됩니다.
- 세 옵션 모두 cube, timeline, timer 상태에 영향을 주지 않습니다.
- Cursor가 변한 frame마다 효과음이 한 번 재생되고, reset에서는 울리지 않으며, ambient 관람은 무음이고, mute가 즉시 적용됩니다. 복원에서의 무음은 복원이 생기는 Phase 14가 확인합니다.
- 소리는 외부 asset 없이 합성되고, WebAudio가 없는 환경에서도 앱이 정상 동작합니다.
- 속도 슬라이더 하나가 사용자 snap, scramble, 되감기 재생의 tempo를 함께 바꾸고, 진행 중인 animation은 튀지 않습니다.
- ThorVG 경계와 renderer는 변경되지 않습니다.
- Native, WASM, TypeScript unit, browser e2e와 production build가 모두 통과합니다.

## 개정 기록

- **1차 구현의 대체 palette를 폐기하고 다시 골랐습니다.** 처음 고른 여섯 색은 deuteranopia만 기준으로 최적화했고, 실제로 그 기준에서는 표준 큐브의 ΔE 22를 36으로 올렸습니다. 그런데 tritanopia에서 재보니 표준 큐브의 11.8보다 **낮은 8.0**이었습니다. 이유는 구조적입니다 — 적록 결핍은 청황 축이 남으므로 그쪽으로 색을 벌리는 것이 공짜인데, tritanopia는 정확히 그 축을 잃습니다. 한 유형만 보고 최적화하면 다른 유형에게 표준 큐브보다 나쁜 판을 주게 되고, "검증된 preset"이라는 이 항목의 전제가 그 자리에서 깨집니다. 다시 고른 set은 세 유형 전부에서 40 부근이고 Classic(12)을 모든 유형에서 이깁니다. Test에 "모든 유형에서 Classic보다 낫다"를 더한 것은 같은 실수가 통과하지 못하게 하기 위한 것입니다.
- **`prefers-reduced-motion`은 초기 배율 2×로 반영했습니다.** 강제하지 않고 슬라이더로 되돌릴 수 있습니다 — 이 앱의 존재 이유가 애니메이션이라 빼앗지 않고 제안만 합니다.
- **문서가 지목한 "가장 어려운 쌍"도 틀렸습니다.** Red/Orange라고 적었으나 변환 후 재보면 표준 큐브의 최악 쌍은 Yellow/Orange(22.2)이고 Red/Orange는 32.7로 세 번째입니다. 색각이 유지하는 축이 명도인데 Red가 훨씬 어둡기 때문입니다. 이 문서가 "직관이 아니라 변환으로 검증하라"고 정해 둔 것이 자기 자신의 전제를 잡아낸 사례라 기록해 둡니다.

- **Palette를 Phase 7.5에서 이 문서로 되돌립니다.** 의존이 없다는 이유로 앞으로 당겨 두었으나, 그 근거는 "지금 바로 할 수 있다"였지 "지금 해야 한다"가 아니었습니다. 실제로 앞당겨 하면 HUD에 palette toggle을 단독으로 붙였다가 이 phase가 옵션 면을 만들 때 그리로 옮기게 되므로, UI를 한 번만 만들도록 셋을 함께 둡니다. Engine 쪽 작업량(`to_color` 분기, 색 거리 test)은 시점과 무관하게 같습니다. 세 옵션은 여전히 서로 독립이므로 구현 단계도 분리되어 있습니다.
- **임의 색 지정(theme editor)은 검토 후 기각했습니다.** 첫째, 이 항목의 유일한 보장인 "변환 후 여섯 색의 상호 거리" test가 성립하지 않게 됩니다 — 사용자가 빨강 계열 여섯을 고를 수 있으므로 고정할 성질이 없어지고, 값을 지키려면 실시간 경고라는 별개 기능이 붙습니다. 둘째, 저장 계층을 두지 않기로 했으므로(Phase 14) 직접 고른 여섯 색이 reload마다 사라져 잘라낸 저장 계층을 되살리자는 압력이 됩니다. 셋째, 손으로 설정해야 하는 접근성은 검증된 preset보다 나쁜 접근성입니다. "고를 수 있다"가 목적이라면 preset을 하나 더 두는 것이 enum 값 하나와 거리 test 한 줄로 끝나며 검증도 ABI도 그대로입니다.

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
