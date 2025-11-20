import pytest
import os


def pytest_addoption(parser):
    parser.addoption("--wifi-password", action="store", default=None, help="Wi-Fi password.")
    parser.addoption("--wifi-ssid", action="store", default=None, help="Wi-Fi SSID.")


@pytest.fixture(scope="session")
def wifi_ssid(request):
    return request.config.getoption("--wifi-ssid")


@pytest.fixture(scope="session")
def wifi_pass(request):
    return request.config.getoption("--wifi-password")


@pytest.fixture(scope="session")
def ci_job_id(request):
    return os.environ.get("CI_JOB_ID")
