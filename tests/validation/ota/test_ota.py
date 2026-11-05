import hashlib
import getpass
import importlib.util
import logging
import os
import re
import socket
import subprocess
import sys
import textwrap
import threading
import time
import warnings
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from socketserver import ThreadingTCPServer
from types import SimpleNamespace
from unittest.mock import Mock

import pytest
from pytest_embedded.unity import UNITY_SUMMARY_LINE_REGEX

ESP32_ROOT = Path(__file__).resolve().parents[3]
ESPOTA = ESP32_ROOT / "tools" / "espota.py"
LOGGER = logging.getLogger(__name__)

_ESPOTA_SPEC = importlib.util.spec_from_file_location("espota_under_test", ESPOTA)
espota = importlib.util.module_from_spec(_ESPOTA_SPEC)
_ESPOTA_SPEC.loader.exec_module(espota)

# IPv4 or IPv6 (may contain ':'); auth is last space-separated token.
# Require a newline so pexpect cannot match a truncated password (e.g. "test" of "test-ota-v6").
ARDUINO_OTA_BEGIN_RE = re.compile(rb"ARDUINO_OTA_BEGIN (\S+) ([0-9]+) (\S+)\r?\n")
ARDUINO_OTA_BEGIN_MAPPED_RE = re.compile(rb"ARDUINO_OTA_BEGIN_MAPPED (\S+) ([0-9]+) (\S+)\r?\n")
ARDUINO_OTA_BEGIN_BADAUTH_RE = re.compile(rb"ARDUINO_OTA_BEGIN_BADAUTH (\S+) ([0-9]+) (\S+)\r?\n")


def _is_ipv6(addr: str) -> bool:
    host = addr.strip("[]")
    if "%" in host:
        host = host.split("%", 1)[0]
    try:
        socket.inet_pton(socket.AF_INET6, host)
        return True
    except OSError:
        return False


def _ipv4_mapped(addr: str) -> str:
    """Convert an IPv4 literal to an IPv4-mapped IPv6 literal (::ffff:a.b.c.d)."""
    host = addr.strip("[]")
    socket.inet_pton(socket.AF_INET, host)  # validate
    return f"::ffff:{host}"


def _ipv6_socket_ok() -> bool:
    """True if the host can create an IPv6 socket (needed for ::ffff: mapped OTA)."""
    try:
        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
        s.close()
        return True
    except OSError:
        return False


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


def _lan_ipv6(v4_hint: str | None = None) -> str:
    """Pick a global host IPv6 address suitable for ArduinoOTA reverse-connect."""
    override = os.environ.get("OTA_HOST_IPV6")
    if override:
        return override

    # Prefer a global address learned via UDP connect (OS picks the right source)
    try:
        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
    except OSError:
        s = None
    if s is not None:
        try:
            s.connect(("2001:4860:4860::8888", 80))
            addr = s.getsockname()[0]
            if addr and not addr.startswith("fe80:") and addr != "::1":
                return addr
        except OSError:
            pass
        finally:
            s.close()

    # Fallback: scan getaddrinfo results for a global unicast address
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET6, socket.SOCK_DGRAM):
            candidate = info[4][0]
            if candidate and not candidate.startswith("fe80:") and not candidate.startswith("::1"):
                return candidate
    except (socket.gaierror, OSError) as e:
        LOGGER.debug("getaddrinfo IPv6 fallback failed (%s); no host global IPv6", e)
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


def _run_espota(
    dut_ip: str,
    dut_port: int,
    host_ip: str,
    firmware: Path,
    password: str | None,
    expect_ok: bool = True,
) -> None:
    if not ESPOTA.is_file():
        pytest.fail(f"espota.py not found at {ESPOTA}")

    cmd = [
        sys.executable,
        str(ESPOTA),
        "-i",
        dut_ip,
        "-I",
        host_ip,
        "-p",
        str(dut_port),
        "-f",
        str(firmware),
        "-t",
        "30",
    ]
    if password:
        cmd.extend(["-a", password])

    LOGGER.info("Running espota: %s", " ".join(cmd))
    # espota -t is 30s per socket op; full upload needs headroom beyond that.
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ESP32_ROOT), timeout=120)
    except subprocess.TimeoutExpired as e:
        if e.stdout:
            LOGGER.info("espota stdout (timed out):\n%s", e.stdout)
        if e.stderr:
            LOGGER.info("espota stderr (timed out):\n%s", e.stderr)
        pytest.fail(f"espota timed out after 120s for {dut_ip}:{dut_port}")
    if result.stdout:
        LOGGER.info("espota stdout:\n%s", result.stdout)
    if result.stderr:
        LOGGER.info("espota stderr:\n%s", result.stderr)
    if expect_ok and result.returncode != 0:
        pytest.fail(f"espota failed (exit {result.returncode}) for {dut_ip}:{dut_port}")
    if not expect_ok and result.returncode == 0:
        pytest.fail(f"espota unexpectedly succeeded for wrong-auth upload to {dut_ip}:{dut_port}")


def _expect_unity_with_arduino_ota(dut, firmware: Path, host_ip: str, host_ipv6: str, timeout: float = 300) -> None:
    """Expect Unity output while serving ArduinoOTA uploads via espota.py."""
    deadline = time.time() + timeout
    log = b""

    while True:
        remaining = max(1.0, deadline - time.time())
        match = dut.expect(
            [ARDUINO_OTA_BEGIN_MAPPED_RE, ARDUINO_OTA_BEGIN_BADAUTH_RE, ARDUINO_OTA_BEGIN_RE, UNITY_SUMMARY_LINE_REGEX],
            timeout=remaining,
        )
        log += dut.pexpect_proc.before

        matched = match.group(0)
        if isinstance(matched, str):
            matched = matched.encode()

        mapped = ARDUINO_OTA_BEGIN_MAPPED_RE.search(matched)
        badauth = ARDUINO_OTA_BEGIN_BADAUTH_RE.search(matched)
        begin = ARDUINO_OTA_BEGIN_RE.search(matched)
        if mapped or badauth or begin:
            log += matched
            m = mapped or badauth or begin
            dut_ip = m.group(1).decode()
            dut_port = int(m.group(2).decode())
            auth = m.group(3).decode()
            if badauth:
                password = "definitely-wrong-password"
            else:
                password = None if auth == "NONE" else auth

            if mapped:
                # Dual-stack edge case: talk to the DUT IPv4 via IPv4-mapped IPv6 literals.
                dut_ip = _ipv4_mapped(dut_ip)
                bind_ip = _ipv4_mapped(host_ip)
            elif _is_ipv6(dut_ip):
                if not host_ipv6:
                    pytest.fail(
                        f"DUT requested IPv6 OTA ({dut_ip}) but host reported no global IPv6 "
                        "(DUT should have ignored this case)"
                    )
                bind_ip = host_ipv6
            else:
                bind_ip = host_ip

            LOGGER.info("ArduinoOTA requested: ip=%s port=%s auth=%s host=%s", dut_ip, dut_port, auth, bind_ip)
            # Give the DUT a moment to enter ArduinoOTA.handle() wait loop
            time.sleep(0.5)
            _run_espota(dut_ip, dut_port, bind_ip, firmware, password, expect_ok=not badauth)
            continue

        # Unity summary reached — parse cases into the junit report
        log += matched
        dut.testsuite.add_unity_test_cases(
            log,
            additional_attrs={
                "app_path": dut.app.app_path,
            },
        )
        return


SIDECAR_ARTIFACTS = (
    "ota.ino.bin.sha256",
    "ota.ino.bin.sha512",
    "ota.ino.bin.md5",
    "upper.sha256",
    "bad.sha256",
    "wrong.sha256",
    "bad.sha512",
    "wrong.sha512",
    "wrong.md5",
)


def _remove_sidecar_artifacts(serve_dir: Path) -> None:
    for artifact in SIDECAR_ARTIFACTS:
        (serve_dir / artifact).unlink(missing_ok=True)


class DualStackHTTPServer(ThreadingTCPServer):
    """HTTP server reachable over IPv4 and IPv6."""

    address_family = socket.AF_INET6
    allow_reuse_address = True
    daemon_threads = True

    def server_bind(self):
        # Prefer dual-stack; if the sockopt is unsupported, still bind IPv6.
        try:
            self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        except (AttributeError, OSError) as e:
            LOGGER.debug("IPV6_V6ONLY=0 not applied (%s); continuing with IPv6 bind", e)
        super().server_bind()


class IPv4HTTPServer(ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def _http_handler(serve_dir: Path, firmware_sha256: str, firmware_sha512: str):
    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            self._omit_digest_header = False
            super().__init__(*args, directory=str(serve_dir), **kwargs)

        def do_GET(self):
            path = self.path.split("?", 1)[0]
            if path == "/redirect-sha256":
                self.send_response(302)
                self.send_header("Location", "/ota.ino.bin.sha256")
                self.end_headers()
                return
            if path == "/slow.sha256":
                body = f"{firmware_sha256}  ota.ino.bin\n".encode()
                self.send_response(200)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                for byte in body:
                    self.wfile.write(bytes((byte,)))
                    self.wfile.flush()
                    time.sleep(0.002)
                return
            if path == "/unknown-length.sha256":
                body = f"{firmware_sha256}\n".encode()
                self.send_response(200)
                self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(body)
                self.close_connection = True
                return
            if path == "/chunked.sha256":
                body = f"{firmware_sha256}  ota.ino.bin\n".encode()
                chunks = (body[:17], body[17:49], body[49:])
                self.send_response(200)
                self.send_header("Transfer-Encoding", "chunked")
                self.end_headers()
                for chunk in chunks:
                    self.wfile.write(f"{len(chunk):X}\r\n".encode())
                    self.wfile.write(chunk + b"\r\n")
                self.wfile.write(b"0\r\n\r\n")
                self.wfile.flush()
                return
            if path == "/large.sha256":
                body = f"{firmware_sha256}\n".encode() + b"x" * 600
                self.send_response(200)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return
            if path == "/boundary.sha256":
                body = b"g" * (512 - len(firmware_sha256)) + firmware_sha256.encode() + b"\n"
                self.send_response(200)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return
            if path == "/stalled.sha256":
                body = f"{firmware_sha256}\n".encode()
                self.send_response(200)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.flush()
                time.sleep(1)
                try:
                    self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError):
                    pass
                return
            if path == "/no-firmware-credentials.sha256":
                if self.headers.get("Authorization") or self.headers.get("X-Firmware-Only"):
                    self.send_response(400)
                    self.end_headers()
                    return
                body = f"{firmware_sha256}\n".encode()
                self.send_response(200)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return
            if path == "/not-modified.bin":
                self.send_response(304)
                self.end_headers()
                return
            if path == "/firmware-noheader.bin":
                # Same binary as ota.ino.bin, but without digest headers (sidecar-only path).
                self.path = "/ota.ino.bin"
                self._omit_digest_header = True
                try:
                    return SimpleHTTPRequestHandler.do_GET(self)
                finally:
                    self._omit_digest_header = False
            self._omit_digest_header = False
            return SimpleHTTPRequestHandler.do_GET(self)

        def end_headers(self):
            path = self.path.split("?", 1)[0]
            if path.endswith("/ota.ino.bin") or path == "/ota.ino.bin":
                if not self._omit_digest_header:
                    self.send_header("x-SHA256", firmware_sha256)
                    self.send_header("x-SHA512", firmware_sha512)
            super().end_headers()

        def log_message(self, format, *args):
            LOGGER.debug("HTTP %s - %s", self.address_string(), format % args)

    return Handler


def test_ota(dut, wifi_ssid, wifi_pass, request):
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

    host_ipv6 = _lan_ipv6(host_ip)
    ipv6_socket_ok = _ipv6_socket_ok()
    if host_ipv6:
        ipv6_mode = "FULL"
        LOGGER.info("Host IPv6 for ArduinoOTA: %s", host_ipv6)
    elif ipv6_socket_ok:
        ipv6_mode = "MAPPED"
        LOGGER.info("Host has IPv6 sockets but no global IPv6; native IPv6 cases will be ignored")
    else:
        ipv6_mode = "NONE"
        LOGGER.warning("Host has no IPv6 support; all IPv6 cases will be ignored")

    serve_dir = firmware.parent
    firmware_bytes = firmware.read_bytes()
    firmware_sha256 = hashlib.sha256(firmware_bytes).hexdigest()
    firmware_sha512 = hashlib.sha512(firmware_bytes).hexdigest()
    firmware_md5 = hashlib.md5(firmware_bytes).hexdigest()

    # Sidecar artifacts for HTTPUpdate checksum URL tests (#12826)
    request.addfinalizer(lambda: _remove_sidecar_artifacts(serve_dir))
    (serve_dir / "ota.ino.bin.sha256").write_text(f"{firmware_sha256}  ota.ino.bin\n", encoding="ascii")
    (serve_dir / "ota.ino.bin.sha512").write_text(f"{firmware_sha512}  ota.ino.bin\n", encoding="ascii")
    (serve_dir / "ota.ino.bin.md5").write_text(f"{firmware_md5}  ota.ino.bin\n", encoding="ascii")
    (serve_dir / "upper.sha256").write_text(f"{firmware_sha256.upper()}\n", encoding="ascii")
    (serve_dir / "bad.sha256").write_text("not-a-valid-digest\n", encoding="ascii")
    (serve_dir / "wrong.sha256").write_text("0" * 64 + "\n", encoding="ascii")
    (serve_dir / "bad.sha512").write_text("not-a-valid-digest\n", encoding="ascii")
    (serve_dir / "wrong.sha512").write_text("0" * 128 + "\n", encoding="ascii")
    (serve_dir / "wrong.md5").write_text("0" * 32 + "\n", encoding="ascii")
    Handler = _http_handler(serve_dir, firmware_sha256, firmware_sha512)

    port = 8766
    server = None
    dual_stack = False
    if ipv6_socket_ok:
        for _ in range(10):
            try:
                server = DualStackHTTPServer(("::", port), Handler)
                dual_stack = True
                break
            except OSError:
                port += 1
    if server is None:
        port = 8766
        for _ in range(10):
            try:
                server = IPv4HTTPServer(("0.0.0.0", port), Handler)
                break
            except OSError:
                port += 1
    if server is None:
        pytest.fail("Could not bind HTTP server port")

    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base_url = f"http://{host_ip}:{port}"
    LOGGER.info(
        "HTTP server at %s (%s) serving %s",
        base_url,
        f"dual-stack :::{port}" if dual_stack else f"IPv4 0.0.0.0:{port}",
        serve_dir,
    )

    try:
        LOGGER.info("Waiting for device to be ready...")
        dut.expect_exact("OTA_READY")

        dut.expect_exact("Send SSID:")
        dut.write(f"{wifi_ssid}\n")

        dut.expect_exact("Send Password:")
        dut.write(f"{wifi_pass or ''}\n")

        dut.expect_exact("Send Server URL (or NONE):")
        dut.write(f"{base_url}\n")

        dut.expect_exact("Send IPv6 Mode (NONE|MAPPED|FULL):")
        dut.write(f"{ipv6_mode}\n")

        dut.expect_exact("Send IPv6 Server Host (or NONE):")
        dut.write(f"{host_ipv6 or 'NONE'}\n")

        dut.expect_exact("Send IPv6 Server Port (or 0):")
        dut.write(f"{port if host_ipv6 else 0}\n")

        LOGGER.info("Running OTA Unity tests (Update/HTTPUpdate/ArduinoOTA)")
        # Extra ArduinoOTA uploads (IPv4/IPv6 interaction cases) need a longer budget.
        _expect_unity_with_arduino_ota(dut, firmware, host_ip, host_ipv6, timeout=600)
    finally:
        if server:
            server.shutdown()
            server.server_close()


# ==================== Host-side espota.py password-prompt tests ====================
# These do not need a DUT; they exercise the uploader's argument/prompt logic.

PASSWORD = "  a'b\"$;\\& end  "


@pytest.fixture
def upload(tmp_path, monkeypatch):
    image = tmp_path / "firmware.bin"
    image.write_bytes(b"firmware fixture")
    connection = Mock()
    connection.recv.return_value = b"OK"
    listener = Mock()
    listener.accept.return_value = (connection, ("127.0.0.1", 1234))
    invitation = Mock(return_value=(True, "AUTH " + "a" * 64, None))
    authenticate = Mock(return_value=(True, None))
    prompt = Mock(return_value=PASSWORD)
    abort_socket = Mock()
    sockets = [listener, abort_socket]

    def create_socket(*args, **kwargs):
        return sockets.pop(0) if sockets else Mock()

    monkeypatch.setattr(espota, "create_socket", create_socket)
    monkeypatch.setattr(espota, "send_invitation_and_get_auth_challenge", invitation)
    monkeypatch.setattr(espota, "authenticate", authenticate)
    monkeypatch.setattr(getpass, "getpass", prompt)
    monkeypatch.setattr(espota, "has_windows_console_input", Mock(return_value=True))
    return SimpleNamespace(
        run=lambda *args: espota.main(["-i", "127.0.0.1", "-f", str(image), *args]),
        listener=listener,
        abort_socket=abort_socket,
        connection=connection,
        invitation=invitation,
        authenticate=authenticate,
        prompt=prompt,
    )


@pytest.mark.parametrize("nonce_length", [32, 64])
def test_missing_password_prompts_for_either_protocol(upload, nonce_length):
    upload.invitation.return_value = (True, "AUTH " + "a" * nonce_length, None)
    assert upload.run() == 0
    upload.prompt.assert_called_once()
    assert upload.authenticate.call_args.args[2] == PASSWORD
    assert upload.authenticate.call_args.kwargs["use_old_protocol"] == (nonce_length == 32)
    upload.connection.sendall.assert_called_once_with(b"firmware fixture")


@pytest.mark.parametrize("password", ["", PASSWORD])
def test_explicit_password_never_prompts(upload, password):
    assert upload.run("--auth=" + password) == 0
    upload.prompt.assert_not_called()
    assert upload.authenticate.call_args.args[2] == password


def test_unauthenticated_device_never_prompts(upload):
    upload.invitation.return_value = (True, "OK", None)
    assert upload.run() == 0
    upload.prompt.assert_not_called()
    upload.authenticate.assert_not_called()
    upload.connection.sendall.assert_called_once_with(b"firmware fixture")


def test_md5_fallback_reuses_prompted_password(upload):
    upload.authenticate.side_effect = [(False, "Authentication Failed"), (True, None)]
    assert upload.run() == 0
    upload.prompt.assert_called_once()
    calls = upload.authenticate.call_args_list
    assert len(calls) == 2
    assert [call.args[2] for call in calls] == [PASSWORD, PASSWORD]
    assert [call.kwargs["use_md5_password"] for call in calls] == [False, True]


def test_explicit_md5_target_can_prompt(upload):
    assert upload.run("--md5-target") == 0
    upload.prompt.assert_called_once()
    assert upload.authenticate.call_args.args[2] == PASSWORD
    assert upload.authenticate.call_args.kwargs["use_md5_password"] is True


@pytest.mark.parametrize("error", [EOFError, KeyboardInterrupt, OSError])
def test_canceled_input_aborts_before_authentication(upload, error, caplog):
    upload.prompt.side_effect = error
    assert upload.run() == 1
    assert "Password input unavailable or canceled" in caplog.text
    upload.authenticate.assert_not_called()
    upload.listener.accept.assert_not_called()
    upload.listener.close.assert_called_once()
    upload.abort_socket.sendto.assert_called_once()
    assert upload.abort_socket.sendto.call_args.args[0] == b"200 0 0\n"
    upload.abort_socket.close.assert_called_once()


def test_getpass_cannot_fall_back_to_echoed_input(upload, caplog):
    def warn_before_fallback(*args):
        warnings.warn("Can not control echo on the terminal.", getpass.GetPassWarning)
        pytest.fail("getpass must stop before falling back to visible input")

    upload.prompt.side_effect = warn_before_fallback
    assert upload.run() == 1
    assert "Use --auth" in caplog.text
    upload.authenticate.assert_not_called()
    upload.listener.accept.assert_not_called()
    upload.listener.close.assert_called_once()


def test_windows_without_console_aborts_before_getpass(upload, monkeypatch):
    monkeypatch.setattr(espota, "os", SimpleNamespace(name="nt", path=os.path))
    monkeypatch.setattr(espota, "has_windows_console_input", Mock(return_value=False))
    assert upload.run() == 1
    upload.prompt.assert_not_called()
    upload.authenticate.assert_not_called()
    upload.listener.accept.assert_not_called()
    upload.listener.close.assert_called_once()


def test_invalid_challenge_does_not_prompt(upload):
    upload.invitation.return_value = (True, "AUTH invalid", None)
    assert upload.run() == 1
    upload.prompt.assert_not_called()
    upload.authenticate.assert_not_called()


def test_failed_invitation_does_not_prompt(upload):
    upload.invitation.return_value = (False, None, "No response from the ESP")
    assert upload.run() == 1
    upload.prompt.assert_not_called()
    upload.authenticate.assert_not_called()


@pytest.mark.parametrize("args", [[], ["--auth=" + PASSWORD]])
def test_debug_output_does_not_include_password(upload, caplog, args):
    caplog.set_level(logging.DEBUG)
    assert upload.run("--debug", *args) == 0
    assert "Options:" in caplog.text
    assert PASSWORD not in caplog.text
    assert repr(PASSWORD) not in caplog.text


@pytest.mark.parametrize("stdin", [subprocess.PIPE, subprocess.DEVNULL], ids=["pipe", "null"])
@pytest.mark.parametrize(
    "creationflags",
    [subprocess.CREATE_NO_WINDOW, subprocess.DETACHED_PROCESS] if sys.platform == "win32" else [0],
)
def test_missing_console_subprocess_exits_promptly(tmp_path, stdin, creationflags):
    image = tmp_path / "firmware.bin"
    image.write_bytes(b"firmware fixture")
    options = {"creationflags": creationflags} if sys.platform == "win32" else {"start_new_session": True}
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(("127.0.0.1", 0))
        receiver.settimeout(10)
        command = [
            sys.executable,
            str(ESPOTA),
            "-i",
            "127.0.0.1",
            "-I",
            "127.0.0.1",
            "-p",
            str(receiver.getsockname()[1]),
            "-P",
            "0",
            "-f",
            str(image),
        ]
        with subprocess.Popen(
            command, stdin=stdin, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, **options
        ) as process:
            try:
                invitation, peer = receiver.recvfrom(1024)
                assert invitation.startswith(b"0 ")
                receiver.sendto(b"AUTH " + b"a" * 64, peer)
                _, stderr = process.communicate(timeout=10)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate()
        assert process.returncode == 1, stderr
        assert "Password input unavailable or canceled" in stderr
        assert "Use --auth" in stderr


@pytest.mark.skipif(sys.platform != "win32", reason="requires a Windows console")
def test_windows_console_probe_accepts_console_input():
    code = textwrap.dedent(
        """
        import importlib.util
        import sys

        spec = importlib.util.spec_from_file_location("espota", sys.argv[1])
        espota = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(espota)
        with open("CONIN$", "r") as console:
            sys.stdin = console
            assert espota.has_windows_console_input()
    """
    )
    result = subprocess.run(
        [sys.executable, "-c", code, str(ESPOTA)],
        creationflags=subprocess.CREATE_NO_WINDOW,
        stdin=subprocess.DEVNULL,
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert result.returncode == 0, result.stderr
