#!/usr/bin/env bash
#
# Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# --------------------------------------------------------------------------
# Backend-isolation guard for the BLE and BLEAudio libraries.
#
# Checks two invariants:
#   1. Public surface headers (<BLE.h>, <BLEAudio.h>, and every project header
#      they directly include) never include a backend/selector header
#      (BLEBackend.h, BLEAudioBackend.h, *.nimble.h, *.bluedroid.h) and never
#      name native stack types or ::Impl in real code.
#   2. Shared (non-*.nimble.* / non-*.bluedroid.*) source and header files
#      under BLE/src and BLEAudio/src never branch on BLE_NIMBLE / BLE_BLUEDROID
#      or raw CONFIG_BT_NIMBLE_* / CONFIG_BT_BLUEDROID_* macros outside the
#      sanctioned single-point guard/selector headers (core/BLEGuards.h,
#      core/BLEBackend.h, BLEAudioBackend.h).
#
# Comment lines (// ... and /* ... */) are stripped before token scans.
#
# Exit status: 0 = clean, 1 = violation(s) found.
# --------------------------------------------------------------------------

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BLE_SRC_DIR="$(cd "${SCRIPT_DIR}/../src" && pwd)"
if [ -d "${SCRIPT_DIR}/../../BLEAudio/src" ]; then
    AUDIO_SRC_DIR="$(cd "${SCRIPT_DIR}/../../BLEAudio/src" && pwd)"
else
    AUDIO_SRC_DIR=""
fi

status=0

indent() {
    printf '    %s\n' "${1//$'\n'/$'\n'    }"
}

strip_comments() {
    awk '
        { line = $0 }
        { gsub(/\/\*.*\*\//, "", line) }
        in_block {
            if (line ~ /\*\//) { sub(/^.*\*\//, "", line); in_block = 0 }
            else { next }
        }
        /\/\*/ { sub(/\/\*.*$/, "", line); in_block = 1 }
        { sub(/\/\/.*$/, "", line) }
        { print line }
    ' "$1"
}

check_surface() {
    local label="$1"
    local umbrella="$2"
    local src_dir="$3"
    local public_headers=()
    local hdr name bad_inc bad_tok

    if [ ! -e "${umbrella}" ]; then
        echo "ERROR: ${label} umbrella header not found: ${umbrella}"
        status=1
        return
    fi

    public_headers=("${umbrella}")
    while IFS= read -r inc; do
        [ -e "${src_dir}/${inc}" ] && public_headers+=("${src_dir}/${inc}")
    done < <(
        grep -oE '^[[:space:]]*#[[:space:]]*include[[:space:]]*"[^"]+"' "${umbrella}" \
            | sed -E 's/.*"([^"]+)".*/\1/'
    )

    for hdr in "${public_headers[@]}"; do
        [ -e "$hdr" ] || continue
        name="${label}/$(basename "$hdr")"

        bad_inc="$(grep -nE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]([^">]*\.(nimble|bluedroid)\.h|[^">]*BLE(Audio)?Backend\.h)' "$hdr" || true)"
        if [ -n "$bad_inc" ]; then
            echo "ERROR: ${name} includes a backend/selector header (public API must stay backend-agnostic):"
            indent "$bad_inc"
            status=1
        fi

        bad_tok="$(strip_comments "$hdr" | grep -nE 'esp_ble_|ble_gap_|ble_hs_|ble_uuid_|::Impl\b|Nimble[A-Za-z]*Impl|Bluedroid[A-Za-z]*Impl' || true)"
        if [ -n "$bad_tok" ]; then
            echo "ERROR: ${name} names a backend-specific type in code (public API must stay backend-agnostic):"
            indent "$bad_tok"
            status=1
        fi
    done
}

check_shared_units() {
    local label="$1"
    local src_dir="$2"
    local file rel bad_leak

    while IFS= read -r -d '' file; do
        rel="${file#"${src_dir}/"}"
        case "$rel" in
            *.nimble.h|*.nimble.cpp|*.nimble.c|*.bluedroid.h|*.bluedroid.cpp|*.bluedroid.c)
                continue
                ;;
            core/BLEGuards.h|core/BLEBackend.h|BLEAudioBackend.h)
                continue
                ;;
            *)
                ;;
        esac

        bad_leak="$(strip_comments "$file" | grep -nE '\b(BLE_NIMBLE|BLE_BLUEDROID|CONFIG_BT_NIMBLE_[A-Z0-9_]*|CONFIG_BT_BLUEDROID_[A-Z0-9_]*)\b|[<"][^">]*\.(nimble|bluedroid)\.h[">]' || true)"
        if [ -n "$bad_leak" ]; then
            echo "ERROR: ${label}/${rel} leaks a backend macro or header into a shared unit:"
            indent "$bad_leak"
            status=1
        fi
    done < <(find "$src_dir" -type f \( -name '*.h' -o -name '*.c' -o -name '*.cpp' \) -print0)
}

check_surface "BLE" "${BLE_SRC_DIR}/BLE.h" "${BLE_SRC_DIR}"
check_shared_units "BLE" "${BLE_SRC_DIR}"

if [ -n "${AUDIO_SRC_DIR}" ]; then
    check_surface "BLEAudio" "${AUDIO_SRC_DIR}/BLEAudio.h" "${AUDIO_SRC_DIR}"
    check_shared_units "BLEAudio" "${AUDIO_SRC_DIR}"

    if [ -d "${AUDIO_SRC_DIR}/engine" ]; then
        bad_engine_cpp="$(find "${AUDIO_SRC_DIR}/engine" -type f -name '*.cpp' || true)"
        if [ -n "$bad_engine_cpp" ]; then
            echo "ERROR: BLEAudio/src/engine must remain pure C (.c/.h only), found .cpp file(s):"
            indent "$bad_engine_cpp"
            status=1
        fi
    fi
fi

if [ "$status" -eq 0 ]; then
    echo "OK: BLE and BLEAudio public headers and shared units are backend-isolated."
fi

exit "$status"
