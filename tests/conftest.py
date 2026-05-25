import pytest
import os
import ipaddress
import random
import string

REGEX_IPV4 = r"(\b(?:(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)\.){3}(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)\b)"


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
