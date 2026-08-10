# Signed OTA Validation Test

Validates signed OTA firmware updates using the `Update` library with RSA-PSS and ECDSA-DER signature verification across multiple key sizes, hash algorithms, and failure scenarios. All keys are generated at test runtime — nothing is hardcoded.

## Test Cases

| Case | Description | Expected |
|---|---|---|
| 1 | RSA-2048 + SHA-256 | PASS |
| 2 | RSA-3072 + SHA-256 | PASS |
| 3 | RSA-4096 + SHA-256 | PASS |
| 4 | RSA-2048 + SHA-384 | PASS |
| 5 | RSA-2048 + SHA-512 | PASS |
| 6 | ECDSA-P256 + SHA-256 | PASS |
| 7 | ECDSA-P384 + SHA-256 | PASS |
| 8 | ECDSA-P256 + SHA-384 | PASS |
| 9 | ECDSA-P256 + SHA-512 | PASS |
| 10 | ECDSA-P384 + SHA-384 | PASS |
| 11 | ECDSA-P384 + SHA-512 | PASS |
| 12 | Wrong RSA key (sign with 2048, verify with 3072) | FAIL |
| 13 | Wrong ECDSA key (sign with P256, verify with P384) | FAIL |
| 14 | Hash mismatch RSA (sign SHA-256, verify SHA-384) | FAIL |
| 15 | Hash mismatch ECDSA (sign SHA-256, verify SHA-384) | FAIL |
| 16 | Corrupted firmware (byte flipped in firmware body) | FAIL |
| 17 | Corrupted signature (byte flipped in signature block) | FAIL |

## Requirements

- **Hardware**: One board with Wi-Fi support (e.g. ESP32, ESP32-S3)
- **Wokwi/QEMU**: Not supported
- **CI Runner**: `wifi_router` (requires LAN access between host and DUT)
- **SoC Config**: `CONFIG_SOC_WIFI_SUPPORTED=y`
- **Build Option**: `-DUPDATE_SIGN` (via `build_opt.h`)
- **Host Tools**: `tools/bin_signing.py` (for key generation and firmware signing)

## Serial Protocol

1. DUT prints `Device ready for signed OTA test`
2. pytest sends Wi-Fi SSID and password; DUT connects to Wi-Fi
3. pytest sends HTTP server base URL (host-side file server)
4. For each case:
   - DUT prints `READY_FOR_CASE`
   - pytest sends: case number, key type (`RSA`/`ECDSA`), hash type, firmware filename
   - DUT prints `SEND_KEY`; pytest sends PEM public key lines followed by `KEY_END`
   - DUT downloads firmware via HTTP, verifies signature, prints `CASE_END N PASS/FAIL`

## Notes

- The pytest harness generates all RSA and ECDSA key pairs at runtime using `tools/bin_signing.py`.
- A temporary HTTP server is started on the host to serve signed firmware binaries.
- Subset of cases can be run via `SIGNED_OTA_CASES=1,3,6,7,12,17` environment variable.
- The DUT does not actually reboot after OTA; it verifies signature validity and reports the result.
