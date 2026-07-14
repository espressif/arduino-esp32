# Bluedroid backend

Bluedroid implementation of the public BLE API. Built when `BLE_BLUEDROID` is set (see
`impl/common/BLEGuards.h`), i.e. when the SDK selects the Bluedroid host
(`CONFIG_BT_BLUEDROID_ENABLED`, e.g. `esp32`). Cross-backend obligations (two-layer Impl model,
subsystem pairs, ordering contract, enforcement) live in
[`DESIGN.md`](../../../DESIGN.md#backend-contract); this file only covers what is specific to
Bluedroid.

## High-level architecture

- **No dedicated host task.** Bluedroid runs its own BTC task internally. Application-registered
  GAP/GATTS/GATTC callbacks are invoked on that single BTC task, so the only concurrency to guard
  is BTC-task vs. application-task access to shared state.
- **App-id registration.** The GATTS server and GATTC client each register an application id and
  receive a `gatts_if` / `gattc_if`. Registration is asynchronous: `begin()`/`start()` block on a
  `BLESync` until the `REG_EVT` (and, for the server, service-creation events) arrive.
- **Synchronous GATT client.** Unlike NimBLE, Bluedroid exposes synchronous attribute queries
  (`esp_ble_gattc_get_attr_count` / `get_all_char` / `get_all_descr`), so remote discovery reads the
  cache directly without per-request callbacks.
- **Deferred advertising restart.** Restarting advertising from inside a GATTS/GAP callback would
  re-enter the BTC task, so the server schedules it on an `esp_timer` (`advRestartTimer`).

## Event routing

Native events reach the shared handlers as follows:

- **GAP.** `BLEClass::Impl::gapCallback` dispatches to advertising, scan, GATT, security, and any
  custom user handler, and signals local-privacy completion.
- **GATTS.** `BLEClass::Impl::gattsCallback` forwards to `BLEServer::Impl::handleGATTS`
  (registration, connect/disconnect, read/write, prepared writes, MTU).
- **GATTC.** `BLEClass::Impl::gattcCallback` forwards to `BLEClient::Impl` (open/close, search,
  notify, MTU).
- **SMP.** `bluedroidSecurityHandleGAP` feeds the shared `BLESecurityImplCommon` pairing/notify
  hooks on `BLESecurity::Impl::instance()`.
- **Connection info.** Handlers build `BLEConnInfo` through `BLEConnInfoImpl::make` /
  `setMTU` / `setConnParams` / `updateSecurityFlags` / `updateSecurityFromAuthComplete` (`ConnInfo`).

## File map

Strict `.h`/`.cpp` subsystem pairs (same layout as NimBLE, without L2CAP):

| Pair | Owns |
| --- | --- |
| `BluedroidCore` | `BLEClass::Impl`; init/teardown, identity, MTU, IRK, whitelist, GAP/GATT callback dispatch |
| `BluedroidServer` | `BLEServer::Impl`; GATTS registration, prepared-write buffers, server events |
| `BluedroidGattAttributes` | `BLEService`/`BLECharacteristic`/`BLEDescriptor` `Impl`s; notify/indicate, descriptor permissions |
| `BluedroidClient` | `BLEClient::Impl`; connection management and GATTC events |
| `BluedroidRemoteGatt` | remote service/characteristic/descriptor `Impl`s; synchronous read/write/subscribe/discovery |
| `BluedroidScan` | `BLEScan::Impl` |
| `BluedroidAdvertising` | `BLEAdvertising::Impl` |
| `BluedroidSecurity` | `BLESecurity::Impl` |
| `BluedroidConnInfo` | `BLEConnInfoImpl` builders/updaters |
| `BluedroidUUID` | `bluedroidUuidFromPublic` / `bluedroidUuidToPublic` |

Bluedroid has no `L2CAP` pair yet; `impl/common/BLEL2CAPImpl.h` stays available for a future one.

## Two-layer Impl model here

Bluedroid keeps almost no stack-specific state: the attribute `Impl`s (`BLEService`,
`BLECharacteristic`, `BLEDescriptor`) and the remote `Impl`s are empty derivations of their shared
`*ImplCommon` bases — they exist so the backend contract has a concrete per-type `Impl`.
`BLEServer::Impl` and `BLEClient::Impl` add the Bluedroid `gatts_if`/`gattc_if`, app id, prepared-write
buffers, and `BLESync` objects. Access is always cast-free `impl.member`.

## Stack-specific quirks

- **Connect via `esp_ble_gattc_enh_open`.** `esp_ble_gattc_open` exists only when
  `CONFIG_BT_BLE_42_FEATURES_SUPPORTED` is on. BLE5-only Bluedroid builds (esp32s31 and
  other `SOC_BLE_50` targets with BLE 4.2 disabled) must use `esp_ble_gattc_enh_open` /
  `esp_ble_gattc_aux_open`. The client sets `is_aux` from `BLE5_SUPPORTED`.
- **Raw AD data only.** The native advertising path uses `esp_ble_gap_config_adv_data_raw` with
  bytes from `BLEAdvDataBuilder`, never `esp_ble_gap_config_adv_data` (which force-expands service
  UUIDs to 128 bits and would diverge from every other path).
- **MTU exchange is blocking inside `connect()`.** The ATT MTU exchange must complete before
  `connect()` returns; issuing another GATT/GAP op while it is in flight makes Bluedroid drop the
  pending procedure and never deliver `CFG_MTU_EVT`.
- **No resolved identity address** on connect/security events, so `getIdAddress()` returns the OTA
  address. Peer address type comes from GATTS/GATTC connect and `AUTH_CMPL`.
- **Live PHY on BLE5 builds.** `setPhy`/`getPhy`/`setDataLen` and `setDefaultPhy` use Bluedroid's
  preferred-PHY / read-PHY / pkt-data-len APIs. `BLEConnInfo` TX/RX PHY is updated from
  `READ_PHY_COMPLETE` and `PHY_UPDATE_COMPLETE` events (defaults to `PHY_1M` until the first event).
  On BLE 4.2-only builds these APIs return `NotSupported`.
- **`BLEConnInfo::keySize`** stays at its default — Bluedroid `AUTH_CMPL` carries no key size
  (NimBLE reports `sec_state.key_size`).
- **Prepared (long) writes** are buffered per connection id in `BLEServer::Impl` (a flat vector
  keyed by `conn_id`, avoiding `<unordered_map>` in every build).
- **Security flags** (encrypted/authenticated/bonded) are latched from `AUTH_CMPL` onto every
  connected client sharing the peer's ACL (Bluedroid multiplexes GATTC app-ids over one physical
  link) and cleared on (re)connect/disconnect.
- **BLE5 ext-adv/scan + periodic.** Extended advertising TX, extended scan, connection PHY/DLE,
  periodic advertising TX, and periodic sync RX are implemented under `#if BLE5_SUPPORTED` /
  `BLE_PERIODIC_ADV_SUPPORTED` (the latter also requires `CONFIG_BT_BLE_50_PERIODIC_ADV_EN`).
  Classic esp32 Bluedroid libs stay on BLE 4.2 (`BLE5_SUPPORTED == 0`) and use the shared
  `NotSupported` fallbacks.
- **No `onConnParamsUpdateRequest`** — Bluedroid has no pre-accept hook; the API is NimBLE-only.
