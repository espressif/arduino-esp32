import pytest
import json
import os
import logging
import ipaddress
import random
import string

REGEX_IPV4 = r"(\b(?:(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)\.){3}(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)\b)"

GCOV_START = "<<<GCOV_DUMP_START>>>"
GCOV_END = "<<<GCOV_DUMP_END>>>"
GCOV_START_TIMEOUT = 5
GCOV_DUMP_TIMEOUT = 60


# Pytest arguments
def pytest_addoption(parser):
    parser.addoption("--wifi-password", action="store", default=None, help="Wi-Fi password.")
    parser.addoption("--wifi-ssid", action="store", default=None, help="Wi-Fi SSID.")


# Fixtures
@pytest.fixture(scope="session")
def wifi_ssid(request):
    return request.config.getoption("--wifi-ssid")


@pytest.fixture(scope="session")
def wifi_pass(request):
    return request.config.getoption("--wifi-password")


@pytest.fixture(scope="session")
def ci_job_id(request):
    return os.environ.get("CI_JOB_ID")


@pytest.fixture
def mute_patterns():
    """Auto-mute gcov dump output in the listener so it never reaches stdout."""
    return [(GCOV_START, GCOV_END)]


def _get_coverage_log_path(request):
    """Derive the coverage log path from the pytest *request* object."""
    xml_path = request.config.getoption("xmlpath", default=None)
    if xml_path:
        return xml_path.rsplit(".xml", 1)[0] + "_coverage.log"
    return os.path.join(os.path.dirname(str(request.fspath)), "coverage_serial.log")


def wait_for_gcov_dump(dut, output_path):
    """Capture one gcov dump session from *dut* and **append** it to *output_path*.

    Delegates to ``dut.capture_payload_to_file()`` which waits for the
    start/end markers, mutes stdout echo during the capture, and writes
    the result to *output_path* in append mode.

    Returns ``True`` if a dump was captured, ``False`` otherwise.
    """
    return dut.capture_payload_to_file(
        start=GCOV_START,
        end=GCOV_END,
        filepath=output_path,
        start_timeout=GCOV_START_TIMEOUT,
        timeout=GCOV_DUMP_TIMEOUT,
    )


@pytest.fixture(autouse=True)
def coverage_capture(dut, request):
    """After each test, wait for one gcov serial dump and save it.

    Skipped when the test already captured dumps explicitly via the
    ``gcov_dump_capture`` fixture.

    Takes ``dut`` as a direct parameter so that this fixture is torn down
    *before* the dut fixture (pytest tears down in reverse dependency order)."""
    yield
    if getattr(request.node, "_gcov_captured", False):
        return
    wait_for_gcov_dump(dut, _get_coverage_log_path(request))


@pytest.fixture
def gcov_dump_capture(dut, request):
    """Yield a callable that captures one gcov dump session.

    Call it after each reboot to accumulate coverage data from every boot
    into the same log file.  Using this fixture prevents the autouse
    ``coverage_capture`` fixture from waiting for an additional dump after
    the test ends.

    Example::

        def test_rebooting(dut, gcov_dump_capture):
            dut.expect_exact("boot 1 output")
            gcov_dump_capture()

            dut.expect_exact("boot 2 output")
            gcov_dump_capture()
    """
    output_path = _get_coverage_log_path(request)

    def _capture():
        request.node._gcov_captured = True
        return wait_for_gcov_dump(dut, output_path)

    yield _capture


@pytest.fixture
def save_perf_result(dut, request):
    """Return a callable that persists a performance result dict to JSON.

    The canonical dict has keys ``test_name``, ``runs``, ``settings``, and
    ``metrics`` (see ``.github/CI_README.md``).  The file is written to
    ``<test_dir>/<target>/result_<test_name><index>.json``, incrementing
    *index* to avoid overwriting previous results from other FQBN configs."""
    def _save(results):
        test_name = results.get("test_name", "unknown")
        target_dir = os.path.join(os.path.dirname(request.path), dut.app.target)
        os.makedirs(target_dir, exist_ok=True)
        file_index = 0
        report_file = os.path.join(target_dir, f"result_{test_name}{file_index}.json")
        while os.path.exists(report_file):
            file_index += 1
            report_file = os.path.join(target_dir, f"result_{test_name}{file_index}.json")
        with open(report_file, "w") as f:
            f.write(json.dumps(results))
        logging.info("Performance result saved to %s", report_file)
    return _save


# Helper functions
def is_valid_ipv4(ip):
    # Check if the IP address is a valid IPv4 address
    try:
        ipaddress.IPv4Address(ip)
        return True
    except ipaddress.AddressValueError:
        return False


def rand_str4():
    # Generate a random string of 4 characters
    return "".join(random.choices(string.ascii_letters + string.digits, k=4))
