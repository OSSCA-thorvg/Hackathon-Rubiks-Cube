# thorvg-rubiks

브라우저에서 동작하는 루빅스 큐브입니다. 크기는 2×2×2부터 28×28×28까지 실행 중에 고를 수 있습니다. 상한이 28인 것은 한 수가 자기 layer를 비트 하나씩으로 적기 때문입니다 — 그보다 넓은 큐브의 수는 기록에도, move log에도, 공유 링크에도 담기지 않습니다.

C++로 작성된 engine이 Cube state, 3D interaction, rendering을 담당하고, [ThorVG](https://github.com/thorvg/thorvg)를 graphics pipeline의 최종 2D rendering backend로 사용합니다. Engine은 Emscripten으로 WASM으로 빌드되며, Web shell은 Vite + Vanilla TypeScript로 구성된 정적 Web application입니다.

ThorVG를 submodule로 포함하고 있어 `--recurse-submodules` 옵션을 사용하길 권장합니다.

```bash
git clone --recurse-submodules https://github.com/OSSCA-thorvg/Hackathon-Rubiks-Cube
```

👉 **[Live Demo](https://ossca-thorvg.github.io/Hackathon-Rubiks-Cube/)**

## 기술 스택

| 영역     | 구성                                        |
| -------- | ------------------------------------------- |
| Frontend | Vite, Vanilla TypeScript, HTML, Vanilla CSS |
| Engine   | C++, ThorVG, Emscripten / WASM              |
| Build    | Meson, Ninja, GitHub Actions                |
| Deploy   | GitHub Pages                                |

## 프로젝트 구조

```text
thorvg-rubiks/
├── web/                    # Vite + TypeScript + HTML/CSS (Browser application)
├── engine/
│   ├── src/cube/           # Cube state, cubie, move (rendering과 무관)
│   ├── src/math/           # vector, matrix, quaternion, projection
│   ├── src/graphics/       # camera, pipeline passes, cube와 전개도 geometry
│   ├── src/interaction/    # picking, drag 해석, snap animation, 시점 orbit
│   ├── src/render/         # ThorVG rendering backend
│   ├── src/app/            # engine lifecycle
│   └── third_party/        # vendored headers (linalg.h)
├── tests/                  # Native unit tests
├── subprojects/            # external C++ dependencies (ThorVG)
└── cross/                  # Emscripten cross configuration
```

`cube`, `math`, `graphics`, `interaction`은 ThorVG를 link하지 않는 별도 build target이고, 그중 `cube`는 아무 dependency도 갖지 않습니다. Cube 도메인이 graphics를, graphics pipeline이 rendering backend를 알지 못하도록 빌드 구조로 막아 둔 것이라, 해당 경계를 바꿀 때는 `engine/src/meson.build`를 함께 확인하세요. Pointer picking을 ThorVG hit test가 아니라 ray cast로 구현한 것도 같은 이유로, 덕분에 interaction test가 renderer 없이 돕니다.

## 빌드 및 실행

WASM 빌드는 Emscripten SDK를 필요로 합니다. `build_wasm.sh`는 `EMSDK` 환경 변수로 SDK를 찾으므로 먼저 emsdk 환경을 활성화합니다.

```bash
source /path/to/emsdk/emsdk_env.sh
```

```bash
# WASM engine 빌드
./build_wasm.sh

# 개발 서버 실행
cd web
npm install
npm run dev
```

테스트:

```bash
# Native unit test
meson setup build/native
meson test -C build/native --print-errorlogs

# TypeScript boundary unit test
npm --prefix web run test:unit

# Browser end-to-end test (build_wasm.sh 이후 실행)
npm --prefix web run test:e2e
```

## 배포

`main`에 push하면 GitHub Actions가 native test, WASM build, browser e2e를 검증한 뒤 [GitHub Pages](https://ossca-thorvg.github.io/Hackathon-Rubiks-Cube/)에 배포합니다. Pull request는 검증만 수행합니다.

CI toolchain은 저장소 파일에 고정되어 있습니다. 버전을 올릴 때 함께 수정하세요.

| Toolchain | 위치 |
| --- | --- |
| Emscripten | `.emscripten-version` |
| Meson, Ninja | `.github/ci-requirements.txt` |
| Node, Python | `.github/workflows/deploy-pages.yml` |
| ThorVG | `subprojects/thorvg` submodule commit |

## 설계 문서

아키텍처, graphics pipeline, interaction 설계, 구현 로드맵 등 상세한 내용은 [docs/DESIGN.md](docs/DESIGN.md)를 참고하세요.
