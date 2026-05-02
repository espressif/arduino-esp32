# BLE Library Migration Guide: v3.x to v4.0

This guide helps you migrate existing sketches from the v3.x `BLEDevice`-based API to the v4.0 `BLE`-based API. It covers every conceptual change, provides complete API mapping tables, before/after code examples for common use cases, and a section for users coming from ArduinoBLE.

---

## Table of Contents

- [Key Conceptual Changes](#key-conceptual-changes)
  - [1. Global Singleton](#1-global-singleton)
  - [2. Shared Handles (No More Raw Pointers)](#2-shared-handles-no-more-raw-pointers)
  - [3. Callbacks (No More Abstract Classes)](#3-callbacks-no-more-abstract-classes)
  - [4. Error Handling (BTStatus)](#4-error-handling-btstatus)
  - [5. Properties and Permissions Separated](#5-properties-and-permissions-separated)
  - [6. Stack-Agnostic (No More #ifdef)](#6-stack-agnostic-no-more-ifdef)
  - [7. Unified Address Type](#7-unified-address-type)
  - [8. Numbered Descriptor Subclasses Replaced](#8-numbered-descriptor-subclasses-replaced)
  - [9. Server Must Be Started](#9-server-must-be-started)
- [Step-by-Step Migration Procedure](#step-by-step-migration-procedure)
- [Complete API Mapping](#complete-api-mapping)
  - [Includes](#includes)
  - [Lifecycle and Configuration](#lifecycle-and-configuration)
  - [GATT Server](#gatt-server)
  - [Server Callbacks](#server-callbacks)
  - [Characteristics](#characteristics)
  - [Characteristic Callbacks](#characteristic-callbacks)
  - [Characteristic Properties](#characteristic-properties)
  - [Descriptors](#descriptors)
  - [GATT Client](#gatt-client)
  - [Client Callbacks](#client-callbacks)
  - [Remote GATT Objects](#remote-gatt-objects)
  - [Scanning](#scanning)
  - [Scan Callbacks](#scan-callbacks)
  - [Advertised Device](#advertised-device)
  - [Advertising](#advertising)
  - [Security](#security)
  - [Security Callbacks](#security-callbacks)
  - [Bond Management](#bond-management)
  - [Whitelist](#whitelist)
  - [IRK (Identity Resolving Key)](#irk-identity-resolving-key)
  - [Address Type](#address-type)
  - [Beacons and Eddystone](#beacons-and-eddystone)
  - [HID](#hid)
- [Before/After Examples](#beforeafter-examples)
  - [Simple Server](#simple-server)
  - [Simple Client](#simple-client)
  - [Scan for Devices](#scan-for-devices)
  - [Secure Server (Passkey)](#secure-server-passkey)
  - [Notifications with Callback](#notifications-with-callback)
  - [Descriptor Usage (BLE2902/BLE2904)](#descriptor-usage-ble2902ble2904)
  - [iBeacon](#ibeacon)
  - [HID Keyboard](#hid-keyboard)
- [Everything That Was Removed](#everything-that-was-removed)
- [New Features in v4.0](#new-features-in-v40)
- [Common Pitfalls](#common-pitfalls)
- [NimBLE-Specific Migration Notes](#nimble-specific-migration-notes)
- [Bluedroid-Specific Migration Notes](#bluedroid-specific-migration-notes)
- [ArduinoBLE Porting Guide](#arduinoble-porting-guide)

---

## Key Conceptual Changes

### 1. Global Singleton

The entry point changed from `BLEDevice` (a class with static methods) to `BLE` (a pre-instantiated global object, like `WiFi` or `Serial`).

```cpp
// v3.x
BLEDevice::init("MyDevice");
BLEDevice::deinit(true);
BLEDevice::getAddress();

// v4.0
BLE.begin("MyDevice");
BLE.end(true);
BLE.getAddress();
```

### 2. Shared Handles (No More Raw Pointers)

All factory and lookup methods now return objects **by value**. Each public class is a lightweight handle wrapping a `std::shared_ptr` internally. Use `.` instead of `->`.

```cpp
// v3.x -- raw pointers, unclear ownership
BLEServer *pServer = BLEDevice::createServer();
BLEService *pService = pServer->createService("180A");
BLECharacteristic *pChar = pService->createCharacteristic("2A29", ...);
pChar->setValue("Hello");

// v4.0 -- value types, shared ownership
BLEServer server = BLE.createServer();
BLEService svc = server.createService("180A");
BLECharacteristic chr = svc.createCharacteristic("2A29",
    BLEProperty::Read, BLEPermissions::OpenRead);
chr.setValue("Hello");
```

**Key behaviors:**
- **Copying** a handle creates shared ownership (both refer to the same underlying resource)
- **Moving** transfers ownership (source becomes null)
- **Default-constructed** handles are null -- test with `if (!server) { ... }`
- No need to `delete` anything -- cleanup is automatic

### 3. Callbacks (No More Abstract Classes)

Abstract callback classes (`BLEServerCallbacks`, `BLECharacteristicCallbacks`, etc.) are replaced by per-event `std::function` lambdas. No more `new MyCallbacks()` leaking memory.

```cpp
// v3.x -- abstract class, heap allocation, memory leak
class MyCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) override { Serial.println("Connected"); }
    void onDisconnect(BLEServer *pServer) override { Serial.println("Disconnected"); }
};
pServer->setCallbacks(new MyCallbacks());  // leaked!

// v4.0 -- lambdas, no allocation
server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
    Serial.println("Connected");
});
server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
    Serial.printf("Disconnected, reason: 0x%02X\n", reason);
});
```

Note: callbacks now receive `BLEConnInfo` (a stack-agnostic connection descriptor) instead of backend-specific structs.

### 4. Error Handling (BTStatus)

Most operations now return `BTStatus` instead of `void` or `bool`. Check with `operator!()` or compare to specific codes.

```cpp
// v3.x -- no error feedback
BLEDevice::init("name");  // void, silent failure
pClient->connect(device);  // bool

// v4.0 -- structured error handling
BTStatus status = BLE.begin("name");
if (!status) {
    Serial.printf("Init failed: %s\n", status.toString());
}

status = client.connect(address);
if (status == BTStatus::Timeout) {
    Serial.println("Connection timed out");
}
```

### 5. Properties and Permissions Separated

In v3.x, properties and permissions were often combined or confused. v4.0 separates them into distinct enums and requires the caller to declare both up front:

- `BLEProperty` -- what the characteristic **can do** (Read, Write, Notify, etc.)
- `BLEPermission` -- what **security level** is required to access it

The mapping is fail-closed: a `Read` or write property is only advertised if a matching permission direction is declared. For notify/indicate-only characteristics use `BLEPermission::None`. Prefer a semantic preset from `BLEPermissions::` for the common cases.

```cpp
// v3.x -- combined, inconsistent across stacks
pService->createCharacteristic("uuid",
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
// Bluedroid permissions:
pChar->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM);
// NimBLE alternative:
pService->createCharacteristic("uuid",
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_AUTHEN);

// v4.0 -- clean separation, same on both stacks, permissions required
auto chr = svc.createCharacteristic("uuid",
    BLEProperty::Read | BLEProperty::Write,
    BLEPermissions::EncryptedReadWrite);

// ...or use the raw bits directly for fine-grained control:
auto chr2 = svc.createCharacteristic("uuid2",
    BLEProperty::Read | BLEProperty::Write,
    BLEPermission::ReadEncrypted | BLEPermission::WriteAuthenticated);
```

### 6. Stack-Agnostic (No More #ifdef)

In v3.x, users had to write `#ifdef` blocks for NimBLE vs Bluedroid differences. v4.0 eliminates this entirely.

```cpp
// v3.x -- required in many sketches
#ifdef CONFIG_BT_NIMBLE_ENABLED
  #include <NimBLEDevice.h>
  // NimBLE-specific callback signatures, property macros...
#else
  #include <BLEDevice.h>
  // Bluedroid-specific callback signatures, permission macros...
#endif

// v4.0 -- one include, one API, both stacks
#include <BLE.h>
```

### 7. Unified Address Type

`BLEAddress` (library-specific) is replaced by `BTAddress` (shared between BLE and BT Classic, defined in `cores/esp32/`).

```cpp
// v3.x
BLEAddress addr = device->getAddress();
std::string str = addr.toString();  // std::string

// v4.0
BTAddress addr = dev.getAddress();
String str = addr.toString();  // Arduino String
```

### 8. Numbered Descriptor Subclasses Replaced

Numbered descriptor subclasses (`BLE2901`, `BLE2902`, `BLE2904`) are replaced by a unified `BLEDescriptor` class with static factories and convenience methods.

```cpp
// v3.x
BLE2902 *p2902 = new BLE2902();
pChar->addDescriptor(p2902);

BLE2904 *p2904 = new BLE2904();
p2904->setFormat(BLE2904::FORMAT_UTF8);
pChar->addDescriptor(p2904);

// v4.0
// CCCD is auto-created for Notify/Indicate characteristics!
// For manual creation:
BLEDescriptor cccd = BLEDescriptor::createCCCD();
BLEDescriptor fmt = BLEDescriptor::createPresentationFormat();
fmt.setFormat(BLEDescriptor::FORMAT_UTF8);
chr.setDescription("My Characteristic");  // creates 0x2901 automatically
```

### 9. Server Must Be Started

v4.0 requires an explicit `server.start()` call after creating all services and characteristics. This commits the GATT database to the stack.

```cpp
// v3.x -- services started individually, no explicit server start
pService->start();
BLEDevice::startAdvertising();

// v4.0 -- server started after creating all services/characteristics
server.start();  // commits GATT database
BLE.startAdvertising();
```

---

## Step-by-Step Migration Procedure

Follow these steps to convert a v3.x sketch to v4.0:

1. **Replace includes:**
   - `#include <BLEDevice.h>` → `#include <BLE.h>`
   - Keep `#include <BLEServer.h>`, `<BLEClient.h>`, `<BLEScan.h>` etc. as needed -- `BLE.h` provides the `BLE` singleton but forward-declares other classes; you still need to include the headers for any classes whose methods you call
   - Remove `<BLEUtils.h>`, `<BLE2902.h>`, `<BLE2904.h>` (no longer exist)
   - Remove all `#ifdef CONFIG_BT_NIMBLE_ENABLED` / `#ifdef CONFIG_BT_BLUEDROID_ENABLED` blocks

2. **Replace BLEDevice calls:**
   - `BLEDevice::init(name)` → `BLE.begin(name)` (check return value!)
   - `BLEDevice::deinit(release)` → `BLE.end(release)`
   - `BLEDevice::createServer()` → `BLE.createServer()` (returns value, not pointer)
   - `BLEDevice::createClient()` → `BLE.createClient()`
   - `BLEDevice::getScan()` → `BLE.getScan()`
   - `BLEDevice::getAdvertising()` → `BLE.getAdvertising()`
   - `BLEDevice::startAdvertising()` → `BLE.startAdvertising()`

3. **Replace all `->` with `.`** on BLE objects (server, service, characteristic, client, scan, etc.)

4. **Replace callback classes with lambdas:**
   - Delete `class MyCallbacks : public BLEServerCallbacks { ... };` definitions
   - Replace `pServer->setCallbacks(new MyCallbacks())` with individual `server.onConnect(...)`, `server.onDisconnect(...)` calls
   - Repeat for characteristic, client, scan, and security callbacks
   - Update callback signatures: parameters now use `BLEConnInfo` instead of backend-specific types

5. **Replace property constants:**
   - `BLECharacteristic::PROPERTY_READ` → `BLEProperty::Read`
   - `BLECharacteristic::PROPERTY_WRITE` → `BLEProperty::Write`
   - `BLECharacteristic::PROPERTY_NOTIFY` → `BLEProperty::Notify`
   - `BLECharacteristic::PROPERTY_INDICATE` → `BLEProperty::Indicate`
   - `BLECharacteristic::PROPERTY_WRITE_NR` → `BLEProperty::WriteNR`
   - See the [full mapping table](#characteristic-properties)

6. **Replace descriptor classes:**
   - Remove `BLE2902` / `BLE2904` / `BLE2901` usage
   - CCCD is auto-created for Notify/Indicate characteristics
   - Use `BLEDescriptor::createPresentationFormat()` for 0x2904
   - Use `chr.setDescription("text")` for 0x2901

7. **Add `server.start()` after creating all services/characteristics and before advertising**

8. **Replace `BLEAddress` with `BTAddress`** and `std::string` with `String`

9. **Replace security setup:**
   - `new BLESecurity()` → `BLE.getSecurity()`
   - `BLEDevice::setSecurityCallbacks(...)` → `sec.onConfirmPasskey(...)`, `sec.onAuthenticationComplete(...)`, etc.
   - `setStaticPIN(pin)` → `setStaticPassKey(pin)`

10. **Handle BTStatus returns** -- check return values of `begin()`, `connect()`, `writeValue()`, etc.

---

## Complete API Mapping

### Includes

| Old (v3.x) | New (v4.0) |
|---|---|
| `#include <BLEDevice.h>` | `#include <BLE.h>` |
| `#include <BLEServer.h>` | `#include <BLEServer.h>` (still needed for `BLEServer` / `BLEService` methods) |
| `#include <BLEUtils.h>` | Removed (not needed) |
| `#include <BLE2902.h>` | Removed (auto-created CCCD; or `#include <BLEDescriptor.h>`) |
| `#include <BLE2904.h>` | Removed (use `BLEDescriptor::createPresentationFormat()`) |
| `#include <BLESecurity.h>` | `#include <BLESecurity.h>` (still needed for `BLESecurity` methods) |
| `#include <BLEBeacon.h>` | `#include <BLEBeacon.h>` |
| `#include <BLEEddystoneURL.h>` | `#include <BLEEddystone.h>` (merged) |
| `#include <BLEEddystoneTLM.h>` | `#include <BLEEddystone.h>` (merged) |
| `#include <BLEHIDDevice.h>` | `#include <BLEHIDDevice.h>` |

### Lifecycle and Configuration

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `BLEDevice::init("name")` | `BLE.begin("name")` | Returns `BTStatus` |
| `BLEDevice::deinit(release)` | `BLE.end(release)` | |
| `BLEDevice::getAddress()` | `BLE.getAddress()` | Returns `BTAddress` |
| `BLEDevice::setPower(level)` | `BLE.setPower(dbm)` | Now in dBm directly |
| `BLEDevice::getPower()` | `BLE.getPower()` | Returns `int8_t` dBm |
| `BLEDevice::setMTU(mtu)` | `BLE.setMTU(mtu)` | Returns `BTStatus` |
| `BLEDevice::getMTU()` | `BLE.getMTU()` | |
| `BLEDevice::setOwnAddrType(type)` | `BLE.setOwnAddressType(type)` | Uses `BTAddress::Type` enum |
| `BLEDevice::createServer()` → `BLEServer*` | `BLE.createServer()` → `BLEServer` | Value, not pointer |
| `BLEDevice::createClient()` → `BLEClient*` | `BLE.createClient()` → `BLEClient` | Value; supports multiple instances |
| `BLEDevice::getScan()` → `BLEScan*` | `BLE.getScan()` → `BLEScan` | Value |
| `BLEDevice::getAdvertising()` → `BLEAdvertising*` | `BLE.getAdvertising()` → `BLEAdvertising` | Value |
| `BLEDevice::startAdvertising()` | `BLE.startAdvertising()` | Returns `BTStatus` |
| `BLEDevice::setCustomGapHandler(fn)` | `BLE.setCustomGapHandler(fn)` | |
| `BLEDevice::setCustomGattsHandler(fn)` | `BLE.setCustomGattsHandler(fn)` | |
| `BLEDevice::setCustomGattcHandler(fn)` | `BLE.setCustomGattcHandler(fn)` | |
| N/A | `BLE.getStack()` | New: identify active stack |
| N/A | `BLE.getStackName()` | New: "NimBLE" or "Bluedroid" |
| N/A | `BLE.isHostedBLE()` | New: ESP32-P4 hosted detection |
| N/A | `BLE.setPins(clk, cmd, d0-d3, rst)` | New: hosted BLE pin config |

### GATT Server

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `pServer->createService(uuid)` | `server.createService(uuid)` | Returns `BLEService` value |
| `pServer->getServiceByUUID(uuid)` | `server.getService(uuid)` | |
| N/A | `server.getServices()` | New: returns all services |
| `pServer->removeService(svc)` | `server.removeService(svc)` | Returns `BTStatus` |
| N/A | `server.start()` | **New and required** |
| `pServer->getConnectedCount()` | `server.getConnectedCount()` | |
| N/A | `server.getConnections()` | New: returns `vector<BLEConnInfo>` |
| `pServer->disconnect(connId)` | `server.disconnect(connHandle)` | |
| `pServer->advertiseOnDisconnect(bool)` | `server.advertiseOnDisconnect(bool)` | |
| `pServer->getPeerMTU(connId)` | `server.getPeerMTU(connHandle)` | |
| N/A | `server.updateConnParams(h, params)` | New |
| N/A | `server.setPhy(h, tx, rx)` | New: BLE5 PHY |
| N/A | `server.setDataLen(h, octets, time)` | New: DLE |

### Server Callbacks

| Old (v3.x) | New (v4.0) |
|---|---|
| `class MyCallbacks : public BLEServerCallbacks { void onConnect(BLEServer*) ... }` | `server.onConnect([](BLEServer s, const BLEConnInfo &conn) { ... })` |
| `void onDisconnect(BLEServer*)` | `server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) { ... })` |
| `void onMtuChanged(BLEServer*, esp_ble_gatts_cb_param_t*)` | `server.onMtuChanged([](BLEServer s, const BLEConnInfo &conn, uint16_t mtu) { ... })` |
| `pServer->setCallbacks(new MyCallbacks())` | Individual `server.on*(lambda)` calls |
| N/A | `server.onConnParamsUpdate(lambda)` |
| N/A | `server.onIdentity(lambda)` |

### Characteristics

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `pService->createCharacteristic(uuid, props)` → `BLECharacteristic*` | `svc.createCharacteristic(uuid, props, perms)` → `BLECharacteristic` | Value; `BLEProperty` + required `BLEPermission` |
| `pChar->setValue("hello")` | `chr.setValue("hello")` | |
| `pChar->setValue(data, len)` | `chr.setValue(data, len)` | |
| `pChar->setValue(uint32_t)` | `chr.setValue(uint32_t)` | Also: `uint16_t`, `int`, `float`, `double` |
| `pChar->getValue()` → `std::string` | `chr.getStringValue()` → `String` | |
| `pChar->getData()` → `uint8_t*` | `chr.getValue(&len)` → `const uint8_t*` | Returns length via pointer |
| `pChar->getLength()` | Length via `chr.getValue(&len)` | |
| `pChar->notify()` | `chr.notify()` | Returns `BTStatus` |
| `pChar->indicate()` | `chr.indicate()` | Returns `BTStatus` |
| N/A | `chr.notify(connHandle, data, len)` | New: per-connection |
| N/A | `chr.indicate(connHandle, data, len)` | New: per-connection |
| `pChar->getSubscribedCount()` | `chr.getSubscribedCount()` | |
| N/A | `chr.isSubscribed(connHandle)` | New |
| N/A | `chr.getSubscribedConnections()` | New |
| `pChar->getUUID()` | `chr.getUUID()` | Returns `BLEUUID` |
| N/A | `chr.getHandle()` | New |
| N/A | `chr.getService()` | New: returns parent service |
| `pChar->setDescription("text")` | `chr.setDescription("text")` | |
| N/A | `chr.toString()` | New |
| N/A | `svc.addIncludedService(otherSvc)` | New: declare an Included Service (GATT §3.2) |
| `pChar->setAccessPermissions(perm)` | `chr.setPermissions(perm)` | Uses `BLEPermission` enum |

### Characteristic Callbacks

| Old (v3.x) | New (v4.0) |
|---|---|
| `class MyChrCb : public BLECharacteristicCallbacks { void onRead(...) ... }` | `chr.onRead([](BLECharacteristic c, const BLEConnInfo &conn) { ... })` |
| `void onWrite(BLECharacteristic *pChr)` | `chr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) { ... })` |
| `void onNotify(BLECharacteristic *pChr)` | `chr.onNotify([](BLECharacteristic c) { ... })` |
| `void onSubscribe(BLECharacteristic*, ..., uint16_t subValue)` | `chr.onSubscribe([](BLECharacteristic c, const BLEConnInfo &conn, uint16_t subValue) { ... })` |
| `void onStatus(BLECharacteristic*, Status, uint32_t)` | `chr.onStatus([](BLECharacteristic c, BLECharacteristic::NotifyStatus status, uint32_t code) { ... })` |
| `pChar->setCallbacks(new MyChrCb())` | Individual `chr.on*(lambda)` calls |

### Characteristic Properties

| Old (v3.x) Bluedroid | Old (v3.x) NimBLE | New (v4.0) |
|---|---|---|
| `BLECharacteristic::PROPERTY_READ` | `NIMBLE_PROPERTY::READ` | `BLEProperty::Read` |
| `BLECharacteristic::PROPERTY_WRITE` | `NIMBLE_PROPERTY::WRITE` | `BLEProperty::Write` |
| `BLECharacteristic::PROPERTY_WRITE_NR` | `NIMBLE_PROPERTY::WRITE_NR` | `BLEProperty::WriteNR` |
| `BLECharacteristic::PROPERTY_NOTIFY` | `NIMBLE_PROPERTY::NOTIFY` | `BLEProperty::Notify` |
| `BLECharacteristic::PROPERTY_INDICATE` | `NIMBLE_PROPERTY::INDICATE` | `BLEProperty::Indicate` |
| `BLECharacteristic::PROPERTY_BROADCAST` | `NIMBLE_PROPERTY::BROADCAST` | `BLEProperty::Broadcast` |
| N/A | `NIMBLE_PROPERTY::SIGNED_WRITE` | `BLEProperty::SignedWrite` |
| N/A | N/A | `BLEProperty::ExtendedProps` |

**Permissions (new, separate enum):**

| Old (v3.x) Bluedroid | Old (v3.x) NimBLE | New (v4.0) |
|---|---|---|
| `ESP_GATT_PERM_READ` | (implicit via property) | `BLEPermission::Read` |
| `ESP_GATT_PERM_READ_ENCRYPTED` | `NIMBLE_PROPERTY::READ_ENC` | `BLEPermission::ReadEncrypted` |
| `ESP_GATT_PERM_READ_ENC_MITM` | `NIMBLE_PROPERTY::READ_AUTHEN` | `BLEPermission::ReadAuthenticated` |
| N/A | `NIMBLE_PROPERTY::READ_AUTHOR` | `BLEPermission::ReadAuthorized` |
| `ESP_GATT_PERM_WRITE` | (implicit via property) | `BLEPermission::Write` |
| `ESP_GATT_PERM_WRITE_ENCRYPTED` | `NIMBLE_PROPERTY::WRITE_ENC` | `BLEPermission::WriteEncrypted` |
| `ESP_GATT_PERM_WRITE_ENC_MITM` | `NIMBLE_PROPERTY::WRITE_AUTHEN` | `BLEPermission::WriteAuthenticated` |
| N/A | `NIMBLE_PROPERTY::WRITE_AUTHOR` | `BLEPermission::WriteAuthorized` |

### Descriptors

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `new BLE2902()` + `pChar->addDescriptor(p2902)` | Auto-created for Notify/Indicate chars | Or `BLEDescriptor::createCCCD()` |
| `new BLE2901()` + `pChar->addDescriptor(p2901)` | `chr.setDescription("text")` | Or `BLEDescriptor::createUserDescription("text")` |
| `new BLE2904()` | `BLEDescriptor::createPresentationFormat()` | |
| `p2904->setFormat(BLE2904::FORMAT_UTF8)` | `fmt.setFormat(BLEDescriptor::FORMAT_UTF8)` | |
| `p2904->setUnit(unit)` | `fmt.setUnit(unit)` | |
| `p2904->setExponent(exp)` | `fmt.setExponent(exp)` | |
| `p2904->setNamespace(ns)` | `fmt.setNamespace(ns)` | |
| `p2904->setDescription(desc)` | `fmt.setFormatDescription(desc)` | |
| `pChar->createDescriptor(uuid)` | `chr.createDescriptor(uuid, perms, maxLen)` | Returns `BLEDescriptor` value |
| `pDesc->setValue(data, len)` | `desc.setValue(data, len)` | |
| `pDesc->getValue()` | `desc.getValue(&len)` | |

### GATT Client

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `BLEDevice::createClient()` → `BLEClient*` | `BLE.createClient()` → `BLEClient` | Multiple instances allowed |
| `pClient->connect(device)` → `bool` | `client.connect(address)` → `BTStatus` | Multiple overloads |
| `pClient->connect(address)` | `client.connect(address, timeoutMs)` | Configurable timeout |
| `pClient->disconnect()` | `client.disconnect()` | Returns `BTStatus` |
| `pClient->isConnected()` | `client.isConnected()` | |
| `pClient->getService(uuid)` → `BLERemoteService*` | `client.getService(uuid)` → `BLERemoteService` | Value type |
| `pClient->getServices()` | `client.getServices()` | Returns `vector<BLERemoteService>` |
| `pClient->getRssi()` | `client.getRSSI()` | |
| `pClient->getPeerAddress()` | `client.getPeerAddress()` | Returns `BTAddress` |
| `pClient->getMTU()` | `client.getMTU()` | |
| `pClient->setMTU(mtu)` | `client.setMTU(mtu)` | |
| N/A | `client.connect(addr, phy, timeout)` | New: PHY selection |
| N/A | `client.connectAsync(addr, phy)` | New: non-blocking connect |
| N/A | `client.cancelConnect()` | New: cancel pending connect |
| N/A | `client.secureConnection()` | New: initiate pairing |
| N/A | `client.discoverServices()` | New: explicit discovery |
| N/A | `client.getValue(svcUUID, chrUUID)` | New: convenience read |
| N/A | `client.setValue(svcUUID, chrUUID, val)` | New: convenience write |
| N/A | `client.setPhy(tx, rx)` | New: BLE5 PHY |
| N/A | `client.setDataLen(octets, time)` | New: DLE |
| N/A | `client.updateConnParams(params)` | New: request connection parameter update |
| N/A | `client.getConnection()` | New: returns `BLEConnInfo` |

### Client Callbacks

| Old (v3.x) | New (v4.0) |
|---|---|
| `class MyCb : public BLEClientCallbacks { void onConnect(BLEClient*) }` | `client.onConnect([](BLEClient c, const BLEConnInfo &conn) { ... })` |
| `void onDisconnect(BLEClient*)` | `client.onDisconnect([](BLEClient c, const BLEConnInfo &conn, uint8_t reason) { ... })` |
| `pClient->setCallbacks(new MyCb())` | Individual `client.on*(lambda)` calls |
| N/A | `client.onConnectFail([](BLEClient c, int reason) { ... })` |
| N/A | `client.onMtuChanged(lambda)` |
| N/A | `client.onConnParamsUpdateRequest(lambda)` |
| N/A | `client.onIdentity(lambda)` |

### Remote GATT Objects

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `pSvc->getCharacteristic(uuid)` → `BLERemoteCharacteristic*` | `svc.getCharacteristic(uuid)` → `BLERemoteCharacteristic` | Value |
| `pChr->readValue()` → `std::string` | `chr.readValue()` → `String` | |
| `pChr->readUInt8()` | `chr.readUInt8()` | |
| `pChr->readUInt16()` | `chr.readUInt16()` | |
| `pChr->readUInt32()` | `chr.readUInt32()` | |
| `pChr->readFloat()` | `chr.readFloat()` | |
| `pChr->writeValue(data, len)` → `void` | `chr.writeValue(data, len)` → `BTStatus` | Error feedback |
| `pChr->writeValue(data, len, false)` | `chr.writeValue(data, len, false)` | Write without response |
| `pChr->canRead()` | `chr.canRead()` | |
| `pChr->canNotify()` | `chr.canNotify()` | |
| `pChr->registerForNotify(callback)` | `chr.subscribe(true, lambda)` | `true`=notify, `false`=indicate |
| N/A | `chr.unsubscribe()` | New |
| `pChr->getDescriptor(uuid)` → `BLERemoteDescriptor*` | `chr.getDescriptor(uuid)` → `BLERemoteDescriptor` | Value |
| `pDesc->readValue()` | `desc.readValue()` | Returns `String` |
| `pDesc->writeValue(data, len)` | `desc.writeValue(data, len)` | Returns `BTStatus` |
| N/A | `chr.readValue(buf, bufLen, timeout)` | New: read into buffer |
| N/A | `chr.readRawData(&len)` | New: cached raw data |
| N/A | `chr.getRemoteService()` | New |
| N/A | `desc.getRemoteCharacteristic()` | New |

### Scanning

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `pScan->start(duration, false)` | `scan.start(durationMs)` | Non-blocking |
| `pScan->start(duration, true)` | `scan.startBlocking(durationMs)` | Blocking; returns results |
| `pScan->stop()` | `scan.stop()` | Returns `BTStatus` |
| `pScan->isScanning()` | `scan.isScanning()` | |
| `pScan->setInterval(interval)` | `scan.setInterval(intervalMs)` | |
| `pScan->setWindow(window)` | `scan.setWindow(windowMs)` | |
| `pScan->setActiveScan(active)` | `scan.setActiveScan(active)` | |
| `pScan->setFilterDuplicates(f)` | `scan.setFilterDuplicates(f)` | |
| `pScan->clearDuplicateCache()` | `scan.clearDuplicateCache()` | |
| `pScan->getResults()` | `scan.getResults()` | Returns `BLEScanResults` value |
| `pScan->clearResults()` | `scan.clearResults()` | |
| `pScan->erase(addr)` | `scan.erase(addr)` | |
| N/A | `scan.startExtended(dur, coded, uncoded)` | New: BLE5 extended scan |
| N/A | `scan.stopExtended()` | New: stop BLE5 extended scan |
| N/A | `scan.createPeriodicSync(addr, sid)` | New: BLE5 periodic sync |
| N/A | `scan.cancelPeriodicSync()` | New: cancel pending sync |
| N/A | `scan.terminatePeriodicSync(handle)` | New: terminate established sync |
| N/A | `scan.onPeriodicSync(lambda)` | New |
| N/A | `scan.onPeriodicReport(lambda)` | New |
| N/A | `scan.onPeriodicLost(lambda)` | New |

### Scan Callbacks

| Old (v3.x) | New (v4.0) |
|---|---|
| `class MyCb : public BLEAdvertisedDeviceCallbacks { void onResult(BLEAdvertisedDevice dev) }` | `scan.onResult([](BLEAdvertisedDevice dev) { ... })` |
| `pScan->setAdvertisedDeviceCallbacks(new MyCb())` | `scan.onResult(lambda)` |
| `pScan->start(duration, completeCb)` | `scan.onComplete(lambda)` + `scan.start(duration)` |

### Advertised Device

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `dev.getAddress()` → `BLEAddress` | `dev.getAddress()` → `BTAddress` | |
| `dev.getName()` → `std::string` | `dev.getName()` → `String` | |
| `dev.getRSSI()` | `dev.getRSSI()` | |
| `dev.getTXPower()` | `dev.getTXPower()` | |
| `dev.getAppearance()` | `dev.getAppearance()` | |
| `dev.getManufacturerData()` → `std::string` | `dev.getManufacturerData(&len)` → `const uint8_t*` | Raw pointer + length |
| `dev.getServiceUUID()` | `dev.getServiceUUID(index)` | Multi-UUID support |
| `dev.haveServiceUUID()` | `dev.haveServiceUUID()` | |
| `dev.isAdvertisingService(uuid)` | `dev.isAdvertisingService(uuid)` | |
| `dev.getServiceData()` | `dev.getServiceData(index, &len)` | Multi-service-data support |
| `dev.getPayload()` | `dev.getPayload()` | |
| `dev.getPayloadLength()` | `dev.getPayloadLength()` | |
| `dev.isConnectable()` | `dev.isConnectable()` | |
| N/A | `dev.getManufacturerCompanyId()` | New |
| N/A | `dev.getManufacturerDataString()` | New |
| N/A | `dev.getServiceUUIDCount()` | New |
| N/A | `dev.getServiceDataCount()` | New |
| N/A | `dev.getPrimaryPhy()` | New: BLE5 |
| N/A | `dev.getSecondaryPhy()` | New: BLE5 |
| N/A | `dev.getAdvSID()` | New: BLE5 |
| N/A | `dev.getPeriodicInterval()` | New: BLE5 |

### Advertising

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `pAdv->start()` | `adv.start(durationMs)` | 0 = indefinite; returns `BTStatus` |
| `pAdv->stop()` | `adv.stop()` | Returns `BTStatus` |
| `pAdv->addServiceUUID(uuid)` | `adv.addServiceUUID(uuid)` | |
| `pAdv->removeServiceUUID(uuid)` | `adv.removeServiceUUID(uuid)` | |
| `pAdv->setScanResponse(bool)` | `adv.setScanResponse(bool)` | |
| `pAdv->setAdvertisementType(type)` | `adv.setType(type)` | Uses `BLEAdvType` enum |
| `pAdv->setMinInterval(min)` | `adv.setInterval(min, max)` | Combined |
| `pAdv->setMaxInterval(max)` | `adv.setInterval(min, max)` | Combined |
| `pAdv->setMinPreferred(val)` | `adv.setMinPreferred(val)` | |
| `pAdv->setMaxPreferred(val)` | `adv.setMaxPreferred(val)` | |
| `pAdv->setIncludeTxPower(bool)` | `adv.setTxPower(bool)` | |
| `pAdv->setAppearance(val)` | `adv.setAppearance(val)` | |
| `pAdv->setAdvertisementData(data)` | `adv.setAdvertisementData(data)` | |
| `pAdv->setScanResponseData(data)` | `adv.setScanResponseData(data)` | |
| `pAdv->setScanFilter(scan, conn)` | `adv.setScanFilter(scan, conn)` | |
| `pAdv->reset()` | `adv.reset()` | |
| N/A | `adv.clearServiceUUIDs()` | New |
| N/A | `adv.setName("name")` | New |
| N/A | `adv.configureExtended(config)` | New: BLE5 |
| N/A | `adv.startExtended(instance, dur)` | New: BLE5 |
| N/A | `adv.stopExtended(instance)` | New: BLE5 |
| N/A | `adv.removeExtended(instance)` | New: BLE5 |
| N/A | `adv.clearExtended()` | New: BLE5 |
| N/A | `adv.configurePeriodicAdv(config)` | New: BLE5 |
| N/A | `adv.setPeriodicAdvData(instance, data)` | New: BLE5 |
| N/A | `adv.startPeriodicAdv(instance)` | New: BLE5 |
| N/A | `adv.stopPeriodicAdv(instance)` | New: BLE5 |
| N/A | `adv.onComplete(lambda)` | New |

### Security

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `new BLESecurity()` | `BLE.getSecurity()` | Singleton handle |
| `pSec->setCapability(cap)` | `sec.setIOCapability(cap)` | Uses `BLESecurity::IOCapability` enum |
| `pSec->setAuthenticationMode(mode)` | `sec.setAuthenticationMode(bonding, mitm, sc)` | 3 booleans |
| `pSec->setStaticPIN(pin)` | `sec.setStaticPassKey(pin)` | |
| `pSec->getPassKey()` | `sec.getPassKey()` | |
| `pSec->setInitEncryptionKey(keys)` | `sec.setInitiatorKeys(keys)` | Uses `KeyDist` enum |
| `pSec->setRespEncryptionKey(keys)` | `sec.setResponderKeys(keys)` | Uses `KeyDist` enum |
| `pSec->setKeySize(size)` | `sec.setKeySize(size)` | |
| N/A | `sec.setRandomPassKey()` | New |
| N/A | `sec.generateRandomPassKey()` | New: static utility |
| N/A | `sec.regenPassKeyOnConnect(bool)` | New |
| N/A | `sec.setForceAuthentication(bool)` | New |
| N/A | `sec.startSecurity(connHandle)` | New |
| N/A | `sec.waitForAuthenticationComplete(timeout)` | New |
| N/A | `sec.resetSecurity()` | New |
| N/A | `sec.onAuthorization(lambda)` | New |
| N/A | `sec.onBondStoreOverflow(lambda)` | New (NimBLE-only) |

### Security Callbacks

| Old (v3.x) | New (v4.0) |
|---|---|
| `class MyCb : public BLESecurityCallbacks { ... }` | Individual `sec.on*(lambda)` calls |
| `BLEDevice::setSecurityCallbacks(new MyCb())` | (not needed -- register per-event) |
| `uint32_t onPassKeyRequest()` | `sec.onPassKeyRequest([](const BLEConnInfo &conn) -> uint32_t { ... })` |
| `void onPassKeyNotify(uint32_t passkey)` | `sec.onPassKeyDisplay([](const BLEConnInfo &conn, uint32_t pk) { ... })` |
| `bool onConfirmPIN(uint32_t num)` | `sec.onConfirmPassKey([](const BLEConnInfo &conn, uint32_t pk) -> bool { ... })` |
| `bool onSecurityRequest()` | `sec.onSecurityRequest([](const BLEConnInfo &conn) -> bool { ... })` |
| `void onAuthenticationComplete(esp_ble_auth_cmpl_t)` (Bluedroid) | `sec.onAuthenticationComplete([](const BLEConnInfo &conn, bool success) { ... })` |
| `void onAuthenticationComplete(NimBLEConnInfo&, ...)` (NimBLE) | Same lambda as above -- stack-agnostic |

### Bond Management

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `BLEDevice::getNumBonds()` | `sec.getBondedDevices().size()` | |
| `BLEDevice::getBondedDevices()` | `sec.getBondedDevices()` | Returns `vector<BTAddress>` |
| `BLEDevice::deleteBond(addr)` | `sec.deleteBond(addr)` | Returns `BTStatus` |
| N/A | `sec.deleteAllBonds()` | New |

### Whitelist

| Old (v3.x) | New (v4.0) |
|---|---|
| `BLEDevice::whiteListAdd(addr)` | `BLE.whiteListAdd(addr)` |
| `BLEDevice::whiteListRemove(addr)` | `BLE.whiteListRemove(addr)` |
| `BLEDevice::onWhiteList(addr)` | `BLE.isOnWhiteList(addr)` |

### IRK (Identity Resolving Key)

| Old (v3.x) | New (v4.0) |
|---|---|
| `BLEDevice::getIRK()` | `BLE.getLocalIRK(buf)` |
| `BLEDevice::getIRKString()` | `BLE.getLocalIRKString()` |
| `BLEDevice::getIRKBase64()` | `BLE.getLocalIRKBase64()` |
| `BLEDevice::getPeerIRK(peer, buf)` | `BLE.getPeerIRK(peer, buf)` |
| `BLEDevice::getPeerIRKReverse(peer)` | `BLE.getPeerIRKReverse(peer)` |

### Address Type

| Old (v3.x) | New (v4.0) |
|---|---|
| `BLEAddress` (library-local) | `BTAddress` (shared in `cores/esp32/`) |
| `BLEAddress(std::string)` | `BTAddress("aa:bb:cc:dd:ee:ff")` or `BTAddress(String)` |
| `addr.toString()` → `std::string` | `addr.toString()` → `String` |
| `addr.getNative()` | `addr.data()` (raw 6-byte array) |

### Beacons and Eddystone

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `BLEBeacon` | `BLEBeacon` | API largely unchanged |
| `beacon.setManufacturerId(id)` | `beacon.setManufacturerId(id)` | |
| `beacon.getAdvertisementData()` | `beacon.getAdvertisementData()` | Returns `BLEAdvertisementData` |
| `BLEEddystoneURL` (separate file) | `BLEEddystoneURL` | `#include <BLEEddystone.h>` (merged) |
| `BLEEddystoneTLM` (separate file) | `BLEEddystoneTLM` | `#include <BLEEddystone.h>` (merged) |

### HID

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `BLEHIDDevice(BLEServer*)` | `BLEHIDDevice(BLEServer)` | Value, not pointer |
| `hid->manufacturer("name")` | `hid.manufacturer("name")` | Dot, not arrow |
| `hid->pnp(sig, vid, pid, ver)` | `hid.pnp(sig, vid, pid, ver)` | |
| `hid->reportMap(map, size)` | `hid.reportMap(map, size)` | |
| `hid->startServices()` | Removed — use `server.start()` instead | Services registered by server |
| `hid->inputReport(id)` → `BLECharacteristic*` | `hid.inputReport(id)` → `BLECharacteristic` | Value |
| `hid->outputReport(id)` → `BLECharacteristic*` | `hid.outputReport(id)` → `BLECharacteristic` | Value |
| `hid->featureReport(id)` → `BLECharacteristic*` | `hid.featureReport(id)` → `BLECharacteristic` | Value |
| `hid->setBatteryLevel(level)` | `hid.setBatteryLevel(level)` | |
| N/A | `hid.bootInput()` → `BLECharacteristic` | New: Boot Keyboard Input (singleton, lazy-created) |
| N/A | `hid.bootOutput()` → `BLECharacteristic` | New: Boot Keyboard Output (singleton, lazy-created) |
| Manual `adv.addServiceUUID(0x1812)` | Automatic | HID Service UUID added to advertising by `BLEHIDDevice` constructor |
| Manual `setAppearance(0x03C1)` | `setAppearance(BLE_APPEARANCE_HID_KEYBOARD)` | Named constants in `HIDTypes.h` |
| No included-service support | Battery Service auto-included in HID Service | Per HIDS 1.0 §3 |
| No External Report Reference | Auto-added on Report Map (references Battery Level) | Per HIDS 1.0 §3.6 |

**HID Appearance Constants** (defined in `HIDTypes.h`):

| Constant | Value |
|---|---|
| `BLE_APPEARANCE_HID_GENERIC` | `0x03C0` |
| `BLE_APPEARANCE_HID_KEYBOARD` | `0x03C1` |
| `BLE_APPEARANCE_HID_MOUSE` | `0x03C2` |
| `BLE_APPEARANCE_HID_JOYSTICK` | `0x03C3` |
| `BLE_APPEARANCE_HID_GAMEPAD` | `0x03C4` |
| `BLE_APPEARANCE_HID_DIGITIZER` | `0x03C5` |
| `BLE_APPEARANCE_HID_CARD_READER` | `0x03C6` |
| `BLE_APPEARANCE_HID_DIGITAL_PEN` | `0x03C7` |
| `BLE_APPEARANCE_HID_BARCODE_SCANNER` | `0x03C8` |

---

## Before/After Examples

### Simple Server

**Before (v3.x):**
```cpp
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        Serial.println("Connected");
    }
    void onDisconnect(BLEServer *pServer) {
        Serial.println("Disconnected");
        BLEDevice::startAdvertising();
    }
};

void setup() {
    Serial.begin(115200);
    BLEDevice::init("MyServer");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    BLECharacteristic *pChar = pService->createCharacteristic(
        CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pChar->addDescriptor(new BLE2902());
    pChar->setValue("Hello World");
    pService->start();

    BLEAdvertising *pAdv = BLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    BLEDevice::startAdvertising();
}
```

**After (v4.0):**
```cpp
#include <BLE.h>
#include <BLEServer.h>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"

void setup() {
    Serial.begin(115200);
    BLE.begin("MyServer");

    BLEServer server = BLE.createServer();
    server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
        Serial.println("Connected");
    });
    server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
        Serial.println("Disconnected");
    });
    server.advertiseOnDisconnect(true);

    BLEService svc = server.createService(SERVICE_UUID);
    BLECharacteristic chr = svc.createCharacteristic(
        CHAR_UUID,
        BLEProperty::Read | BLEProperty::Notify,
        BLEPermissions::OpenRead
    );
    // CCCD (BLE2902) is auto-created for Notify characteristics!
    chr.setValue("Hello World");
    server.start();

    BLE.getAdvertising().addServiceUUID(BLEUUID(SERVICE_UUID));
    BLE.startAdvertising();
}
```

### Simple Client

**Before (v3.x):**
```cpp
#include <BLEDevice.h>

BLEClient *pClient = BLEDevice::createClient();
pClient->connect(myDevice);  // BLEAdvertisedDevice*
BLERemoteService *pSvc = pClient->getService(serviceUUID);
BLERemoteCharacteristic *pChr = pSvc->getCharacteristic(charUUID);
std::string val = pChr->readValue();
pChr->writeValue("hello");
pChr->registerForNotify([](BLERemoteCharacteristic *pChr, uint8_t *data, size_t len, bool isNotify) {
    Serial.printf("Notification: %.*s\n", len, data);
});
```

**After (v4.0):**
```cpp
#include <BLE.h>
#include <BLEClient.h>

BLEClient client = BLE.createClient();
BTStatus status = client.connect(address, 5000);
if (!status) {
    Serial.printf("Connect failed: %s\n", status.toString());
    return;
}
BLERemoteService svc = client.getService(serviceUUID);
BLERemoteCharacteristic chr = svc.getCharacteristic(charUUID);
String val = chr.readValue();
chr.writeValue("hello");
chr.subscribe(true, [](BLERemoteCharacteristic c, const uint8_t *data, size_t len, bool isNotify) {
    Serial.printf("Notification: %.*s\n", len, data);
});
```

### Scan for Devices

**Before (v3.x):**
```cpp
#include <BLEDevice.h>

class MyCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        Serial.printf("Found: %s\n", advertisedDevice.getName().c_str());
    }
};

void setup() {
    BLEDevice::init("");
    BLEScan *pScan = BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new MyCallbacks());
    pScan->setActiveScan(true);
    pScan->start(5, false);
}
```

**After (v4.0):**
```cpp
#include <BLE.h>
#include <BLEScan.h>

void setup() {
    BLE.begin("");
    BLEScan scan = BLE.getScan();
    scan.setActiveScan(true);
    scan.onResult([](BLEAdvertisedDevice dev) {
        Serial.printf("Found: %s\n", dev.getName().c_str());
    });
    scan.start(5000);
}
```

### Secure Server (Passkey)

**Before (v3.x):**
```cpp
#include <BLEDevice.h>
#include <BLESecurity.h>

class MySecurityCallbacks : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() override { return 123456; }
    void onPassKeyNotify(uint32_t pass_key) override {
        Serial.printf("Passkey: %06d\n", pass_key);
    }
    bool onConfirmPIN(uint32_t pin) override { return true; }
    bool onSecurityRequest() override { return true; }
    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
        Serial.println(cmpl.success ? "Auth OK" : "Auth FAIL");
    }
};

void setup() {
    BLEDevice::init("SecureServer");
    BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
    BLESecurity *pSec = new BLESecurity();
    pSec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    pSec->setCapability(ESP_IO_CAP_OUT);
    pSec->setStaticPIN(123456);
    // ... create server, services, etc.
}
```

**After (v4.0):**
```cpp
#include <BLE.h>
#include <BLESecurity.h>

void setup() {
    BLE.begin("SecureServer");

    BLESecurity sec = BLE.getSecurity();
    sec.setAuthenticationMode(true, true, true);  // bonding, MITM, SC
    sec.setIOCapability(BLESecurity::DisplayOnly);
    sec.setStaticPassKey(123456);
    sec.onPassKeyDisplay([](const BLEConnInfo &conn, uint32_t pk) {
        Serial.printf("Passkey: %06lu\n", pk);
    });
    sec.onAuthenticationComplete([](const BLEConnInfo &conn, bool success) {
        Serial.println(success ? "Auth OK" : "Auth FAIL");
    });
    // ... create server, services, etc.
}
```

### Notifications with Callback

**Before (v3.x):**
```cpp
class MyChrCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pChr) override { /* ... */ }
    void onWrite(BLECharacteristic *pChr) override {
        std::string val = pChr->getValue();
        Serial.printf("Written: %s\n", val.c_str());
    }
    void onNotify(BLECharacteristic *pChr) override { /* ... */ }
    void onSubscribe(BLECharacteristic *pChr, BLEDescriptor *pDesc, uint16_t subValue) override { /* ... */ }
};
pChar->setCallbacks(new MyChrCallbacks());
pChar->notify();
```

**After (v4.0):**
```cpp
chr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    String val = c.getStringValue();
    Serial.printf("Written: %s\n", val.c_str());
});
chr.onSubscribe([](BLECharacteristic c, const BLEConnInfo &conn, uint16_t subValue) {
    Serial.printf("Subscription changed: %u\n", subValue);
});
chr.notify();  // notify all subscribers
chr.notify(connHandle);  // or notify a specific connection
```

### Descriptor Usage (BLE2902/BLE2904)

**Before (v3.x):**
```cpp
#include <BLE2902.h>
#include <BLE2904.h>

BLECharacteristic *pChar = pService->createCharacteristic(uuid,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

// Manually add CCCD
BLE2902 *p2902 = new BLE2902();
p2902->setNotifications(true);
pChar->addDescriptor(p2902);

// Add presentation format
BLE2904 *p2904 = new BLE2904();
p2904->setFormat(BLE2904::FORMAT_UTF8);
p2904->setUnit(0x2700);  // unitless
pChar->addDescriptor(p2904);

// Add user description
pChar->setDescription("Temperature");
```

**After (v4.0):**
```cpp
BLECharacteristic chr = svc.createCharacteristic(uuid,
    BLEProperty::Read | BLEProperty::Notify,
    BLEPermissions::OpenRead);
// CCCD is auto-created! No BLE2902 needed.

// Presentation format (create via characteristic so it's automatically attached)
BLEDescriptor fmt = chr.createDescriptor(BLEUUID((uint16_t)0x2904));
fmt.setFormat(BLEDescriptor::FORMAT_UTF8);
fmt.setUnit(0x2700);

// User description (convenience method -- creates and attaches automatically)
chr.setDescription("Temperature");
```

### iBeacon

**Before (v3.x):**
```cpp
#include <BLEDevice.h>
#include <BLEBeacon.h>

BLEDevice::init("");
BLEBeacon beacon;
beacon.setProximityUUID(BLEUUID("FDA50693-A4E2-4FB1-AFCF-C6EB07647825"));
beacon.setMajor(1);
beacon.setMinor(1);
// ... complex adv data setup with oBeacon.getManufacturerData() ...
BLEDevice::startAdvertising();
```

**After (v4.0):**
```cpp
#include <BLE.h>
#include <BLEBeacon.h>

BLE.begin("");
BLEBeacon beacon;
beacon.setProximityUUID(BLEUUID("FDA50693-A4E2-4FB1-AFCF-C6EB07647825"));
beacon.setMajor(1);
beacon.setMinor(1);
beacon.setSignalPower(-59);
BLEAdvertisementData advData = beacon.getAdvertisementData();
BLE.getAdvertising().setAdvertisementData(advData);
BLE.startAdvertising();
```

### HID Keyboard

**Before (v3.x):**
```cpp
#include <BLEDevice.h>
#include <BLEHIDDevice.h>

BLEDevice::init("Keyboard");
BLEServer *pServer = BLEDevice::createServer();
BLEHIDDevice *hid = new BLEHIDDevice(pServer);
hid->manufacturer("Espressif");
hid->reportMap(reportDescriptor, sizeof(reportDescriptor));
BLECharacteristic *input = hid->inputReport(1);
hid->startServices();

BLEAdvertising *pAdv = BLEDevice::getAdvertising();
pAdv->setAppearance(0x03C1);
pAdv->addServiceUUID(BLEUUID((uint16_t)0x1812));
BLEDevice::startAdvertising();
```

**After (v4.0):**
```cpp
#include <BLE.h>
#include <BLEHIDDevice.h>

BLE.begin("Keyboard");
BLEServer server = BLE.createServer();
BLEHIDDevice hid(server);
hid.manufacturer("Espressif");
hid.reportMap(reportDescriptor, sizeof(reportDescriptor));
BLECharacteristic input = hid.inputReport(1);
server.start();

// HID Service UUID (0x1812) is auto-added to advertising.
// Battery Service is auto-included in HID Service.
BLEAdvertising adv = BLE.getAdvertising();
adv.setAppearance(BLE_APPEARANCE_HID_KEYBOARD);  // named constant from HIDTypes.h
adv.start();
```

---

## Everything That Was Removed

| Removed | Replacement |
|---|---|
| `BLEDevice` class (static methods) | `BLE` global singleton |
| `BLEAddress` (library-local) | `BTAddress` (shared in `cores/esp32/`) |
| `BLEServerCallbacks` abstract class | `server.onConnect(lambda)`, `server.onDisconnect(lambda)`, etc. |
| `BLECharacteristicCallbacks` abstract class | `chr.onRead(lambda)`, `chr.onWrite(lambda)`, etc. |
| `BLEDescriptorCallbacks` abstract class | `desc.onRead(lambda)`, `desc.onWrite(lambda)` |
| `BLEAdvertisedDeviceCallbacks` abstract class | `scan.onResult(lambda)` |
| `BLEClientCallbacks` abstract class | `client.onConnect(lambda)`, `client.onDisconnect(lambda)`, etc. |
| `BLESecurityCallbacks` abstract class | `sec.onPassKeyRequest(lambda)`, `sec.onConfirmPassKey(lambda)`, etc. |
| `BLE2901` descriptor subclass | `chr.setDescription("text")` or `BLEDescriptor::createUserDescription("text")` |
| `BLE2902` descriptor subclass | Auto-created for Notify/Indicate; or `BLEDescriptor::createCCCD()` |
| `BLE2904` descriptor subclass | `BLEDescriptor::createPresentationFormat()` |
| `BLEUtils` class | Removed (utility functions inlined or not needed) |
| `BLEValue` class | Replaced by direct `setValue()`/`getValue()` with standard types |
| `BLEDisconnectedException` | `BTStatus::NotConnected` return value |
| `NIMBLE_PROPERTY::*` macros | `BLEProperty::*` and `BLEPermission::*` enums |
| `ESP_GATT_PERM_*` macros | `BLEPermission::*` enum |
| `PROPERTY_READ_AUTHEN` (combined) | `BLEProperty::Read` + `BLEPermission::ReadAuthenticated` (separated) |
| `setAccessPermissions()` | `chr.setPermissions(BLEPermission::...)` |
| `setCallbacks(new ...)` pattern | `on*(lambda)` pattern (no heap allocation) |
| `new BLECharacteristic(...)` constructor | `svc.createCharacteristic(uuid, properties, permissions)` factory (permissions required, fail-closed) |
| `new BLEDescriptor(...)` on heap | `chr.createDescriptor(uuid, perms, maxLen)` factory (preferred) or stack-construct `BLEDescriptor(uuid)` |
| `BLEEddystoneURL.h` (separate) | Merged into `BLEEddystone.h` |
| `BLEEddystoneTLM.h` (separate) | Merged into `BLEEddystone.h` |
| `std::string` in public API | `String` (Arduino) in all public methods |

---

## New Features in v4.0

These features are new and were not available in v3.x:

| Feature | API |
|---|---|
| Multiple simultaneous BLE clients | `BLE.createClient()` (call multiple times) |
| Explicit server start | `server.start()` |
| Per-connection notifications/indications | `chr.notify(connHandle)`, `chr.indicate(connHandle)` |
| Query subscribed connections | `chr.getSubscribedConnections()`, `chr.isSubscribed(connHandle)` |
| Disconnect reason in callbacks | `uint8_t reason` parameter in disconnect handlers |
| Connection parameter updates | `server.updateConnParams(h, params)`, `client.updateConnParams(params)` |
| BLE5 PHY selection (server/client) | `setPhy(h, tx, rx)`, `getPhy(h, tx, rx)` |
| BLE5 Data Length Extension | `setDataLen(h, octets, time)` |
| BLE5 extended advertising | `adv.configureExtended(config)`, `adv.startExtended(...)`, `adv.stopExtended(...)`, `adv.removeExtended(...)`, `adv.clearExtended()` |
| BLE5 periodic advertising | `adv.configurePeriodicAdv(config)`, `adv.setPeriodicAdvData(...)`, `adv.startPeriodicAdv(...)`, `adv.stopPeriodicAdv(...)` |
| BLE5 extended scanning | `scan.startExtended(dur, coded, uncoded)`, `scan.stopExtended()` |
| BLE5 periodic sync | `scan.createPeriodicSync(addr, sid)` |
| Async (non-blocking) connect | `client.connectAsync(addr, phy)` |
| Cancel pending connect | `client.cancelConnect()` |
| Initiate security on connection | `client.secureConnection()`, `sec.startSecurity(connHandle)` |
| Block until auth completes | `sec.waitForAuthenticationComplete(timeout)` |
| Random passkey generation | `sec.setRandomPassKey()`, `sec.generateRandomPassKey()` |
| Auto-regen passkey per connection | `sec.regenPassKeyOnConnect(true)` |
| Delete all bonds | `sec.deleteAllBonds()` |
| Bond store overflow callback | `sec.onBondStoreOverflow(lambda)` (NimBLE) |
| Authorization handler | `sec.onAuthorization(lambda)` |
| Force authentication | `sec.setForceAuthentication(true)` |
| Connect-fail callback (client) | `client.onConnectFail(lambda)` |
| Connection params request callback | `client.onConnParamsUpdateRequest(lambda)` |
| Identity (RPA resolved) callback | `server.onIdentity(lambda)`, `client.onIdentity(lambda)` |
| Stack identification | `BLE.getStack()`, `BLE.getStackName()` |
| Hosted BLE (ESP32-P4) | `BLE.setPins(...)`, `BLE.isHostedBLE()` |
| Periodic advertising sync | `scan.createPeriodicSync(addr, sid)`, `scan.cancelPeriodicSync()`, `scan.terminatePeriodicSync(handle)` |
| `BLEConnInfo` connection descriptor | Stack-agnostic connection info in all callbacks |
| L2CAP CoC channels | `BLE.createL2CAPServer(psm, mtu)`, `BLE.connectL2CAP(connHandle, psm, mtu)` |
| BLEStream (NUS over Stream) | `BLEStream bleStream; bleStream.begin("name")` — Arduino `Stream` API over Nordic UART Service. Connect/disconnect callbacks receive `BLEConnInfo` and disconnect reason. |
| Included Services | `svc.addIncludedService(otherSvc)` — GATT Included Service declarations |
| HoGP spec-compliant HID | `BLEHIDDevice` auto-includes Battery Service, adds External Report Reference, auto-advertises HID UUID |
| Boot Protocol characteristics | `hid.bootInput()`, `hid.bootOutput()` — singleton, lazy-created |
| HID Appearance constants | `BLE_APPEARANCE_HID_KEYBOARD`, `BLE_APPEARANCE_HID_GAMEPAD`, etc. in `HIDTypes.h` |

---

## Common Pitfalls

1. **Forgetting `server.start()`** -- In v3.x, there was no explicit server start. In v4.0, you must call `server.start()` after creating all services/characteristics and before advertising. Without it, the GATT database is not committed.

2. **Still using `->` instead of `.`** -- All BLE objects are now values, not pointers. Replace `pServer->` with `server.`, `pChar->` with `chr.`, etc.

3. **Not checking `BTStatus`** -- Operations like `BLE.begin()`, `client.connect()`, `chr.writeValue()` now return `BTStatus`. Ignoring failures leads to hard-to-debug issues.

4. **Allocating callback objects with `new`** -- The `on*(lambda)` pattern does not require heap allocation. Remove all `new MyCallbacks()` patterns.

5. **Using `BLE2902` manually** -- CCCD descriptors are automatically created for characteristics with `Notify` or `Indicate` properties. Remove manual `BLE2902` usage.

6. **Using `std::string`** -- The v4.0 API uses Arduino `String` throughout. Replace `std::string` with `String`.

7. **Mixing properties and permissions** -- In v3.x (especially NimBLE), security was sometimes set via property flags like `PROPERTY_READ_AUTHEN`. In v4.0, use `BLEProperty` for capabilities and `BLEPermission` for security requirements.

8. **Backend-specific callback parameters** -- Callbacks no longer receive `esp_ble_gatts_cb_param_t*` or `ble_gap_conn_desc*`. They receive `BLEConnInfo` which provides the same information in a stack-agnostic way.

9. **HID advertising changes** -- `BLEHIDDevice` now automatically adds the HID Service UUID (0x1812) to advertising data. Remove any manual `adv.addServiceUUID(BLEUUID((uint16_t)0x1812))` calls to avoid duplicate entries.

10. **Device name in scan response** -- When scan response is enabled (default), the device name is placed in the scan response per CSS Part A §1.2, keeping the primary advertising data free for flags and service UUIDs. This is consistent across both NimBLE and Bluedroid.

---

## NimBLE-Specific Migration Notes

If your v3.x code used the NimBLE backend (via `CONFIG_BT_NIMBLE_ENABLED`):

- **Remove `#include <NimBLEDevice.h>`** -- Use `#include <BLE.h>` instead.
- **Remove `NIMBLE_PROPERTY::*` macros** -- Use `BLEProperty::*` and `BLEPermission::*` enums.
- **Remove all `#ifdef CONFIG_BT_NIMBLE_ENABLED` guards** -- The API is now the same regardless of backend.
- **`NimBLEConnInfo` → `BLEConnInfo`** -- The connection descriptor is now stack-agnostic.
- **`NimBLEAddress` → `BTAddress`** -- Unified address type.
- **`NimBLEUUID` → `BLEUUID`** -- Already the same in most cases.
- **Features like `cancelConnect()` remain available** -- They return `BTStatus::NotSupported` if the backend doesn't support them (e.g., on Bluedroid).

## Bluedroid-Specific Migration Notes

If your v3.x code used the Bluedroid backend (via `CONFIG_BT_BLUEDROID_ENABLED`):

- **Remove `esp_ble_*` types from callbacks** -- Replace `esp_ble_gatts_cb_param_t*` with `BLEConnInfo`.
- **Replace `esp_ble_auth_cmpl_t` in security callback** -- Use the `(BLEConnInfo, bool success)` signature.
- **Replace `ESP_GATT_PERM_*` macros** -- Use `BLEPermission::*` enum.
- **Replace `ESP_IO_CAP_*` constants** -- Use `BLESecurity::IOCapability` enum members.
- **Replace `ESP_LE_AUTH_*` auth mode constants** -- Use `sec.setAuthenticationMode(bonding, mitm, sc)` with three booleans.
- **`BLEAddress` → `BTAddress`** -- The old `BLEAddress` wrapping `esp_bd_addr_t` is replaced.
- **BLE5 features are now available on Bluedroid too** -- Extended advertising, periodic advertising, etc. are supported on both stacks.

---

## ArduinoBLE Porting Guide

For users migrating from the official [ArduinoBLE](https://www.arduino.cc/reference/en/libraries/arduinoble/) library:

| ArduinoBLE | ESP32 BLE v4.0 | Notes |
|---|---|---|
| `BLE.begin()` | `BLE.begin("name")` | Device name is optional but recommended |
| `BLE.end()` | `BLE.end()` | |
| `BLE.advertise()` | `BLE.startAdvertising()` | |
| `BLE.stopAdvertise()` | `BLE.stopAdvertising()` | |
| `BLE.scan()` | `BLE.getScan().start(duration)` | |
| `BLE.available()` | Use `scan.onResult(lambda)` | Callback-based |
| `BLE.central()` | `server.getConnectedCount() > 0` | |
| `BLEService svc("uuid", ...)` | `server.createService(BLEUUID("uuid"))` | Factory method |
| `BLECharacteristic chr("uuid", props, size)` | `svc.createCharacteristic(BLEUUID("uuid"), props, perms)` | Factory method; permissions required |
| `chr.writeValue(val)` | `chr.setValue(val)` (server-side) | |
| `chr.readValue()` | `chr.getStringValue()` (server-side) | |
| `chr.subscribe()` | `remoteChr.subscribe(true, callback)` (client-side) | |
| `chr.setEventHandler(handler)` | `chr.onWrite(lambda)`, `chr.onRead(lambda)` | Per-event handlers |
| `BLE.setLocalName("name")` | Set in `BLE.begin("name")` | |
| `BLE.setDeviceName("name")` | Set in `BLE.begin("name")` | |
| `BLEDescriptor` | `BLEDescriptor` | Similar API |

**Key differences from ArduinoBLE:**

- ESP32 BLE is more feature-rich: security/pairing, BLE5 extended/periodic advertising, multi-instance clients, HID, beacons, dual-stack support
- ESP32 uses `BLEProperty` enum class (type-safe) instead of integer flags
- ESP32 uses `BTStatus` return values instead of boolean/integer returns
- ESP32 provides `BLEConnInfo` in callbacks for connection details
- ESP32 supports both NimBLE and Bluedroid backends transparently
- ESP32 has dedicated HID, Beacon, and Eddystone helper classes

---

For full API documentation, see the [BLE API Reference](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ble.html).
