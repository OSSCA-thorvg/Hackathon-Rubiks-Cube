# thorvg-rubiks

브라우저에서 동작하는 3×3×3 루빅스 큐브입니다.

C++로 작성된 engine이 Cube state, 3D interaction, rendering을 담당하고, [ThorVG](https://github.com/thorvg/thorvg)를 graphics pipeline의 최종 2D rendering backend로 사용합니다. Engine은 Emscripten으로 WASM으로 빌드되며, Web shell은 Vite + Vanilla TypeScript로 구성된 정적 Web application입니다.

<!-- TODO: 배포 후 데모 링크 추가 -->
<!-- 👉 **[Live Demo](https://<username>.github.io/thorvg-rubiks/)** -->

## 기술 스택

| 영역 | 구성 |
|---|---|
| Frontend | Vite, Vanilla TypeScript, HTML, Vanilla CSS |
| Engine | C++, ThorVG, Emscripten / WASM |
| Build | Meson, Ninja, GitHub Actions |
| Deploy | GitHub Pages |

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

```bash
# WASM engine 빌드
./build_wasm.sh

# 개발 서버 실행
cd web
npm install
npm run dev
```

Native unit test:

```bash
meson test -C build/native
```

## 설계 문서

아키텍처, graphics pipeline, interaction 설계, 구현 로드맵 등 상세한 내용은 [docs/DESIGN.md](docs/DESIGN.md)를 참고하세요.
