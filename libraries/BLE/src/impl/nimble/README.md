# NimBLE backend

NimBLE implementation of the public BLE API. Built when `BLE_NIMBLE` is set (see
`impl/common/BLEGuards.h`), i.e. when the SDK selects the Apache NimBLE host
(`CONFIG_BT_NIMBLE_ENABLED`, e.g. `esp32s3`, `esp32c3`). Cross-backend obligations
(two-layer Impl model, subsystem pairs, ordering contract, enforcement) live in
[`DESIGN.md`](../../../DESIGN.md#backend-contract); this file only covers what is
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

- **GAP.** A single `ble_gap_event_listener` (registered in `Core`) plus per-object GAP callbacks
  fan out to `BLEServer::Impl`, `BLEClient::Impl`, `BLEAdvertising::Impl`, and `BLEScan::Impl`, and
  to the security hooks on `BLESecurity::Impl::instance()`.
- **GATTS.** The characteristic/descriptor access callbacks
  (`BLECharacteristic::Impl::accessCallback` / `descAccessCallback`) invoke the user read/write
  hooks and copy the value to/from the ATT mbuf.
- **GATTC.** Discovery and read/write completion callbacks (in `RemoteGatt`) fill the shared caches
  and release the waiting `BLESync`.
- **SMP.** Pairing/bonding events feed the shared `BLESecurityImplCommon` pairing/notify hooks.
- **Connection info.** Every handler builds `BLEConnInfo` through `BLEConnInfoImpl::fromDesc`
  (`ConnInfo`) so all paths report identical link data.

## File map

Strict `.h`/`.cpp` subsystem pairs (same layout as Bluedroid, plus L2CAP):

| Pair | Owns |
| --- | --- |
| `NimBLECore` | `BLEClass::Impl`; host task, init/teardown, identity, MTU, IRK, whitelist |
| `NimBLEServer` | `BLEServer::Impl`; GATTS registration and server events |
| `NimBLEGattAttributes` | `BLEService`/`BLECharacteristic`/`BLEDescriptor` `Impl`s; `nimbleRegisterGattServices`, access callbacks, notify/indicate |
| `NimBLEClient` | `BLEClient::Impl`; connection management and GATTC events |
| `NimBLERemoteGatt` | remote service/characteristic/descriptor `Impl`s; async read/write/subscribe/discovery + notify routing |
| `NimBLEScan` | `BLEScan::Impl` |
| `NimBLEAdvertising` | `BLEAdvertising::Impl`; legacy and (when `BLE5_SUPPORTED`) extended/periodic adv |
| `NimBLESecurity` | `BLESecurity::Impl` |
| `NimBLEConnInfo` | `BLEConnInfoImpl::fromDesc` |
| `NimBLEUUID` | `nimbleUuidFromPublic` / `nimbleUuidToPublic` |
| `NimBLEL2CAP` | `BLEL2CAPServer`/`BLEL2CAPChannel` `Impl`s (NimBLE-only feature) |

## Two-layer Impl model here

Every `Foo::Impl` under `impl/nimble/` inherits its `FooImplCommon` base (when one exists) and adds
only NimBLE-native state (e.g. `ble_uuid_any_t nimbleUUID`, per-request `BLESync` objects, connection
handles). `BLEClass::Impl` and `BLEScan`/`BLEAdvertising`/L2CAP `Impl`s have no shared base. Access is
always cast-free `impl.member`.

## Stack-specific quirks

- **Async discovery is lazy and blocking to the caller.** The aggregate getters
  (`getCharacteristics()`, `getDescriptors()`) trigger a one-time discovery if the cache is empty,
  matching the targeted getters.
- **128-bit UUID byte order** is little-endian in `ble_uuid_any_t` and big-endian in `BLEUUID`;
  `NimBLEUUID` reverses the 16 bytes.
- **CCCD is assumed at value-handle + 1** on the subscribe path.
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
  may block between chunks on `COC_TX_UNSTALLED` (NimBLE itself does not segment oversized SDUs).
  RX flatten uses a fixed 256-byte stack threshold; larger SDUs spill to the heap. Channel MTU
  is independent of that threshold (caller-configured: 128, 256, 512, …).
- **`onIdentity`** fires at most once per connection (`identityDispatched` latch, reset on CONNECT).
- **`onConnParamsUpdateRequest`** is NimBLE-only (returns accept/reject; no Bluedroid equivalent).
