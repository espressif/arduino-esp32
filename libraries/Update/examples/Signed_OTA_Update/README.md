# Signed OTA Update Example

This example demonstrates how to perform secure OTA (Over-The-Air) updates with signature verification on ESP32 devices using Arduino.

## Overview

Code signing ensures that only firmware signed with your private key will be accepted by your devices. This protects against unauthorized firmware updates, even if an attacker gains access to your update server.

## Features

- **RSA Signature Verification**: Supports RSA-2048, RSA-3072, and RSA-4096
- **ECDSA Signature Verification**: Supports ECDSA-P256 and ECDSA-P384
- **Multiple Hash Algorithms**: SHA-256, SHA-384, and SHA-512
- **Automatic Signature Verification**: Signatures are verified automatically during OTA update
- **Secure by Default**: Update fails if signature verification fails

## Prerequisites

1. **Python 3** with the `cryptography` package:
   ```bash
   pip install cryptography
   ```

2. **ESP32 Arduino Core** with Update library

## Quick Start Guide

### Step 1: Generate Key Pair

Generate an RSA-2048 key pair (recommended):
```bash
python <ARDUINO_ROOT>/tools/bin_signing.py --generate-key rsa-2048 --out private_key.pem
python <ARDUINO_ROOT>/tools/bin_signing.py --extract-pubkey private_key.pem --out public_key.pem
```

Or generate an ECDSA-P256 key pair (smaller, faster):
```bash
python <ARDUINO_ROOT>/tools/bin_signing.py --generate-key ecdsa-p256 --out private_key.pem
python <ARDUINO_ROOT>/tools/bin_signing.py --extract-pubkey private_key.pem --out public_key.pem
```

Where `<ARDUINO_ROOT>` is your ESP32 Arduino installation path (e.g., `~/Arduino/hardware/espressif/esp32/`).

**IMPORTANT**: Keep `private_key.pem` secure! Anyone with access to it can sign firmware for your devices.

### Step 2: Update the Example Sketch

1. Copy the generated `public_key.h` to the example directory
2. Open `Signed_OTA_Update.ino`
3. Update Wi-Fi credentials:
   ```cpp
   const char *ssid = "YOUR_SSID";
   const char *password = "YOUR_PASSWORD";
   ```
4. Update firmware URL:
   ```cpp
   const char *firmwareUrl = "http://your-server.com/firmware_signed.bin";
   ```
5. Uncomment the appropriate key type (RSA or ECDSA)
6. Uncomment the appropriate hash algorithm (SHA-256, SHA-384, or SHA-512)

### Step 3: Build and Upload Initial Firmware

1. Compile and upload the sketch to your ESP32
2. Open Serial Monitor to verify it's running

### Step 4: Build and Sign New Firmware

1. Make changes to your sketch (e.g., add a version number)
2. Build the sketch and export the binary:
   - Arduino IDE: `Sketch` → `Export Compiled Binary`
   - Find the application `.bin` file in the `build` folder of your sketch folder. For example `build/espressif.esp32.esp32c6/Signed_OTA_Update.ino.bin`.

3. Sign the binary:
   ```bash
   python <ARDUINO_ROOT>/tools/bin_signing.py --bin <APPLICATION_BIN_FILE> --key private_key.pem --out firmware_signed.bin
   ```

   For other hash algorithms (for example SHA-384):
   ```bash
   python <ARDUINO_ROOT>/tools/bin_signing.py --bin <APPLICATION_BIN_FILE> --key private_key.pem --out firmware_signed.bin --hash sha384
   ```

### Step 5: Host the Signed Firmware

Upload `firmware_signed.bin` to your web server and make it accessible at the URL you configured.

### Step 6: Perform OTA Update

Reset your ESP32. It will:
1. Connect to Wi-Fi
2. Download the signed firmware
3. Verify the signature
4. Apply the update if signature is valid
5. Reboot with the new firmware

## Security Considerations

### Private Key Management

- **NEVER** commit your private key to version control
- Store it securely (encrypted storage, HSM, etc.)
- Limit access to authorized personnel only
- Consider using separate keys for development and production

### Recommended Practices

1. **Use HTTPS**: While signature verification protects firmware integrity, HTTPS protects against MitM attacks
2. **Key Rotation**: Periodically rotate keys (requires firmware update to include new public key)

## Signature Schemes Comparison

| Scheme | Key Size | Signature Size | Verification Speed | Security |
|--------|----------|----------------|-------------------|----------|
| RSA-2048 | 2048 bits | 256 bytes | Medium | High |
| RSA-3072 | 3072 bits | 384 bytes | Slower | Very High |
| RSA-4096 | 4096 bits | 512 bytes | Slowest | Maximum |
| ECDSA-P256 | 256 bits | 64 bytes | Fast | High |
| ECDSA-P384 | 384 bits | 96 bytes | Fast | Very High |

**Recommendation**: RSA-2048 or ECDSA-P256 provide good security with reasonable performance.

## Hash Algorithms Comparison

| Algorithm | Output Size | Speed | Security |
|-----------|-------------|-------|----------|
| SHA-256 | 32 bytes | Fast | High |
| SHA-384 | 48 bytes | Medium | Very High |
| SHA-512 | 64 bytes | Medium | Very High |

**Recommendation**: SHA-256 is sufficient for most applications.

## Troubleshooting

### "Signature verification failed"

- Ensure the firmware was signed with the correct private key
- Verify that the public key in the sketch matches the private key used for signing
- Check that the signature scheme (RSA/ECDSA) and hash algorithm match between signing and verification
- Ensure the signed binary wasn't corrupted during transfer

### "Failed to install signature verification"

- Check that `installSignature()` is called before `Update.begin()`
- Ensure hash and sign objects are properly initialized

### "Public key parsing failed"

- Verify the public key PEM format is correct
- Ensure PUBLIC_KEY_LEN matches the actual key length

## Advanced Usage

### Verifying a Signed Binary

You can verify a signed binary without flashing it:

```bash
python bin_signing.py --verify firmware_signed.bin --pubkey public_key.pem
```

### Using Different Hash Algorithms

Match the hash algorithm between signing and verification:

**Signing with SHA-384:**
```bash
python bin_signing.py --bin firmware.bin --key private_key.pem --out firmware_signed.bin --hash sha384
```

**Sketch configuration:**
```cpp
#define USE_SHA384
```

## API Reference

### Classes

- **UpdaterRSAVerifier**: RSA signature verifier
- **UpdaterECDSAVerifier**: ECDSA signature verifier

### Methods

```cpp
// Install signature verification (call before Update.begin())
bool Update.installSignature(UpdaterVerifyClass *sign);
```

### Error Codes

- `UPDATE_ERROR_SIGN (14)`: Signature verification failed

## License

This example is part of the Arduino-ESP32 project and is licensed under the Apache License 2.0.

## Support

For issues and questions:
- GitHub: https://github.com/espressif/arduino-esp32/issues
- Documentation: https://docs.espressif.com/
