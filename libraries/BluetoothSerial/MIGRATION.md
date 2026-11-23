# BluetoothSerial Migration Guide: v3.x to v4.0

This guide helps you migrate existing sketches that use the `BluetoothSerial` library from the v3.x API to the v4.0 API. It covers every breaking change with before/after examples.

> **Note:** Bluetooth Classic (BR/EDR) is available on the **original ESP32** and on
> **ESP32-S31**. On ESP32-S31 the prebuilt libraries enable NimBLE only, so
> `BluetoothSerial` requires using Arduino as an ESP-IDF component or rebuilding the
> static libraries. It is not available on ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6,
> ESP32-H2, or ESP32-P4.

---

## Table of Contents

- [Key Changes](#key-changes)
  - [1. BTStatus Return Values](#1-btstatus-return-values)
  - [2. BTAddress for Remote Addresses](#2-btaddress-for-remote-addresses)
  - [3. Lifecycle: begin() / end()](#3-lifecycle-begin--end)
  - [4. Connection API](#4-connection-api)
  - [5. Discovery API](#5-discovery-api)
  - [6. Security Callbacks](#6-security-callbacks)
  - [7. Auth-Complete Callback Signature](#7-auth-complete-callback-signature)
  - [8. Bond Management](#8-bond-management)
  - [9. Data Callback](#9-data-callback)
  - [10. PIMPL Ownership Model](#10-pimpl-ownership-model)
  - [11. Removed Methods](#11-removed-methods)
  - [12. Multi-Client Acceptor APIs](#12-multi-client-acceptor-apis)
- [Complete API Mapping](#complete-api-mapping)
- [Before/After Examples](#beforeafter-examples)
  - [Basic Serial Bridge](#basic-serial-bridge)
  - [Master (Initiator) Connect](#master-initiator-connect)
  - [SSP Pairing with Numeric Comparison](#ssp-pairing-with-numeric-comparison)
  - [Bond Management](#bond-management-example)

---

## Key Changes

### 1. BTStatus Return Values

Nearly every method that previously returned `void` or `bool` now returns `BTStatus`.
`BTStatus` has an `explicit operator bool()` that returns `true` on success,
so existing code that checks `if (SerialBT.connect(...))` continues to work.
Use `status.toString()` or compare to `BTStatus::Timeout` etc. for richer diagnostics.

```cpp
// v3.x
SerialBT.begin("ESP32");          // void -- silent failure
bool ok = SerialBT.connect("Peer"); // bool

// v4.0
BTStatus s = SerialBT.begin("ESP32");
if (!s) { Serial.printf("Init failed: %s\n", s.toString()); }

BTStatus s2 = SerialBT.connect(String("Peer"), 10000);
if (s2 == BTStatus::Timeout) { Serial.println("Connection timed out"); }
```

### 2. BTAddress for Remote Addresses

`connect()` no longer accepts a raw `uint8_t[6]` array. Use `BTAddress` (defined in `cores/esp32/BTAddress.h`, shared with BLE).

```cpp
// v3.x
uint8_t addr[6] = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33};
SerialBT.connect(addr);

// v4.0
BTAddress addr("AA:BB:CC:11:22:33");
SerialBT.connect(addr);
```

### 3. Lifecycle: begin() / end()

`begin()` now returns `BTStatus`. The `isMaster` parameter was renamed to `isInitiator`.

```cpp
// v3.x
SerialBT.begin("ESP32", true);   // void, isMaster=true

// v4.0
BTStatus s = SerialBT.begin("ESP32", true);  // isInitiator=true
if (!s) { /* handle failure */ }
```

`end()` gains an optional `releaseMemory` parameter (replaces the old `flush` parameter):

```cpp
// v3.x
SerialBT.end();

// v4.0
SerialBT.end();           // same behavior
SerialBT.end(true);       // also frees additional internal allocations
```

### 4. Connection API

`connect()` now returns `BTStatus` and accepts either a name with a timeout or a `BTAddress`. `disconnect()` returns `BTStatus`. The `connected()` method no longer accepts a timeout parameter.

```cpp
// v3.x
bool ok = SerialBT.connect("MyDevice");          // blocks, returns bool
bool ok2 = SerialBT.connect(rawAddr);            // raw uint8_t[6]
bool dc = SerialBT.disconnect();
bool isConn = SerialBT.connected(500);           // optional timeout ms

// v4.0
BTStatus ok = SerialBT.connect(String("MyDevice"), 10000);  // explicit timeout
BTStatus ok2 = SerialBT.connect(BTAddress("AA:BB:CC:11:22:33"));
BTStatus dc = SerialBT.disconnect();
bool isConn = SerialBT.connected();              // no timeout parameter
```

### 5. Discovery API

The synchronous `discover()` now returns `std::vector<BluetoothSerial::DiscoveryResult>` instead of a `BTScanResults*`. The asynchronous variant is `discoverAsync()` (unchanged name, but signature changed). `discoverAsyncStop()` is renamed to `discoverStop()`.

```cpp
// v3.x
BTScanResults *results = SerialBT.discover(10000);
SerialBT.discoverAsync(myCallback, 10000);  // callback: void(BTAdvertisedDevice*)
SerialBT.discoverAsyncStop();

// v4.0
auto results = SerialBT.discover(10000);  // std::vector<DiscoveryResult>
for (const auto &dev : results) {
    Serial.printf("%s %s\n",
        dev.address.toString().c_str(), dev.name.c_str());
}

SerialBT.discoverAsync([](const BluetoothSerial::DiscoveryResult &dev) {
    Serial.printf("Found: %s\n", dev.address.toString().c_str());
}, 10000);
SerialBT.discoverStop();  // was: discoverAsyncStop()
```

Each `DiscoveryResult` contains:

```cpp
struct DiscoveryResult {
    BTAddress address;
    String    name;
    uint32_t  cod  = 0;     // Class of Device
    int8_t    rssi = -128;
};
```

### 6. Security Callbacks

#### onConfirmRequest

The callback signature changed from `void(uint32_t)` to `bool(uint32_t)`. The callback now **returns the accept/reject decision directly** — there is no separate `confirmReply()` call.

```cpp
// v3.x
SerialBT.onConfirmRequest([](uint32_t passkey) {
    Serial.printf("Confirm: %06lu\n", passkey);
    // ...user responds later via confirmReply()
});
SerialBT.confirmReply(true);   // called from elsewhere

// v4.0 -- return the decision directly from the callback
SerialBT.onConfirmRequest([](uint32_t passkey) -> bool {
    Serial.printf("Confirm: %06lu? Auto-accepting.\n", passkey);
    return true;   // accept
});
// confirmReply() has been removed
```

#### onKeyRequest / respondPasskey (replaced)

v3.x split passkey entry across a `void()` callback and a later `respondPasskey()` call that trusted
one global "last peer" address. v4.0 replaces the pair with a value-returning handler that receives
the peer address, so there is no cross-peer state:

| v3.x | v4.0 |
|---|---|
| `void onKeyRequest(cb)` | `BTStatus onPassKeyRequest(cb)` — `cb` returns the passkey |
| `void respondPasskey(uint32_t)` | Return the passkey from the `onPassKeyRequest` callback |
| *(no display API)* | `BTStatus onPassKeyDisplay(cb)` — receives the generated passkey to show |

`onPassKeyRequest` runs on the keyboard side: the peer displays a six-digit code and the handler
returns it (0–999999). Returning a value above 999999 rejects the pairing; with no handler the
request is declined instead of timing out. `onPassKeyDisplay` runs on the display side and receives
the code the stack generated for the user to type on the peer.

```cpp
// v3.x (removed)
SerialBT.onKeyRequest([]() {
    SerialBT.respondPasskey(123456);   // which peer?
});

// v4.0 -- value returned synchronously, peer address supplied
SerialBT.onPassKeyRequest([](const BTAddress &peer) -> uint32_t {
    Serial.printf("Passkey shown on %s: ", peer.toString().c_str());
    // ... read six digits ...
    return passkey;                    // 0-999999
});

// v4.0 -- display side
SerialBT.onPassKeyDisplay([](const BTAddress &peer, uint32_t passkey) {
    Serial.printf("Passkey for %s: %06lu\n", peer.toString().c_str(), (unsigned long)passkey);
});
```

Passkey Entry is selected by `enableSSP(true, false)` (KeyboardOnly) against a display-capable peer,
or by `enableSSP(false, true)` (DisplayOnly) against a keyboard-only peer. Two KeyboardOnly boards
are both prompted, so entering the same code on both consoles works as a fixed PIN. Numeric
Comparison (`enableSSP(true, true)` + `onConfirmRequest`) remains the recommended model for
phones and PCs.

### 7. Auth-Complete Callback Signature

The `onAuthComplete` callback no longer receives the remote address. It receives only a `bool` success flag.

```cpp
// v3.x
SerialBT.onAuthComplete([](uint8_t *remoteAddress, bool success) {
    Serial.printf("Auth %s\n", success ? "OK" : "FAILED");
});

// v4.0
SerialBT.onAuthComplete([](bool success) {
    Serial.printf("Auth %s\n", success ? "OK" : "FAILED");
});
```

### 8. Bond Management

`getBondedDevices()` now returns `std::vector<BTAddress>` instead of a count integer.
`deleteBondedDevices()` is replaced by `deleteBond(address)` and `deleteAllBonds()`, both returning `BTStatus`.

```cpp
// v3.x
int count = SerialBT.getBondedDevices();
uint8_t addr[6];
SerialBT.getBondedDeviceAddress(0, addr);
SerialBT.deleteBondedDevices();

// v4.0
auto bonded = SerialBT.getBondedDevices();   // std::vector<BTAddress>
Serial.printf("Bonded: %d\n", bonded.size());
for (const auto &addr : bonded) {
    Serial.println(addr.toString());
}
BTStatus s = SerialBT.deleteAllBonds();
// or delete a specific one:
BTStatus s2 = SerialBT.deleteBond(bonded[0]);
```

### 9. Data Callback

The data callback is registered via `onData()` (previously a lower-level mechanism). The callback type is now `std::function<void(const uint8_t*, size_t)>`.

```cpp
// v3.x
SerialBT.register_callback(myCallback);   // BTCallback raw function pointer

// v4.0
SerialBT.onData([](const uint8_t *data, size_t len) {
    Serial.write(data, len);
});
```

### 10. PIMPL Ownership Model

`BluetoothSerial` now uses the PIMPL pattern. The object is **not copyable** but is **movable**.

```cpp
// v3.x -- undefined/unsafe copy
BluetoothSerial bt2 = SerialBT;   // potentially copied shared state

// v4.0 -- move is explicit, copy is deleted
BluetoothSerial bt2 = std::move(SerialBT);  // OK: transfers ownership
// BluetoothSerial bt3 = SerialBT;          // compile error: copy deleted
```

FreeRTOS resources (queues, semaphores, task) are allocated in `begin()` and released in `end()` or the destructor. Resource-creation failures are now surfaced through the `BTStatus` return of `begin()`.

### 11. Removed Methods

| Removed (v3.x) | Replacement (v4.0) |
|---|---|
| `SerialBT.begin()` returning `void`/`bool` | `BTStatus SerialBT.begin(...)` |
| `SerialBT.connect(uint8_t *)` | `BTStatus SerialBT.connect(const BTAddress &)` |
| `SerialBT.connect(String)` returning `bool` | `BTStatus SerialBT.connect(const String &, uint32_t)` |
| `SerialBT.disconnect()` returning `bool` | `BTStatus SerialBT.disconnect()` |
| `SerialBT.connected(int timeout)` | `bool SerialBT.connected() const` (no timeout) |
| `SerialBT.confirmReply(bool)` | Callback registered via `onConfirmRequest` returns `bool` directly |
| `SerialBT.onKeyRequest(cb)` | `BTStatus SerialBT.onPassKeyRequest(cb)` — callback returns the passkey |
| `SerialBT.respondPasskey(uint32_t)` | Return the passkey from the `onPassKeyRequest` callback |
| `SerialBT.onAuthComplete(void(uint8_t*, bool))` | `onAuthComplete(void(bool))` — no address |
| `SerialBT.getBondedDevices()` returning `int` | Returns `std::vector<BTAddress>` |
| `SerialBT.getBondedDeviceAddress(int, uint8_t*)` | Iterate `getBondedDevices()` vector |
| `SerialBT.deleteBondedDevices()` | `BTStatus deleteAllBonds()` |
| `SerialBT.discoverAsyncStop()` | `BTStatus SerialBT.discoverStop()` |
| `SerialBT.register_callback(BTCallback)` | `BTStatus SerialBT.onData(std::function<void(const uint8_t*, size_t)>)` |
| `SerialBT.setPin(const char*, uint8_t len)` | `void SerialBT.setPin(const char*)` — length not needed |

### 12. Multi-Client Acceptor APIs

v4.0 acceptor mode can keep several SPP clients connected. Existing `write()`/`read()` keep
broadcast-out / merged-in `Stream` semantics so single-peer sketches stay valid. New methods:

```cpp
SerialBT.onConnect([](const BTAddress &addr) { /* ... */ });
SerialBT.onDisconnect([](const BTAddress &addr) { /* ... */ });
SerialBT.onPeerData([](const BTAddress &addr, const uint8_t *data, size_t len) {
    SerialBT.writeTo(addr, data, len);  // echo to that peer only
});

size_t n = SerialBT.peerCount();
auto peers = SerialBT.peers();  // std::vector<BTAddress>
SerialBT.disconnect(peers[0]);  // drop one client
```

See `examples/MultiClientSerial`. BLE NUS equivalent: `BLEStream` (`libraries/BLE`).

---

## Complete API Mapping

### Lifecycle

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `void begin(String name, bool isMaster)` | `BTStatus begin(const String &name, bool isInitiator)` | Returns `BTStatus` |
| `void end()` | `void end(bool releaseMemory = false)` | Optional memory release |

### Connection

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `bool connect(String remoteName)` | `BTStatus connect(const String &name, uint32_t timeoutMs)` | Explicit timeout |
| `bool connect(uint8_t *remoteAddress, ...)` | `BTStatus connect(const BTAddress &address, uint8_t channel = 0)` | `BTAddress` replaces raw array |
| `bool disconnect()` | `BTStatus disconnect()` | Returns `BTStatus` |
| `bool connected(int timeout = 0)` | `bool connected() const` | No timeout |
| `bool hasClient()` | `bool hasClient() const` | |

### Stream Interface

Unchanged: `available()`, `read()`, `write()`, `peek()`, `flush()`, `print()`, `println()`.

### Discovery

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `BTScanResults* discover(int timeout)` | `std::vector<DiscoveryResult> discover(uint32_t timeoutMs)` | Returns vector |
| `void discoverAsync(BTAdvertisedDeviceCb, int)` | `BTStatus discoverAsync(DiscoveryHandler, uint32_t)` | `std::function` callback |
| `void discoverAsyncStop()` | `BTStatus discoverStop()` | Renamed |

### Security

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `void enableSSP()` | `void enableSSP(bool input = false, bool output = false)` | Parameters explicit |
| `void disableSSP()` | `void disableSSP()` | Unchanged |
| `void setPin(const char *pin, uint8_t len)` | `void setPin(const char *pin)` | Length dropped (uses `strlen`) |
| `void onConfirmRequest(std::function<void(uint32_t)>)` | `BTStatus onConfirmRequest(std::function<bool(uint32_t)>)` | Callback returns bool |
| `void confirmReply(bool)` | **Removed** | Return bool from `onConfirmRequest` callback |
| `void onKeyRequest(cb)` | `BTStatus onPassKeyRequest(std::function<uint32_t(const BTAddress &)>)` | Callback returns the passkey |
| `void respondPasskey(uint32_t)` | Returned from `onPassKeyRequest` | No out-of-band reply call |
| *(none)* | `BTStatus onPassKeyDisplay(std::function<void(const BTAddress &, uint32_t)>)` | New: display side of Passkey Entry |
| `void onAuthComplete(std::function<void(uint8_t*, bool)>)` | `BTStatus onAuthComplete(std::function<void(bool)>)` | No address parameter |

### Bond Management

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `int getBondedDevices()` | `std::vector<BTAddress> getBondedDevices()` | Returns vector |
| `void getBondedDeviceAddress(int, uint8_t*)` | Iterate vector from `getBondedDevices()` | |
| `void deleteBondedDevices()` | `BTStatus deleteAllBonds()` | Returns `BTStatus` |
| N/A | `BTStatus deleteBond(const BTAddress &address)` | New: delete single bond |

### Data Callback

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `void register_callback(BTCallback cb)` | `BTStatus onData(std::function<void(const uint8_t*, size_t)>)` | `std::function`, returns `BTStatus` |

### Multi-Client (acceptor)

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| N/A (single peer) | `size_t peerCount() const` | Connected client count |
| N/A | `std::vector<BTAddress> peers() const` | Snapshot of connected addresses |
| N/A | `size_t writeTo(const BTAddress &, const uint8_t *, size_t)` | Unicast to one peer |
| N/A | `BTStatus onPeerData(PeerDataHandler)` | RX with source address |
| N/A | `BTStatus onConnect` / `onDisconnect` | Per-peer connect/disconnect |
| N/A | `BTStatus disconnect(const BTAddress &)` | Drop one peer |

### Info

| Old (v3.x) | New (v4.0) | Notes |
|---|---|---|
| `BTAddress getAddress()` | `BTAddress getAddress() const` | Returns shared `BTAddress` type |
| `bool operator bool()` | `explicit operator bool() const` | Now `explicit` -- no implicit conversion |

---

## Before/After Examples

### Basic Serial Bridge

**Before (v3.x):**

```cpp
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

void setup() {
    Serial.begin(115200);
    SerialBT.begin("ESP32-BT");
}

void loop() {
    if (Serial.available())   SerialBT.write(Serial.read());
    if (SerialBT.available()) Serial.write(SerialBT.read());
}
```

**After (v4.0):**

```cpp
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

void setup() {
    Serial.begin(115200);
    BTStatus s = SerialBT.begin("ESP32-BT");
    if (!s) { Serial.printf("BT init failed: %s\n", s.toString()); }
}

void loop() {
    if (Serial.available())   SerialBT.write(Serial.read());
    if (SerialBT.available()) Serial.write(SerialBT.read());
}
```

### Master (Initiator) Connect

**Before (v3.x):**

```cpp
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

void setup() {
    Serial.begin(115200);
    SerialBT.begin("ESP32-Master", true);
    if (SerialBT.connect("ESP32-Slave")) {
        Serial.println("Connected!");
    }
}
```

**After (v4.0):**

```cpp
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

void setup() {
    Serial.begin(115200);
    BTStatus s = SerialBT.begin("ESP32-Master", true);
    if (!s) { return; }

    BTStatus conn = SerialBT.connect(String("ESP32-Slave"), 10000);
    if (conn) {
        Serial.println("Connected!");
    } else {
        Serial.printf("Connect failed: %s\n", conn.toString());
    }
}
```

### SSP Pairing with Numeric Comparison

**Before (v3.x):**

```cpp
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

void setup() {
    SerialBT.enableSSP();
    SerialBT.onConfirmRequest([](uint32_t passkey) {
        Serial.printf("Confirm %06lu (y/n)? ", passkey);
        while (!Serial.available()) delay(100);
        char c = Serial.read();
        SerialBT.confirmReply(c == 'y' || c == 'Y');
    });
    SerialBT.onAuthComplete([](uint8_t *addr, bool success) {
        Serial.printf("Auth %s\n", success ? "OK" : "FAILED");
    });
    SerialBT.begin("ESP32-SSP");
}
```

**After (v4.0):**

```cpp
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

void setup() {
    SerialBT.enableSSP(true, true);   // display + keyboard -> numeric comparison
    SerialBT.onConfirmRequest([](uint32_t passkey) -> bool {
        Serial.printf("Confirm %06lu? Auto-accepting.\n", passkey);
        return true;   // accept directly; confirmReply() no longer exists
    });
    SerialBT.onAuthComplete([](bool success) {
        Serial.printf("Auth %s\n", success ? "OK" : "FAILED");
    });
    BTStatus s = SerialBT.begin("ESP32-SSP");
    if (!s) { Serial.println("BT init failed!"); }
}
```

### Bond Management Example

**Before (v3.x):**

```cpp
int count = SerialBT.getBondedDevices();
Serial.printf("Bonded: %d\n", count);
SerialBT.deleteBondedDevices();
```

**After (v4.0):**

```cpp
auto bonded = SerialBT.getBondedDevices();  // std::vector<BTAddress>
Serial.printf("Bonded: %d\n", bonded.size());
for (const auto &addr : bonded) {
    Serial.println(addr.toString());
}
BTStatus s = SerialBT.deleteAllBonds();
Serial.printf("Delete all bonds: %s\n", s ? "OK" : "FAILED");
```
