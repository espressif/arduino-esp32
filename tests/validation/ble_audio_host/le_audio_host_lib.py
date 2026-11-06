"""
Host-side helpers for the LE Audio host-interop suite.

Not a test module (no ``test_`` prefix, so pytest does not collect it). Groups
the Bluetooth SIG UUID tables, Bumble GATT helpers for the control-plane phases,
Auracast announcement scan, and the Bumble ASCS/CIS/BIS + LC3 plumbing for the
audio data-plane phases.

Everything here is written for a Linux host with an LE-Audio-capable HCI
controller owned exclusively by Bumble (BlueZ must be stopped/masked; see
``setup_bumble_hci.sh`` and README). Helpers degrade to a clear "unavailable"
result where possible so the test file can turn a missing dependency into a
pytest SKIP rather than a hard failure.
"""

from __future__ import annotations

import asyncio
import errno
import functools
import logging
import math
import os
import socket
import struct
import subprocess
import tempfile
import time
import wave
from contextlib import asynccontextmanager
from typing import Any

LOGGER = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Bluetooth SIG UUIDs (16-bit, expanded to the full base UUID)
# ---------------------------------------------------------------------------

_BASE = "0000{:04x}-0000-1000-8000-00805f9b34fb"


def uuid16(v: int) -> str:
    """Expand a 16-bit SIG UUID to its full 128-bit string form."""
    return _BASE.format(v)


# LE Audio service UUIDs.
SVC = {
    "PACS": uuid16(0x1850),  # Published Audio Capabilities (BAP)
    "ASCS": uuid16(0x184E),  # Audio Stream Control (BAP unicast)
    "BASS": uuid16(0x184F),  # Broadcast Audio Scan (Scan Delegator)
    "VCS": uuid16(0x1844),  # Volume Control (VCP)
    "MICS": uuid16(0x184D),  # Microphone Control (MICP)
    "MCS": uuid16(0x1848),  # Media Control (MCP)
    "GMCS": uuid16(0x1849),  # Generic Media Control
    "TBS": uuid16(0x184B),  # Telephone Bearer (CCP)
    "GTBS": uuid16(0x184C),  # Generic Telephone Bearer
    "CSIS": uuid16(0x1846),  # Coordinated Set Identification (CSIP)
    "CAS": uuid16(0x1853),  # Common Audio (CAP)
    "HAS": uuid16(0x1854),  # Hearing Access
    "TMAS": uuid16(0x1855),  # Telephony and Media Audio (TMAP)
    "GMAS": uuid16(0x1858),  # Gaming Audio (GMAP)
    "BCAST_ANNOUNCE": uuid16(0x1852),  # Broadcast Audio Announcement (adv)
    "BASIC_ANNOUNCE": uuid16(0x1851),  # Basic Audio Announcement (periodic)
    "PBA": uuid16(0x1856),  # Public Broadcast Announcement (adv)
}

# Characteristic UUIDs used by the control-plane phases.
CHR = {
    # VCS
    "VOL_STATE": uuid16(0x2B7D),
    "VOL_CP": uuid16(0x2B7E),
    "VOL_FLAGS": uuid16(0x2B7F),
    # First characteristic of the services VCS/MICS include, used to tell a
    # fault in an included service apart from one in its including service.
    "AICS_STATE": uuid16(0x2B77),
    "AICS_CP": uuid16(0x2B7B),
    # Write-without-response only, and readable, so a write can be confirmed by
    # reading the value back instead of trusting the write's own status.
    "AICS_DESCRIPTION": uuid16(0x2B7C),
    "VOCS_STATE": uuid16(0x2B80),
    "VOCS_CP": uuid16(0x2B82),
    "VOCS_DESCRIPTION": uuid16(0x2B83),
    # MICS
    "MIC_MUTE": uuid16(0x2BC3),
    # PACS
    "SINK_PAC": uuid16(0x2BC9),
    "SINK_LOC": uuid16(0x2BCA),
    "SOURCE_PAC": uuid16(0x2BCB),
    "SOURCE_LOC": uuid16(0x2BCC),
    "AVAIL_CTX": uuid16(0x2BCD),
    "SUPP_CTX": uuid16(0x2BCE),
    # ASCS
    "ASE_SINK": uuid16(0x2BC4),
    "ASE_SOURCE": uuid16(0x2BC5),
    "ASE_CP": uuid16(0x2BC6),
    # BASS
    "BASS_CP": uuid16(0x2BC7),
    "BASS_RX_STATE": uuid16(0x2BC8),
    # CSIS
    "SIRK": uuid16(0x2B84),
    "SET_SIZE": uuid16(0x2B85),
    "SET_LOCK": uuid16(0x2B86),
    "RANK": uuid16(0x2B87),
    # MCS
    "MEDIA_PLAYER_NAME": uuid16(0x2B93),
    "MEDIA_STATE": uuid16(0x2BA3),
    "MEDIA_CP": uuid16(0x2BA4),
    # TBS / GTBS
    "BEARER_PROVIDER": uuid16(0x2BB3),
    "CALL_STATE": uuid16(0x2BBD),
    "CALL_CP": uuid16(0x2BBE),
    # HAS
    "HA_FEATURES": uuid16(0x2BDA),
    "HA_PRESET_CP": uuid16(0x2BDB),
    "HA_ACTIVE_PRESET": uuid16(0x2BDC),
    # TMAS / GMAS
    "TMAP_ROLE": uuid16(0x2B51),
    "GMAP_ROLE": uuid16(0x2C00),
}

# --- Volume Control Point opcodes (VCP) ---
VCP_OP_SET_ABS_VOL = 0x04
VCP_OP_UNMUTE = 0x05
VCP_OP_MUTE = 0x06

# --- Media Control Point opcodes (MCP) ---
MCP_OP_PLAY = 0x01
MCP_OP_PAUSE = 0x02

# --- Call Control Point opcodes (CCP) ---
CCP_OP_ACCEPT = 0x00
CCP_OP_TERMINATE = 0x01
CCP_OP_ORIGINATE = 0x04

# --- Hearing Aid Preset Control Point opcodes (HAS) ---
# 0x04 is Write Preset Name, not Set Active Preset -- sending 0x04 with a bare
# index makes the server read a zero-length name and answer 0x84.
HAS_OP_READ_PRESETS = 0x01
HAS_OP_WRITE_PRESET_NAME = 0x04
HAS_OP_SET_ACTIVE = 0x05
HAS_OP_SET_NEXT = 0x06
HAS_OP_SET_PREV = 0x07

# LC3_16_2_1 (matches the DUT's PACS: BLEAudioUnicastServer defaults)
LC3_SAMPLE_RATE_HZ = 16000
LC3_FRAME_DURATION_US = 10000
LC3_OCTETS_PER_FRAME = 40
LC3_FRAMES_PER_SDU = 1
# AIC8800 needs btusb firmware — use hci-socket, not raw usb:VID:PID.
DEFAULT_TRANSPORT = "hci-socket:0"


def default_transport() -> str:
    return os.environ.get("BUMBLE_TRANSPORT", DEFAULT_TRANSPORT)


def bumble_available() -> bool:
    try:
        import bumble  # noqa: F401
        import lc3  # noqa: F401

        return True
    except Exception:
        return False


def adapter_present() -> bool:
    """True if a Bluetooth HCI node or the reference AIC8800 USB dongle is present."""
    from pathlib import Path

    bt = Path("/sys/class/bluetooth")
    try:
        if any(p.name.startswith("hci") for p in bt.iterdir()):
            return True
    except OSError:
        pass
    # USB path used when btusb is unbound for Bumble.
    try:
        for vendor in Path("/sys/bus/usb/devices").glob("*/idVendor"):
            if vendor.read_text(encoding="utf-8").strip() != "368b":
                continue
            prod = vendor.with_name("idProduct")
            if prod.exists() and prod.read_text(encoding="utf-8").strip() == "8d81":
                return True
    except OSError:
        pass
    return False


def _norm_uuid(u) -> str:
    """Normalize a Bumble UUID or string to lowercase dashed 128-bit form."""
    from bumble.core import UUID

    if isinstance(u, UUID):
        h = u.to_hex_str().lower().replace("-", "")
    else:
        h = str(u).lower().replace("-", "")
    if len(h) == 4:
        h = f"0000{h}00001000800000805f9b34fb"
    if len(h) == 32:
        return f"{h[0:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:32]}"
    return str(u).lower()


def _as_uuid(s: str):
    from bumble.core import UUID

    return UUID(s)


# ---------------------------------------------------------------------------
# Bumble HCI session + scan / connect / pair
# ---------------------------------------------------------------------------


def _hci_index_from_transport(transport: str) -> int | None:
    if transport.startswith("hci-socket:"):
        try:
            return int(transport.split(":", 1)[1] or "0")
        except ValueError:
            return 0
    if transport.startswith("linux:hci"):
        try:
            return int(transport.replace("linux:hci", "") or "0")
        except ValueError:
            return 0
    return None


# linux/bluetooth/hci.h
_HCIDEVUP = 0x400448C9
_HCIDEVDOWN = 0x400448CA


def hci_dev_info(hci_index: int = 0) -> dict[str, Any] | None:
    """Return HCIGETDEVINFO fields (flags bit names) or None on failure."""
    import fcntl

    flag_names = [
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
    di = bytearray(128)
    struct.pack_into("<H", di, 0, hci_index)
    try:
        sock = socket.socket(
            getattr(socket, "AF_BLUETOOTH", 31),
            socket.SOCK_RAW | socket.SOCK_CLOEXEC,
            getattr(socket, "BTPROTO_HCI", 1),
        )
    except OSError as e:
        LOGGER.warning("hci_dev_info: socket: %s", e)
        return None
    try:
        last: OSError | None = None
        for ioc in (
            (2 << 30) | (4 << 16) | (ord("H") << 8) | 211,
            (2 << 30) | (92 << 16) | (ord("H") << 8) | 211,
        ):
            try:
                fcntl.ioctl(sock.fileno(), ioc, di)
                last = None
                break
            except OSError as e:
                last = e
        if last is not None:
            LOGGER.warning("hci_dev_info: ioctl: %s", last)
            return None
    finally:
        sock.close()
    flags = struct.unpack_from("<I", di, 16)[0]
    bits = [flag_names[i] for i in range(len(flag_names)) if flags & (1 << i)]
    bdaddr = ":".join(f"{b:02x}" for b in di[10:16])
    name = di[2:10].split(b"\0", 1)[0].decode(errors="replace")
    return {"flags": flags, "bits": bits, "bdaddr": bdaddr, "name": name}


def _hci_ioctl(req: int, hci_index: int) -> None:
    import fcntl

    sock = socket.socket(
        getattr(socket, "AF_BLUETOOTH", 31),
        socket.SOCK_RAW | socket.SOCK_CLOEXEC,
        getattr(socket, "BTPROTO_HCI", 1),
    )
    try:
        fcntl.ioctl(sock.fileno(), req, hci_index)
    finally:
        sock.close()


def hci_dev_up(hci_index: int = 0) -> bool:
    """HCIDEVUP — finishes kernel SETUP/CONFIG so USER channel can bind later."""
    node = f"/sys/class/bluetooth/hci{hci_index}"
    if not os.path.exists(node):
        return False
    LOGGER.info("hci_dev_up: bringing hci%d UP (complete SETUP)", hci_index)
    try:
        _hci_ioctl(_HCIDEVUP, hci_index)
        LOGGER.info("hci_dev_up: HCIDEVUP ok for hci%d", hci_index)
        return True
    except OSError as e:
        # EALREADY = already up
        if getattr(e, "errno", None) in (errno.EALREADY, errno.EBUSY):
            LOGGER.info("hci_dev_up: hci%d already up/busy (%s)", hci_index, e)
            return True
        LOGGER.warning("hci_dev_up: %s", e)
        return False


def hci_dev_down(hci_index: int = 0) -> bool:
    """Bring hciN DOWN so HCI_CHANNEL_USER can bind (Bumble requirement)."""
    node = f"/sys/class/bluetooth/hci{hci_index}"
    if not os.path.exists(node):
        LOGGER.warning("hci_dev_down: hci%d missing", hci_index)
        return False
    LOGGER.info("hci_dev_down: bringing hci%d DOWN", hci_index)
    ok = False
    try:
        _hci_ioctl(_HCIDEVDOWN, hci_index)
        ok = True
        LOGGER.info("hci_dev_down: HCIDEVDOWN ioctl ok for hci%d", hci_index)
    except OSError as e:
        LOGGER.warning("hci_dev_down: ioctl failed: %s", e)

    info = hci_dev_info(hci_index)
    if info is not None:
        LOGGER.info(
            "hci_dev_down: flags=0x%x %s bdaddr=%s",
            info["flags"],
            info["bits"] or ["(none)"],
            info["bdaddr"],
        )
        if info["flags"] & 1:
            LOGGER.warning("hci_dev_down: HCI_UP still set")
            ok = False
    time.sleep(0.2)
    return ok


def hci_prepare_user_channel(hci_index: int = 0, settle_s: float = 8.0) -> bool:
    """Clear stuck HCI_SETUP/CONFIG so HCI_CHANNEL_USER bind succeeds.

    Kernel returns EBUSY for USER bind while HCI_SETUP/HCI_CONFIG are set in
    the *persistent* flag word (invisible via HCIGETDEVINFO's ``flags``). A
    never-powered controller also shows bdaddr 00:00:00:00:00:00. Powering UP
    once completes setup; then DOWN for Bumble.
    """
    info = hci_dev_info(hci_index)
    if info is not None:
        LOGGER.info(
            "hci_prepare: before flags=0x%x %s bdaddr=%s",
            info["flags"],
            info["bits"] or ["(none)"],
            info["bdaddr"],
        )

    hci_dev_up(hci_index)
    deadline = time.time() + settle_s
    while time.time() < deadline:
        info = hci_dev_info(hci_index)
        if info is None:
            time.sleep(0.2)
            continue
        up = bool(info["flags"] & 1)
        raw_zero = info["bdaddr"].replace(":", "") == "000000000000"
        if up or not raw_zero:
            LOGGER.info(
                "hci_prepare: settled flags=0x%x %s bdaddr=%s",
                info["flags"],
                info["bits"] or ["(none)"],
                info["bdaddr"],
            )
            break
        time.sleep(0.25)
    else:
        LOGGER.warning("hci_prepare: settle timeout; continuing to DOWN anyway")

    ok = hci_dev_down(hci_index)
    # AUTO_OFF / SETUP teardown can need a beat before USER bind.
    time.sleep(0.5)
    return ok


def rebind_hci_usb(hci_index: int = 0) -> bool:
    """Last-resort: unbind/bind btusb (reloads firmware). Prefer hci_dev_down."""
    dev = f"/sys/class/bluetooth/hci{hci_index}/device"
    if not os.path.exists(dev):
        LOGGER.warning("rebind: hci%d device node missing", hci_index)
        return False
    try:
        usb_iface = os.path.basename(os.path.realpath(dev))
        driver = os.path.realpath(os.path.join(dev, "driver"))
        unbind = os.path.join(driver, "unbind")
        bind = os.path.join(driver, "bind")
        if not os.access(unbind, os.W_OK):
            LOGGER.warning("rebind: no write access to %s (need root)", unbind)
            return False
        LOGGER.info("rebind: unbinding %s from %s", usb_iface, os.path.basename(driver))
        with open(unbind, "w", encoding="utf-8") as f:
            f.write(usb_iface)
        time.sleep(0.5)
        with open(bind, "w", encoding="utf-8") as f:
            f.write(usb_iface)
        time.sleep(1.0)
        hci_dev_down(hci_index)
        return os.path.exists(f"/sys/class/bluetooth/hci{hci_index}")
    except OSError as e:
        LOGGER.warning("rebind failed: %s", e)
        return False


@asynccontextmanager
async def bumble_session(transport: str | None = None):
    """Open exclusive HCI, power on a CIS-capable Device, yield it, then close."""
    from bumble.device import Device, DeviceConfiguration
    from bumble.hci import Address
    from bumble.pairing import PairingConfig, PairingDelegate
    from bumble.transport import open_transport

    t = transport or default_transport()
    # Raw USB skips AIC firmware load → HCI Reset hangs.
    if t.startswith("usb:"):
        LOGGER.warning(
            "transport %s skips btusb firmware on AIC8800; using hci-socket:0", t
        )
        t = "hci-socket:0"
    hci_index = _hci_index_from_transport(t)

    async def _open(spec: str):
        LOGGER.info("bumble: opening transport %s", spec)
        return await asyncio.wait_for(open_transport(spec), timeout=15.0)

    if hci_index is not None:
        hci_prepare_user_channel(hci_index)

    try:
        hci_cm = await _open(t)
    except (OSError, asyncio.TimeoutError, Exception) as e:
        LOGGER.warning("bumble: open %s failed (%s); retry after prepare", t, e)
        info = hci_dev_info(hci_index) if hci_index is not None else None
        if info is not None:
            LOGGER.warning(
                "bumble: hci flags at fail: 0x%x %s bdaddr=%s",
                info["flags"],
                info["bits"] or ["(none)"],
                info["bdaddr"],
            )
        if hci_index is None:
            raise
        for attempt in range(3):
            hci_prepare_user_channel(hci_index)
            time.sleep(0.5 * (attempt + 1))
            try:
                hci_cm = await _open(t)
                break
            except (OSError, asyncio.TimeoutError, Exception) as e2:
                LOGGER.warning("bumble: retry %d open failed: %s", attempt + 1, e2)
        else:
            raise e

    try:
        async with hci_cm as hci_transport:
            # Fresh random identity each session: DUT may still hold an NVS bond for
            # a previous host address while MemoryKeyStore is empty → SMP disconnect.
            host_addr = Address.generate_static_address()
            config = DeviceConfiguration(
                name="BLEAH-Host",
                address=host_addr,
                keystore="MemoryKeyStore",
            )
            config.le_enabled = True
            config.cis_enabled = True
            config.eatt_enabled = False
            device = Device.from_config_with_hci(
                config, hci_transport.source, hci_transport.sink
            )
            device.pairing_config_factory = lambda _conn: PairingConfig(
                sc=True,
                mitm=False,
                bonding=True,
                delegate=PairingDelegate(
                    io_capability=PairingDelegate.IoCapability.NO_OUTPUT_NO_INPUT
                ),
            )
            LOGGER.info("bumble: power_on (addr=%s)", host_addr)
            try:
                await asyncio.wait_for(device.power_on(), timeout=15.0)
            except asyncio.TimeoutError as e:
                raise TimeoutError(
                    f"HCI power_on timed out on {t} "
                    "(controller not answering Reset — check firmware / transport)"
                ) from e
            LOGGER.info("bumble: powered on (%s)", t)
            await ensure_iso_ready(device)
            try:
                yield device
            finally:
                for conn in list(getattr(device, "connections", {}).values()):
                    try:
                        await conn.disconnect()
                    except Exception:  # noqa: BLE001
                        pass
    finally:
        # USER-channel close leaves adapter UP → next phase gets EBUSY.
        if hci_index is not None:
            hci_dev_down(hci_index)


def _hci_reason_name(reason) -> str:
    """Best-effort HCI disconnect reason label for logs."""
    try:
        code = int(reason)
    except (TypeError, ValueError):
        return repr(reason)
    known = {
        0x13: "REMOTE_USER_TERMINATED",
        0x16: "TERMINATED_BY_LOCAL_HOST",
        0x3E: "CONNECTION_FAILED_TO_BE_ESTABLISHED",
        0x08: "CONNECTION_TIMEOUT",
        0x3B: "UNACCEPTABLE_CONNECTION_PARAMETERS",
    }
    return f"0x{code:02x}/{known.get(code, 'UNKNOWN')}"


async def connect_and_pair(
    device,
    address,
    timeout: float = 20.0,
    attempts: int = 5,
    *,
    name: str | None = None,
):
    """LE connect + Just Works pair; returns the Connection.

    Retries on flaky ACL bring-up. HCI reason 0x3E (62,
    CONNECTION_FAILED_TO_BE_ESTABLISHED) is common on this bench — the
    controller creates a handle then drops the link before the connection is
    usable. Detect that *before* pair() (otherwise pair hangs), back off, and
    optionally re-scan by @p name between attempts.
    """
    from bumble.core import PhysicalTransport
    from bumble.hci import OwnAddressType

    last_err: BaseException | None = None
    peer = address
    for i in range(attempts):
        connection = None
        if i > 0 and name:
            found = await scan_by_name(device, name, timeout=min(12.0, timeout))
            if found is not None:
                peer = found
                LOGGER.info("connect_and_pair: re-scanned peer %s", peer)
            else:
                LOGGER.warning("connect_and_pair: re-scan missed %r; retrying %s", name, peer)

        try:
            LOGGER.info(
                "connect_and_pair: attempt %d/%d connect %s",
                i + 1,
                attempts,
                peer,
            )
            connection = await device.connect(
                peer,
                transport=PhysicalTransport.LE,
                own_address_type=OwnAddressType.RANDOM,
                timeout=timeout,
            )
            disc_reason: list[object] = []
            disc_event = asyncio.Event()

            def _on_disc(reason=None):
                disc_reason.append(reason)
                disc_event.set()
                # 0x16 = we tore the link down; only warn for unexpected drops.
                try:
                    code = int(reason) if reason is not None else -1
                except (TypeError, ValueError):
                    code = -1
                if code != 0x16:
                    LOGGER.warning(
                        "bumble: peer disconnected during connect/pair reason=%s",
                        _hci_reason_name(reason),
                    )

            connection.on("disconnection", _on_disc)

            # Watch for early 0x3E / other drops before starting SMP. pair()
            # registered after a silent drop will hang until cancelled.
            try:
                await asyncio.wait_for(disc_event.wait(), timeout=0.8)
            except asyncio.TimeoutError:
                pass
            else:
                reason = disc_reason[-1] if disc_reason else None
                last_err = ConnectionError(
                    f"early disconnect after connect reason={_hci_reason_name(reason)}"
                )
                LOGGER.warning(
                    "connect_and_pair attempt %d/%d: %s",
                    i + 1,
                    attempts,
                    last_err,
                )
                await asyncio.sleep(0.5 + 0.5 * i)
                continue

            try:
                await asyncio.wait_for(connection.pair(), timeout=15.0)
                LOGGER.info(
                    "bumble: paired (encrypted=%s)",
                    getattr(connection, "is_encrypted", "?"),
                )
            except asyncio.TimeoutError as e:
                last_err = e
                LOGGER.warning(
                    "pair() timed out attempt %d/%d disc=%s",
                    i + 1,
                    attempts,
                    [_hci_reason_name(r) for r in disc_reason],
                )
                try:
                    await asyncio.wait_for(connection.disconnect(), timeout=2.0)
                except Exception:  # noqa: BLE001
                    pass
                await asyncio.sleep(0.5 + 0.5 * i)
                continue
            except asyncio.CancelledError as e:
                reason = disc_reason[-1] if disc_reason else None
                LOGGER.warning(
                    "pair() cancelled attempt %d/%d disc=%s",
                    i + 1,
                    attempts,
                    [_hci_reason_name(r) for r in disc_reason],
                )
                if getattr(connection, "is_encrypted", False):
                    pass  # treat as success below
                else:
                    last_err = ConnectionError(
                        f"pair cancelled reason={_hci_reason_name(reason)}"
                    ) if reason is not None else e
                    # Dead handle (typical after 0x3E): skip HCI Disconnect.
                    if not disc_event.is_set():
                        try:
                            await asyncio.wait_for(connection.disconnect(), timeout=2.0)
                        except Exception:  # noqa: BLE001
                            pass
                    await asyncio.sleep(0.5 + 0.5 * i)
                    continue
            except Exception as e:  # noqa: BLE001
                LOGGER.warning("pair() raised %s (link may already be encrypted)", e)

            try:
                connection.remove_listener("disconnection", _on_disc)
            except Exception:  # noqa: BLE001
                pass

            # ATT MTU lives on Peer/gatt_client — Connection has no request_mtu.
            # Default MTU 23 only carries ~20-byte values; ASE Config Codec is
            # ~30 bytes and silently hangs without an Exchange MTU.
            from bumble.device import Peer as _Peer

            try:
                mtu = await _Peer(connection).request_mtu(517)
                LOGGER.info("bumble: ATT MTU %s", mtu)
                if mtu < 64:
                    raise ConnectionError(f"ATT MTU {mtu} too small for ASCS/ASE CP")
            except Exception as e:  # noqa: BLE001
                LOGGER.error("request_mtu failed: %s", e)
                last_err = e
                try:
                    await asyncio.wait_for(connection.disconnect(), timeout=2.0)
                except Exception:  # noqa: BLE001
                    pass
                await asyncio.sleep(0.5 + 0.5 * i)
                continue
            return connection
        except asyncio.CancelledError as e:
            last_err = e
            LOGGER.warning(
                "connect_and_pair attempt %d/%d cancelled: %s", i + 1, attempts, e
            )
            await asyncio.sleep(0.5 + 0.5 * i)
        except Exception as e:  # noqa: BLE001
            last_err = e
            LOGGER.warning(
                "connect_and_pair attempt %d/%d failed: %s", i + 1, attempts, e
            )
            if connection is not None:
                try:
                    await asyncio.wait_for(connection.disconnect(), timeout=2.0)
                except Exception:  # noqa: BLE001
                    pass
            await asyncio.sleep(0.5 + 0.5 * i)
    assert last_err is not None
    raise last_err


def controller_iso_roles(device) -> set[str]:
    """Return ISO role names supported by the powered-on controller."""
    from bumble.hci import LeFeature, LeFeatureMask

    roles: set[str] = set()
    checks = {
        "cis-central": LeFeature.CONNECTED_ISOCHRONOUS_STREAM_CENTRAL,
        "cis-peripheral": LeFeature.CONNECTED_ISOCHRONOUS_STREAM_PERIPHERAL,
        "iso-broadcaster": LeFeature.ISOCHRONOUS_BROADCASTER,
        "sync-receiver": LeFeature.SYNCHRONIZED_RECEIVER,
    }
    for name, feat in checks.items():
        try:
            if device.supports_le_features(LeFeatureMask(1 << int(feat))):
                roles.add(name)
        except Exception:  # noqa: BLE001
            pass
    return roles


async def ensure_iso_ready(device) -> None:
    """Enable CIS Host Support and record whether ISO HCI data is usable.

    AIC8800 / stock ``btusb`` quirks after ``power_on``:

    1. ``HCI_LE_Create_CIS`` returns ``COMMAND_DISALLOWED (0x0C)`` unless the
       Connected Isochronous Stream (Host Support) feature bit is set. Bumble
       only sets it when the controller advertises ``HCI_LE_Set_Host_Feature``;
       some firmwares omit that bit in the supported-commands mask but still
       accept (and need) the host-feature write — so we always attempt it once
       at power-on, before any ACL.
    2. ``LE_Read_Buffer_Size_V2`` may report ``iso_data_packet_length=0``. That
       is honest: the controller will establish CIS/BIG but reject HCI ISO data
       packets (``HCI_HARDWARE_ERROR`` while pumping; DUT PLC silence). Do **not**
       synthesize a fake ISO queue — mark the session and let audio phases skip.
    """
    from bumble.hci import (
        HCI_LE_SET_HOST_FEATURE_COMMAND,
        LeFeature,
        HCI_LE_Read_Buffer_Size_V2_Command,
        HCI_LE_Set_Host_Feature_Command,
        HCI_LE_READ_BUFFER_SIZE_V2_COMMAND,
    )

    host = device.host
    roles = controller_iso_roles(device)
    LOGGER.info("controller ISO roles: %s", roles)

    # Assert CIS Host Support before any ACL (required for Create CIS).
    try:
        await host.send_sync_command(
            HCI_LE_Set_Host_Feature_Command(
                bit_number=LeFeature.CONNECTED_ISOCHRONOUS_STREAM,
                bit_value=1,
            )
        )
        LOGGER.info(
            "bumble: Set_Host_Feature CIS Host Support=1 (cmd_supported=%s)",
            host.supports_command(HCI_LE_SET_HOST_FEATURE_COMMAND),
        )
    except Exception as e:  # noqa: BLE001
        LOGGER.warning("bumble: Set_Host_Feature CIS failed: %s", e)

    iso_len = 0
    iso_n = 0
    if host.supports_command(HCI_LE_READ_BUFFER_SIZE_V2_COMMAND):
        try:
            buf = await host.send_sync_command(HCI_LE_Read_Buffer_Size_V2_Command())
            iso_len = int(buf.iso_data_packet_length)
            iso_n = int(buf.total_num_iso_data_packets)
            LOGGER.info(
                "bumble: LE Buffer Size V2 iso_len=%s iso_packets=%s "
                "le_acl_len=%s le_acl_packets=%s",
                iso_len,
                iso_n,
                buf.le_acl_data_packet_length,
                buf.total_num_le_acl_data_packets,
            )
        except Exception as e:  # noqa: BLE001
            LOGGER.warning("bumble: LE_Read_Buffer_Size_V2 failed: %s", e)

    # Stash for phase skip checks (real HCI ISO buffers only).
    device.bleah_iso_hci_len = iso_len
    device.bleah_iso_hci_packets = iso_n
    if host.iso_packet_queue is not None:
        q = host.iso_packet_queue
        LOGGER.info(
            "bumble: ISO packet queue ready (max_size=%s max_in_flight=%s)",
            q.max_packet_size,
            q.max_in_flight,
        )
        return

    if iso_len > 0 and iso_n > 0:
        return

    if roles:
        LOGGER.warning(
            "bumble: controller claims ISO roles %s but reports no ISO HCI "
            "buffers (iso_len=%s iso_packets=%s) — CIS/BIG link setup may work, "
            "ISO data plane will not (skip unicast_audio / broadcast_audio)",
            sorted(roles),
            iso_len,
            iso_n,
        )


def controller_iso_hci_ok(device) -> bool:
    """True when the controller reported non-zero ISO HCI buffers at power_on."""
    iso_len = int(getattr(device, "bleah_iso_hci_len", 0) or 0)
    iso_n = int(getattr(device, "bleah_iso_hci_packets", 0) or 0)
    if iso_len > 0 and iso_n > 0 and device.host.iso_packet_queue is not None:
        return True
    return False


def require_iso_hci(device, what: str) -> None:
    """Raise a skip-friendly RuntimeError when ISO HCI data is unavailable."""
    if controller_iso_hci_ok(device):
        return
    iso_len = getattr(device, "bleah_iso_hci_len", "?")
    iso_n = getattr(device, "bleah_iso_hci_packets", "?")
    roles = sorted(controller_iso_roles(device))
    raise RuntimeError(
        f"{what} needs ISO HCI data buffers; controller reports "
        f"iso_len={iso_len} iso_packets={iso_n} roles={roles}. "
        "AIC8800D80 + stock btusb typically advertises CIS/BIG roles but "
        "returns zero ISO HCI buffers — use a dongle that reports non-zero "
        "LE_Read_Buffer_Size_V2 ISO fields (e.g. nRF5340 Audio / Intel AX)."
    )


async def scan_by_name(device, name: str, timeout: float = 15.0):
    """Scan until a connectable advertisement with local name @p name appears.

    Returns the peer ``Address``, or None on timeout.
    """
    from bumble.core import AdvertisingData

    found = asyncio.get_running_loop().create_future()
    LOGGER.info("bumble: scanning for name=%r (timeout=%.0fs)", name, timeout)

    def on_adv(advertisement) -> None:
        if found.done():
            return
        ad = advertisement.data
        if ad is None:
            return
        adv_name = ad.get(AdvertisingData.COMPLETE_LOCAL_NAME) or ad.get(
            AdvertisingData.SHORTENED_LOCAL_NAME
        )
        if adv_name == name and advertisement.is_connectable:
            found.set_result(advertisement.address)

    device.on("advertisement", on_adv)
    try:
        await device.start_scanning(active=True, filter_duplicates=False)
        try:
            addr = await asyncio.wait_for(asyncio.shield(found), timeout=timeout)
            LOGGER.info("bumble: found %s", addr)
            return addr
        except asyncio.TimeoutError:
            LOGGER.warning("bumble: scan timeout for name=%r", name)
            return None
    finally:
        device.remove_listener("advertisement", on_adv)
        try:
            await device.stop_scanning()
        except Exception:  # noqa: BLE001
            pass
        await asyncio.sleep(0.3)


async def find_broadcast(device, broadcast_id: int, timeout: float = 20.0):
    """Scan extended advertising for Broadcast Audio Announcement 0x1852.

    Returns a dict with broadcast_id / name / address, or None.
    """
    from bumble import gatt
    from bumble.core import AdvertisingData
    from bumble.profiles import bap

    hit: dict[str, Any] = {}
    done = asyncio.Event()

    def on_adv(advertisement) -> None:
        if done.is_set():
            return
        ad = advertisement.data
        if ad is None:
            return
        for svc_uuid, data in ad.get_all(AdvertisingData.SERVICE_DATA_16_BIT_UUID) or []:
            if svc_uuid != gatt.GATT_BROADCAST_AUDIO_ANNOUNCEMENT_SERVICE:
                continue
            try:
                ann = bap.BroadcastAudioAnnouncement.from_bytes(data)
            except Exception:  # noqa: BLE001
                if len(data) < 3:
                    continue
                bid = data[0] | (data[1] << 8) | (data[2] << 16)
            else:
                bid = ann.broadcast_id
            if bid != broadcast_id:
                continue
            name = ad.get(AdvertisingData.COMPLETE_LOCAL_NAME) or ad.get(
                AdvertisingData.SHORTENED_LOCAL_NAME
            )
            has_pba = any(
                u == gatt.GATT_PUBLIC_BROADCAST_ANNOUNCEMENT_SERVICE
                for u, _ in (ad.get_all(AdvertisingData.SERVICE_DATA_16_BIT_UUID) or [])
            )
            hit.update(
                {
                    "bid": bid,
                    "name": name,
                    "address": str(advertisement.address),
                    "has_pba": has_pba,
                }
            )
            done.set()
            return

    device.on("advertisement", on_adv)
    try:
        await device.start_scanning(active=True, filter_duplicates=False)
        try:
            await asyncio.wait_for(done.wait(), timeout=timeout)
        except asyncio.TimeoutError:
            return None
        return hit or None
    finally:
        device.remove_listener("advertisement", on_adv)
        try:
            await device.stop_scanning()
        except Exception:  # noqa: BLE001
            pass
        await asyncio.sleep(0.3)


# ---------------------------------------------------------------------------
# Bumble GATT client wrapper (control-plane phases)
# ---------------------------------------------------------------------------


class Gatt:
    """Thin async GATT client around a Bumble Peer on one connection."""

    def __init__(self, device, connection, peer):
        self.device = device
        self.connection = connection
        self.peer = peer
        self._services = list(peer.services)
        self._chars = []
        for s in self._services:
            self._chars.extend(list(s.characteristics))

    @classmethod
    async def connect(
        cls,
        device,
        address,
        timeout: float = 20.0,
        *,
        name: str | None = None,
    ):
        connection = await connect_and_pair(
            device, address, timeout=timeout, name=name
        )
        from bumble.device import Peer

        peer = Peer(connection)
        await peer.discover_services()
        for service in peer.services:
            await service.discover_characteristics()
        return cls(device, connection, peer)

    async def pair(self) -> bool:
        """Best-effort re-pair; connection path already pairs once."""
        try:
            await self.connection.pair()
            return True
        except asyncio.CancelledError:
            return bool(getattr(self.connection, "is_encrypted", False))
        except Exception:  # noqa: BLE001
            return bool(getattr(self.connection, "is_encrypted", False))

    async def disconnect(self):
        try:
            await self.connection.disconnect()
        except Exception:  # noqa: BLE001
            pass

    def service_uuids(self):
        return {_norm_uuid(s.uuid) for s in self._services}

    def layout(self):
        names = {v.lower(): k for k, v in {**SVC, **CHR}.items()}
        lines = []
        for s in sorted(self._services, key=lambda x: x.handle):
            su = _norm_uuid(s.uuid)
            label = names.get(su, "?")
            lines.append(f"  svc h={s.handle:<5} {su[4:8]} {label}")
            for c in sorted(s.characteristics, key=lambda x: x.handle):
                cu = _norm_uuid(c.uuid)
                clabel = names.get(cu, "?")
                props = str(c.properties)
                lines.append(f"    chr h={c.handle:<5} {cu[4:8]} {clabel:<18} {props}")
        return "\n".join(lines)

    def has_service(self, uuid: str) -> bool:
        return _norm_uuid(uuid) in self.service_uuids()

    def has_char(self, uuid: str) -> bool:
        u = _norm_uuid(uuid)
        return any(_norm_uuid(c.uuid) == u for c in self._chars)

    def chars(self, uuid: str):
        u = _norm_uuid(uuid)
        found = [c for c in self._chars if _norm_uuid(c.uuid) == u]
        return sorted(found, key=lambda c: c.handle)

    def _char(self, uuid: str):
        found = self.chars(uuid)
        if not found:
            raise KeyError(f"characteristic {uuid} not found")
        return found[0]

    async def read(self, uuid: str) -> bytes:
        val = await self._char(uuid).read_value()
        return bytes(val)

    async def write(self, uuid: str, data: bytes, response: bool = True):
        await self._char(uuid).write_value(bytes(data), with_response=response)

    async def read_h(self, handle: int) -> bytes:
        for c in self._chars:
            if c.handle == handle:
                return bytes(await c.read_value())
        raise KeyError(f"handle {handle} not found")

    async def write_h(self, handle: int, data: bytes, response: bool = True):
        for c in self._chars:
            if c.handle == handle:
                await c.write_value(bytes(data), with_response=response)
                return
        raise KeyError(f"handle {handle} not found")

    async def subscribe(self, uuid: str, handler=None):
        """Enable notifications/indications; return the list values are appended to."""
        received: list[bytes] = []

        def _cb(value):
            data = bytes(value)
            received.append(data)
            if handler is not None:
                handler(data)

        await self._char(uuid).subscribe(_cb)
        return received

    async def unsubscribe(self, uuid: str):
        try:
            await self._char(uuid).unsubscribe()
        except Exception:  # noqa: BLE001
            pass


async def vcp_set_absolute_volume(gatt: Gatt, volume: int):
    """Read the change counter from Volume State, then Set Absolute Volume."""
    state = await gatt.read(CHR["VOL_STATE"])  # [volume, mute, change_counter]
    change_counter = state[2] if len(state) >= 3 else 0
    await gatt.write(CHR["VOL_CP"], bytes([VCP_OP_SET_ABS_VOL, change_counter, volume & 0xFF]))


async def ccp_originate(gatt: Gatt, uri: str):
    await gatt.write(CHR["CALL_CP"], bytes([CCP_OP_ORIGINATE]) + uri.encode("ascii"))


async def ccp_accept(gatt: Gatt, call_index: int):
    await gatt.write(CHR["CALL_CP"], bytes([CCP_OP_ACCEPT, call_index & 0xFF]))


async def ccp_terminate(gatt: Gatt, call_index: int):
    await gatt.write(CHR["CALL_CP"], bytes([CCP_OP_TERMINATE, call_index & 0xFF]))


def parse_call_state(value: bytes):
    """Decode the Call State characteristic into [(index, state, flags), ...]."""
    calls = []
    for off in range(0, len(value) - 2, 3):
        calls.append((value[off], value[off + 1], value[off + 2]))
    return calls


async def has_set_active_preset(gatt: Gatt, index: int):
    await gatt.write(CHR["HA_PRESET_CP"], bytes([HAS_OP_SET_ACTIVE, index & 0xFF]))


# ---------------------------------------------------------------------------
# Tone helpers (shared by unicast / broadcast analysis)
# ---------------------------------------------------------------------------


def make_tone_pcm(freq=1000, seconds=4.0, rate=LC3_SAMPLE_RATE_HZ, amplitude=8000) -> bytes:
    """Return mono 16-bit little-endian PCM for a pure sine tone."""
    n = int(rate * seconds)
    frames = bytearray()
    for i in range(n):
        v = int(amplitude * math.sin(2 * math.pi * freq * i / rate))
        frames += struct.pack("<h", v)
    return bytes(frames)


def make_tone_wav(path, freq=1000, seconds=4.0, rate=LC3_SAMPLE_RATE_HZ, amplitude=8000):
    """Write a mono 16-bit PCM WAV holding a pure sine tone."""
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(make_tone_pcm(freq=freq, seconds=seconds, rate=rate, amplitude=amplitude))


def pcm_tone_metrics(pcm: bytes, freq=1000, rate=LC3_SAMPLE_RATE_HZ):
    """Return (rms, goertzel_ratio) for raw s16le mono PCM."""
    if not pcm or len(pcm) < 2:
        return 0.0, 0.0
    samples = struct.unpack("<%dh" % (len(pcm) // 2), pcm)
    if not samples:
        return 0.0, 0.0
    sq = sum(s * s for s in samples)
    rms = math.sqrt(sq / len(samples))
    w0 = 2 * math.pi * freq / rate
    coeff = 2 * math.cos(w0)
    s_prev = s_prev2 = 0.0
    for x in samples:
        s = x + coeff * s_prev - s_prev2
        s_prev2 = s_prev
        s_prev = s
    power = s_prev2 * s_prev2 + s_prev * s_prev - coeff * s_prev * s_prev2
    total = sq if sq else 1.0
    return rms, power / total


def wav_tone_metrics(path, freq=1000, rate=LC3_SAMPLE_RATE_HZ):
    """Return (rms, goertzel_ratio) for a recorded WAV."""
    try:
        with wave.open(path, "rb") as w:
            n = w.getnframes()
            sr = w.getframerate() or rate
            raw = w.readframes(n)
    except Exception:
        return 0.0, 0.0
    return pcm_tone_metrics(raw, freq=freq, rate=sr)


def tmp_path(suffix=".wav"):
    fd, path = tempfile.mkstemp(suffix=suffix)
    os.close(fd)
    return path


# ---------------------------------------------------------------------------
# Unicast client (ASCS / CIS / LC3)
# ---------------------------------------------------------------------------


def _lc3_codec_config():
    from bumble.profiles.bap import (
        AudioLocation,
        CodecSpecificConfiguration,
        FrameDuration,
        SamplingFrequency,
    )

    # DUT UnicastServer uses BLEAudioLocation::Mono (0). FRONT_LEFT (bit0) is
    # what the vendor client presets use, but the host-interop sink advertises
    # Mono — match that so Config Codec is accepted.
    return CodecSpecificConfiguration(
        sampling_frequency=SamplingFrequency.FREQ_16000,
        frame_duration=FrameDuration.DURATION_10000_US,
        audio_channel_allocation=AudioLocation(0),
        octets_per_codec_frame=LC3_OCTETS_PER_FRAME,
        codec_frames_per_sdu=LC3_FRAMES_PER_SDU,
    )


async def unicast_client_stream(
    device,
    address,
    *,
    name: str | None = None,
    tone_seconds: float = 5.0,
    collect_source: bool = False,
) -> dict:
    """Drive BAP Unicast Client against a Unicast Server at @p address.

    Configures LC3_16_2_1 on the sink ASE (and optionally one source ASE), sets
    up a CIS, pumps an encoded 1 kHz tone toward the sink, and optionally
    collects decoded PCM from the source. @p name enables re-scan between
    connect retries when ACL bring-up fails (HCI 0x3E).

    Returns a dict with keys: sink_frames_sent, source_pcm (bytes), source_rms,
    source_tone_ratio.
    """
    import lc3
    from bumble.device import CigParameters, CisLink, Peer
    from bumble.hci import CodecID, CodingFormat, PhyBit
    from bumble.profiles.ascs import (
        ASE_Config_Codec,
        ASE_Config_QOS,
        ASE_Enable,
        ASE_Receiver_Start_Ready,
        AseStateMachine,
        AudioStreamControlServiceProxy,
    )

    async def ase_write(op, *, with_response: bool = False) -> None:
        # ASE CP supports Write + WWR; prefer WWR (common BAP client pattern).
        # Requires ATT MTU > default — connect_and_pair exchanges MTU first.
        payload = bytes(op)
        LOGGER.info(
            "unicast: ASE CP write op=0x%02x len=%d with_response=%s payload=%s",
            payload[0] if payload else -1,
            len(payload),
            with_response,
            payload.hex(),
        )
        await asyncio.wait_for(
            ascs.ase_control_point.write_value(payload, with_response=with_response),
            timeout=8.0,
        )

    async def wait_state(ase_char, ase_id: int, want: int, timeout: float = 10.0):
        """Wait for ASE state via notification queue, falling back to read."""
        q = notifications[ase_id]
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                data = await asyncio.wait_for(
                    q.get(), timeout=min(0.5, deadline - time.monotonic())
                )
                if len(data) >= 2 and data[0] == ase_id and data[1] == want:
                    return data
            except asyncio.TimeoutError:
                cur = bytes(await ase_char.read_value())
                LOGGER.debug(
                    "unicast: ASE %s poll state=%s want=%s",
                    ase_id,
                    cur[1] if len(cur) > 1 else cur.hex(),
                    want,
                )
                if len(cur) >= 2 and cur[0] == ase_id and cur[1] == want:
                    return cur
        cur = bytes(await ase_char.read_value())
        raise TimeoutError(
            f"ASE {ase_id} did not reach state {want} (now {cur.hex()})"
        )

    connection = await connect_and_pair(device, address, name=name)
    peer = Peer(connection)
    LOGGER.info("unicast: discovering ASCS on %s", address)
    ascs = await peer.discover_service_and_create_proxy(AudioStreamControlServiceProxy)
    if ascs is None:
        await connection.disconnect()
        raise RuntimeError("ASCS not found on peer")

    if not ascs.sink_ase:
        await connection.disconnect()
        raise RuntimeError("no Sink ASE on peer")
    if ascs.ase_control_point is None:
        await connection.disconnect()
        raise RuntimeError("no ASE Control Point on peer")

    sink_val = bytes(await ascs.sink_ase[0].read_value())
    sink_id = sink_val[0]
    LOGGER.info("unicast: sink ASE id=%s state=%s", sink_id, sink_val[1] if len(sink_val) > 1 else "?")
    source_id = None
    source_char = None
    if collect_source and ascs.source_ase:
        source_val = bytes(await ascs.source_ase[0].read_value())
        source_id = source_val[0]
        source_char = ascs.source_ase[0]
        LOGGER.info(
            "unicast: source ASE id=%s state=%s",
            source_id,
            source_val[1] if len(source_val) > 1 else "?",
        )

    notifications: dict[int, asyncio.Queue] = {sink_id: asyncio.Queue()}
    if source_id is not None:
        notifications[source_id] = asyncio.Queue()
    cp_notes: list[bytes] = []

    def on_notify(data: bytes, ase_id: int):
        notifications[ase_id].put_nowait(bytes(data))

    def on_cp(data: bytes):
        raw = bytes(data)
        cp_notes.append(raw)
        LOGGER.info("unicast: ASE CP notify %s", raw.hex())

    # Spec: enable ASE CP notifications before writing the control point.
    await ascs.ase_control_point.subscribe(on_cp)
    await ascs.sink_ase[0].subscribe(functools.partial(on_notify, ase_id=sink_id))
    if source_char is not None and source_id is not None:
        await source_char.subscribe(functools.partial(on_notify, ase_id=source_id))

    config = _lc3_codec_config()
    ase_ids = [sink_id] + ([source_id] if source_id is not None else [])
    n = len(ase_ids)
    LOGGER.info("unicast: ASE_Config_Codec ids=%s cfg=%s", ase_ids, bytes(config).hex())
    await ase_write(
        ASE_Config_Codec(
            ase_id=ase_ids,
            target_latency=[2] * n,
            target_phy=[2] * n,  # LE 2M
            codec_id=[CodingFormat(CodecID.LC3)] * n,
            codec_specific_configuration=[config] * n,
        )
    )
    try:
        await wait_state(ascs.sink_ase[0], sink_id, AseStateMachine.State.CODEC_CONFIGURED)
    except TimeoutError:
        # Retry once with FRONT_LEFT (vendor client preset default) in case the
        # peer rejects Mono (0) channel allocation.
        from bumble.profiles.bap import (
            AudioLocation,
            CodecSpecificConfiguration,
            FrameDuration,
            SamplingFrequency,
        )

        alt = CodecSpecificConfiguration(
            sampling_frequency=SamplingFrequency.FREQ_16000,
            frame_duration=FrameDuration.DURATION_10000_US,
            audio_channel_allocation=AudioLocation.FRONT_LEFT,
            octets_per_codec_frame=LC3_OCTETS_PER_FRAME,
            codec_frames_per_sdu=LC3_FRAMES_PER_SDU,
        )
        LOGGER.warning(
            "unicast: Codec Configured timeout (cp_notes=%s); retry FRONT_LEFT cfg=%s",
            [x.hex() for x in cp_notes],
            bytes(alt).hex(),
        )
        await ase_write(
            ASE_Config_Codec(
                ase_id=ase_ids,
                target_latency=[2] * n,
                target_phy=[2] * n,
                codec_id=[CodingFormat(CodecID.LC3)] * n,
                codec_specific_configuration=[alt] * n,
            )
        )
        await wait_state(ascs.sink_ase[0], sink_id, AseStateMachine.State.CODEC_CONFIGURED)
    if source_char is not None and source_id is not None:
        await wait_state(source_char, source_id, AseStateMachine.State.CODEC_CONFIGURED)

    cig_id = 1
    cis_id = 1
    sdu_interval = LC3_FRAME_DURATION_US
    max_sdu = LC3_OCTETS_PER_FRAME * LC3_FRAMES_PER_SDU
    rtn = 2
    max_latency = 40
    presentation_delay = 40000

    LOGGER.info("unicast: ASE_Config_QOS")
    await ase_write(
        ASE_Config_QOS(
            ase_id=ase_ids,
            cig_id=[cig_id] * n,
            cis_id=[cis_id] * n,
            sdu_interval=[sdu_interval] * n,
            framing=[0] * n,
            phy=[2] * n,
            max_sdu=[max_sdu] * n,
            retransmission_number=[rtn] * n,
            max_transport_latency=[max_latency] * n,
            presentation_delay=[presentation_delay] * n,
        )
    )
    await wait_state(ascs.sink_ase[0], sink_id, AseStateMachine.State.QOS_CONFIGURED)
    if source_char is not None and source_id is not None:
        await wait_state(source_char, source_id, AseStateMachine.State.QOS_CONFIGURED)

    if source_id is not None and source_char is not None:
        LOGGER.info("unicast: Enable source ASE %s", source_id)
        await ase_write(ASE_Enable(ase_id=[source_id], metadata=[b""]))
        await wait_state(source_char, source_id, AseStateMachine.State.ENABLING)

    LOGGER.info("unicast: Enable sink ASE %s", sink_id)
    await ase_write(ASE_Enable(ase_id=[sink_id], metadata=[b""]))
    try:
        await wait_state(
            ascs.sink_ase[0], sink_id, AseStateMachine.State.ENABLING, timeout=8.0
        )
    except TimeoutError:
        cur = bytes(await ascs.sink_ase[0].read_value())
        LOGGER.warning(
            "unicast: sink ASE %s not ENABLING after Enable (%s); continuing to Create CIS",
            sink_id,
            cur.hex(),
        )
        if len(cur) < 2 or cur[1] not in (
            AseStateMachine.State.ENABLING,
            AseStateMachine.State.STREAMING,
            AseStateMachine.State.QOS_CONFIGURED,
        ):
            raise

    # HCI CIG after ASE Enable (matches Bumble bap_test / common BAP client order).
    cis_params = CigParameters.CisParameters(
        cis_id=cis_id,
        max_sdu_c_to_p=max_sdu,
        max_sdu_p_to_c=max_sdu if source_id is not None else 0,
        phy_c_to_p=PhyBit.LE_2M,
        phy_p_to_c=PhyBit.LE_2M,
        rtn_c_to_p=rtn,
        rtn_p_to_c=rtn,
    )
    LOGGER.info("unicast: setup_cig cig_id=%s cis_id=%s max_sdu=%s", cig_id, cis_id, max_sdu)
    cis_handles = await device.setup_cig(
        CigParameters(
            cig_id=cig_id,
            cis_parameters=[cis_params],
            sdu_interval_c_to_p=sdu_interval,
            sdu_interval_p_to_c=sdu_interval,
            max_transport_latency_c_to_p=max_latency,
            max_transport_latency_p_to_c=max_latency,
        )
    )
    cis_handle = cis_handles[0]

    LOGGER.info(
        "unicast: create_cis cis_handle=%s acl_handle=%s iso_queue=%s",
        cis_handle,
        connection.handle,
        device.host.iso_packet_queue is not None,
    )
    try:
        cis_links = await device.create_cis([(cis_handle, connection)])
    except Exception as e:
        raise RuntimeError(
            f"HCI_LE_Create_CIS failed (cis={cis_handle} acl={connection.handle}): {e}. "
            "Common causes: CIS Host Support bit not set, controller ISO not ready, "
            "or Create CIS while a previous CIS command is still pending."
        ) from e
    cis_link = cis_links[0]

    if source_id is not None and source_char is not None:
        await ase_write(ASE_Receiver_Start_Ready(ase_id=[source_id]))
        await wait_state(source_char, source_id, AseStateMachine.State.STREAMING)

    sink_state = bytes(await ascs.sink_ase[0].read_value())
    if len(sink_state) < 2 or sink_state[1] != AseStateMachine.State.STREAMING:
        await wait_state(
            ascs.sink_ase[0], sink_id, AseStateMachine.State.STREAMING, timeout=8.0
        )
    LOGGER.info("unicast: sink STREAMING")

    await cis_link.setup_data_path(direction=CisLink.Direction.HOST_TO_CONTROLLER)
    if source_id is not None:
        await cis_link.setup_data_path(direction=CisLink.Direction.CONTROLLER_TO_HOST)

    encoder = lc3.Encoder(
        frame_duration_us=LC3_FRAME_DURATION_US,
        sample_rate_hz=LC3_SAMPLE_RATE_HZ,
        num_channels=1,
    )
    decoder = None
    if source_id is not None:
        decoder = lc3.Decoder(
            frame_duration_us=LC3_FRAME_DURATION_US,
            sample_rate_hz=LC3_SAMPLE_RATE_HZ,
            num_channels=1,
        )

    pcm_out = bytearray()

    def on_iso(packet):
        if decoder is None or not packet.iso_sdu_fragment:
            return
        try:
            pcm_out.extend(decoder.decode(packet.iso_sdu_fragment, bit_depth=16))
        except Exception as e:  # noqa: BLE001
            LOGGER.debug("LC3 decode error: %s", e)

    if source_id is not None:
        cis_link.sink = on_iso

    pcm = make_tone_pcm(freq=1000, seconds=tone_seconds)
    frame_samples = encoder.get_frame_samples()
    frame_bytes = frame_samples * 2  # s16le mono
    frames_sent = 0
    offset = 0
    next_t = time.monotonic()
    LOGGER.info("unicast: pumping %s s of LC3 tone", tone_seconds)
    while offset + frame_bytes <= len(pcm):
        chunk = pcm[offset : offset + frame_bytes]
        offset += frame_bytes
        sdu = encoder.encode(pcm=chunk, num_bytes=max_sdu, bit_depth=16)
        cis_link.write(sdu)
        frames_sent += 1
        next_t += LC3_FRAME_DURATION_US / 1_000_000.0
        delay = next_t - time.monotonic()
        if delay > 0:
            await asyncio.sleep(delay)

    await asyncio.sleep(0.5)

    rms, ratio = pcm_tone_metrics(bytes(pcm_out), freq=1000)
    result = {
        "sink_frames_sent": frames_sent,
        "source_pcm": bytes(pcm_out),
        "source_rms": rms,
        "source_tone_ratio": ratio,
    }

    try:
        await connection.disconnect()
    except Exception:  # noqa: BLE001
        pass
    return result


# ---------------------------------------------------------------------------
# Broadcast sink capture (PA / BIG sync + LC3 decode)
# ---------------------------------------------------------------------------


async def broadcast_sink_capture(
    device,
    broadcast_id: int,
    *,
    capture_seconds: float = 5.0,
    sync_timeout: float = 20.0,
) -> dict:
    """Sync to an Auracast source advertising @p broadcast_id and decode LC3.

    Returns dict with pcm / rms / tone_ratio / broadcast_id.
    """
    import collections
    import lc3
    from bumble.apps.auracast import BroadcastScanner
    from bumble.device import BigSyncParameters

    if "sync-receiver" not in controller_iso_roles(device):
        raise RuntimeError("controller lacks Synchronized Receiver ISO role")

    scanner = BroadcastScanner(device=device, filter_duplicates=False, sync_timeout=sync_timeout)
    matched = asyncio.get_running_loop().create_future()

    def on_new(broadcast):
        if broadcast.broadcast_id == broadcast_id and not matched.done():
            matched.set_result(broadcast)

    scanner.on("new_broadcast", on_new)
    await scanner.start()
    try:
        broadcast = await asyncio.wait_for(matched, timeout=sync_timeout)
    except asyncio.TimeoutError as e:
        await scanner.stop()
        raise RuntimeError(
            f"broadcast id 0x{broadcast_id:06x} not seen within {sync_timeout}s"
        ) from e

    ready = asyncio.Event()

    def on_bcast_change():
        if broadcast.basic_audio_announcement and broadcast.biginfo:
            ready.set()

    broadcast.on("change", on_bcast_change)
    if broadcast.basic_audio_announcement and broadcast.biginfo:
        ready.set()
    if not ready.is_set():
        try:
            await asyncio.wait_for(ready.wait(), timeout=sync_timeout)
        except asyncio.TimeoutError as e:
            await scanner.stop()
            raise RuntimeError("BASE / BIGInfo not received") from e

    await scanner.stop()

    subgroup = broadcast.basic_audio_announcement.subgroups[0]
    configuration = subgroup.codec_specific_configuration
    assert configuration is not None
    sampling_frequency = configuration.sampling_frequency
    frame_duration = configuration.frame_duration
    assert sampling_frequency is not None and frame_duration is not None

    bis_indices = [bis.index for bis in subgroup.bis]
    # Some BASE encoders use 0-based indices; BIG sync expects 1-based.
    if bis_indices and min(bis_indices) == 0:
        bis_indices = [i + 1 for i in bis_indices]
    LOGGER.info(
        "broadcast: create_big_sync id=0x%06x bis=%s rate=%s",
        broadcast_id,
        bis_indices,
        sampling_frequency.hz,
    )
    try:
        big_sync = await asyncio.wait_for(
            device.create_big_sync(
                broadcast.sync,
                BigSyncParameters(
                    big_sync_timeout=0x4000,
                    bis=bis_indices,
                    broadcast_code=None,
                ),
            ),
            timeout=sync_timeout,
        )
    except asyncio.TimeoutError as e:
        try:
            await broadcast.sync.terminate()
        except Exception:  # noqa: BLE001
            pass
        raise RuntimeError("create_big_sync timed out") from e

    num_bis = len(big_sync.bis_links)
    decoder = lc3.Decoder(
        frame_duration_us=frame_duration.us,
        sample_rate_hz=sampling_frequency.hz,
        num_channels=num_bis,
    )
    lc3_queues: list[collections.deque[bytes]] = [
        collections.deque() for _ in range(num_bis)
    ]
    pcm_out = bytearray()
    done_at = time.monotonic() + capture_seconds
    finished = asyncio.Event()

    def sink(queue: collections.deque, packet):
        queue.append(packet.iso_sdu_fragment or b"")
        while lc3_queues and all(lc3_queues):
            frame = b"".join(q.popleft() for q in lc3_queues)
            if not frame:
                continue
            try:
                pcm_out.extend(decoder.decode(frame, bit_depth=16))
            except Exception as e:  # noqa: BLE001
                LOGGER.debug("broadcast LC3 decode error: %s", e)
            if time.monotonic() >= done_at:
                finished.set()

    for i, bis_link in enumerate(big_sync.bis_links):
        bis_link.sink = functools.partial(sink, lc3_queues[i])
        await bis_link.setup_data_path(direction=bis_link.Direction.CONTROLLER_TO_HOST)

    try:
        await asyncio.wait_for(finished.wait(), timeout=capture_seconds + 5.0)
    except asyncio.TimeoutError:
        LOGGER.warning(
            "broadcast: capture window ended with %d PCM bytes", len(pcm_out)
        )

    try:
        await big_sync.terminate()
    except Exception:  # noqa: BLE001
        pass
    try:
        await broadcast.sync.terminate()
    except Exception:  # noqa: BLE001
        pass

    pcm = bytes(pcm_out)
    rms, ratio = pcm_tone_metrics(pcm, freq=1000, rate=sampling_frequency.hz)
    return {
        "broadcast_id": broadcast_id,
        "pcm": pcm,
        "rms": rms,
        "tone_ratio": ratio,
        "sample_rate": sampling_frequency.hz,
    }
