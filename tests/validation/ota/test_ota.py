import logging
import os
import socket
import sys
import threading
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from socketserver import ThreadingTCPServer

import pytest


def _lan_ip() -> str:
    override = os.environ.get("OTA_HOST_IP")
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
    return ""


def _build_dir_from_config(config) -> Path | None:
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


def _find_firmware(build_dir: Path) -> Path | None:
    for name in ("ota.ino.bin",):
        p = build_dir / name
        if p.is_file():
            return p
    return None


def test_ota(dut, wifi_ssid, wifi_pass, request):
    LOGGER = logging.getLogger(__name__)

    if not wifi_ssid:
        pytest.fail("WiFi SSID is required but not provided. Use --wifi-ssid argument.")

    bd = _build_dir_from_config(request.config)
    if bd is None:
        pytest.fail("Missing --build-dir (run via tests_run.sh).")

    firmware = _find_firmware(bd)
    if firmware is None:
        pytest.fail(f"Firmware binary not found in {bd}")

    host_ip = _lan_ip()
    if not host_ip:
        pytest.fail("Could not determine host LAN IP")

    serve_dir = firmware.parent

    class ReusableTCPServer(ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True

    handler_class = type(
        "Handler",
        (SimpleHTTPRequestHandler,),
        {
            "__init__": lambda self, *a, **kw: SimpleHTTPRequestHandler.__init__(
                self, *a, directory=str(serve_dir), **kw
            )
        },
    )

    port = 8766
    server = None
    for _ in range(10):
        try:
            server = ReusableTCPServer(("0.0.0.0", port), handler_class)
            break
        except OSError:
            port += 1
    if server is None:
        pytest.fail("Could not bind HTTP server port")

    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base_url = f"http://{host_ip}:{port}"
    LOGGER.info("HTTP server at %s serving %s", base_url, serve_dir)

    try:
        LOGGER.info("Waiting for device to be ready...")
        dut.expect_exact("OTA_READY")

        dut.expect_exact("Send SSID:")
        dut.write(f"{wifi_ssid}\n")

        dut.expect_exact("Send Password:")
        dut.write(f"{wifi_pass or ''}\n")

        dut.expect_exact("Send Server URL (or NONE):")
        dut.write(f"{base_url}\n")

        LOGGER.info("Running OTA Unity tests")
        dut.expect_unity_test_output(timeout=120)
    finally:
        if server:
            server.shutdown()
            server.server_close()
