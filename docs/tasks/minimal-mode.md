# Minimal mode

## Status

`Completed` — 2026-10-30.

Phase 번호가 없는 횡단 작업입니다. 엔진은 한 줄도 바뀌지 않았습니다.

## Objective

페이지를 처음 열면 ThorVG 데모로 읽히게 합니다. 기본 화면은 큐브와 전개도, 그리고 버튼 한 줄뿐이고, 지금까지의 앱 전체는 `?extended`를 붙이면 그대로 열립니다.

- 기본(minimal) 화면에 있는 것은 제목, 상태 줄, 3D 큐브와 Net을 나란히 둔 무대(Split, 좁은 화면에서는 위아래), Scramble·Reset·Solve와 full app으로 가는 아이콘 링크, 그리고 오른쪽 위의 해·달 테마 토글입니다.
- 드래그로 층을 돌리고 빈 곳을 드래그해 시점을 돌리는 것은 그대로입니다.
- `?extended`는 오늘까지의 앱과 똑같아야 합니다. 기존 test가 그것을 지킵니다.

## 바뀐 것

### 모드는 시작할 때 한 번 정합니다

`web/src/AppMode.ts`가 `AppMode = 'minimal' | 'extended'`와 `appModeOf(search)`를 둡니다. `bootstrap`이 주소의 query에서 모드를 한 번 읽어 `createGameShell`과 `startApp`에 넘기고, `startApp`이 game controller에 넘깁니다. 주소에 `extended`가 있으면 extended이고, 없으면 minimal입니다.

### 화면은 같은 shell을 덜 보여 줍니다

Game controller는 `GameUi`의 모든 control을 받아 연결합니다. 이것을 모드별로 나누면 controller 전체를 고쳐야 해서, minimal도 같은 markup을 쓰고 root의 `data-mode="minimal"`을 stylesheet가 읽어 나머지를 숨깁니다. `display: none`이라 탭 순서와 접근성 트리에서도 빠집니다. 숨기는 것은 크기 칩, 헤더 도구(⌘K 트리거, Share, Settings), 공유 카드, 타이머, 축 배지, View rail, 페인트 바, 타임라인, Details rail과 패널, Settings, 명령 메뉴, 그리고 dock의 Rewind·Undo·Redo입니다.

Minimal에만 있는 세 control은 minimal일 때만 씁니다. Dock의 `#minimal-reset`과 `#full-version` 링크, 헤더의 `#theme-toggle`입니다. 링크는 글자 없이 패널이 늘어나는 아이콘(`ICONS.more`)만 보이고, 이름 "Full version"은 `aria-label`과 tooltip(`title`)에 있습니다. 링크는 `./?extended`라는 상대 주소라서 배포 경로(`/Hackathon-Rubiks-Cube/`)가 그대로 남습니다.

### 붙이지 않는 것

Minimal에서 `bootstrap`은 명령 메뉴, Settings drawer, 크기 칩, Details 패널, depth stepper, 테마 선택기, lighting slider를 붙이지 않습니다. 명령 메뉴는 숨기기만 한 것이 아니라 붙이지 않았으므로 ⌘K/Ctrl+K를 듣는 listener 자체가 없습니다.

Game controller는 `mode`가 minimal이면 window의 `keydown`을 듣지 않아 면 회전 키가 없고, click sound 대신 `SILENT_SOUND`를 씁니다. 소리를 끄는 Mute가 Settings에 있어서, Settings가 없는 화면에서는 끌 수 없는 소리를 내지 않습니다. 오디오 context를 열려고 첫 gesture를 기다리는 listener도 없습니다.

`startApp`은 minimal에서 `?lighting=` 조정 query를 읽지 않습니다.

테마는 처음에 시스템을 따르고, 헤더 오른쪽 위의 해·달 버튼(`attachThemeToggle`)으로 바꿉니다. 버튼은 지금 칠해진 테마를 보여 줍니다(밝으면 해, 어두우면 달, `aria-pressed`가 어두움). 누르면 반대 테마를 Light 또는 Dark로 확정해 저장하므로, 그 뒤로는 시스템이 바뀌어도 따라가지 않습니다. 저장소는 full app의 Settings와 같은 키(`thorvg-rubiks-theme`)라서 한쪽에서 고른 테마가 다른 쪽에서도 열리고, `index.html`의 inline script도 두 모드 모두 저장된 값을 먼저 적용합니다. 브라우저가 큐브를 돌릴 수 없는 화면(`unsupported`)에서는 theme controller가 만들어지지 않으므로 버튼도 숨깁니다.

### Reset

새 엔진 호출이 아니라 full app에 이미 있는 Settings의 두 버튼을 누릅니다. 먼저 `Home view`, 그다음 `Reset session`이라 상태 줄이 세션의 문장으로 끝납니다. 엔진이 준비된 동안만 켜지고, 로딩 중과 오류 뒤에는 꺼집니다. 두 버튼이 꺼지는 때와 같습니다.

Scramble(재생 중에는 Stop)과 Solve는 full app의 버튼 그대로입니다. Solve의 busy·disabled 동작도 같습니다.

### 캔버스

`bootstrap`은 두 모드 모두에 같은 view 목록(Net, Rings, Axes)을 넘깁니다. 그래서 큐브 캔버스에는 큐브만 남습니다. Minimal에서는 링과 축 배지의 캔버스에 상자가 없어서 엔진이 그 surface를 치워 둡니다. Split과 Net은 엔진의 기본값이고 minimal에는 바꿀 수단이 없습니다.

### 공유 링크

- **Extended에서 만든 링크는 extended로 열립니다.** 링크는 현재 주소에 fragment만 바꿔 만들기 때문에 `?extended`가 함께 갑니다. e2e가 `search === '?extended'`를 확인합니다.
- **`?extended` 없는 `#s=` 링크는 minimal에서 그 큐브를 엽니다.** 복원(`openSharedState`)은 엔진만 쓰고 UI에 기대지 않아서, extended로 돌려보낼 이유가 없습니다. Minimal에는 Share가 없으므로 이런 링크는 이 변경 전에 만든 링크뿐입니다.
- 10×10 이상의 옛 링크를 minimal에서 열면 Solve가 오래 걸린다는 안내가 나옵니다. Minimal에는 Rewind가 없으므로 그 안내는 Rewind를 말하지 않습니다.

## Verification

- `npm --prefix web run build`(tsc + vite)가 통과합니다.
- Unit test는 369개에서 382개가 되었고 모두 통과합니다. `bootstrap`은 기본 모드, minimal이 lifecycle에 넘기는 것(lighting·blocked 없음, 같은 view 목록, theme factory에 모드), Reset 배선, 붙지 않는 메뉴·Settings·패널을 봅니다. `GameShell`은 두 모드의 markup을, `GameController`는 키 listener와 소리 listener가 없음을, `AppLifecycle`은 모드 전달과 lighting query를 봅니다.
- E2e는 77개에서 84개가 되었고 모두 통과합니다. 기존 spec은 모두 `./?extended`(`./?extended#s=…`, `./?extended&lighting=…`)를 열고 assertion은 그대로입니다. 공유 test에 링크가 `?extended`를 지니는지 한 줄을 더했습니다. 새 `minimal.spec.ts`는 데스크톱과 폰 너비에서 보이는 control이 정확히 Scramble·Reset·Solve·Full version인지, 장면 계약(큐브와 넷이 각자의 캔버스에 혼자)을 확인합니다. ⌘K와 면 키가 아무것도 하지 않는지, Scramble·Solve·Reset과 드래그 회전·시점 회전, Full version 링크, `?extended` 없는 공유 링크의 복원도 확인합니다.
- `vite preview`로 데스크톱(1280×800)과 폰(375×812) 너비에서 눈으로 확인했습니다.

## 개정: 아이콘 링크와 테마 토글 (2026-10-30)

- `Full version` 글자 링크를 dock의 둥근 아이콘 버튼으로 바꿨습니다. 아이콘은 네 칸 중 한 칸이 `+`인 그림(`ICONS.more`)으로, 이 화면보다 패널이 많은 앱으로 간다는 뜻입니다.
- 헤더 오른쪽 위에 해·달 토글을 더했습니다(`ICONS.sun`, 새 `ICONS.moon`). 평소에는 옅은 테두리 안의 아이콘만 보이고, 마우스를 올리면 테두리와 채움이 진해집니다. Minimal도 full app과 같은 저장소에 테마를 저장하도록 바꿨고, 그래서 `createPageTheme`과 `ThemeFactory`는 모드를 받지 않습니다.
- Unit test는 382개에서 386개, e2e는 84개에서 85개가 되었습니다. 새 e2e는 시스템이 밝을 때 해가 보이고, 누르면 어두워져 저장되고, Full version으로 가면 full app이 Dark로 열리는지 봅니다.
