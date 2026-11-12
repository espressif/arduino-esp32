#!/usr/bin/env bash
# Run as root (pkexec/sudo). Prepares nothing — call setup_bumble_hci.sh --once first.
set -euo pipefail
export HOME="${HOME:-/home/lucassvaz}"
export ESPPORT="${ESPPORT:-/dev/ttyUSB0}"
export BUMBLE_TRANSPORT="${BUMBLE_TRANSPORT:-hci-socket:0}"
export BLE_AUDIO_PHONE="${BLE_AUDIO_PHONE:-1}"
export PYTHONUNBUFFERED=1

HOST_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HOST_DIR/../../.." && pwd)"
VENV="$HOST_DIR/.venv/bin"
BUILD_DIR="$HOME/.arduino/tests/esp32s31/ble_audio_host/build.tmp"

cd "$HOST_DIR"
echo "=== smoke ISO ==="
"$VENV/python" "$HOST_DIR/smoke_iso.py"

cd "$REPO"
echo "=== pytest ble_audio_host ==="
# Invoke via the venv python — the pytest console script shebang may still
# point at a relocated tree (this venv was created under ~/arduino-esp32).
exec "$VENV/python" -m pytest -s \
  "$HOST_DIR/test_ble_audio_host.py" \
  --build-dir "$BUILD_DIR" \
  --embedded-services esp,arduino \
  --skip-autoflash y \
  --tb=short
