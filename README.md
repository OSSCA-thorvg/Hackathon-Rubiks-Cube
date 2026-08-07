# thorvg-rubiks

브라우저에서 동작하는 3×3×3 루빅스 큐브입니다.

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
├── web/          # Vite + TypeScript + HTML/CSS (Browser application)
├── engine/       # C++ / WASM engine
├── tests/        # Native unit tests
├── subprojects/  # external C++ dependencies (ThorVG)
└── cross/        # Emscripten cross configuration
```

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
