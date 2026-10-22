# NimBLE backend

NimBLE implementation of the public BLE API. Built when `BLE_NIMBLE` is set (see
`core/BLEGuards.h`), i.e. when the SDK selects the Apache NimBLE host
(`CONFIG_BT_NIMBLE_ENABLED`, e.g. `esp32s3`, `esp32c3`). Cross-backend obligations
(two-layer Impl model, subsystem pairs, ordering contract, enforcement) live in
[`DESIGN.md`](../DESIGN.md#backend-contract); this file only covers what is
specific to NimBLE.

## High-level architecture

- **Host task.** `BLEClass::begin()` initializes the controller, configures the NimBLE port and
  store, and starts the NimBLE host on its own FreeRTOS task (`BLEClass::Impl::hostTask` runs
  `nimble_port_run()`). All host callbacks execute on that task, so any state they share with the
  application task is mutex-guarded.
- **Sync/reset.** The host signals readiness via `BLEClass::Impl::onSync` (sets the GAP name and
  ensures an identity address) and clears readiness on `onReset`.
- **Async GATT client.** NimBLE's GATT client is callback-driven. The remote read/write/subscribe
  and service/characteristic/descriptor discovery paths issue the `ble_gattc_*` call, then block on
  a `BLESync` that the corresponding completion callback releases.

## Event routing

Native events reach the shared handlers as follows:

- **GAP.** A single `ble_gap_event_listener` (registered in `BLE.nimble`) plus per-object GAP callbacks
  fan out to `BLEServer::Impl`, `BLEClient::Impl`, `BLEAdvertising::Impl`, and `BLEScan::Impl`, and
  to the security hooks on `BLESecurity::Impl::instance()`.
- **GATTS.** The characteristic/descriptor access callbacks
  (`BLECharacteristic::Impl::accessCallback` / `descAccessCallback`) invoke the user read/write
  hooks and copy the value to/from the ATT mbuf.
- **GATTC.** Discovery and read/write completion callbacks (in `remote/BLERemoteGatt.nimble`) fill the
  shared caches and release the waiting `BLESync`.
- **SMP.** Pairing/bonding events feed the shared `BLESecurityImplCommon` pairing/notify hooks.
- **Connection info.** Every handler builds `BLEConnInfo` through `BLEConnInfoImpl::fromDesc`
  (`types/BLEConnInfo.nimble`) so all paths report identical link data.

## File map

Strict `.h`/`.cpp` subsystem pairs (same layout as Bluedroid, plus L2CAP). Each pair
is the `BLEFoo.nimble.{h,cpp}` file inside its component folder:

| File | Owns |
| --- | --- |
| `BLE.nimble` *(src/ root)* | `BLEClass::Impl`; host task, init/teardown, identity, MTU, IRK, whitelist |
| `server/BLEServer.nimble` | `BLEServer::Impl`; GATTS registration and server events |
| `gatt/BLEGattAttributes.nimble` | `BLEService`/`BLECharacteristic`/`BLEDescriptor` `Impl`s; `nimbleRegisterGattServices`, access callbacks, notify/indicate |
| `client/BLEClient.nimble` | `BLEClient::Impl`; connection management and GATTC events |
| `remote/BLERemoteGatt.nimble` | remote service/characteristic/descriptor `Impl`s; async read/write/subscribe/discovery + notify routing |
| `scan/BLEScan.nimble` | `BLEScan::Impl` |
| `advertising/BLEAdvertising.nimble` | `BLEAdvertising::Impl`; legacy and (when `BLE5_SUPPORTED`) extended/periodic adv |
| `security/BLESecurity.nimble` | `BLESecurity::Impl` |
| `types/BLEConnInfo.nimble` | `BLEConnInfoImpl::fromDesc` |
| `types/BLEUUID.nimble` | `nimbleUuidFromPublic` / `nimbleUuidToPublic` |
| `l2cap/BLEL2CAP.nimble` | `BLEL2CAPServer`/`BLEL2CAPChannel` `Impl`s (NimBLE-only feature) |

## Two-layer Impl model here

Every `Foo::Impl` in a `.nimble.*` file inherits its `FooImplCommon` base (when one exists) and adds
only NimBLE-native state (e.g. `ble_uuid_any_t nimbleUUID`, per-request `BLESync` objects, connection
handles). `BLEClass::Impl` and `BLEScan`/`BLEAdvertising`/L2CAP `Impl`s have no shared base. Access is
always cast-free `impl.member`.

## Stack-specific quirks

- **Async discovery is lazy and blocking to the caller.** The aggregate getters
  (`getCharacteristics()`, `getDescriptors()`) trigger a one-time discovery if the cache is empty,
  matching the targeted getters.
- **128-bit UUID byte order** is little-endian in `ble_uuid_any_t` and big-endian in `BLEUUID`;
  the `types/BLEUUID.nimble` converter reverses the 16 bytes.
- **CCCD discovery** on `subscribe()`/`unsubscribe()` resolves `0x2902` via `getDescriptor(0x2902)` within the characteristic's bounded handle range rather than assuming `valHandle + 1`.
- **Service Changed invalidation** caches and delivers the peer's real four-byte 0x2A05 range, but conservatively detaches the full discovered remote tree before the callback because remote handles do not retain enough range metadata for partial invalidation. The sketch must rediscover before reusing remote handles.
- **Service removal while connected.** `ble_gatts_reset()` returns `BLE_HS_EBUSY` while GAP is active or a peer is connected; `bleServerRemoveService` falls back to `ble_gatts_svc_set_visibility(startHandle, 0)` + `ble_svc_gatt_changed(startHandle, endHandle)` so runtime removal and Service Changed (`0x2A05`) match Bluedroid.
- **Re-advertise event lifetime.** `nimbleResetReAdvertiseEvent()` drops the deferred
  re-advertise event's backing reference on `end()` so a later `begin()` re-initializes it (avoids a
  use-after-free across a stack restart — the FreeRTOS NimBLE port frees the event pool on
  `nimble_port_deinit()`).
- **MTU exchange** runs after `connect()` returns, but `onConnect` has already fired, so
  `onMtuChanged` still lands after `onConnect` (matches Bluedroid ordering). Blocking `connect()`
  waits on `mtuSync` so `getMTU()` is valid on return.
- **BLE5 routing.** When `BLE5_SUPPORTED`, legacy `start()`/`scan` use the extended controller API
  with legacy PDUs on a reserved top instance (`kLegacyInstance`) so they coexist with user
  `startExtended` sets. Extended scan reports merge scan responses via `mergeScanResponse`.
- **L2CAP `write()`** splits payloads larger than `peer_coc_mtu` into MTU-sized SDUs and
  application-task calls may block between chunks on `COC_TX_UNSTALLED` (NimBLE itself does not
  segment oversized SDUs). Calls from host callbacks return `Busy` instead of blocking when more
  credits are needed. RX flatten uses a fixed 256-byte stack threshold; larger SDUs spill to the
  heap. Channel MTU is independent of that threshold (caller-configured: 128, 256, 512, …).
- **L2CAP channel operations** from application tasks use a tracked host-queue event while an
  operation is queued or in flight. The host task therefore owns all native channel-pointer
  dereferences, ordered with `COC_DISCONNECTED`; one operation mutex covers each complete public
  write/get-MTU/disconnect transaction. Completion returns the event backing to ESP-IDF's bounded
  NPL pool before waking the caller, so retained detached shells consume no slots. Shutdown closes
  the enqueue gate before stopping the host, then removes and deinitializes any still-active events
  and releases waiters before NimBLE destroys the pool. Detached shell write/disconnect calls return
  `NotConnected` consistently before and after an `end()`/`begin()` cycle.
- **Late L2CAP connect completion** is deferred onto a host-queue callout after a blocking
  `connectL2CAP()` timeout. NimBLE invokes `COC_CONNECTED` before releasing its connect signaling
  procedure, so the orphan disconnect cannot be submitted inline; its context remains owned until
  `COC_DISCONNECTED` or host teardown.
- **`onIdentity`** fires at most once per connection (`identityDispatched` latch, reset on CONNECT).
- **`onConnParamsUpdateRequest`** is NimBLE-only (returns accept/reject; no Bluedroid equivalent).
