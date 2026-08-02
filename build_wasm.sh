#!/usr/bin/env bash

set -euo pipefail

readonly PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly BUILD_DIR="${PROJECT_DIR}/build/wasm"
readonly CROSS_FILE="${PROJECT_DIR}/cross/wasm32.txt"
readonly -a THORVG_OPTIONS=(
    '-Dthorvg:engines=cpu'
    '-Dthorvg:extra=[]'
    '-Dthorvg:file=false'
    '-Dthorvg:loaders=[]'
    '-Dthorvg:savers=[]'
    '-Dthorvg:static=true'
    '-Dthorvg:threads=false'
    '-Dthorvg:tools=[]'
)

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
    meson setup --reconfigure "${BUILD_DIR}" \
        --cross-file "${CROSS_FILE}" \
        "${THORVG_OPTIONS[@]}"
else
    meson setup "${BUILD_DIR}" \
        --cross-file "${CROSS_FILE}" \
        "${THORVG_OPTIONS[@]}"
fi

meson compile -C "${BUILD_DIR}"

echo
echo "WASM build completed:"
echo "  ${BUILD_DIR}/engine/src/thorvg-rubiks.js"
echo "  ${BUILD_DIR}/engine/src/thorvg-rubiks.wasm"
