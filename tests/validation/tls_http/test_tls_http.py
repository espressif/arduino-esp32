import logging
import os
import re
import ssl
import socket
import subprocess
import tempfile
import pytest


def _name_key(name):
    return tuple(tuple(rdn) for rdn in name)


def _decode_pem_cert(pem):
    """Decode a PEM certificate into the dict shape used by SSLSocket.getpeercert()."""
    if isinstance(pem, str):
        pem = pem.encode()
    fd, path = tempfile.mkstemp(suffix=".pem")
    try:
        os.write(fd, pem if pem.endswith(b"\n") else pem + b"\n")
        os.close(fd)
        fd = None
        return ssl._ssl._test_decode_cert(path)
    finally:
        if fd is not None:
            os.close(fd)
        os.unlink(path)


def _root_der_from_presented_chain(ctx, pem_blocks):
    """Map a presented chain onto a trust-anchor CA from the default store.

    Prefer a presented certificate that is already a trust anchor (same choice
    as SSLSocket.get_verified_chain()[-1]). Otherwise use the issuer of the
    last extra cert, which is typically a cross-signing root.
    """
    store = {_name_key(info["subject"]): der for info, der in zip(ctx.get_ca_certs(), ctx.get_ca_certs(binary_form=True))}
    if not store:
        raise RuntimeError("Default CA store is empty")

    for pem in reversed(pem_blocks):
        info = _decode_pem_cert(pem)
        subject = _name_key(info["subject"])
        issuer = _name_key(info["issuer"])
        if subject in store:
            return store[subject]
        if issuer in store:
            return store[issuer]
    raise RuntimeError("Could not map server chain to a trust-anchor CA")


def _openssl_showcerts(hostname, port):
    proc = subprocess.run(
        ["openssl", "s_client", "-connect", f"{hostname}:{port}", "-servername", hostname, "-showcerts"],
        input=b"",
        capture_output=True,
        timeout=15,
        check=False,
    )
    pems = re.findall(rb"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", proc.stdout, re.S)
    if not pems:
        err = proc.stderr.decode("utf-8", "replace")[:300]
        raise RuntimeError(f"openssl s_client did not return certificates: {err}")
    return pems


def _der_to_pem(der):
    import base64

    b64 = base64.encodebytes(der).decode()
    return f"-----BEGIN CERTIFICATE-----\n{b64}-----END CERTIFICATE-----\n"


def _get_server_ca_cert(hostname="postman-echo.com", port=443):
    """Fetch the root CA certificate for a given host at test time."""
    ctx = ssl.create_default_context()
    with ctx.wrap_socket(socket.socket(), server_hostname=hostname) as s:
        s.settimeout(10)
        s.connect((hostname, port))
        get_chain = getattr(s, "get_verified_chain", None)
        if callable(get_chain):
            chain = get_chain()
            if not chain:
                raise RuntimeError("Could not retrieve certificate chain")
            return _der_to_pem(chain[-1])

    # Python < 3.13 has no SSLSocket.get_verified_chain(). Reconstruct the
    # trust anchor from the server-presented chain plus the default CA store.
    pems = _openssl_showcerts(hostname, port)
    return _der_to_pem(_root_der_from_presented_chain(ctx, pems))


def test_tls_http(dut, wifi_ssid, wifi_pass):
    LOGGER = logging.getLogger(__name__)

    if not wifi_ssid:
        pytest.fail("WiFi SSID is required but not provided. Use --wifi-ssid argument.")

    LOGGER.info("Waiting for device to be ready...")
    dut.expect_exact("TLS_HTTP_READY")

    dut.expect_exact("Send SSID:")
    LOGGER.info(f"Sending WiFi credentials: SSID={wifi_ssid}")
    dut.write(f"{wifi_ssid}\n")

    dut.expect_exact("Send Password:")
    LOGGER.info("Sending WiFi password")
    dut.write(f"{wifi_pass or ''}\n")

    dut.expect_exact("SEND_CA_CERT")
    LOGGER.info("Fetching CA cert for postman-echo.com and sending to DUT")
    try:
        pem = _get_server_ca_cert()
    except Exception as e:
        pytest.fail(f"Failed to fetch CA cert for postman-echo.com: {e}")
    for line in pem.strip().splitlines():
        dut.write(f"{line}\n")
    dut.write("CERT_END\n")

    dut.expect(r"GOT_CERT len=\d+", timeout=10)
    LOGGER.info("Running TLS/HTTP Unity tests")
    dut.expect_unity_test_output(timeout=180)
