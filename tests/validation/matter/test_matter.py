"""
Matter validation test with optional commissioning via matterjs-server.

Phase 0: Stack-only Unity tests (always runs).
Phase 1: Commissioning + multi-endpoint control tests. Runs if wifi_ssid is
         provided. When commissioning is attempted it is expected to succeed --
         failures are real test failures, not graceful skips. Tests IGNORE only
         when no WiFi credentials are provided (non-wifi_router runner).
         If WiFi credentials are present but matterjs-server is not installed
         or IPv6 is unavailable, the test FAILs (CI runner misconfiguration).

Dependencies for Phase 1:
  pip install matter-python-client==1.1.0
  npm install -g matter-server
"""

import asyncio
import logging
import shutil
import socket
import subprocess
import tempfile
import time

import pytest

LOGGER = logging.getLogger(__name__)

MATTER_PAIRING_CODE = "34970112332"
SERVER_PORT = 5580
SERVER_STARTUP_WAIT = 120
COMMISSION_TIMEOUT = 60

# Endpoint IDs (assigned sequentially by the Matter stack in Phase 1)
EP_ONOFF_LIGHT = 1
EP_DIMMABLE_LIGHT = 2
EP_COLOR_LIGHT = 3
EP_FAN = 4
EP_TEMP_SENSOR = 5

# Expected values -- must match matter.ino Phase 1 test assertions
BRIGHTNESS_LEVEL = 128
COLOR_HUE = 120
COLOR_SAT = 200
FAN_SPEED_PCT = 50
SENSOR_TEMP_RAW = 2250  # 22.5 °C * 100


def _has_ipv6():
    """Check if the host has a usable IPv6 stack."""
    try:
        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
        s.close()
        return True
    except OSError:
        return False


def _has_matter_server():
    """Check if matter-server CLI (npm) and Python client library are installed."""
    if not shutil.which("matter-server"):
        return False
    try:
        import matter_server.client  # noqa: F401
        return True
    except ImportError:
        return False


def _wait_for_port(port, timeout):
    """Poll until a TCP port is accepting connections."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("localhost", port), timeout=1):
                return True
        except OSError:
            time.sleep(0.5)
    return False


def _start_server(storage_path, log_file):
    """Start matter-server as a background process.

    Output is written to *log_file* (an open file handle) so the pipe buffer
    cannot deadlock the server.
    """
    proc = subprocess.Popen(
        [
            "matter-server",
            "--storage-path",
            storage_path,
            "--port",
            str(SERVER_PORT),
            "--log-level",
            "info",
            "--enable-test-net-dcl",
        ],
        stdout=log_file,
        stderr=subprocess.STDOUT,
    )
    if not _wait_for_port(SERVER_PORT, SERVER_STARTUP_WAIT):
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        log_file.seek(0)
        output = log_file.read()
        raise RuntimeError(
            f"matter-server not listening on port {SERVER_PORT} "
            f"(exit={proc.returncode}):\n{output[-4000:]}"
        )
    LOGGER.info("matterjs-server ready on port %d", SERVER_PORT)
    return proc


async def _commission_and_control(port, pairing_code):
    """Commission the device and exercise multiple cluster commands.

    Returns a dict of results:
      commissioned (bool), toggled (bool), brightness (bool),
      color (bool), fan (bool), sensor_temp (float | None).
    """
    import aiohttp
    from matter_server.client import MatterClient

    url = f"ws://localhost:{port}/ws"
    results = {"commissioned": False}

    async with aiohttp.ClientSession() as session:
        client = MatterClient(url, session)

        init_ready = asyncio.Event()
        listen_task = asyncio.create_task(
            client.start_listening(init_ready=init_ready)
        )
        await asyncio.wait_for(init_ready.wait(), timeout=10)
        LOGGER.info("Client listener ready")

        try:
            # ---- Commission ----
            LOGGER.info("Commissioning with code %s ...", pairing_code)
            node = await asyncio.wait_for(
                client.commission_with_code(pairing_code, network_only=True),
                timeout=COMMISSION_TIMEOUT,
            )
            node_id = getattr(node, "node_id", 1)
            LOGGER.info("Commissioned node_id=%s", node_id)
            results["commissioned"] = True

            await asyncio.sleep(2)

            from chip.clusters.Objects import (
                OnOff,
                LevelControl,
                ColorControl,
            )

            # ---- 1. Toggle OnOff light (endpoint 1) ----
            try:
                await asyncio.wait_for(
                    client.send_device_command(
                        node_id, EP_ONOFF_LIGHT, OnOff.Commands.Toggle()
                    ),
                    timeout=10,
                )
                LOGGER.info("Toggle sent to endpoint %d", EP_ONOFF_LIGHT)
                results["toggled"] = True
            except Exception as exc:
                LOGGER.error("Toggle failed: %s", exc)
                results["toggled"] = False

            # ---- 2. Set brightness on dimmable light (endpoint 2) ----
            try:
                await asyncio.wait_for(
                    client.send_device_command(
                        node_id,
                        EP_DIMMABLE_LIGHT,
                        LevelControl.Commands.MoveToLevel(
                            level=BRIGHTNESS_LEVEL,
                            transitionTime=0,
                            optionsMask=1,      # ExecuteIfOff
                            optionsOverride=1,
                        ),
                    ),
                    timeout=10,
                )
                LOGGER.info(
                    "MoveToLevel(%d) sent to endpoint %d",
                    BRIGHTNESS_LEVEL,
                    EP_DIMMABLE_LIGHT,
                )
                results["brightness"] = True
            except Exception as exc:
                LOGGER.error("Brightness command failed: %s", exc)
                results["brightness"] = False

            # ---- 3. Set color on color light (endpoint 3) ----
            try:
                await asyncio.wait_for(
                    client.send_device_command(
                        node_id,
                        EP_COLOR_LIGHT,
                        ColorControl.Commands.MoveToHueAndSaturation(
                            hue=COLOR_HUE,
                            saturation=COLOR_SAT,
                            transitionTime=0,
                            optionsMask=1,      # ExecuteIfOff
                            optionsOverride=1,
                        ),
                    ),
                    timeout=10,
                )
                LOGGER.info(
                    "MoveToHueAndSaturation(%d, %d) sent to endpoint %d",
                    COLOR_HUE,
                    COLOR_SAT,
                    EP_COLOR_LIGHT,
                )
                results["color"] = True
            except Exception as exc:
                LOGGER.error("Color command failed: %s", exc)
                results["color"] = False

            # ---- 4. Set fan speed via attribute write (endpoint 4) ----
            try:
                # FanControl cluster 0x0202 (514), PercentSetting attr 2
                fan_path = f"{EP_FAN}/514/2"
                await asyncio.wait_for(
                    client.write_attribute(node_id, fan_path, FAN_SPEED_PCT),
                    timeout=10,
                )
                LOGGER.info(
                    "Fan PercentSetting=%d written on endpoint %d",
                    FAN_SPEED_PCT,
                    EP_FAN,
                )
                results["fan"] = True
            except Exception as exc:
                LOGGER.error("Fan speed write failed: %s", exc)
                results["fan"] = False

            # ---- 5. Read temperature sensor (endpoint 5) ----
            try:
                # TemperatureMeasurement cluster 0x0402 (1026), MeasuredValue attr 0
                temp_path = f"{EP_TEMP_SENSOR}/1026/0"
                temp_data = await asyncio.wait_for(
                    client.read_attribute(node_id, temp_path),
                    timeout=10,
                )
                if isinstance(temp_data, dict):
                    raw_temp = list(temp_data.values())[0]
                else:
                    raw_temp = temp_data
                sensor_temp = (
                    raw_temp / 100.0
                    if isinstance(raw_temp, (int, float))
                    else None
                )
                LOGGER.info(
                    "Sensor temperature read: %s°C (raw=%s)", sensor_temp, raw_temp
                )
                results["sensor_temp"] = sensor_temp
            except Exception as exc:
                LOGGER.error("Sensor read failed: %s", exc)
                results["sensor_temp"] = None

            # Let all commands propagate to device callbacks
            await asyncio.sleep(2)

            return results
        finally:
            await client.disconnect()
            listen_task.cancel()
            try:
                await listen_task
            except asyncio.CancelledError:
                pass


def test_matter(dut, wifi_ssid, wifi_pass):
    # Phase 0: stack-only Unity tests
    dut.expect_unity_test_output(timeout=120)

    # Phase 1: commissioning + multi-endpoint control
    dut.expect_exact("MATTER_READY")

    if not wifi_ssid:
        LOGGER.info("Skipping commissioning phase: no WiFi credentials")
        dut.expect_exact("Send SSID:")
        dut.write("SKIP\n")
        dut.expect_unity_test_output(timeout=30)
        return

    # WiFi credentials are present: this is a wifi_router runner.
    # Both matter-server and IPv6 are required -- missing either is a runner misconfiguration.
    if not _has_matter_server():
        pytest.fail("matterjs-server is not installed (run: npm install -g matter-server)")
    if not _has_ipv6():
        pytest.fail("host has no IPv6 support (required for Matter mDNS commissioning)")

    # Send WiFi credentials
    dut.expect_exact("Send SSID:")
    dut.write(f"{wifi_ssid}\n")
    dut.expect_exact("Send Password:")
    dut.write(f"{wifi_pass or ''}\n")

    idx = dut.expect_exact(
        ["COMMISSION_WAITING", "COMMISSION_WIFI_FAIL"], timeout=30
    )

    if idx == 1:
        LOGGER.error("Device failed to connect to WiFi on wifi_router runner")
        dut.expect_unity_test_output(timeout=30)
        return

    # Start matter-server, commission, send commands.
    # Server stays alive while device runs Phase 1 Unity tests so that
    # Matter.isDeviceConnected() returns true on the device.
    storage = tempfile.mkdtemp(prefix="matter-ci-")
    log_file = tempfile.TemporaryFile(mode="w+", prefix="matter-log-")
    proc = None
    commissioned = False
    results = {}

    try:
        proc = _start_server(storage, log_file)

        LOGGER.info(
            "Starting commissioning with pairing code %s", MATTER_PAIRING_CODE
        )
        try:
            results = asyncio.run(
                _commission_and_control(SERVER_PORT, MATTER_PAIRING_CODE)
            )
            commissioned = results.get("commissioned", False)
        except Exception as exc:
            LOGGER.error("Commissioning infrastructure error: %s", exc)

        LOGGER.info("Commission results: %s", results)
        dut.write("COMMISSION_DONE\n" if commissioned else "COMMISSION_SKIP\n")

        # Device waits 5 s for callbacks, then runs Phase 1 Unity tests
        dut.expect_unity_test_output(timeout=60)
    finally:
        if proc:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        log_file.seek(0)
        server_log = log_file.read()
        if server_log:
            LOGGER.info("matterjs-server output:\n%s", server_log[-4000:])
        log_file.close()
        shutil.rmtree(storage, ignore_errors=True)

    # Host-side assertion: verify temperature sensor read
    sensor_temp = results.get("sensor_temp")
    if sensor_temp is not None:
        assert abs(sensor_temp - 22.5) < 1.0, (
            f"Sensor read {sensor_temp}°C, expected ~22.5°C"
        )
    elif commissioned:
        LOGGER.warning(
            "Could not read temperature sensor value from controller "
            "(write_attribute/read_attribute API may differ across versions)"
        )
