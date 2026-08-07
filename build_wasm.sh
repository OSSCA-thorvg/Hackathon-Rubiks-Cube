#!/usr/bin/env bash

set -euo pipefail

readonly PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly BUILD_DIR="${PROJECT_DIR}/build/wasm"
readonly CROSS_FILE="${PROJECT_DIR}/cross/wasm32.txt"

# ThorVG subproject options live in engine/meson.build so that the native
# and WASM builds cannot drift apart.

if [[ -z "${EMSDK:-}" ]]; then
    echo "Error: EMSDK is not configured." >&2
    echo "Run: source /path/to/emsdk/emsdk_env.sh" >&2
    exit 1
fi

if [[ ! -f "${EMSDK}/emsdk_env.sh" ]]; then
    echo "Error: EMSDK does not point to a valid SDK: ${EMSDK}" >&2
    exit 1
fi

source "${EMSDK}/emsdk_env.sh" >/dev/null

if [[ -f "${BUILD_DIR}/meson-private/coredata.dat" ]]; then
    meson setup --reconfigure "${BUILD_DIR}" --cross-file "${CROSS_FILE}"
else
    meson setup "${BUILD_DIR}" --cross-file "${CROSS_FILE}"
fi

meson compile -C "${BUILD_DIR}"

readonly WASM_JS="${BUILD_DIR}/engine/src/thorvg-rubiks.js"
readonly WASM_BINARY="${BUILD_DIR}/engine/src/thorvg-rubiks.wasm"
readonly WEB_WASM_DIR="${PROJECT_DIR}/web/src/wasm/generated"

for artifact in "${WASM_JS}" "${WASM_BINARY}"; do
    if [[ ! -f "${artifact}" ]]; then
        echo "Error: expected artifact is missing: ${artifact}" >&2
        exit 1
    fi
done

mkdir -p "${WEB_WASM_DIR}"
cp "${WASM_JS}" "${WASM_BINARY}" "${WEB_WASM_DIR}/"

echo
echo "WASM build completed:"
echo "  ${WASM_JS}"
echo "  ${WASM_BINARY}"
echo
echo "Artifacts synchronized for Vite:"
echo "  ${WEB_WASM_DIR}/thorvg-rubiks.js"
echo "  ${WEB_WASM_DIR}/thorvg-rubiks.wasm"
