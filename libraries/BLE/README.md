# BLE for ESP32 Arduino Core

A comprehensive reference for ESP32 Bluetooth Low Energy (BLE) pairing and security implementation using the ESP32 Arduino BLE library.

## Overview

This guide provides ESP32 developers with comprehensive information about BLE security implementation using the ESP32 Arduino BLE library. It covers both Bluedroid (ESP32) and NimBLE (other SoCs) implementations with realistic scenarios and troubleshooting guidance.

Issues and questions should be raised here: https://github.com/espressif/arduino-esp32/issues <br> (please don't use https://github.com/nkolban/esp32-snippets/issues or https://github.com/h2zero/NimBLE-Arduino/issues)

## Security

### Quick Start

1. **Choose your ESP32's IO capabilities** using `IOCapability` enum values
2. **Configure authentication requirements** with `setAuthenticationMode()`
3. **Set up security** using the `BLESecurity` handle from `BLE.getSecurity()`
4. **Handle stack differences** between Bluedroid (ESP32) and NimBLE (other SoCs)
5. **Test with NVS clearing** during development

### Understanding BLE Pairing

#### Pairing vs Bonding
- **Pairing**: The process of establishing encryption keys between devices
- **Bonding**: Storing those keys for future reconnections (persistent pairing)

#### Pairing Types
- **Legacy Pairing**: Original BLE pairing (Bluetooth 4.0/4.1)
- **Secure Connections**: Enhanced security (Bluetooth 4.2+) using FIPS-approved algorithms

#### Security Levels
- **Just Works**: Encryption without user verification (vulnerable to passive eavesdropping)
- **MITM Protected**: User verification prevents man-in-the-middle attacks

### IO Capabilities Explained

The ESP32 BLE library defines the following IO capabilities:

| Capability | Enum Value | Can Display | Can Input | Can Confirm | Example Devices |
|------------|-----------|-------------|-----------|-------------|-----------------|
| **No Input No Output** | `NoInputNoOutput` | No | No | No | Sensor nodes, beacons, simple actuators |
| **Display Only** | `DisplayOnly` | Yes | No | No | E-ink displays, LED matrix displays |
| **Keyboard Only** | `KeyboardOnly` | No | Yes | No | Button-only devices, rotary encoders |
| **Display Yes/No** | `DisplayYesNo` | Yes | No | Yes | Devices with display + confirmation button |
| **Keyboard Display** | `KeyboardDisplay` | Yes | Yes | Yes | Full-featured ESP32 devices with UI |

### Pairing Methods Explained

#### Just Works
- **Security**: Encryption only (no MITM protection)
- **User Experience**: Automatic, no user interaction
- **Use Case**: Convenience over security (fitness trackers, mice)
- **Vulnerability**: Susceptible to passive eavesdropping during pairing

#### Passkey Entry
- **Security**: Full MITM protection
- **User Experience**: One device shows 6-digit code, other inputs it
- **Use Case**: Keyboards pairing to computers
- **Process**:
  1. Display device shows random 6-digit number (000000-999999)
  2. Input device user types the number
  3. Pairing succeeds if numbers match

#### Numeric Comparison (Secure Connections Only)
- **Security**: Full MITM protection
- **User Experience**: Both devices show same number, user confirms match
- **Use Case**: Two smartphones/tablets pairing
- **Process**:
  1. Both devices display identical 6-digit number
  2. User verifies numbers match on both screens
  3. User confirms "Yes" on both devices

#### Out-of-Band (OOB) (Not supported by this library)
- **Security**: Highest security level
- **User Experience**: Uses external channel (NFC, QR code)
- **Use Case**: High-security applications
- **Priority**: Always used when OOB data is available

### Pairing Methods Compatibility Matrix

Here is the compatibility matrix for the pairing methods depending on the IO capabilities of the devices.
Note that the initiator is the device that starts the pairing process (usually the client) and the responder is
the device that accepts the pairing request (usually the server).

![Pairing Methods Compatibility Matrix](https://www.bluetooth.com/wp-content/uploads/2016/06/screen-shot-06-08-16-at-0124-pm.png)

### Bluedroid vs NimBLE

Bluedroid and NimBLE are two different Bluetooth stack implementations.

#### Bluedroid

Bluedroid is the default Bluetooth stack in ESP-IDF. It supports both Bluetooth Classic and Bluetooth LE. It is used by the ESP32 in the Arduino Core.

Bluedroid requires more flash and RAM than NimBLE. In the refactored API, security properties are configured uniformly via `BLEPermission` flags on descriptors and `BLEProperty` flags on characteristics for both stacks.

The original source of the Bluedroid project, **which is not maintained anymore**, can be found here: https://github.com/nkolban/esp32-snippets

**Bluedroid will be replaced by NimBLE in version 4.0.0 of the Arduino Core. Bluetooth Classic and Bluedroid will no longer be supported but can be used by using Arduino as an ESP-IDF component.**

#### NimBLE

NimBLE is a lightweight Bluetooth stack for Bluetooth LE only. It is used by all SoCs that are not the ESP32.

NimBLE requires less flash and RAM than Bluedroid. Access permissions for characteristics and descriptors are configured through property and permission flags using the unified `BLEProperty` and `BLEPermission` enum classes.

Some parts of the NimBLE implementation are based on the work of h2zero, which can be found here: https://github.com/h2zero/NimBLE-Arduino. For a more customizable and feature-rich implementation of the NimBLE stack, you can use the [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) library.

### Common Scenarios

Here are some common scenarios for the pairing methods depending on the IO capabilities of the devices. Check also the secure BLE examples in the ESP32 Arduino Core for more detailed usage examples.

#### Scenario 1: Mobile App <-> ESP32 Sensor Node
- **Devices**: Mobile (`DisplayYesNo`) <-> ESP32 Sensor (`NoInputNoOutput`)
- **MITM**: Not achievable with this IO combination (falls back to Just Works)
- **Characteristic Authentication**: Bonding only
- **Result**: Just Works with bonding for reconnection
- **Use Case**: Weather stations, environmental monitors

#### Scenario 2: ESP32 Smart Lock <-> Mobile App
- **Devices**: ESP32 Lock (`DisplayOnly`) <-> Mobile (`KeyboardDisplay`)
- **MITM**: Required for security
- **Characteristic Authentication**: Bonding + Secure Connection + MITM
- **Result**: Passkey Entry (ESP32 displays, mobile enters)
- **Implementation**: Static passkey or dynamic display

#### Scenario 3: ESP32 Configuration Device <-> Admin Tool
- **Devices**: ESP32 (`KeyboardDisplay`) <-> Admin Tool (`KeyboardDisplay`)
- **MITM**: Required for configuration security
- **Characteristic Authentication**: Bonding + Secure Connection + MITM
- **Result**:
  - Legacy: Passkey Entry
  - Secure Connections: Numeric Comparison
- **Use Case**: Industrial IoT configuration, network setup

#### Scenario 4: ESP32 Beacon <-> Scanner App
- **Devices**: ESP32 Beacon (`NoInputNoOutput`) <-> Scanner (`DisplayYesNo`)
- **MITM**: Not required (broadcast only)
- **Characteristic Authentication**: None
- **Result**: No pairing required
- **Use Case**: Asset tracking, proximity detection

#### Scenario 5: ESP32 Smart Home Hub <-> Multiple Devices
- **Devices**: ESP32 Hub (`DisplayYesNo`) <-> Various sensors (`NoInputNoOutput`)
- **MITM**: Not possible when any of the peers are `NoInputNoOutput`
- **Characteristic Authentication**: Bonding only (no MITM possible with `NoInputNoOutput`)
- **Result**: Just Works only
- **Use Case**: Centralized home automation controller

### Implementation Guidelines

#### For ESP32 Device Developers

##### Choosing IO Capabilities
```cpp
#include <BLE.h>

BLESecurity sec = BLE.getSecurity();

// Conservative approach - limits pairing methods but ensures compatibility
sec.setIOCapability(NoInputNoOutput);    // Just Works only

// Balanced approach - good UX with optional security
sec.setIOCapability(DisplayYesNo);       // Just Works or Numeric Comparison

// Maximum security - supports all methods
sec.setIOCapability(KeyboardDisplay);    // All pairing methods available
```

##### Authentication Configuration
```cpp
BLESecurity sec = BLE.getSecurity();

// Low security applications (sensors, environmental monitoring)
sec.setAuthenticationMode(false, false, false);  // No bonding, no MITM, no SC

// Standard security with bonding (smart home devices)
sec.setAuthenticationMode(true, false, false);   // Bonding only

// MITM protection required (access control, payments)
sec.setAuthenticationMode(true, true, false);    // Bonding + MITM

// Maximum security with Secure Connections (critical systems)
sec.setAuthenticationMode(true, true, true);     // Bonding + MITM + Secure Connections
```

##### Static Passkey Example (from secure examples)
```cpp
#include <BLE.h>

BLESecurity sec = BLE.getSecurity();

// Set a static passkey for consistent pairing experience
sec.setStaticPassKey(123456);
sec.setIOCapability(KeyboardDisplay);       // Required for MITM even with static passkey
sec.setAuthenticationMode(true, true, true); // Bonding + MITM + SC
```

### Security Considerations

#### When to Require MITM
- **Always**: Payment systems, medical devices, access control
- **Usually**: File transfers, personal data sync, keyboards
- **Optional**: Fitness trackers, environmental sensors, mice
- **Never**: Beacons, broadcast-only devices

#### Legacy vs Secure Connections
- **Legacy**: Compatible with all BLE devices (2010+)
- **Secure Connections**: Better security but requires Bluetooth 4.2+ (2014+)
- **Recommendation**: Support both, prefer Secure Connections when available

#### Configuring Secure Characteristics
```cpp
#include <BLE.h>

// Create a characteristic that requires encryption + MITM on both directions.
// Use one of the BLEPermissions:: presets for a clear intent, or combine
// raw BLEPermission bits manually for custom setups.
BLECharacteristic chr = svc.createCharacteristic(
    CHAR_UUID,
    BLEProperty::Read | BLEProperty::Write,
    BLEPermissions::AuthenticatedReadWrite
);

// Check which stack is running
Serial.printf("Using BLE stack: %s\n", BLE.getStackName());
```

#### Known Vulnerabilities
1. **Just Works**: Vulnerable to passive eavesdropping during initial pairing
2. **Legacy Pairing**: Uses weaker cryptographic algorithms
3. **Passkey Brute Force**: 6-digit passkeys have only 1M combinations
4. **Physical Security**: Displayed passkeys can be shoulder-surfed

### Troubleshooting

#### Common Issues

##### Pairing Always Uses Just Works
```cpp
// Problem: Missing MITM flag
sec.setAuthenticationMode(true, false, true);  // Bonding + SC but no MITM

// Solution: Add MITM protection
sec.setAuthenticationMode(true, true, true);   // Bonding + MITM + SC
```

##### Static passkey not being requested / Nothing happens when trying to read secure characteristic
```cpp
// Problem: Wrong IO capability for MITM
sec.setIOCapability(NoInputNoOutput);  // Can't support MITM

// Solution: Set proper capability even for static passkey
sec.setIOCapability(KeyboardDisplay);  // Required for MITM
sec.setStaticPassKey(123456);
```

##### Secure Characteristic Access Fails
```cpp
// Solution: declare the required permission level up front
BLECharacteristic chr = svc.createCharacteristic(
    CHAR_UUID,
    BLEProperty::Read | BLEProperty::Write,
    BLEPermissions::AuthenticatedReadWrite
);
```

##### Pairing Works Once, Then Fails (NVS Cache Issue)
```cpp
// Solution: Clear NVS for testing/development
Serial.println("Clearing NVS pairing data for testing...");
nvs_flash_erase();
nvs_flash_init();
```

##### Connection drops during pairing
```cpp
#include <BLE.h>

// Solution: Use security callbacks for better error handling
BLESecurity sec = BLE.getSecurity();
sec.onAuthenticationComplete([](const BLEConnInfo &conn, bool success) {
    if (success) {
        Serial.println("Pairing successful!");
    } else {
        Serial.println("Pairing failed!");
    }
});
```

### Complete Properties and Permissions Reference

#### BLEProperty (Characteristic Properties)

Properties define the capabilities of a characteristic. Combine with bitwise OR.

```cpp
// Standard Bluetooth properties
BLEProperty::Read          // Read operation
BLEProperty::Write         // Write operation
BLEProperty::WriteNR       // Write without response
BLEProperty::Notify        // Notifications
BLEProperty::Indicate      // Indications
BLEProperty::Broadcast     // Broadcast
BLEProperty::SignedWrite   // Signed write command
BLEProperty::ExtendedProps // Extended properties
```

#### BLEPermission (Access Permissions)

Permissions define the security requirements for accessing an attribute. The
mapping is fail-closed: a `BLEProperty::Read` or write property is only
advertised to the client if a matching permission direction is also declared.
Use `BLEPermission::None` for notify/indicate-only characteristics (no GATT
read/write path).

Combine with bitwise OR:

```cpp
// Basic (no security) — open access at the GATT layer
BLEPermission::Read                  // Plain read (open)
BLEPermission::Write                 // Plain write (open)

// Encryption required
BLEPermission::ReadEncrypted         // Read requires encryption
BLEPermission::WriteEncrypted        // Write requires encryption

// Authentication required (MITM protection)
BLEPermission::ReadAuthenticated     // Read requires encryption + MITM
BLEPermission::WriteAuthenticated    // Write requires encryption + MITM

// Authorization required
BLEPermission::ReadAuthorized        // Read requires authorization callback
BLEPermission::WriteAuthorized       // Write requires authorization callback
```

##### Semantic presets (`BLEPermissions::`)

Self-documenting names for the common cases, encoding both direction
(Read / Write / ReadWrite) and security level:

```cpp
BLEPermissions::OpenRead              BLEPermissions::OpenWrite              BLEPermissions::OpenReadWrite
BLEPermissions::EncryptedRead         BLEPermissions::EncryptedWrite         BLEPermissions::EncryptedReadWrite
BLEPermissions::AuthenticatedRead     BLEPermissions::AuthenticatedWrite     BLEPermissions::AuthenticatedReadWrite
BLEPermissions::AuthorizedRead        BLEPermissions::AuthorizedWrite        BLEPermissions::AuthorizedReadWrite
```

#### Usage Examples by Security Level

##### No Security
```cpp
BLECharacteristic chr = svc.createCharacteristic(
    CHAR_UUID,
    BLEProperty::Read | BLEProperty::Write,
    BLEPermissions::OpenReadWrite
);
```

##### Encryption Only
```cpp
BLECharacteristic chr = svc.createCharacteristic(
    CHAR_UUID,
    BLEProperty::Read | BLEProperty::Write,
    BLEPermissions::EncryptedReadWrite
);
```

##### MITM Protection (Authentication)
```cpp
BLECharacteristic chr = svc.createCharacteristic(
    CHAR_UUID,
    BLEProperty::Read | BLEProperty::Write,
    BLEPermissions::AuthenticatedReadWrite
);
```

##### Authorization Required
```cpp
BLECharacteristic chr = svc.createCharacteristic(
    CHAR_UUID,
    BLEProperty::Read | BLEProperty::Write,
    BLEPermissions::AuthorizedReadWrite
);
```

#### Debug Tips
1. **Log pairing features** exchanged between devices
2. **Monitor pairing method** selected by the stack
3. **Check timeout values** for user input methods
4. **Verify key distribution** flags match on both sides

### Standards References

- **Bluetooth Core Specification v5.4**: Volume 3, Part H (Security Manager)
- **Bluetooth Assigned Numbers**: IO Capability values
- **FIPS-140-2**: Cryptographic standards for Secure Connections

---

*This guide is based on the ESP32 Arduino BLE library implementation and the official Bluetooth Core Specification. For the latest API documentation, refer to the ESP32 Arduino BLE library source code and examples.*
