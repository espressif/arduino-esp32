# BLE Library Design

## Purpose

This document is for maintainers and AI tools.
It describes the architecture, invariants, and design rules that should guide future changes.
It intentionally does not describe every file.

## Main goals

The library is built around these goals:

- expose one public BLE API for all supported BLE stacks
- use value-style public handles instead of user-managed raw pointers
- standardize result handling on `BTStatus`
- separate GATT properties from security permissions
- support multiple concurrent BLE clients
- keep advanced features such as BLE5, BLEStream, and L2CAP behind the same design rules
- fix long-standing lifecycle, ownership, and spec-compliance issues from the original Kolban-era library

## Non-goals

These are **not** design goals:

- source compatibility with the legacy Kolban-style BLE API
- ArduinoBLE API compatibility in the core library itself
- exposing NimBLE- or Bluedroid-specific types in user-facing APIs
- letting public behavior drift per backend

Backend differences may exist internally, but they should stay behind the common API surface.

## High-level architecture

The library is split into three layers:

1. **Public API layer** (`src/`): `BLE.h` and all public handle types (`BLEServer`, `BLEClient`, `BLEService`, `BLECharacteristic`, `BLEDescriptor`, `BLEScan`, `BLEAdvertising`, `BLESecurity`, remote GATT types, `BLEStream`, `BLEL2CAP*`).
2. **Backend-selection and infrastructure layer** (`src/impl/`): `BLEGuards.h`, synchronization primitives, GATT validation, and thin backend bridge headers (`BLEServerBackend.h`, `BLEClientBackend.h`, `BLEScanBackend.h`, etc.).
3. **Concrete backend layer** (`src/impl/nimble/` and `src/impl/bluedroid/`): full NimBLE and Bluedroid implementations.

The public layer owns the API contract.
Backends implement that contract and should not redefine it.

## Backend model

Backend choice is centralized in `impl/BLEGuards.h`.
All BLE code must use these guards instead of repeating raw SDK config checks.

Stack selection guards:

- `BLE_ENABLED`: any supported BLE backend is available
- `BLE_NIMBLE`: NimBLE backend selected
- `BLE_BLUEDROID`: Bluedroid backend selected

Feature guards (derived from Kconfig / SoC capabilities):

- `BLE_GATT_SERVER_SUPPORTED`: GATT server role available
- `BLE_GATT_CLIENT_SUPPORTED`: GATT client role available
- `BLE_SMP_SUPPORTED`: Security Manager Protocol available
- `BLE_SCANNING_SUPPORTED`: BLE scanning available
- `BLE_ADVERTISING_SUPPORTED`: BLE advertising available
- `BLE5_SUPPORTED`: BLE 5.0 features (extended advertising, PHY selection, periodic advertising)
- `BLE_L2CAP_SUPPORTED`: L2CAP CoC channels; NimBLE only, requires explicit Kconfig

### Hosted BLE

NimBLE also runs on ESP32-P4 via the esp-hosted bridge (`CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE`).
`BLEGuards.h` accounts for this: `BLE_NIMBLE` is set even when the SoC has no native BT controller.
`BLEClass::isHostedBLE()` and `BLEClass::setPins()` expose the hosted-specific configuration.

### Design rule

Do not push backend-specific conditionals into user code or public APIs unless there is no alternative.
If a feature is stack-exclusive, prefer keeping the public API and returning `BTStatus::NotSupported` at runtime with a log message.

## Public object model

### `BLE`

`BLE` is the global entry point following the Arduino convention (`WiFi`, `Wire`, etc.).
It owns stack initialization, shutdown, controller-level settings, and factories for the rest of the library.

### Singleton-style handles

These objects are effectively global shared handles returned by `BLE`:

- `BLE.createServer()`
- `BLE.getScan()`
- `BLE.getAdvertising()`
- `BLE.getSecurity()`

Repeated calls return handles to the same underlying object.

### Per-instance handles

`BLE.createClient()` returns a fresh client instance each time.
This is a hard design requirement: multi-client support must remain possible.

### GATT handles

Server-side and client-side GATT objects are lightweight handles:

- local: `BLEService`, `BLECharacteristic`, `BLEDescriptor`
- remote: `BLERemoteService`, `BLERemoteCharacteristic`, `BLERemoteDescriptor`

These objects are copied by value but refer to shared underlying state.

### Connection metadata

`BLEConnInfo` is the stack-agnostic connection descriptor.
It replaces backend-specific callback parameter types and stores its data inline.
Public APIs should prefer `BLEConnInfo` over raw NimBLE/Bluedroid structs.

## PIMPL pattern

Every public BLE class that represents a radio resource uses the PIMPL pattern: the public header forward-declares `struct Impl` and holds only a `std::shared_ptr<Impl>` as its data member.
The `Impl` struct is defined entirely inside the backend `.cpp` file, so public headers never include NimBLE or Bluedroid headers.
This gives clean backend isolation: changing an Impl definition is a `.cpp`-only change with no effect on the public ABI.

**Exception: `BLEStream`** uses a raw `Impl*` (not `shared_ptr`) and is non-copyable/non-movable.
It owns server or client resources exclusively and follows RAII semantics: `begin()`/`beginClient()` allocate, `end()` or the destructor releases.
This is intentional — `BLEStream` wraps a higher-level convenience API (Arduino `Stream`) where shared-handle semantics do not apply.

## Ownership and lifetime rules

BLE objects represent physical or radio resources — there is only one physical server, one physical connection, one characteristic attribute.
These resources cannot be deep-copied.
`std::shared_ptr<Impl>` is used instead of `std::unique_ptr<Impl>` for two reasons:

- it allows multiple handles to the same resource from different places in user code (a global variable, a callback capture, a function parameter), all equally valid
- `std::shared_ptr` type-erases its deleter, so it does not require the complete `Impl` type at the header declaration site — forward-declaring `struct Impl;` is sufficient

Because `std::shared_ptr` has well-defined copy, move, and destruction semantics, all five special member functions are compiler-generated defaults on every public handle class.
No user `delete` is ever needed or safe.

Handle states and semantics:

- **default-constructed**: empty handle, `operator bool() == false`
- **copy**: both handles share the same Impl (refcount incremented); both are equally valid
- **move**: source becomes empty, destination acquires ownership
- **destruction**: refcount decremented; Impl is freed when the last handle is released
- **multiple handles**: interchangeable — operations on any handle affect the same underlying resource

The null-handle guard is standardized through the `BLE_CHECK_IMPL` macro in `impl/BLEImplHelpers.h`.
If a public handle has no Impl, methods return a safe default or `BTStatus::InvalidState`.

### Parent-child ownership

BLE objects form strict ownership trees:

- Server → Service → Characteristic → Descriptor
- Client → RemoteService → RemoteCharacteristic → RemoteDescriptor

Parent Impls hold `std::shared_ptr` to child Impls (strong ownership).
Child Impls hold a raw `Impl*` back-reference to their parent (non-owning).
This direction is safe because a parent always outlives its children — destroying a service destroys its characteristics.
Using `shared_ptr` in both directions would create reference cycles and leak everything.

### `enable_shared_from_this` and handle reconstruction

Impl structs that need to reconstruct a public handle from a raw `Impl*` inherit from `std::enable_shared_from_this<Impl>`.
This solves two problems that arise because internal code often holds only a raw `Impl*`:

1. **Callback dispatch** — Backend event handlers (static C callbacks from NimBLE/Bluedroid) receive a raw `Impl*` via context pointers. To invoke the user's `std::function` callback they must construct a public handle (e.g. `BLECharacteristic`), which requires a `shared_ptr<Impl>`. `shared_from_this()` safely promotes the raw pointer without creating a second ownership group.

2. **Child-to-parent navigation** — Methods like `BLEDescriptor::getCharacteristic()` and `BLEService::getServer()` need to return a public handle from the non-owning raw parent `Impl*`. `shared_from_this()` on the parent Impl produces the `shared_ptr` needed to construct that handle.

**Safety constraint:** `shared_from_this()` may only be called on an Impl that is already owned by at least one `shared_ptr`. This is always the case in practice because Impls are created via `std::make_shared` in factory methods (`createServer`, `createService`, etc.) and the parent keeps the owning `shared_ptr` alive for the entire lifetime of the child.

### Lifecycle after `BLE.end()`

When `BLE.end()` is called, each Impl releases its child ownership chains, frees internal heap allocations, and marks itself as not initialized.
The bulk of memory is reclaimed immediately.
If the user still holds handles, each Impl enters a minimal shell state (~16–32 bytes) that persists until the last handle is released, at which point the shell is freed automatically.
This means `BLE.end(true)` (controller memory release) works correctly even if the user has forgotten to clear global handles.

### True value types

The following types carry no Impl and use plain value semantics with no special considerations:
`BTAddress`, `BTStatus`, `BLEUUID`, `BLEConnInfo`, `BLEAdvertisementData`, `BLEScanResults`.

### Design rule

New public BLE handle types should follow the shared-handle model unless there is a strong, explicit reason not to.
Do not introduce new user-owned heap objects for normal BLE flows.

## Callback model

All public BLE classes use per-event registration via `std::function`.
Examples include `BLECharacteristic::onWrite(...)`, `BLEClient::onDisconnect(...)`, and `BLEScan::onResult(...)`.
Every class that registers callbacks also provides `resetCallbacks()` to clear them all.

Do not add new `Callbacks` abstract classes or function-pointer callback types.
When extending the library, always use the per-event `std::function` registration style.

### Callback dispatch rule

Callbacks are copied under the object mutex and invoked after the lock is released.
This avoids calling user code while internal locks are held.
Preserve that pattern.

### Execution-context rule

BLE callbacks can run on stack-controlled contexts, not only on the Arduino `loop()` thread.
They must stay short, avoid blocking the stack, and avoid assumptions about call ordering relative to sketch code.

## Concurrency and synchronization

This library uses FreeRTOS primitives directly.
It deliberately avoids `std::mutex` to keep binary size down.

Current primitives:

- `BLELockGuard` (`impl/BLEMutex.h`): RAII wrapper around a FreeRTOS recursive mutex
- `BLESync` (`impl/BLESync.h`): blocking bridge for async stack operations

`BLESync` is used when the public API offers a blocking operation while the backend completes it asynchronously.
The expected pattern is: prepare with `take()`, start async stack work, wait with `wait(timeoutMs)`, release from the stack callback with `give(status)`.

### Design rules

- use `SemaphoreHandle_t` + `BLELockGuard` for internal BLE mutexes
- do not introduce `std::mutex` into BLE internals
- when wrapping async backend flows with synchronous public APIs, use `BLESync` instead of ad-hoc synchronization
- never invoke user callbacks while holding an internal mutex

## Error model

`BTStatus` is the standard status/result type for BLE operations.
It is shared with BT Classic infrastructure and is intentionally tiny.

### Design rules

- return `BTStatus` for operations that can fail
- use specific statuses (`Timeout`, `NotFound`, `NotSupported`, `AlreadyConnected`, `InvalidState`, etc.) instead of generic failure where possible
- do not mix `bool`, raw stack error codes, and silent failure semantics in new public BLE APIs
- convert backend-native errors into `BTStatus` at the boundary

## GATT model and rules

### Properties vs permissions

The library treats these as separate concepts:

- `BLEProperty`: what a characteristic can do (read, write, notify, indicate, etc.)
- `BLEPermission`: what security level is required to access it

This separation is fundamental and must be preserved.
Do not reintroduce stack-specific property/permission conflation into public APIs.

### GATT validation

`impl/BLECharacteristicValidation.h` provides spec-compliance checks run in two stages:

- **Construction time**: validates properties and permissions against the Bluetooth Core Spec
- **Registration time**: validates the complete descriptor set after any backend-specific auto-creation (e.g., Bluedroid CCCD)

Hard errors prevent registration; soft violations surface as log warnings.

### Service staging and startup

Creating services and characteristics builds up an in-memory GATT model.
`BLEServer::start()` is the registration point that pushes that model into the backend stack.
Code should not assume that `createService()` alone means the service already exists in the controller.

### Descriptor rules

- CCCD is auto-created for notify/indicate characteristics
- standard descriptor helpers are exposed through `BLEDescriptor`
- descriptor behavior should remain stack-agnostic from the caller's perspective

### Service removal

`BLEServer::removeService()` has backend-specific implementation details:

- NimBLE rebuilds the full GATT database
- Bluedroid deletes the service through the stack

The public semantic is one API regardless.
Any new code must preserve this abstraction.

## Lifecycle rules

`BLE.begin()` initializes the selected backend and `BLE.end()` shuts it down.
The BLE singleton tracks whether the stack is initialized and whether controller memory was permanently released.

Important lifecycle invariants:

- most BLE work is invalid before `BLE.begin()`
- `BLE.end(true)` is terminal for the process lifetime; reinitialization is blocked after controller memory release
- factories return invalid handles when BLE is not initialized
- shutdown must leave the library in a safe, non-dangling state even if public handles still exist

### Design rule

Changes to lifecycle code must be validated against memory-release and reinitialization behavior.
This is a critical contract for sketches that switch between radio features at runtime.

## Stack-agnostic API rules

The public API should describe BLE concepts, not backend implementation details.
That means:

- use `BTAddress`, not backend address structs
- use `BLEConnInfo`, not backend callback params
- keep backend names out of normal user-facing method signatures
- keep feature parity where practical
- if parity is impossible, fail predictably with `BTStatus::NotSupported`

This rule applies even when one backend requires substantially more internal plumbing than the other.

## Feature-specific notes

### BLE5

Extended advertising, periodic advertising, extended scanning, periodic sync, PHY control, and data length control all belong to the common API surface when supported.
Gate backend availability through `BLE5_SUPPORTED` and runtime status reporting, not by fragmenting the public interface.

### BLEStream

`BLEStream` is a higher-level convenience layer that exposes the Nordic UART Service through the Arduino `Stream` API.
It follows the same stack-agnostic and lifecycle rules as the lower-level BLE objects.

### L2CAP CoC

L2CAP CoC is guarded by `BLE_L2CAP_SUPPORTED` and only available when the backend and Kconfig support it.
This is an accepted exception because capability depends on both backend and build configuration.
Even so, the public API remains generic and does not expose backend types.

## Testing as executable design

The BLE validation suite in `tests/validation/ble/` is the best executable summary of the intended behavior.
It exercises lifecycle, GATT server/client setup, read/write/notify/indicate flows, large ATT writes, descriptor behavior, security/pairing, BLE5 extended features, reconnect cycles, `BLEStream`, L2CAP CoC, and memory release.

### Design rule

When behavior changes, update the validation tests or add new ones so the intended contract stays executable.
For cross-stack changes, think in terms of common behavior first and backend mechanics second.

## Maintainer rules

When changing this library, keep these rules in mind:

1. **Protect the common API surface.** Public BLE APIs should not require NimBLE/Bluedroid conditionals in sketches.
2. **Prefer handle semantics.** Public objects should remain cheap, copyable handles with shared ownership.
3. **Use `BTStatus`.** New failure paths should be explicit and structured.
4. **Preserve property/permission separation.** Do not collapse them back together.
5. **Keep callbacks safe.** Copy callback state under lock, invoke outside the lock, and assume stack-thread execution.
6. **Use the existing synchronization primitives.** `BLELockGuard` and `BLESync` are the standard internal tools.
7. **Keep lifecycle strict.** Initialization, shutdown, and memory release semantics are core library contracts.
8. **Treat tests as part of the design.** If the intended behavior changes, the validation suite should change with it.
9. **Use feature guards correctly.** Check the right guard (`BLE_GATT_SERVER_SUPPORTED`, `BLE_SMP_SUPPORTED`, etc.) rather than the broad stack guard.

## Practical guidance for AI tools

If you are modifying this library:

- start from the public contract in the headers and the validation tests
- check whether the change affects both backends
- prefer adapting backend code to the shared API rather than changing the shared API to fit one backend
- look for existing patterns in handle ownership, callback dispatch, and `BTStatus` conversion before inventing a new one
- treat `MIGRATION.md` as a user-facing transition guide and this document as the maintainer-facing architecture guide

If the code and this document disagree, follow the code and update this document.

## Reference material

- ESP-IDF BT component: <https://github.com/espressif/esp-idf/tree/master/components/bt>
- ESP-IDF NimBLE host: <https://github.com/espressif/esp-idf/tree/master/components/bt/host/nimble>
- ESP-IDF NimBLE Kconfig options: <https://github.com/espressif/esp-idf/blob/master/components/bt/host/nimble/Kconfig.in>
- ESP-IDF Bluedroid host: <https://github.com/espressif/esp-idf/tree/master/components/bt/host/bluedroid>
- ESP-IDF Bluedroid Kconfig options: <https://github.com/espressif/esp-idf/blob/master/components/bt/host/bluedroid/Kconfig.in>
- Pre-refactor library: <https://github.com/espressif/arduino-esp32/tree/494670d3c010115b06604e8b69ffd64781910486/libraries/BLE>
- Original Kolban BLE library: <https://github.com/nkolban/ESP32_BLE_Arduino>
- NimBLE-Arduino (h2zero): <https://github.com/h2zero/NimBLE-Arduino>
