#!/usr/bin/env bash
# Root wrapper: free HCI for Bumble, then run the host suite.
set -euo pipefail
export HOME="${HOME:-/home/lucassvaz}"
export ESPPORT="${ESPPORT:-/dev/ttyUSB0}"
export BUMBLE_TRANSPORT="${BUMBLE_TRANSPORT:-hci-socket:0}"
export BLE_AUDIO_PHONE="${BLE_AUDIO_PHONE:-1}"
export PYTHONUNBUFFERED=1
HOST_DIR="$(cd "$(dirname "$0")" && pwd)"
chmod 666 /dev/ttyUSB0 2>/dev/null || true
"$HOST_DIR/setup_bumble_hci.sh" --once
exec "$HOST_DIR/run_host_suite.sh"
