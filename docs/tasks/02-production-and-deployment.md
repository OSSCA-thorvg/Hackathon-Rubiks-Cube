# Phase 2: Production and Deployment

## Status

`Completed`

## Objective

Phase 1에서 검증한 Web–WASM–ThorVG 경로를 깨끗한 Linux 환경에서 재현하고 GitHub Pages에 배포합니다.
Pull request마다 native test, WASM build, Vite production build와 browser end-to-end test를 실행하고, `main` branch에 반영된 검증 완료 artifact만 배포합니다.

```text
Pull request / push
        ↓
Checkout + ThorVG submodule
        ↓
Pinned build toolchain
        ↓
Native build and test
        ↓
WASM build
        ↓
Vite production build
        ↓
Browser end-to-end test
        ↓
GitHub Pages artifact
        ↓
Deploy from main only
```

이 phase가 완료되면 이후 기능 phase는 같은 CI와 production URL을 회귀 검증 환경으로 사용합니다.

## Dependencies

- [Phase 1: WebAssembly Vertical Slice](./01-wasm-vertical-slice.md)가 완료되어야 합니다.
- `build_wasm.sh`가 generated JavaScript와 WASM을 `web/src/wasm/generated`에 동기화해야 합니다.
- `web` package에 Vitest 기반 `test:unit`과 Playwright 기반 `test:e2e` script가 있어야 합니다.
- Native test는 Catch2를 meson wrapdb subproject로 사용하므로 CI의 meson configure 단계에 네트워크 접근이 필요합니다.
- GitHub repository의 Pages source를 **GitHub Actions**로 설정할 권한이 필요합니다.

## Scope

- Reproducible toolchain version 관리
- GitHub Actions continuous integration workflow
- ThorVG submodule checkout
- Native build와 unit test
- WASM build와 artifact 검증
- Vite production build
- Production preview 기반 browser end-to-end test
- GitHub Pages base path 구성
- GitHub Pages artifact upload와 deployment
- Workflow permissions, concurrency와 failure behavior 정의
- 배포된 production URL smoke verification

## Out of scope

- Custom domain과 `CNAME`
- Preview environment별 임시 URL
- Pull request별 Pages deployment
- Release tag와 semantic version automation
- Multi-platform CI matrix
- WebGL 및 WebGPU에 필요한 COOP/COEP header
- Performance benchmark와 bundle budget enforcement
- CDN 또는 별도 hosting provider

## Architecture decisions

### Workflow structure

`.github/workflows/deploy-pages.yml` 하나에서 `verify`와 `deploy` job을 분리합니다.

```text
verify
├── checkout
├── toolchain setup
├── native tests
├── WASM build
├── Vite build
├── browser e2e
└── Pages artifact upload (main only)
       ↓
deploy (main only)
└── GitHub Pages deployment
```

- Pull request는 `verify` 전체를 실행하지만 배포하지 않습니다.
- `main` push와 `main`에서 실행한 manual dispatch만 배포합니다.
- `deploy`는 `verify`가 성공하고 Pages artifact가 생성된 경우에만 실행합니다.
- Build와 deploy를 분리하여 배포 권한이 build step에 노출되지 않게 합니다.

### Workflow triggers

```yaml
on:
  pull_request:
  push:
    branches: [main]
  workflow_dispatch:
```

문서나 소스 변경을 path filter로 제외하지 않습니다.
초기에는 workflow 누락 가능성을 줄이기 위해 모든 pull request와 `main` push를 검증합니다.

### Toolchain policy

CI는 runner에 우연히 설치된 버전을 사용하지 않습니다.
프로젝트가 검증한 major 또는 exact version을 repository 파일과 workflow에 명시합니다.

| Tool | Policy |
| --- | --- |
| Runner | `ubuntu-latest` |
| Node.js | Node 24 major 고정 |
| npm | Node에 포함된 버전과 committed `package-lock.json` 사용 |
| Python | Python 3.13 major/minor 고정 |
| Meson | CI requirements file에 exact version 고정 |
| Ninja | CI requirements file에 exact version 고정 |
| Emscripten | 별도 version file에 exact SDK version 고정 |
| ThorVG | Git submodule commit으로 고정 |

Vite 8이 요구하는 Node 범위 안에서 CI와 로컬의 차이를 줄이기 위해 Node 24를 사용합니다.
Emscripten version은 Phase 1을 통과한 SDK로 고정하며 `latest`나 moving branch를 사용하지 않습니다.

Workflow에서 사용하는 공식 action의 기준 major version은 다음과 같습니다.

| Action | Version |
| --- | --- |
| `actions/checkout` | `v7` |
| `actions/setup-node` | `v7` |
| `actions/setup-python` | `v7` |
| `actions/cache` | `v6` |
| `actions/configure-pages` | `v6` |
| `actions/upload-pages-artifact` | `v5` |
| `actions/deploy-pages` | `v5` |

Action major를 올릴 때는 runner 요구 버전과 breaking change를 확인합니다.

### Emscripten installation

제3자 setup action 대신 공식 `emscripten-core/emsdk` repository와 CLI를 사용합니다.

```text
Read pinned version
        ↓
Restore emsdk cache
        ↓ cache miss
Clone official emsdk
        ↓
emsdk install <version>
        ↓
emsdk activate <version>
        ↓
source emsdk_env.sh
```

- SDK cache key는 runner OS와 pinned Emscripten version을 포함합니다.
- Cache miss에서만 clone과 install을 수행합니다.
- `build_wasm.sh`에는 CI 전용 경로나 SDK 다운로드 로직을 추가하지 않습니다.
- Workflow가 `EMSDK`와 Emscripten PATH를 준비한 뒤 기존 build script를 호출합니다.

### Dependency installation

- Checkout은 ThorVG를 포함하도록 `submodules: recursive`를 사용합니다.
- Web dependency는 `npm ci --prefix web`으로 lockfile과 정확히 일치하게 설치합니다.
- Playwright는 Chromium만 설치하고 Linux system dependency도 함께 준비합니다.
- Meson과 Ninja는 pinned CI requirements file에서 설치합니다.

### Build order

검증 순서는 local workflow와 동일하게 유지합니다.

1. Native Meson setup
2. Native compile
3. Native tests
4. WASM build and synchronization
5. Web dependency installation
6. TypeScript unit tests
7. Vite production build
8. Production preview 기반 browser e2e
9. Pages artifact inspection

Native test가 실패하면 비용이 더 큰 WASM과 browser 단계는 실행하지 않습니다.

## Production base path

Repository Pages URL은 다음 하위 경로를 사용합니다.

```text
https://ossca-thorvg.github.io/Hackathon-Rubiks-Cube/
```

Vite production build의 base는 `/Hackathon-Rubiks-Cube/`로 설정합니다.
Development server는 `/`를 사용합니다.

`vite preview`는 config를 `command === 'serve'`로 해석하므로 command만으로 분기하면 preview가 base `/`에서 서빙됩니다.
그 상태에서 build output은 `/Hackathon-Rubiks-Cube/` asset을 참조하므로 asset 요청이 SPA fallback에 걸려 HTML이 반환되고, preview 기반 browser test는 MIME 오류로 실패합니다.
Preview는 production output을 검증하는 경로이므로 `isPreview`를 함께 분기해 build와 같은 base를 사용합니다.

```ts
import { defineConfig, type UserConfig } from 'vite';

export default defineConfig(({ command, isPreview }): UserConfig => ({
  base: command === 'build' || isPreview ? '/Hackathon-Rubiks-Cube/' : '/',
}));
```

Generated module과 WASM binary는 Vite module graph 안에 있으므로 base path는 번들링과 asset URL 재작성에 자동 반영됩니다.
TypeScript가 WASM URL을 직접 구성하지 않으므로 `import.meta.env.BASE_URL`을 사용할 곳도 없습니다.
Production test는 `/Hackathon-Rubiks-Cube/` 경로로 page를 열어 root-relative URL 회귀를 검출합니다.

Custom domain을 도입하면 base path 정책을 다시 결정하며 이 phase에서는 다루지 않습니다.

## Pages artifact contract

`web/dist`만 Pages artifact로 업로드합니다.

Generated module과 WASM binary는 Vite module graph 안에 있으므로 다른 asset과 같이 content hash가 붙은 이름으로 `assets/`에 방출됩니다.
별도의 `wasm/` 디렉터리나 고정된 파일 이름은 존재하지 않습니다.

```text
web/dist/
├── index.html
├── favicon.svg
└── assets/
    ├── index-<hash>.js
    ├── index-<hash>.css
    ├── thorvg-rubiks-<hash>.js
    └── thorvg-rubiks-<hash>.wasm
```

Upload 전에 다음을 확인합니다.

- `index.html`이 존재합니다.
- `assets/`에 `thorvg-rubiks-*.wasm`과 `thorvg-rubiks-*.js`가 정확히 하나씩 존재하며 비어 있지 않습니다.
- `index.html`이 production base path의 asset을 참조합니다.
- Artifact 안에 source tree, `node_modules`, native binary와 build directory가 포함되지 않습니다.
- Artifact 안에 symbolic link가 없습니다.

## Browser verification

Browser test는 development server가 아니라 production output을 대상으로 실행합니다.

- Vite preview 또는 정적 HTTP server로 `web/dist`를 제공합니다.
- Base URL은 repository Pages path를 포함합니다.
- Chromium에서 page를 열고 `Loading → Ready` 전환을 기다립니다.
- Console error와 unhandled page error가 없어야 합니다.
- Phase 1 rendered scene contract의 중앙과 모서리 pixel을 검사합니다.
- JavaScript와 WASM request가 모두 성공했는지 확인합니다.
- Resize 후 pixel과 canvas drawing buffer를 다시 검사합니다.

배포 후 smoke verification은 `deploy-pages`가 반환한 URL에서 최소한 다음을 확인합니다.

- HTTP page load 성공
- JavaScript와 WASM asset load 성공
- Ready 상태 도달

배포 직후 외부 URL 검증은 일시적인 Pages 전파 지연을 고려해 제한된 횟수만 재시도하고, 무한 retry는 사용하지 않습니다.

## Permissions and security

Workflow 기본 권한은 read-only로 제한합니다.

```yaml
permissions:
  contents: read
```

`deploy` job에만 다음 권한을 부여합니다.

```yaml
permissions:
  pages: write
  id-token: write
```

- Pull request code는 Pages write 또는 OIDC token 권한으로 실행하지 않습니다.
- Repository secret이나 personal access token을 사용하지 않습니다.
- GitHub Pages deployment는 `github-pages` environment를 사용합니다.
- Deploy job URL은 `deploy-pages` step output으로 기록합니다.
- Third-party action을 추가해야 한다면 floating branch가 아니라 reviewed commit SHA로 고정합니다.

## Concurrency

Pages deployment는 다음 concurrency group을 사용합니다.

```yaml
concurrency:
  group: github-pages
  cancel-in-progress: true
```

새로운 `main` commit이 들어오면 이전 배포를 취소하여 오래된 artifact가 나중에 배포되는 것을 방지합니다.

이 group은 **deploy job에만** 적용합니다.
Workflow 레벨에 두면 서로 다른 pull request의 verify까지 같은 group에 묶여 서로를 취소합니다.

## Failure behavior

- Submodule checkout 실패는 즉시 workflow를 실패시킵니다.
- Native test 실패 시 WASM build와 배포를 실행하지 않습니다.
- WASM artifact 누락 또는 빈 파일은 Vite build 전에 실패합니다.
- Vite build 또는 browser e2e 실패 시 Pages artifact를 업로드하지 않습니다.
- Pull request에서는 검증 성공 여부와 관계없이 배포 job을 생성하지 않습니다.
- Deploy 실패는 verify 결과와 별도로 표시하되 workflow 전체는 실패합니다.
- 실패를 숨기는 `continue-on-error`는 사용하지 않습니다.

## Implementation steps

### 1. Pin toolchains

- [x] Phase 1에서 검증한 Emscripten exact version 기록
- [x] Meson과 Ninja exact version을 CI requirements file에 기록
- [x] Node 24와 Python 3.13을 workflow에 명시
- [x] Local setup 문서에서 동일한 최소 toolchain 확인 방법 제공

### 2. Configure production build

- [x] `web/vite.config.ts` 추가
- [x] Development와 production/preview base path 분리
- [x] Production build에서 WASM asset URL이 base path를 반영하는지 확인
- [x] `npm --prefix web run build` 결과에서 WASM artifact 확인
- [x] Playwright base URL에 production base path 반영
- [x] e2e에 console error, page error와 JS/WASM asset 응답 검증 추가

### 3. Add continuous integration

- [x] `.github/workflows/deploy-pages.yml` 추가
- [x] Recursive submodule checkout 구성
- [x] Node, Python, Meson과 Ninja 설정
- [x] Official emsdk 설치와 version-aware cache 구성
- [x] Native build와 test step 추가
- [x] WASM build step 추가
- [x] npm lockfile install과 Vite build step 추가
- [x] Chromium 설치와 production browser e2e step 추가
- [x] Pages artifact contract 검증 step 추가

### 4. Add deployment

- [x] Main-only Pages artifact upload 구성
- [x] `github-pages` environment를 사용하는 deploy job 추가
- [x] `actions/configure-pages`로 Pages site 구성 (deploy job, 첫 실행 시 enablement)
- [x] `pages: write`와 `id-token: write`를 deploy job으로 제한
- [x] Deployment concurrency 구성
- [x] Repository Pages source가 GitHub Actions인지 확인

### 5. Verify production

- [x] Pull request에서 verify job 전체 통과
- [x] `main` push에서 verify 후 deploy 실행 확인
- [x] Deployment URL smoke verification
- [x] Repository subpath에서 WASM module URL 확인
- [x] 새 commit이 이전 deployment를 안전하게 대체하는지 확인

## Acceptance criteria

- 깨끗한 `ubuntu-latest` runner에서 추가 수동 설정 없이 workflow가 실행됩니다.
- ThorVG submodule이 recorded commit으로 checkout됩니다.
- Native compile과 모든 native test가 통과해야 다음 단계가 실행됩니다.
- Pinned Emscripten으로 WASM JavaScript와 binary가 생성됩니다.
- `npm ci`와 Vite production build가 committed lockfile을 사용합니다.
- Browser e2e가 production base path에서 Phase 1 rendered scene contract를 통과합니다.
- Pull request workflow는 배포 권한을 갖거나 Pages deployment를 수행하지 않습니다.
- `main`의 검증된 `web/dist`만 GitHub Pages에 배포됩니다.
- 배포 artifact에 `index.html`, application assets와 두 WASM artifact만 필요한 구조로 포함됩니다.
- 배포 URL에서 application이 Ready 상태에 도달하고 ThorVG 사각형을 표시합니다.
- Workflow에는 로컬 SDK 경로, personal token과 repository secret이 포함되지 않습니다.

## Verification commands

CI를 추가하기 전에 production build를 로컬에서 재현합니다.

```bash
# Native verification
meson setup build/native
meson compile -C build/native
meson test -C build/native --print-errorlogs

# WASM verification
source /path/to/emsdk/emsdk_env.sh
./build_wasm.sh

# Web production verification
npm ci --prefix web
npm --prefix web run test:unit
npm --prefix web run build
npm --prefix web run test:e2e
```

GitHub에서는 다음 event를 각각 확인합니다.

```text
pull_request    → verify only
push main      → verify + deploy
workflow_dispatch on main → verify + deploy
```

## Completion

모든 acceptance criteria를 충족하고 실제 deployment URL을 검증한 뒤 다음 작업을 수행합니다.

- 이 문서의 status를 `Completed`로 변경합니다.
- 상위 [`IMPLEMENTATION_PLAN.md`](./IMPLEMENTATION_PLAN.md)의 Phase 2를 완료 처리합니다.
- 배포 URL과 toolchain update 절차를 README에 기록합니다.
- Phase 3 math and graphics foundation 세부 문서를 작성합니다.

## References

- [GitHub Pages custom workflow](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
- [GitHub Pages publishing source](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site)
- [Vite public base path](https://main.vite.dev/guide/build#public-base-path)
- [Emscripten SDK](https://github.com/emscripten-core/emsdk)
- [Checkout action submodule configuration](https://github.com/actions/checkout)
