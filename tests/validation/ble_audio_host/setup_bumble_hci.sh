#!/usr/bin/env bash
# Free the AIC8800 (or other) USB Bluetooth adapter for Bumble hci-socket.
#
# AIC8800D80 (368b:8d81): aic_load_fw must upload BT firmware before btusb HCI
# works. HCIDEVUP errno 110 + bdaddr 00:00:… means firmware is missing/wedged —
# recover with a full USB re-enumerate (or physically replug the dongle).
#
# Usage:
#   sudo ./setup_bumble_hci.sh --once
#   sudo ./setup_bumble_hci.sh --undo
#   sudo python3 ./probe_hci_user.py

set -euo pipefail

DEFAULT_TRANSPORT="hci-socket:0"
TRANSPORT="${BUMBLE_TRANSPORT:-$DEFAULT_TRANSPORT}"
AIC_VID="368b"
AIC_PID="8d81"
ONCE=0
for arg in "$@"; do
  case "$arg" in
    --once) ONCE=1 ;;
    --help|-h)
      sed -n '2,14p' "$0"
      exit 0
      ;;
  esac
done

hci_index_from_transport() {
  case "$TRANSPORT" in
    hci-socket:*) echo "${TRANSPORT#hci-socket:}" ;;
    linux:hci*) echo "${TRANSPORT#linux:hci}" ;;
    *) echo 0 ;;
  esac
}

if [[ "$TRANSPORT" == usb:* ]]; then
  echo "Note: raw usb: hangs on HCI Reset until AIC firmware is loaded."
  echo "      Using ${DEFAULT_TRANSPORT} after firmware recovery."
  TRANSPORT="$DEFAULT_TRANSPORT"
fi

HCI_INDEX="$(hci_index_from_transport)"
HCI_INDEX="${HCI_INDEX:-0}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

find_aic_usb() {
  local d
  for d in /sys/bus/usb/devices/*; do
    [[ -f "$d/idVendor" && -f "$d/idProduct" ]] || continue
    if [[ "$(cat "$d/idVendor")" == "$AIC_VID" && "$(cat "$d/idProduct")" == "$AIC_PID" ]]; then
      basename "$d"
      return 0
    fi
  done
  return 1
}

stop_user_audio_bluetooth() {
  if [[ -z "${SUDO_USER:-}" ]]; then
    return 0
  fi
  echo "Stopping user WirePlumber/PipeWire Bluetooth clients..."
  sudo -u "$SUDO_USER" systemctl --user stop wireplumber pipewire pipewire-pulse 2>/dev/null || true
  mkdir -p /run/ble_audio_host
  echo 1 >/run/ble_audio_host/stopped_user_audio
}

restore_user_audio() {
  if [[ ! -f /run/ble_audio_host/stopped_user_audio ]]; then
    return 0
  fi
  rm -f /run/ble_audio_host/stopped_user_audio
  if [[ -n "${SUDO_USER:-}" ]]; then
    echo "Restarting user PipeWire/WirePlumber..."
    sudo -u "$SUDO_USER" systemctl --user start pipewire wireplumber pipewire-pulse 2>/dev/null || true
  fi
}

# Re-enumerate the AIC combo stick so aic_load_fw uploads BT firmware again.
reset_aic_usb() {
  local usb
  usb="$(find_aic_usb)" || {
    echo "AIC ${AIC_VID}:${AIC_PID} not found on USB."
    return 1
  }
  echo "Re-enumerating USB ${usb} (aic_load_fw must reload BT firmware)..."
  # Drop BT interfaces first so a dead hci0 goes away.
  for iface in /sys/bus/usb/devices/"${usb}":*; do
    [[ -e "$iface/driver" ]] || continue
    local drv
    drv="$(basename "$(readlink -f "$iface/driver")")"
    if [[ "$drv" == "btusb" ]]; then
      echo "  unbind $(basename "$iface") from btusb"
      echo "$(basename "$iface")" >"/sys/bus/usb/drivers/btusb/unbind" 2>/dev/null || true
    fi
  done
  sleep 0.3

  if [[ -e "/sys/bus/usb/devices/${usb}/authorized" ]]; then
    echo 0 >"/sys/bus/usb/devices/${usb}/authorized"
    sleep 1
    echo 1 >"/sys/bus/usb/devices/${usb}/authorized"
  elif [[ -e "/sys/bus/usb/devices/${usb}/reset" ]]; then
    echo 0 >"/sys/bus/usb/devices/${usb}/reset" 2>/dev/null \
      || echo 1 >"/sys/bus/usb/devices/${usb}/reset" 2>/dev/null || true
  else
    echo "No authorized/reset sysfs; unplug/replug the dongle manually."
    return 1
  fi

  echo "Waiting for hci${HCI_INDEX} after firmware reload..."
  local i
  for i in $(seq 1 40); do
    if [[ -e "/sys/class/bluetooth/hci${HCI_INDEX}" ]]; then
      echo "hci${HCI_INDEX} is back (after ${i}00ms-ish)."
      sleep 1
      return 0
    fi
    sleep 0.25
  done
  echo "WARNING: hci${HCI_INDEX} did not reappear — physically unplug/replug the dongle."
  return 1
}

bring_hci_ready() {
  local idx="$1"
  if [[ ! -e "/sys/class/bluetooth/hci${idx}" ]]; then
    echo "hci${idx} missing — trying USB re-enumerate..."
    reset_aic_usb || true
  fi
  if [[ ! -e "/sys/class/bluetooth/hci${idx}" ]]; then
    echo "hci${idx} still missing."
    return 1
  fi

  # Dead HCI (no BDADDR) → reset before the long HCIDEVUP timeout.
  if python3 - "$idx" <<'PY'
import fcntl, socket, struct, sys
idx = int(sys.argv[1])
di = bytearray(128)
struct.pack_into("<H", di, 0, idx)
s = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_RAW | socket.SOCK_CLOEXEC, socket.BTPROTO_HCI)
try:
    fcntl.ioctl(s.fileno(), (2 << 30) | (4 << 16) | (ord("H") << 8) | 211, di)
finally:
    s.close()
addr = di[10:16]
sys.exit(0 if any(addr) else 1)
PY
  then
    :
  else
    echo "hci${idx} has no BDADDR (firmware dead) — re-enumerating USB first..."
    reset_aic_usb || true
  fi

  echo "Preparing hci${idx} for HCI_CHANNEL_USER (UP → DOWN → USER bind)..."
  if python3 "${SCRIPT_DIR}/probe_hci_user.py" "${idx}"; then
    return 0
  fi

  echo "Probe failed. Resetting AIC USB and retrying..."
  reset_aic_usb || true
  if [[ ! -e "/sys/class/bluetooth/hci${idx}" ]]; then
    return 1
  fi
  python3 "${SCRIPT_DIR}/probe_hci_user.py" "${idx}" || {
    echo "ERROR: HCI still dead after reset."
    echo "  Physically unplug and replug the AIC8800 dongle, then re-run:"
    echo "    sudo $0 --once"
    return 1
  }
}

undo() {
  echo "Restoring bluetooth.service + user audio..."
  systemctl unmask --runtime bluetooth.service 2>/dev/null || true
  systemctl start bluetooth.service 2>/dev/null || true
  restore_user_audio
}

if [[ "${1:-}" == "--undo" ]]; then
  undo
  exit 0
fi

# Clear undo trap early when using --once so a failed probe cannot restore BlueZ
# mid-diagnosis; we only undo on unexpected script death before --once completes
# if ONCE=0.
trap undo EXIT

echo "Stopping and runtime-masking bluetooth.service..."
systemctl stop bluetooth.service 2>/dev/null || true
systemctl mask --runtime bluetooth.service
sleep 0.3

if pids=$(pidof bluetoothd 2>/dev/null || true); [[ -n "${pids:-}" ]]; then
  echo "Killing leftover bluetoothd: $pids"
  # shellcheck disable=SC2086
  kill -TERM $pids 2>/dev/null || true
  sleep 0.4
  if pids=$(pidof bluetoothd 2>/dev/null || true); [[ -n "${pids:-}" ]]; then
    # shellcheck disable=SC2086
    kill -KILL $pids 2>/dev/null || true
  fi
fi

stop_user_audio_bluetooth

# Disarm undo before probe when --once: probe failure must not bring BlueZ back.
if [[ "$ONCE" -eq 1 ]]; then
  trap - EXIT
fi

bring_hci_ready "$HCI_INDEX"

echo "BUMBLE_TRANSPORT=${TRANSPORT}"
echo "Export for the suite:"
echo "  export BUMBLE_TRANSPORT=${TRANSPORT}"
echo "Do not start BlueZ / WirePlumber while the suite runs."
echo "Open the transport as root (sudo -E pytest ...)."
echo "When finished: sudo $0 --undo"

if [[ "$ONCE" -eq 1 ]]; then
  exit 0
fi

trap undo EXIT
echo "Press Ctrl-C when the suite is finished (will restore bluetooth + WP)."
while true; do sleep 3600; done
