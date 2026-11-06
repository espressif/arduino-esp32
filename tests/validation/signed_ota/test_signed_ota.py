"""
Hardware validation for signed OTA (tools/bin_signing.py + Update library).

All keys are generated at test runtime — no pre-generated keys in the repo.
Public keys are sent to the DUT over serial for each test case.
Each key/hash/scenario combination is reported as a sub-result.

Requires WiFi (--wifi-ssid) and LAN access between host and DUT.
Optional: SIGNED_OTA_CASES=1,3,6,7,12,17 for a minimal critical subset.
"""

from __future__ import annotations

import logging
import os
import shutil
import socket
import subprocess
import sys
import threading
import time
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from socketserver import ThreadingTCPServer
from typing import Optional

import pytest

LOGGER = logging.getLogger(__name__)

ESP32_ROOT = Path(__file__).resolve().parents[3]
BIN_SIGNING = ESP32_ROOT / "tools" / "bin_signing.py"

KEY_TYPES = [
    ("rsa2048", "rsa-2048"),
    ("rsa3072", "rsa-3072"),
    ("rsa4096", "rsa-4096"),
    ("ecdsa256", "ecdsa-p256"),
    ("ecdsa384", "ecdsa-p384"),
]

# (num, sign_key, verify_key, sign_hash, verify_hash, is_rsa, expect_pass, special)
CASE_DEFS = [
    (1, "rsa2048", "rsa2048", "sha256", "sha256", True, True, None),
    (2, "rsa3072", "rsa3072", "sha256", "sha256", True, True, None),
    (3, "rsa4096", "rsa4096", "sha256", "sha256", True, True, None),
    (4, "rsa2048", "rsa2048", "sha384", "sha384", True, True, None),
    (5, "rsa2048", "rsa2048", "sha512", "sha512", True, True, None),
    (6, "ecdsa256", "ecdsa256", "sha256", "sha256", False, True, None),
    (7, "ecdsa384", "ecdsa384", "sha256", "sha256", False, True, None),
    (8, "ecdsa256", "ecdsa256", "sha384", "sha384", False, True, None),
    (9, "ecdsa256", "ecdsa256", "sha512", "sha512", False, True, None),
    (10, "ecdsa384", "ecdsa384", "sha384", "sha384", False, True, None),
    (11, "ecdsa384", "ecdsa384", "sha512", "sha512", False, True, None),
    (12, "rsa2048", "rsa3072", "sha256", "sha256", True, False, None),
    (13, "ecdsa256", "ecdsa384", "sha256", "sha256", False, False, None),
    (14, "rsa2048", "rsa2048", "sha256", "sha384", True, False, None),
    (15, "ecdsa256", "ecdsa256", "sha256", "sha384", False, False, None),
    (16, "rsa2048", "rsa2048", "sha256", "sha256", True, False, "corrupt_fw"),
    (17, "ecdsa256", "ecdsa256", "sha256", "sha256", False, False, "corrupt_sig"),
]

CASE_LABELS = {
    1: "rsa2048_sha256",
    2: "rsa3072_sha256",
    3: "rsa4096_sha256",
    4: "rsa2048_sha384",
    5: "rsa2048_sha512",
    6: "ecdsa256_sha256",
    7: "ecdsa384_sha256",
    8: "ecdsa256_sha384",
    9: "ecdsa256_sha512",
    10: "ecdsa384_sha384",
    11: "ecdsa384_sha512",
    12: "wrong_rsa_key",
    13: "wrong_ecdsa_key",
    14: "hash_mismatch_rsa",
    15: "hash_mismatch_ecdsa",
    16: "corrupt_firmware",
    17: "corrupt_signature",
}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _lan_ip() -> str:
    override = os.environ.get("SIGNED_OTA_HOST_IP")
    if override:
        return override
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        pass
    finally:
        s.close()
    for _, _, _, _, sockaddr in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET, socket.SOCK_DGRAM):
        candidate = sockaddr[0]
        if candidate and not candidate.startswith("127."):
            return candidate
    raise RuntimeError("Unable to determine host LAN IP. Set SIGNED_OTA_HOST_IP env var.")


def _build_dir_from_config(config) -> Optional[Path]:
    for getter in (
        lambda: config.getoption("build_dir", default=None),
        lambda: config.getoption("--build-dir", default=None),
    ):
        try:
            v = getter()
            if v:
                return Path(v)
        except Exception:
            pass
    for a in sys.argv:
        if a.startswith("--build-dir="):
            return Path(a.split("=", 1)[1])
    for i, a in enumerate(sys.argv):
        if a == "--build-dir" and i + 1 < len(sys.argv):
            return Path(sys.argv[i + 1])
    return None


def _run_bin_signing(args: list[str]) -> None:
    cmd = [sys.executable, str(BIN_SIGNING)] + args
    LOGGER.info("Running: %s", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=str(ESP32_ROOT))


def _find_payload_bin(build_dir: Path) -> Path:
    for name in ("signed_ota.ino.bin",):
        p = build_dir / name
        if p.is_file():
            return p
    raise FileNotFoundError(f"No signed_ota firmware in {build_dir}")


def _generate_keys(key_dir: Path) -> None:
    key_dir.mkdir(parents=True, exist_ok=True)
    for name, ktype in KEY_TYPES:
        priv = key_dir / f"{name}.pem"
        pub = key_dir / f"{name}_pub.pem"
        _run_bin_signing(["--generate-key", ktype, "--out", str(priv)])
        _run_bin_signing(["--extract-pubkey", str(priv), "--out", str(pub)])


def _prepare_artifacts(payload: Path, key_dir: Path, http_dir: Path, cases: list) -> None:
    http_dir.mkdir(parents=True, exist_ok=True)
    signed_cache: dict[tuple[str, str], Path] = {}

    pairs = {(c[1], c[3]) for c in cases}
    for sign_key, sign_hash in pairs:
        out = http_dir / f"_base_{sign_key}_{sign_hash}.bin"
        priv = key_dir / f"{sign_key}.pem"
        _run_bin_signing(["--bin", str(payload), "--key", str(priv), "--out", str(out), "--hash", sign_hash])
        signed_cache[(sign_key, sign_hash)] = out

    for case_def in cases:
        num, sign_key, _, sign_hash, _, _, _, special = case_def
        base = signed_cache[(sign_key, sign_hash)]
        dest = http_dir / f"fw_case{num}.bin"

        if special == "corrupt_fw":
            data = bytearray(base.read_bytes())
            data[100] ^= 0xFF
            dest.write_bytes(bytes(data))
        elif special == "corrupt_sig":
            data = bytearray(base.read_bytes())
            sig_start = len(data) - 512
            data[sig_start + 12] ^= 0xFF
            dest.write_bytes(bytes(data))
        else:
            shutil.copyfile(base, dest)


def _http_handler_class(directory: Path):
    d = str(directory)

    class QuietHandler(SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=d, **kwargs)

        def copyfile(self, source, outputfile):
            try:
                super().copyfile(source, outputfile)
            except (BrokenPipeError, ConnectionResetError):
                pass

    return QuietHandler


def _send_pem(dut, pem_path: Path) -> None:
    pem_text = pem_path.read_text().strip()
    for line in pem_text.splitlines():
        dut.write(f"{line}\n")
        time.sleep(0.02)
    dut.write("KEY_END\n")


def _run_case(dut, key_dir, case_def):
    """Drive a single case over serial and assert the expected outcome."""
    num, sign_key, verify_key, sign_hash, verify_hash, is_rsa, expect_pass, special = case_def
    filename = f"fw_case{num}.bin"
    pub_pem = key_dir / f"{verify_key}_pub.pem"

    dut.expect_exact("READY_FOR_CASE", timeout=30)
    dut.write(f"{num}\n")

    dut.expect_exact("SEND_KEY_TYPE", timeout=10)
    dut.write(f"{'RSA' if is_rsa else 'ECDSA'}\n")

    dut.expect_exact("SEND_HASH_TYPE", timeout=10)
    dut.write(f"{verify_hash.upper()}\n")

    dut.expect_exact("SEND_FILENAME", timeout=10)
    dut.write(f"{filename}\n")

    dut.expect_exact("SEND_KEY", timeout=10)
    _send_pem(dut, pub_pem)
    dut.expect("GOT_KEY", timeout=10)

    match = dut.expect([f"CASE_END {num} PASS", f"CASE_END {num} FAIL"], timeout=300)
    got_pass = b"PASS" in match.group()

    if expect_pass:
        assert got_pass, f"Case {num} expected PASS but got FAIL"
    else:
        assert not got_pass, f"Case {num} expected FAIL but got PASS"


# ---------------------------------------------------------------------------
# Test function — one pytest item, sub-results recorded via record_property
# ---------------------------------------------------------------------------


def test_signed_ota(dut, wifi_ssid, wifi_pass, tmp_path, request, record_property) -> None:
    if not wifi_ssid:
        pytest.fail("WiFi SSID is required. Pass --wifi-ssid.")

    bd = _build_dir_from_config(request.config)
    if bd is None:
        pytest.fail("Missing --build-dir (run via tests_run.sh).")

    if not BIN_SIGNING.is_file():
        pytest.fail(f"bin_signing.py not found at {BIN_SIGNING}")

    cases = list(CASE_DEFS)

    payload = _find_payload_bin(bd)
    key_dir = tmp_path / "keys"
    http_dir = tmp_path / "http"

    LOGGER.info("Generating keys...")
    _generate_keys(key_dir)

    LOGGER.info("Preparing signed artifacts...")
    _prepare_artifacts(payload, key_dir, http_dir, cases)

    host = _lan_ip()
    port = 8765

    class ReusableTCPServer(ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True

    server = None
    for _ in range(10):
        try:
            server = ReusableTCPServer(("0.0.0.0", port), _http_handler_class(http_dir))
            break
        except OSError:
            port += 1
    if server is None:
        pytest.fail("Could not bind HTTP server port")

    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base_url = f"http://{host}:{port}"
    LOGGER.info("HTTP server at %s serving %s", base_url, http_dir)

    try:
        dut.expect_exact("Device ready for signed OTA test", timeout=120)
        dut.expect_exact("Send SSID:", timeout=30)
        dut.write(f"{wifi_ssid}\n")
        dut.expect_exact("Send Password:", timeout=30)
        dut.write(f"{wifi_pass or ''}\n")
        dut.expect_exact(f"SSID: {wifi_ssid}", timeout=30)
        dut.expect_exact(f"Password: {wifi_pass or ''}", timeout=30)
        dut.expect("WiFi connected", timeout=90)
        dut.expect_exact("Send HTTP server base URL", timeout=30)
        dut.write(f"{base_url}\n")
        dut.expect_exact(f"Server base: {base_url}", timeout=30)

        passed = []
        failed = []

        for case_def in cases:
            num = case_def[0]
            label = CASE_LABELS.get(num, f"case{num}")
            LOGGER.info("Running case %d (%s)", num, label)
            try:
                _run_case(dut, key_dir, case_def)
                passed.append(label)
                record_property(f"case_{label}", "PASS")
                LOGGER.info("PASSED: case %d (%s)", num, label)
            except Exception as e:
                failed.append((label, str(e)))
                record_property(f"case_{label}", f"FAIL: {e}")
                LOGGER.error("FAILED: case %d (%s): %s", num, label, e)

        summary = f"{len(passed)} passed, {len(failed)} failed out of {len(cases)}"
        LOGGER.info("Summary: %s", summary)
        record_property("summary", summary)

        if failed:
            lines = [f"  {lbl}: {err}" for lbl, err in failed]
            pytest.fail(f"{len(failed)} case(s) failed:\n" + "\n".join(lines))
    finally:
        server.shutdown()
        server.server_close()
