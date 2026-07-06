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
# Backend-isolation guard for the public BLE API (review item 14).
#
# The public headers (libraries/BLE/src/BLE*.h) must never leak backend detail:
#   * they must NOT include any backend selector / backend Impl header
#     (impl/nimble/*, impl/bluedroid/*, impl/BLEBackend.h);
#   * they must NOT name native stack types (esp_ble_*, ble_gap_*, ble_hs_*,
#     ble_uuid_*) or per-type backend Impl structs in actual code.
#
# Comment lines (// ... and Doxygen ' * ...') are ignored so the invariant can
# be described in prose without tripping the guard.
#
# Exit status: 0 = clean, 1 = violation(s) found.
# --------------------------------------------------------------------------

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "${SCRIPT_DIR}/../src" && pwd)"

status=0

# Strip block comments, line comments and pure Doxygen lines so the token scan
# only sees real code. (awk keeps this dependency-free and portable.)
strip_comments() {
  awk '
    { line = $0 }
    # drop /* ... */ that open and close on the same line
    { gsub(/\/\*.*\*\//, "", line) }
    in_block {
      if (line ~ /\*\//) { sub(/^.*\*\//, "", line); in_block = 0 }
      else { next }
    }
    /\/\*/ { sub(/\/\*.*$/, "", line); in_block = 1 }
    { sub(/\/\/.*$/, "", line) }         # trailing // comments
    { print line }
  ' "$1"
}

for hdr in "${SRC_DIR}"/BLE*.h; do
  [ -e "$hdr" ] || continue
  name="$(basename "$hdr")"

  # 1) Forbidden includes (checked on raw #include lines).
  bad_inc="$(grep -nE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"](impl/(nimble|bluedroid)/|impl/BLEBackend\.h)' "$hdr" || true)"
  if [ -n "$bad_inc" ]; then
    echo "ERROR: ${name} includes a backend/selector header (public API must stay backend-agnostic):"
    echo "$bad_inc" | sed 's/^/    /'
    status=1
  fi

  # 2) Forbidden tokens in real code (comments stripped first).
  bad_tok="$(strip_comments "$hdr" | grep -nE 'esp_ble_|ble_gap_|ble_hs_|ble_uuid_|::Impl\b|Nimble[A-Za-z]*Impl|Bluedroid[A-Za-z]*Impl' || true)"
  if [ -n "$bad_tok" ]; then
    echo "ERROR: ${name} names a backend-specific type in code (public API must stay backend-agnostic):"
    echo "$bad_tok" | sed 's/^/    /'
    status=1
  fi
done

if [ "$status" -eq 0 ]; then
  echo "OK: public BLE headers are backend-agnostic."
fi

exit "$status"
