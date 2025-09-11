# BLE for ESP32 Arduino Core

A comprehensive reference for ESP32 Bluetooth Low Energy (BLE) pairing and security implementation using the ESP32 Arduino BLE library.

## Overview

This guide provides ESP32 developers with comprehensive information about BLE security implementation using the ESP32 Arduino BLE library. It covers both Bluedroid (ESP32) and NimBLE (other SoCs) implementations with realistic scenarios and troubleshooting guidance.

Issues and questions should be raised here: https://github.com/espressif/arduino-esp32/issues <br> (please don't use https://github.com/nkolban/esp32-snippets/issues or https://github.com/h2zero/NimBLE-Arduino/issues)

## Security

### Quick Start

1. **Choose your ESP32's IO capabilities** using `ESP_IO_CAP_*` constants
2. **Configure authentication requirements** with properties or permissions
3. **Set up security** using `BLESecurity` class methods
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

| Capability | Library Constant | Can Display | Can Input | Can Confirm | Example Devices |
|------------|-----------------|-------------|-----------|-------------|-----------------|
| **No Input No Output** | `ESP_IO_CAP_NONE` | ❌ | ❌ | ❌ | Sensor nodes, beacons, simple actuators |
| **Display Only** | `ESP_IO_CAP_OUT` | ✅ | ❌ | ❌ | E-ink displays, LED matrix displays |
| **Keyboard Only** | `ESP_IO_CAP_IN` | ❌ | ✅ | ❌ | Button-only devices, rotary encoders |
| **Display Yes/No** | `ESP_IO_CAP_IO` | ✅ | ❌ | ✅ | Devices with display + confirmation button |
| **Keyboard Display** | `ESP_IO_CAP_KBDISP` | ✅ | ✅ | ✅ | Full-featured ESP32 devices with UI |

### Pairing Methods Explained

#### 🔓 Just Works
- **Security**: Encryption only (no MITM protection)
- **User Experience**: Automatic, no user interaction
- **Use Case**: Convenience over security (fitness trackers, mice)
- **Vulnerability**: Susceptible to passive eavesdropping during pairing

#### 🔐 Passkey Entry
- **Security**: Full MITM protection
- **User Experience**: One device shows 6-digit code, other inputs it
- **Use Case**: Keyboards pairing to computers
- **Process**:
  1. Display device shows random 6-digit number (000000-999999)
  2. Input device user types the number
  3. Pairing succeeds if numbers match

#### 🔐 Numeric Comparison (Secure Connections Only)
- **Security**: Full MITM protection
- **User Experience**: Both devices show same number, user confirms match
- **Use Case**: Two smartphones/tablets pairing
- **Process**:
  1. Both devices display identical 6-digit number
  2. User verifies numbers match on both screens
  3. User confirms "Yes" on both devices

#### 🔐 Out-of-Band (OOB) (Not supported by this library)
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

Bluedroid requires more flash and RAM than NimBLE and access permissions for characteristics and descriptors are set using a specific API through the `setAccessPermissions()` function.

The original source of the Bluedroid project, **which is not maintained anymore**, can be found here: https://github.com/nkolban/esp32-snippets

**Bluedroid will be replaced by NimBLE in version 4.0.0 of the Arduino Core. Bluetooth Classic and Bluedroid will no longer be supported but can be used by using Arduino as an ESP-IDF component.**

#### NimBLE

NimBLE is a lightweight Bluetooth stack for Bluetooth LE only. It is used by all SoCs that are not the ESP32.

NimBLE requires less flash and RAM than Bluedroid. Access permissions for characteristics are set using exclusive properties in the characteristic creation. Access permissions for descriptors are set using the `setAccessPermissions()` function just like in Bluedroid.

Some parts of the NimBLE implementation are based on the work of h2zero, which can be found here: https://github.com/h2zero/NimBLE-Arduino. For a more customizable and feature-rich implementation of the NimBLE stack, you can use the [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) library.

### Common Scenarios

Here are some common scenarios for the pairing methods depending on the IO capabilities of the devices. Check also the secure BLE examples in the ESP32 Arduino Core for more detailed usage examples.

#### Scenario 1: Mobile App ↔ ESP32 Sensor Node
- **Devices**: Mobile (`ESP_IO_CAP_IO`) ↔ ESP32 Sensor (`ESP_IO_CAP_NONE`)
- **MITM**: Not achievable with this IO combination (falls back to Just Works)
- **Characteristic Authentication**: Bonding only
- **Result**: Just Works with bonding for reconnection
- **Use Case**: Weather stations, environmental monitors

#### Scenario 2: ESP32 Smart Lock ↔ Mobile App
- **Devices**: ESP32 Lock (`ESP_IO_CAP_OUT`) ↔ Mobile (`ESP_IO_CAP_KBDISP`)
- **MITM**: Required for security
- **Characteristic Authentication**: Bonding + Secure Connection + MITM
- **Result**: Passkey Entry (ESP32 displays, mobile enters)
- **Implementation**: Static passkey or dynamic display

#### Scenario 3: ESP32 Configuration Device ↔ Admin Tool
- **Devices**: ESP32 (`ESP_IO_CAP_KBDISP`) ↔ Admin Tool (`ESP_IO_CAP_KBDISP`)
- **MITM**: Required for configuration security
- **Characteristic Authentication**: Bonding + Secure Connection + MITM
- **Result**:
  - Legacy: Passkey Entry
  - Secure Connections: Numeric Comparison
- **Use Case**: Industrial IoT configuration, network setup

#### Scenario 4: ESP32 Beacon ↔ Scanner App
- **Devices**: ESP32 Beacon (`ESP_IO_CAP_NONE`) ↔ Scanner (`ESP_IO_CAP_IO`)
- **MITM**: Not required (broadcast only)
- **Characteristic Authentication**: None
- **Result**: No pairing required
- **Use Case**: Asset tracking, proximity detection

#### Scenario 5: ESP32 Smart Home Hub ↔ Multiple Devices
- **Devices**: ESP32 Hub (`ESP_IO_CAP_IO`) ↔ Various sensors (`ESP_IO_CAP_NONE`)
- **MITM**: Not possible when any of the peers are `ESP_IO_CAP_NONE`
- **Characteristic Authentication**: Bonding only (no MITM possible with `ESP_IO_CAP_NONE`)
- **Result**: Just Works only
- **Use Case**: Centralized home automation controller

### Implementation Guidelines

#### For ESP32 Device Developers

##### Choosing IO Capabilities
```cpp
#include <BLESecurity.h>

// Conservative approach - limits pairing methods but ensures compatibility
pSecurity->setCapability(ESP_IO_CAP_NONE);    // Just Works only

// Balanced approach - good UX with optional security
pSecurity->setCapability(ESP_IO_CAP_IO);      // Just Works or Numeric Comparison

// Maximum security - supports all methods
pSecurity->setCapability(ESP_IO_CAP_KBDISP);  // All pairing methods available
```

##### Authentication Configuration
```cpp
BLESecurity *pSecurity = new BLESecurity();

// Low security applications (sensors, environmental monitoring)
pSecurity->setAuthenticationMode(ESP_LE_AUTH_NO_BOND);

// Standard security with bonding (smart home devices)
pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

// MITM protection required (access control, payments)
pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_MITM | ESP_LE_AUTH_BOND);

// Maximum security with Secure Connections (critical systems)
pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);

// Alternative syntax (more readable for complex requirements)
pSecurity->setAuthenticationMode(true, true, true);  // Bonding, MITM, Secure Connections
```

##### Static Passkey Example (from secure examples)
```cpp
// Set a static passkey for consistent pairing experience
#define DEVICE_PASSKEY 123456
pSecurity->setPassKey(true, DEVICE_PASSKEY);  // static=true, passkey=123456
pSecurity->setCapability(ESP_IO_CAP_KBDISP);  // Required for MITM even with static passkey
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

#### Implementation Differences
```cpp
// Basic characteristic properties (both stacks)
uint32_t properties = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE;

// NimBLE: Add authentication properties (ignored by Bluedroid)
properties |= BLECharacteristic::PROPERTY_READ_AUTHEN | BLECharacteristic::PROPERTY_WRITE_AUTHEN;

BLECharacteristic *pCharacteristic = pService->createCharacteristic(CHAR_UUID, properties);

// Bluedroid: Set access permissions (ignored by NimBLE)
pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);

// Check which stack is running
String stackType = BLEDevice::getBLEStackString();
Serial.println("Using BLE stack: " + stackType);
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
// ❌ Problem: Missing MITM flag
pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);

// ✅ Solution: Add MITM protection
pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
```

##### Static passkey not being requested / Nothing happens when trying to read secure characteristic
```cpp
// ❌ Problem: Wrong IO capability for MITM
pSecurity->setCapability(ESP_IO_CAP_NONE);  // Can't support MITM

// ✅ Solution: Set proper capability even for static passkey
pSecurity->setCapability(ESP_IO_CAP_KBDISP);  // Required for MITM
pSecurity->setPassKey(true, 123456);
```

##### Secure Characteristic Access Fails
```cpp
// ❌ Problem: Wrong security method for stack
// Bluedroid approach (won't work on NimBLE)
uint32_t properties = BLECharacteristic::PROPERTY_READ;
pCharacteristic = pService->createCharacteristic(uuid, properties);
pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM);  // Ignored by NimBLE!

// ✅ Solution: Use both methods for cross-compatibility
uint32_t properties = BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_READ_AUTHEN;  // For NimBLE
pCharacteristic = pService->createCharacteristic(uuid, properties);
pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM);  // For Bluedroid
```

##### Pairing Works Once, Then Fails (NVS Cache Issue)
```cpp
// ✅ Solution: Clear NVS for testing/development
Serial.println("Clearing NVS pairing data for testing...");
nvs_flash_erase();
nvs_flash_init();
```

##### Default Passkey Warning
```
*WARNING* Using default passkey: 123456
*WARNING* Please use a random passkey or set a different static passkey
```
```cpp
// ✅ Solution: Change from default
#define CUSTOM_PASSKEY 567890  // Your unique passkey
pSecurity->setPassKey(true, CUSTOM_PASSKEY);
```

##### Connection drops during pairing
```cpp
// ✅ Solution: Implement security callbacks for better error handling
class MySecurityCallbacks : public BLESecurityCallbacks {
  void onAuthenticationComplete(esp_ble_auth_cmpl_t param) override {
    if (param.success) {
      Serial.println("Pairing successful!");
    } else {
      Serial.printf("Pairing failed, reason: %d\n", param.fail_reason);
    }
  }
};

BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
```

#### Cross-Platform Best Practice
```cpp
// Always use both methods for maximum compatibility
uint32_t secure_properties = BLECharacteristic::PROPERTY_READ |
                             BLECharacteristic::PROPERTY_WRITE |
                             BLECharacteristic::PROPERTY_READ_AUTHEN |   // NimBLE
                             BLECharacteristic::PROPERTY_WRITE_AUTHEN;   // NimBLE

BLECharacteristic *pChar = pService->createCharacteristic(uuid, secure_properties);

// Bluedroid permissions (ignored by NimBLE, but doesn't hurt)
pChar->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
```

### Complete Properties and Permissions Reference

#### Bluedroid

Bluedroid uses properties to define the capabilities of a characteristic and permissions to define the access permissions. NimBLE will ignore the access permissions.

##### Supported Properties
```cpp
BLECharacteristic::PROPERTY_READ        // Read operation
BLECharacteristic::PROPERTY_WRITE       // Write operation
BLECharacteristic::PROPERTY_WRITE_NR    // Write without response
BLECharacteristic::PROPERTY_NOTIFY      // Notifications
BLECharacteristic::PROPERTY_INDICATE    // Indications
BLECharacteristic::PROPERTY_BROADCAST   // Broadcast
```

##### Characteristic and Descriptor Access Permissions
```cpp
// Basic permissions
ESP_GATT_PERM_READ                  // Read allowed
ESP_GATT_PERM_WRITE                 // Write allowed

// Encryption required
ESP_GATT_PERM_READ_ENCRYPTED        // Read requires encryption
ESP_GATT_PERM_WRITE_ENCRYPTED       // Write requires encryption

// Authentication required (MITM protection)
ESP_GATT_PERM_READ_ENC_MITM         // Read requires encryption + MITM
ESP_GATT_PERM_WRITE_ENC_MITM        // Write requires encryption + MITM

// Authorization required
ESP_GATT_PERM_READ_AUTHORIZATION    // Read requires authorization callback
ESP_GATT_PERM_WRITE_AUTHORIZATION   // Write requires authorization callback
```

#### NimBLE

NimBLE uses properties to define both the capabilities of a characteristic and the access permissions. Bluedroid will ignore the NimBLE exclusive properties.

##### Supported Properties
```cpp
// Basic properties
BLECharacteristic::PROPERTY_READ        // Read operation
BLECharacteristic::PROPERTY_WRITE       // Write operation
BLECharacteristic::PROPERTY_WRITE_NR    // Write without response
BLECharacteristic::PROPERTY_NOTIFY      // Notifications
BLECharacteristic::PROPERTY_INDICATE    // Indications
BLECharacteristic::PROPERTY_BROADCAST   // Broadcast

// NimBLE specific properties

// Encryption required
BLECharacteristic::PROPERTY_READ_ENC    // Read requires encryption
BLECharacteristic::PROPERTY_WRITE_ENC   // Write requires encryption

// Authentication required (MITM protection)
BLECharacteristic::PROPERTY_READ_AUTHEN   // Read requires encryption + MITM protection
BLECharacteristic::PROPERTY_WRITE_AUTHEN  // Write requires encryption + MITM protection

// Authorization required
BLECharacteristic::PROPERTY_READ_AUTHOR   // Read requires authorization callback
BLECharacteristic::PROPERTY_WRITE_AUTHOR  // Write requires authorization callback
```

##### Descriptor Access Permissions
```cpp
// Basic permissions
ESP_GATT_PERM_READ                  // Read allowed
ESP_GATT_PERM_WRITE                 // Write allowed

// Encryption required
ESP_GATT_PERM_READ_ENCRYPTED        // Read requires encryption
ESP_GATT_PERM_WRITE_ENCRYPTED       // Write requires encryption

// Authentication required (MITM protection)
ESP_GATT_PERM_READ_ENC_MITM         // Read requires encryption + MITM
ESP_GATT_PERM_WRITE_ENC_MITM        // Write requires encryption + MITM

// Authorization required
ESP_GATT_PERM_READ_AUTHORIZATION    // Read requires authorization callback
ESP_GATT_PERM_WRITE_AUTHORIZATION   // Write requires authorization callback
```

#### Usage Examples by Security Level

##### No Security (Both Stacks)
```cpp
uint32_t properties = BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE;
```

##### Encryption Only
```cpp
// NimBLE
uint32_t properties = BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_READ_ENC |
                      BLECharacteristic::PROPERTY_WRITE_ENC;

// Bluedroid
uint32_t properties = BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE;
pChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
```

##### MITM Protection (Authentication)
```cpp
// NimBLE
uint32_t properties = BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_READ_AUTHEN |
                      BLECharacteristic::PROPERTY_WRITE_AUTHEN;

// Bluedroid
uint32_t properties = BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE;
pChar->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
```

##### Authorization Required
```cpp
// NimBLE
uint32_t properties = BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_READ_AUTHOR |
                      BLECharacteristic::PROPERTY_WRITE_AUTHOR;

// Bluedroid
uint32_t properties = BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE;
pChar->setAccessPermissions(ESP_GATT_PERM_READ_AUTHORIZATION | ESP_GATT_PERM_WRITE_AUTHORIZATION);
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
