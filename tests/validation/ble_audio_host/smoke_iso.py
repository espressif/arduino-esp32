#!/usr/bin/env python3
"""Power on the local HCI via Bumble and print ISO roles + HCI ISO buffers."""

import asyncio
import sys

import le_audio_host_lib as le


async def main() -> int:
    print("transport", le.default_transport())
    async with le.bumble_session() as device:
        roles = sorted(le.controller_iso_roles(device))
        iso_len = getattr(device, "bleah_iso_hci_len", 0)
        iso_n = getattr(device, "bleah_iso_hci_packets", 0)
        q = getattr(device.host, "iso_packet_queue", None)
        print("iso_roles", roles)
        print(f"LE Buffer Size V2 iso_len={iso_len} iso_packets={iso_n}")
        print("iso_packet_queue", None if q is None else "ready")
        ok = le.controller_iso_hci_ok(device)
        print("iso_hci_ok", ok)
        if not ok:
            print(
                "FAIL: ISO HCI data buffers are zero — unicast_audio / "
                "broadcast_audio will skip",
                file=sys.stderr,
            )
            return 1
        missing = {"cis-central", "sync-receiver"} - set(roles)
        if missing:
            print(f"WARN: missing ISO roles {sorted(missing)}", file=sys.stderr)
        return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
