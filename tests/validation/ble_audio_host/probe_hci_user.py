#!/usr/bin/env python3
"""Probe / prepare HCI_CHANNEL_USER. Run: sudo python3 probe_hci_user.py

EBUSY with flags=0x0 and bdaddr 00:00:… usually means HCI_SETUP/HCI_CONFIG is
stuck (those bits are not in HCIGETDEVINFO). Fix: HCIDEVUP → wait → HCIDEVDOWN.
"""

from __future__ import annotations

import ctypes
import fcntl
import os
import socket
import struct
import sys
import time

AF_BLUETOOTH = getattr(socket, "AF_BLUETOOTH", 31)
BTPROTO_HCI = getattr(socket, "BTPROTO_HCI", 1)
HCI_CHANNEL_USER = 1
HCIDEVUP = 0x400448C9
HCIDEVDOWN = 0x400448CA
FLAG_NAMES = [
    "UP",
    "INIT",
    "RUNNING",
    "PSCAN",
    "ISCAN",
    "AUTH",
    "ENCRYPT",
    "INQUIRY",
    "RAW",
]


def _ioctl(req: int, idx: int) -> None:
    s = socket.socket(AF_BLUETOOTH, socket.SOCK_RAW | socket.SOCK_CLOEXEC, BTPROTO_HCI)
    try:
        fcntl.ioctl(s.fileno(), req, idx)
    finally:
        s.close()


def get_dev_info(idx: int) -> dict:
    di = bytearray(128)
    struct.pack_into("<H", di, 0, idx)
    s = socket.socket(AF_BLUETOOTH, socket.SOCK_RAW | socket.SOCK_CLOEXEC, BTPROTO_HCI)
    try:
        last = None
        for ioc in (
            (2 << 30) | (4 << 16) | (ord("H") << 8) | 211,
            (2 << 30) | (92 << 16) | (ord("H") << 8) | 211,
        ):
            try:
                fcntl.ioctl(s.fileno(), ioc, di)
                last = None
                break
            except OSError as e:
                last = e
        if last:
            raise last
    finally:
        s.close()
    flags = struct.unpack_from("<I", di, 16)[0]
    return {
        "dev_id": struct.unpack_from("<H", di, 0)[0],
        "name": di[2:10].split(b"\0", 1)[0].decode(),
        "bdaddr": ":".join(f"{b:02x}" for b in di[10:16]),
        "flags": flags,
        "flag_bits": [FLAG_NAMES[i] for i in range(len(FLAG_NAMES)) if flags & (1 << i)],
    }


def try_user_bind(idx: int) -> bool:
    s = socket.socket(AF_BLUETOOTH, socket.SOCK_RAW | socket.SOCK_CLOEXEC, BTPROTO_HCI)
    libc = ctypes.CDLL("libc.so.6", use_errno=True)
    addr = struct.pack("<HHH", AF_BLUETOOTH, idx, HCI_CHANNEL_USER)
    buf = ctypes.create_string_buffer(addr)
    ctypes.set_errno(0)
    rc = libc.bind(s.fileno(), buf, len(addr))
    err = ctypes.get_errno()
    print(f"USER bind hci{idx}: rc={rc} errno={err} ({os.strerror(err) if err else 'ok'})")
    s.close()
    return rc == 0


def main() -> int:
    idx = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    if not os.path.exists(f"/sys/class/bluetooth/hci{idx}"):
        print(f"hci{idx} missing")
        return 1

    print("=== initial ===")
    print(get_dev_info(idx))

    print("=== HCIDEVUP (complete SETUP/CONFIG) ===")
    try:
        _ioctl(HCIDEVUP, idx)
        print("HCIDEVUP ok")
    except OSError as e:
        print(f"HCIDEVUP: {e}")

    for _ in range(40):
        info = get_dev_info(idx)
        if (info["flags"] & 1) or info["bdaddr"].replace(":", "") != "000000000000":
            print("settled:", info)
            break
        time.sleep(0.25)
    else:
        print("settle timeout:", get_dev_info(idx))

    print("=== HCIDEVDOWN ===")
    try:
        _ioctl(HCIDEVDOWN, idx)
        print("HCIDEVDOWN ok")
    except OSError as e:
        print(f"HCIDEVDOWN: {e}")
    time.sleep(0.5)
    print("after down:", get_dev_info(idx))

    ok = try_user_bind(idx)
    print("=== after bind ===")
    print(get_dev_info(idx))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
