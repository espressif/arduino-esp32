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

Backend choice is centralized in `impl/common/BLEGuards.h`.
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

Repeated calls return handles to the same underlying object. Implementations live in
`impl/<stack>/*Core.cpp`; each starts with the same two-line `isInitialized()` guard (written out
directly, not through a shared wrapper), then writes its own lazy-singleton alloc and backend
post-create wiring directly (Bluedroid additionally sets `s_instance` for GAP event routing and may
call register helpers like `bluedroidRegisterGattsApp`). There's no generic factory helper: the
lazy-singleton/fresh-instance split and the post-create logic genuinely differ per factory, so a
shared "one size fits all" abstraction would either need to be bypassed regularly (as Bluedroid's
`createServer` already must be, for its asynchronous GATTS registration retry) or made generic
enough that it stopped being easier to read than the explicit code it replaced.
`BLEClass()`/`~BLEClass()` need a complete `BLEClass::Impl` (backend-specific), so they're defined
once per backend `*Core.cpp` rather than shared — 7 identical lines duplicated is cheaper than a
shared header for this one case.

### Per-instance handles

`BLE.createClient()` returns a fresh client instance each time.
This is a hard design requirement: multi-client support must remain possible.
Each backend's `createClient()` writes the same guard + fresh-alloc pattern directly; Bluedroid
additionally delegates stack registration to `bluedroidRegisterClient` in `BluedroidClient.cpp`.

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

**Impl composition model (two-layer: shared base + concrete backend `Impl`).**
Where a class has meaningful stack-agnostic state/logic, the concrete `Foo::Impl` — defined by the
active backend — inherits the shared base directly. There is no intermediate mixin and no empty combiner:

```cpp
// impl/common/BLEServerImpl.h  — shared, compiled in every build
struct BLEServerImplCommon : std::enable_shared_from_this<BLEServer::Impl> { /* shared state + methods */ };

// impl/nimble/NimBLEServer.h   — the NimBLE build's concrete Impl
struct BLEServer::Impl : BLEServerImplCommon { /* NimBLE state + static handlers */ };
```

- `_impl` is a `std::shared_ptr<Impl>` pointing at the concrete `Foo::Impl` (`make_shared<Impl>()`).
- Stack-agnostic state and dispatch live in `*ImplCommon` (`src/impl/common/BLE*Impl.h`), shared by both backends (kills duplication — item 4).
- Backend-only state/statics live on `Foo::Impl` itself, **defined under `impl/<stack>/`**. The file location (and the Doxygen "NimBLE-specific"/"Bluedroid-specific" brief) tells you a member's layer: `FooImplCommon::` = shared, `Foo::Impl::` in `impl/<stack>/` = backend (item 2).
- Access is **uniform and cast-free** via inheritance: `_impl->mtx`, `_impl->gattsIf`, `service->server->gattsIf` all resolve. No `static_cast`, no `backend()` helper, no mixin name to learn.
- The single sanctioned access pattern in `.cpp` code is the `BLE_CHECK_IMPL(...)` null-guard macro, which yields a cast-free `impl` reference (item 3).
- Backends reconstruct a public handle from a raw `Impl*` via `enable_shared_from_this` and a static `*ImplCommon::makeHandle` factory, avoiding extra `friend` declarations.

This shared-base split is applied wherever there is real cross-backend duplication (Server, Service, Characteristic, Descriptor, Client, the three Remote types, Security, AdvertisedDevice). Classes that are inherently per-backend with no shared state (`BLEClass`, `Scan`, `Advertising`, `BLEL2CAPServer`) define `Foo::Impl` directly with no base. All backend `Impl` layouts are pulled in through the single umbrella selector `impl/BLEBackend.h`, which also `#include`s the compile-time contract check `impl/common/BLEBackendContract.h`; a CI guard (`tests/check_backend_isolation.sh`) fails the build if a public header names a backend type or includes a backend/selector header. Backend obligations are spelled out in [Backend contract](#backend-contract) below; stack-specific notes live in `src/impl/nimble/README.md` and `src/impl/bluedroid/README.md`.

**Handle construction: `makeHandle` vs. the private constructor.**
Every public handle's `std::shared_ptr<Impl>` constructor is **private** — handles must only be minted by the library, never fabricated by user code around an arbitrary `Impl*`. Code that needs to build a handle falls into two groups:

- *Friends* of the handle — the owning public classes (e.g. `BLEServer` building a `BLEService`) and the class's own `*ImplCommon` — call the private constructor directly.
- *Backend code* — the `impl/<stack>/` translation units (the concrete `Foo::Impl` and its `static` GAP/GATT callbacks) — is deliberately **not** a friend. It mints handles through the one static factory `*ImplCommon::makeHandle(...)`, which is a member of the single befriended type per class.

The point is to keep the `friend` surface minimal (item 5): each handle declares exactly one `friend struct BLE<T>ImplCommon` instead of befriending every backend TU and free function across both stacks. `makeHandle` additionally centralizes the raw-`Impl*` → owning-`shared_ptr` reconstruction (`impl->shared_from_this()`) for the callback paths that receive a bare `Impl*` through a `void *arg`, so that step is written and audited once rather than at every call site. The rejected alternatives: a **public constructor** would break encapsulation (any caller could wrap a foreign/dangling `Impl*`), and **per-backend `friend` declarations** would proliferate friendship (two backends × many TUs) for no clarity gain. So inside friend/public code the constructor is used directly; backend code uses `makeHandle`.

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

The null-handle guard is standardized through the `BLE_CHECK_IMPL` macro in `impl/common/BLEImplHelpers.h`.
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
`BTAddress`, `BTStatus`, `BLEUUID`, `BLEConnInfo`, `BLEAdvertisementData`, `BLEScan::Results`.

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

- `BLELockGuard` (`impl/common/BLEMutex.h`): RAII wrapper around a FreeRTOS recursive mutex
- `BLESync` (`impl/common/BLESync.h`): blocking bridge for async stack operations

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

`impl/common/BLECharacteristicValidation.h` provides spec-compliance checks run in two stages:

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

The extended/periodic **advertising** API is **per instance, per setter** (`setExt*`, `startExtended`, `setPeriodicAdv*`, `startPeriodicAdv`), mirroring the legacy setter style and the BLE5 receive side — not a config-struct form.
Because extended advertising is inherently multi-instance (SID, secondary PHY, per-set data, anonymous mode), it needs its own per-instance surface distinct from the single-instance legacy path; legacy stays the simple default.
Current status: on NimBLE with `BLE5_SUPPORTED`, both the **receive** side (extended discovery + periodic sync) and the **transmit** side (extended + periodic advertising) are implemented over the extended controller API. When BLE5 is enabled, legacy `start()`/`stop()` and scanning are also routed through the extended engine using legacy PDUs (a reserved top instance broadcasts the legacy `ADV_IND` so it can coexist with user extended sets) — see [Advertising / scanning routing](#advertising--scanning-capability-gated-routing) below. Bluedroid mirrors that surface when `BLE5_SUPPORTED` is on (BLE5-capable chips with `CONFIG_BT_BLE_50_FEATURES_SUPPORTED`): extended adv/scan, connection PHY/DLE, `setDefaultPhy`, periodic advertising TX (`CONFIG_BT_BLE_50_PERIODIC_ADV_EN`), and periodic sync RX. Classic esp32 stays on BLE 4.2 and uses the shared `NotSupported` fallbacks. Where a backend lacks a capability, the `NotSupported` behavior is a single shared fallback in the stack-agnostic layer, not a per-backend copy. On non-BLE-5 builds every BLE5 method returns `BTStatus::NotSupported` (setters are no-ops), so callers must check the returned status. On-air BLE5 TX/sync verification requires a 2-DUT BLE5 rig (HITL).

### BLEStream

`BLEStream` is a higher-level convenience layer that exposes the Nordic UART Service through the Arduino `Stream` API.
It follows the same stack-agnostic and lifecycle rules as the lower-level BLE objects.

In **server mode**, multiple centrals may connect at once. As a single Arduino `Stream`, `write()`/`print()` broadcast to every subscribed peer and `read()`/`available()` drain one merged RX buffer. Additive APIs attribute traffic per peer: `peerCount()`, `peers()`, `writeTo(peer, ...)`, `onPeerData(...)`, and `disconnect(addr)`. Client mode remains 1:1. See `examples/MultiClientUART`.

### L2CAP CoC

L2CAP CoC is guarded by `BLE_L2CAP_SUPPORTED` and only available when the backend and Kconfig support it.
This is an accepted exception because capability depends on both backend and build configuration.
Even so, the public API remains generic and does not expose backend types.
`write()` splits payloads larger than the peer CoC MTU into MTU-sized SDUs and may block briefly between chunks while waiting for peer credits (`COC_TX_UNSTALLED`). NimBLE itself does not segment oversized SDUs — that is done in the Arduino wrapper.
On receive, SDUs up to a fixed 256-byte stack threshold are flattened without a heap allocation; larger SDUs (when the channel MTU is bigger) spill to a heap vector. Channel MTU itself is whatever the caller configured (128, 256, 512, …) and is independent of that RX threshold. After flatten, the received `sdu_rx` mbuf is freed and a fresh buffer is posted via `ble_l2cap_recv_ready` — omitting the free drains the CoC pool and stalls peer credits.

## Backend contract

Checklist for adding a third BLE backend (or understanding what an existing one owns). Codifies the two-layer Impl model above.

### File layout (layer signal)

```
src/BLEFoo.h                     public handle  (backend-agnostic; no NimBLE/Bluedroid headers)
src/BLEFoo.cpp                   public-API method definitions (backend-agnostic)
impl/common/BLEFooImpl.{h,cpp}   struct BLEFooImplCommon   -> SHARED   (compiled in every build)
impl/<stack>/*.{h,cpp}           struct BLEFoo::Impl       -> BACKEND  (only this stack's build)
```

A member on `BLEFooImplCommon` is **shared**. A member on `BLEFoo::Impl` (under `impl/<stack>/`) is **stack-specific**. Access is always cast-free `impl.member`. The active backend is selected in exactly one place: `impl/BLEBackend.h`.

### Backend subsystem pairs

Each backend lives in `impl/<stack>/` as strict `.h`/`.cpp` **subsystem pairs** (every `.cpp` has a same-named header; the two backends are structurally identical except NimBLE-only `L2CAP`):

| Pair | Owns |
| --- | --- |
| `Core` | `BLEClass::Impl`: init/teardown, identity, MTU, IRK, whitelist, GAP hooks |
| `Server` | `BLEServer::Impl`: GATTS registration + server GAP/GATTS events |
| `GattAttributes` | Service / Characteristic / Descriptor `Impl`s (notify, permissions, registration) |
| `Client` | `BLEClient::Impl`: connection management + GATTC events |
| `RemoteGatt` | Remote service / characteristic / descriptor `Impl`s |
| `Scan` / `Advertising` / `Security` | Respective `Impl`s |
| `ConnInfo` / `UUID` | Native ↔ public bridges |
| `L2CAP` | *(NimBLE only)* L2CAP server/channel `Impl`s |

`impl/BLEBackend.h` includes the headers that define a `Foo::Impl` so the contract sees every `Impl`. See each backend's `README.md` for event-routing maps and quirks.

### Accessing `_impl`

`_impl` is null only for a default-constructed or moved-from handle. Use `BLE_CHECK_IMPL` (`impl/common/BLEImplHelpers.h`):

- **`src/*.cpp` (public facade):** `BLE_CHECK_IMPL(<return-on-null>)` is mandatory as the first statement of any method that touches `_impl`.
- **`impl/**` (backend TUs):** prefer the macro; a hand-written `if (!_impl)` is allowed only when the null path needs extra work the macro cannot express. Never mix the macro and a manual `*_impl` alias in one method.
- **Trivial validity checks** (e.g. `operator bool`): write `return _impl != nullptr;` directly.

### What a backend must provide

1. **Type definitions** — one `struct Foo::Impl` per PIMPL class, inheriting `FooImplCommon` when a shared base exists (enforced by `BLEBackendContract.h`). Baseless classes (`BLEClass`, `Scan`, `Advertising`, L2CAP) define `Foo::Impl` directly. `BLEAdvertisedDevice` is fully shared — no backend `Impl` needed.
2. **Backend-specific public methods** — anything not already defined in `src/*.cpp` that talks to the native stack. **Exception — `BLEClass`:** all lifecycle, identity, MTU/PHY/whitelist, custom handlers, **and** factory accessors live in `impl/<stack>/*Core.cpp` only.
3. **Agnostic hooks** declared in `impl/common/` and implemented per backend (e.g. `bleServerRemoveService`). Validation helpers such as `bleValidateCharProps` are shared — consume them, do not reimplement.
4. **Lifecycle + event routing** — init/deinit in `*Core.cpp`; route GAP/GATT into shared handlers (`handleGAP` / `handleGATTS` / `handleGATTC`, advertising/scan/security). Fire shared `BLESecurityImplCommon` pairing hooks on the process-wide `BLESecurity::Impl::instance()`.
5. **Resource teardown in `end()`** — the library owns cleanup of resources it created. `end()` already resets advertising and stops scanning; backends with periodic-sync must call `BLEScan::terminateAllPeriodicSyncs()` **while the host is still enabled**. Terminating after host disable races teardown.

### Observable ordering (must match across backends)

- **Scan results:** cache into `results` **before** `onResult`, so `getResults()` inside the handler sees the current device.
- **Client connect:** deliver `onConnect` **before** unblocking a blocking `connect()`.
- **Client disconnect:** tear down connection state, then deliver `onDisconnect` only if the link had reached connected.
- **Client MTU exchange:** initiate from the **caller's thread inside blocking `connect()`**, after the connect sync is released, and **block until it completes**. Bluedroid drops a pending MTU procedure if another GATT/GAP op starts while it is in flight. NimBLE may run the exchange after `connect()` returns; `onMtuChanged` still lands after `onConnect`. `connectAsync()` does **not** auto-exchange — call `setMTU()` after `onConnect`. MTU is not yet negotiated *inside* `onConnect`.
- **Default scan timing:** interval == window == 30 ms (continuous active scan) unless overridden.
- **`BLEConnInfo::getIdAddress()`:** resolved identity when the stack provides one (NimBLE `peer_id_addr`), else OTA. Bluedroid has no resolved-identity field → always OTA, never empty.

### Advertising / scanning: capability-gated routing

The public API is backend-agnostic; routing depends on `BLE5_SUPPORTED`:

- **`BLE5_SUPPORTED == 0`:** `start()`/`stop()` and scan use the **native legacy** controller API. AD/scan-response payload is still serialized via `BLEAdvDataBuilder` and pushed with the controller's *raw* data API — never a per-stack field struct. Bluedroid must not use `esp_ble_gap_config_adv_data` (force-expands every service UUID to 128 bits).
- **`BLE5_SUPPORTED == 1`:** the same public calls run over the **extended** controller API with **legacy PDUs**. A backend must: reserve the top controller instance for the legacy set (`kLegacyInstance`); map `advType` to legacy-PDU flags; build raw AD via `BLEAdvDataBuilder`; merge extended scan responses by address via `mergeScanResponse`; clear extended state in `reset()`; add its term to `BLE5_ADV_TX_SUPPORTED` so the shared `NotSupported` fallback disables itself.

**Preferred connection-interval AD (0x12) is emitted** when either `setMinPreferred` or `setMaxPreferred` is non-zero. `BLEAdvDataBuilder` places the Slave Connection Interval Range AD in the primary payload (a zero side copies the other bound). Connection parameters are still negotiated post-connect; the AD is only a hint to centrals.

### Friend bridges (cross-class private access)

Friendship is not inherited. When a backend `Impl` must touch privates of a *different* public class, use the existing agnostic bridge — do not add a backend `friend` to a public header:

- `BLEScanImplCommon::makeAdvertisedDeviceHandle` / `mergeScanResponse`
- `BLEL2CAPChannelImplCommon::makeHandle`
- `BLEServerImplCommon::forwardGapEvent`
- `*ImplCommon::makeHandle` — reconstruct a public handle from a raw `Impl*` in a `void *arg` callback

### Naming conventions

- Object mutex on every Impl: **`mtx`** (one name).
- Callback storage: `on<Event>Cb`; public typedefs: `<Event>Handler`.
- `connHandle` = stack-agnostic 16-bit connection handle; `connId` = Bluedroid-native GATT id (backend only); `handle` = GATT attribute handle.
- Stack-specific free functions: `nimble*` / `bluedroid*` prefix. UUID converters: `<stack>Uuid{From,To}Public`. Uniform helpers: neutral `ble*` name.

### Enforcement

- **Compile time:** `impl/common/BLEBackendContract.h` `static_assert`s shared-base inheritance and that baseless `Impl`s are defined.
- **Isolation:** `tests/check_backend_isolation.sh` fails if a public `src/BLE*.h` names a backend type or includes a backend/selector header.
- **Link time:** any public/agnostic method left undefined by the backend is a linker error.

### Known non-normalizable divergences

- **`BLEConnInfo` real PHY on Bluedroid:** on BLE5 builds, cached from `ESP_GAP_BLE_READ_PHY_COMPLETE_EVT` / `ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT` (defaults to `PHY_1M` until the first event). On BLE 4.2-only controllers the read-PHY API is unavailable by design, so the field stays `PHY_1M`. Peer address type is taken from GATTS/GATTC connect and `AUTH_CMPL` events.
- **`client.onConnParamsUpdateRequest`:** NimBLE-only. Bluedroid's L2CAP layer accepts/negotiates connection-parameter updates internally and only surfaces the result (`ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT`) after the fact, so there is no application pre-accept hook to bridge.
- **Bluedroid `BLEConnInfo::keySize`:** `AUTH_CMPL` carries no key size; stays at default (NimBLE reports `sec_state.key_size`).
- **Bluedroid BLE5 surface:** extended adv/scan, PHY/DLE, periodic adv TX, and periodic sync RX are implemented under `BLE5_SUPPORTED` / `BLE_PERIODIC_ADV_SUPPORTED`. Classic esp32 Bluedroid libs remain BLE 4.2-only.
- **HITL-pending coverage:** multi-central BLEStream under load (two simultaneous centrals with per-peer routing; needs a third DUT). Passkey-entry pairing + Numeric-Comparison reject, periodic terminate/sync-lost, L2CAP bulk across the credit stall + reconnect, conn-param reject, and the `deleteBond` round-trip are now covered by phases 28-34. See `tests/validation/ble/README.md`.

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
