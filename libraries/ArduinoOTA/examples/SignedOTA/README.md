# SignedOTA - Secure OTA Updates with Signature Verification

This example demonstrates how to perform secure OTA updates with cryptographic signature verification using the ArduinoOTA library.

## Overview

**SignedOTA** adds an extra layer of security to Arduino OTA updates by requiring all firmware to be cryptographically signed with your private key. This protects against:

- ✅ Unauthorized firmware updates
- ✅ Man-in-the-middle attacks
- ✅ Compromised networks
- ✅ Firmware tampering
- ✅ Supply chain attacks

Even if an attacker gains access to your network, they **cannot** install unsigned firmware on your devices.

## Features

- **RSA & ECDSA Support**: RSA-2048/3072/4096 and ECDSA-P256/P384
- **Multiple Hash Algorithms**: SHA-256, SHA-384, SHA-512
- **Arduino IDE Compatible**: Works with standard Arduino OTA workflow
- **Optional Password Protection**: Add password authentication in addition to signature verification
- **Easy Integration**: Just a few lines of code

## Requirements

- **ESP32 Arduino Core 3.3.0+**
- **Python 3.6+** with `cryptography` library
- **OTA-capable partition scheme** (e.g., "Minimal SPIFFS (1.9MB APP with OTA)")

## Quick Start Guide

### 1. Generate Cryptographic Keys

```bash
# Navigate to Arduino ESP32 tools directory
cd <ARDUINO_ROOT>/tools

# Install Python dependencies
pip install cryptography

# Generate RSA-2048 key pair (recommended)
python bin_signing.py --generate-key rsa-2048 --out private_key.pem

# Extract public key
python bin_signing.py --extract-pubkey private_key.pem --out public_key.pem
```

**⚠️ IMPORTANT: Keep `private_key.pem` secure! Anyone with this key can sign firmware for your devices.**

### 2. Setup the Example

1. Copy `public_key.h` (generated in step 1) to this sketch directory
2. Open `SignedOTA.ino` in Arduino IDE
3. Configure WiFi credentials:
   ```cpp
   const char *ssid = "YourWiFiSSID";
   const char *password = "YourWiFiPassword";
   ```
4. Select appropriate partition scheme:
   - **Tools → Partition Scheme → "Minimal SPIFFS (1.9MB APP with OTA)"**

### 3. Upload Initial Firmware

1. Connect your ESP32 via USB
2. Upload the sketch normally
3. Open Serial Monitor (115200 baud)
4. Note the device IP address

### 4. Build & Sign Firmware for OTA Update Example

**Option A: Using Arduino IDE**

```bash
# Export compiled binary
# In Arduino IDE: Sketch → Export Compiled Binary

# Sign the firmware
cd <ARDUINO_ROOT>/tools
python bin_signing.py \
  --bin /path/to/SignedOTA.ino.bin \
  --key private_key.pem \
  --out firmware_signed.bin
```

**Option B: Using arduino-cli**

```bash
# Compile and export
arduino-cli compile --fqbn esp32:esp32:esp32 --export-binaries SignedOTA

# Sign the firmware
cd <ARDUINO_ROOT>/tools
python bin_signing.py \
  --bin build/esp32.esp32.esp32/SignedOTA.ino.bin \
  --key private_key.pem \
  --out firmware_signed.bin
```

### 5. Upload Signed Firmware via OTA

Upload the signed firmware using `espota.py`:

```bash
python <ARDUINO_ROOT>/tools/espota.py -i <device-ip> -f firmware_signed.bin
```

The device will automatically:
1. Receive the signed firmware (firmware + signature)
2. Hash only the firmware portion
3. Verify the signature
4. Install if valid, reject if invalid

**Note**: You can also use the Update library's `Signed_OTA_Update` example for HTTP-based OTA updates.

## Configuration Options

### Hash Algorithms

Choose one in `SignedOTA.ino`:

```cpp
#define USE_SHA256  // Default, fastest
// #define USE_SHA384
// #define USE_SHA512
```

**Must match** the `--hash` parameter when signing:

```bash
python bin_signing.py --bin firmware.bin --key private.pem --out signed.bin --hash sha256
```

### Signature Algorithms

Choose one in `SignedOTA.ino`:

```cpp
#define USE_RSA  // For RSA keys
// #define USE_ECDSA  // For ECDSA keys
```

### Optional Password Protection

Add password authentication **in addition to** signature verification:

```cpp
const char *ota_password = "yourpassword";  // Set password
// const char *ota_password = nullptr;  // Disable password
```

## How It Works

```
┌─────────────────┐
│  Build Firmware │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Sign Firmware  │ ← Uses your private key
│  (bin_signing)  │
└────────┬────────┘
         │
         ▼
┌─────────────────────────┐
│ firmware_signed.bin     │
│ [firmware][signature]   │
└────────┬────────────────┘
         │
         ▼ OTA Upload
┌─────────────────────────┐
│      ESP32 Device       │
│  ┌──────────────────┐   │
│  │ Verify Signature │   │ ← Uses your public key
│  │  ✓ or ✗          │   │
│  └──────────────────┘   │
│         │               │
│    ✓ Valid?             │
│    ├─ Yes: Install      │
│    └─ No:  Reject       │
└─────────────────────────┘
```

## Troubleshooting

### "Begin Failed" Error

**Cause**: Signature verification setup failed, or partition scheme issue

**Solutions**:
1. Check partition scheme (use "Minimal SPIFFS (1.9MB APP with OTA)")
2. Verify `public_key.h` is in the sketch directory
3. Check hash and signature algorithm match your key type

### "End Failed" Error

**Cause**: Signature verification failed

**Solutions**:
1. Ensure firmware was signed with the **correct private key**
2. Verify hash algorithm matches (SHA-256, SHA-384, SHA-512)
3. Check firmware wasn't corrupted during signing/transfer
4. Confirm you signed the **correct** `.bin` file

### "Receive Failed" Error

**Cause**: Network timeout or connection issue

**Solutions**:
1. Check WiFi signal strength
2. Ensure device is reachable on the network
3. Try increasing timeout: `ArduinoOTA.setTimeout(5000)`

### Upload Fails

**Issue**: OTA upload fails or times out

**Solutions**:
1. Verify device is on the same network
2. Check firewall settings aren't blocking port 3232
3. Ensure WiFi signal strength is adequate
4. If using password protection, ensure the password is correct
5. Try: `python <ARDUINO_ROOT>/tools/espota.py -i <device-ip> -f firmware_signed.bin -d`

## Security Considerations

### Best Practices

✅ **Keep private key secure**: Never commit to git, store encrypted
✅ **Use strong keys**: RSA-2048+ or ECDSA-P256+
✅ **Use HTTPS when possible**: For additional transport security
✅ **Add password authentication**: Extra layer of protection
✅ **Rotate keys periodically**: Generate new keys every 1-2 years

### What This Protects Against

- ✅ Unsigned firmware installation
- ✅ Firmware signed with wrong key
- ✅ Tampered/corrupted firmware
- ✅ Network-based attacks (when combined with password)

### What This Does NOT Protect Against

- ❌ Physical access (USB flashing still works)
- ❌ Downgrade attacks (no version checking by default)
- ❌ Replay attacks (no timestamp/nonce by default)
- ❌ Key compromise (if private key is stolen)

### Additional Security

For production deployments, consider:

1. **Add version checking** to prevent downgrades
2. **Add timestamp validation** to prevent replay attacks
3. **Use secure boot** for additional protection
4. **Store keys in HSM** or secure key management system
5. **Implement key rotation** mechanism

## Advanced Usage

### Using ECDSA Instead of RSA

ECDSA keys are smaller and faster:

```bash
# Generate ECDSA-P256 key
python bin_signing.py --generate-key ecdsa-p256 --out private_key.pem
python bin_signing.py --extract-pubkey private_key.pem --out public_key.pem
```

In `SignedOTA.ino`:

```cpp
#define USE_SHA256
#define USE_ECDSA  // Instead of USE_RSA
```

### Using SHA-384 or SHA-512

For higher security:

```bash
# Sign with SHA-384
python bin_signing.py --bin firmware.bin --key private.pem --out signed.bin --hash sha384
```

In `SignedOTA.ino`:

```cpp
#define USE_SHA384  // Instead of USE_SHA256
#define USE_RSA
```

### Custom Partition Label

To update a specific partition:

```cpp
ArduinoOTA.setPartitionLabel("my_partition");
```

## Support

For issues and questions:

- Update Library README: `libraries/Update/README.md`
- ESP32 Arduino Core: https://github.com/espressif/arduino-esp32
- Forum: https://github.com/espressif/arduino-esp32/discussions

## License

This library is part of the Arduino-ESP32 project and is licensed under the Apache License 2.0.
