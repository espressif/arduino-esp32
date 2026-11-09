# Bluedroid backend

Bluedroid implementation of the public BLE API. Built when `BLE_BLUEDROID` is set (see
`core/BLEGuards.h`), i.e. when the SDK selects the Bluedroid host
(`CONFIG_BT_BLUEDROID_ENABLED`, e.g. `esp32`). Cross-backend obligations (two-layer Impl model,
subsystem pairs, ordering contract, enforcement) live in
[`DESIGN.md`](../DESIGN.md#backend-contract); this file only covers what is specific to
Bluedroid.

## High-level architecture

- **No dedicated host task.** Bluedroid runs its own BTC task internally. Application-registered
  GAP/GATTS/GATTC callbacks are invoked on that single BTC task, so the only concurrency to guard
  is BTC-task vs. application-task access to shared state. `BLE.end()` from BTC claims `ending`
  before returning and defers teardown to a `ble-end` worker so `begin()` cannot revive the
  session while the host is still up.
- **App-id registration.** The GATTS server and GATTC client each register an application id and
  receive a `gatts_if` / `gattc_if`. Registration is asynchronous: `begin()`/`start()` block on a
  `BLESync` until the `REG_EVT` (and, for the server, service-creation events) arrive.
- **Synchronous GATT client.** Unlike NimBLE, Bluedroid exposes synchronous attribute queries
  (`esp_ble_gattc_get_attr_count` / `get_all_char` / `get_all_descr`), so remote discovery reads the
  cache directly without per-request callbacks.
- **Deferred advertising restart.** Restarting advertising from inside a GATTS/GAP callback would
  re-enter the BTC task, so the server schedules it on an `esp_timer` (`advRestartTimer`).
  `stop()`/`start()` cancel that timer and bump `reAdvEpoch` even when advertising is already down,
  matching NimBLE so a late disconnect restart cannot collide with a sketch that has taken over
  advertising. The timer callback holds `reAdvMtx` across `startAdvertising()`; public advertising
  calls take that mutex before admission so `stop()` waits the in-flight restart out and then stops
  native GAP. `ADV_STOP_COMPLETE` / `EXT_ADV_STOP_COMPLETE` / `PERIODIC_ADV_STOP_COMPLETE` clear
  advertising flags only when `giveIfAdvWait` consumes the matching wait (`LegacyStop` / `ExtStop` /
  `PeriodicStop`), so a leftover stop after start cannot leave the radio up while `isAdvertising()`
  is false. Duration-expiry still uses `durationExpiryStop` and dispatches `onComplete` on a worker
  (not BTC) so a handler may call `start()`. A failed native stop leaves `isAdvertising` set.

## Event routing

Native events reach the shared handlers as follows:

- **GAP.** `BLEClass::Impl::gapCallback` dispatches to advertising, scan, GATT, security, and any
  custom user handler, and signals local-privacy completion. When LE Audio is
  instantiated it also forwards every host GAP event to the engine; ISO events
  reach the raw transport through that engine bridge.
- **GATTS.** `BLEClass::Impl::gattsCallback` forwards to `BLEServer::Impl::handleGATTS`
  (registration, connect/disconnect, read/write, prepared writes, MTU).
- **GATTC.** `BLEClass::Impl::gattcCallback` forwards to `BLEClient::Impl` (open/close, search,
  notify, MTU).
- **SMP.** `bluedroidSecurityHandleGAP` feeds the shared `BLESecurityImplCommon` pairing/notify
  hooks on `BLESecurity::Impl::instance()`. OOB is not supported: `ESP_GAP_BLE_OOB_REQ_EVT` /
  `ESP_GAP_BLE_SC_OOB_REQ_EVT` disconnect the ACL instead of injecting a zero TK (the public
  Temporary Key). Directed advertising copies `setDirectedAddress` into `peer_addr` /
  `peer_addr_type`. BLE5 directed `start()` zeros leftover AD/SCAN_RSP on the reserved instance
  before `ext_adv_set_params` so high-duty directed is not HCI Invalid Parameters (0x12).
- **Connection info.** Handlers build `BLEConnInfo` through `BLEConnInfoImpl::make` /
  `setMTU` / `setConnParams` / `updateSecurityFlags` / `updateSecurityFromAuthComplete`
  (`types/BLEConnInfo.bluedroid`).

## File map

Strict `.h`/`.cpp` subsystem pairs (same layout as NimBLE, without L2CAP). Each pair is
the `BLEFoo.bluedroid.{h,cpp}` file inside its component folder:

| File | Owns |
| --- | --- |
| `BLE.bluedroid` *(src/ root)* | `BLEClass::Impl`; init/teardown, identity, MTU, IRK, whitelist, GAP/GATT callback dispatch |
| `server/BLEServer.bluedroid` | `BLEServer::Impl`; GATTS registration, prepared-write buffers, server events |
| `gatt/BLEGattAttributes.bluedroid` | `BLEService`/`BLECharacteristic`/`BLEDescriptor` `Impl`s; notify/indicate, descriptor permissions |
| `client/BLEClient.bluedroid` | `BLEClient::Impl`; connection management and GATTC events |
| `remote/BLERemoteGatt.bluedroid` | remote service/characteristic/descriptor `Impl`s; synchronous read/write/subscribe/discovery |
| `scan/BLEScan.bluedroid` | `BLEScan::Impl` |
| `advertising/BLEAdvertising.bluedroid` | `BLEAdvertising::Impl` |
| `security/BLESecurity.bluedroid` | `BLESecurity::Impl` |
| `types/BLEConnInfo.bluedroid` | `BLEConnInfoImpl` builders/updaters |
| `types/BLEUUID.bluedroid` | `bluedroidUuidFromPublic` / `bluedroidUuidToPublic` |

Bluedroid has no `L2CAP` pair yet; `l2cap/BLEL2CAPImpl.h` stays available for a future one.

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
- **LE Audio central links.** Audio peers must be opened with
  `BLEAudio::connect()`, which uses the engine's own GATTC interface. Bluedroid
  supports classic `BLEServer` coexistence through incremental multi-app
  registration and therefore does not use NimBLE's `BLEGattDatabase`
  coordinator. The core still runs the audio/raw-ISO pre-end hooks before
  disabling Bluedroid, including on callback-deferred `BLE.end()`. Audio GATT
  service deletion waits for BTA's delete completion before the next session
  registers services, so handles cannot overlap stale BTA entries. The GATTS
  read/write proxy also falls back to the adapter's created-service table for
  services Bluedroid advertises but the engine did not register in `gatt_db`
  (a PACS with no PAC records), matching NimBLE's always-present PACS.
- **One Bluedroid host shared with Classic.** Bluedroid is a process-wide
  host. `btBluedroidHostAcquire()`/`btBluedroidHostRelease()` in the core
  reference-count it: the first user starts the controller and host, the last
  one stops them, and a host already enabled by an external component is
  adopted and left up. `BLE` and `BluetoothSerial` register their callbacks on
  that host and never initialize or deinitialize it themselves. `BLE.end()`
  while Classic still holds the host leaves it up and records the `end(true)`
  contract without a physical free; the last user performs the teardown.
  `BluetoothSerial::end()` waits for `ESP_SPP_UNINIT_EVT` while it still holds
  the host reference, because `esp_spp_deinit()` is asynchronous and an
  immediate re-`begin()` would otherwise race the old SPP instance. The S31
  memory-release policy and the coordinated future reclaim checklist are in
  `DESIGN.md`, "To reclaim the unused half in a BTDM build".
- **Raw ISO ACL tracking.** The ISO adapter records a physical LE ACL at GATTC
  CONNECT, not only at its own app's GATTC OPEN; `BLEIso::connectCis()` can
  therefore use an existing `BLEClient` handle. The later OPEN is idempotent,
  and disconnect removes the same captured ACL entry. If a CIS disconnection
  tears down its parent ACL before Bluedroid surfaces the local CIS-complete
  event, the ISO host completes any remaining channels on ACL loss so
  `BLEIso::end()` does not retain a dead CIS.
- **PAST receive API.** IDF keys the receive-parameter API by peer address,
  while the public surface is keyed by a connection handle. The Bluedroid
  implementation resolves the handle against the live `BLEClient` and
  `BLEServer` connection tables (GATT connection id, with the HCI ACL handle as
  a fallback), then arms mode `DUP_FILTER_DISABLED`. `stopPeriodicSyncReceive()`
  restores `NO_SYNC_EVT` with default parameters because Bluedroid has no
  dedicated disable command. The audio Broadcast Sink's BASS path uses the same
  arming helper directly with the assistant address carried by the synchronous
  request event, and requires
  `CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER`; the GAP callback records the
  transferred sync for normal callbacks and `BLE.end()` teardown.
- **Raw AD data only.** The native advertising path uses `esp_ble_gap_config_adv_data_raw` with
  bytes from `BLEAdvDataBuilder`, never `esp_ble_gap_config_adv_data` (which force-expands service
  UUIDs to 128 bits and would diverge from every other path).
- **Legacy advertising duration.** The BLE 4.2 advertising API has no native duration argument, so
  a one-shot `esp_timer` stops the session at the requested millisecond duration. Only that
  timer-driven stop dispatches `onComplete`; an explicit `stop()` or restart is silent.
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
- **Service Changed invalidates the full remote tree.** Bluedroid consumes the 0x2A05 indication
  internally and exposes only the peer address, not its handle range. The backend therefore
  detaches every discovered remote handle and reports the conservative full range
  `0x0001–0xFFFF` to a registered Service Changed callback; callers must rediscover.
- **Security flags** (encrypted/authenticated/bonded) are latched from `AUTH_CMPL` onto every
  connected client sharing the peer's ACL (Bluedroid multiplexes GATTC app-ids over one physical
  link) and cleared on (re)connect/disconnect.
- **BLE5 ext-adv/scan + periodic.** Extended advertising TX, extended scan, connection PHY/DLE,
  periodic advertising TX, and periodic sync RX are implemented under `#if BLE5_SUPPORTED` /
  `BLE_PERIODIC_ADV_SUPPORTED` (the latter also requires `CONFIG_BT_BLE_50_PERIODIC_ADV_EN`).
  Periodic-sync callback addresses are converted from Bluedroid's MSB-first `esp_bd_addr_t` to the
  public LSB-first `BTAddress` representation.
  Classic esp32 Bluedroid libs stay on BLE 4.2 (`BLE5_SUPPORTED == 0`) and use the shared
  `NotSupported` fallbacks. Admitted `reset()` and `beginBackendShutdown()` wait for each
  extended/periodic stop complete, then for `EXT_ADV_SET_CLEAR_COMPLETE`. Public
  `clearExtended()` does the same wait and only clears software flags on success.
  The ESP-IDF call only queues BTC; the controller rejects `HCI_LE_Clear_Advertising_Sets`
  with Command Disallowed while a set is still enabled (Vol 4, Part E, §7.8.60).
  Those GAP completes signal `advSync` only when that command is armed, so a
  fire-and-forget clear or an extra periodic-stop cannot release a later
  `start()`/`startExtended()` wait.
- **Legacy scan-response clear.** `start()` with no custom scan response (or `setScanResponse(false)`)
  calls `esp_ble_gap_config_scan_rsp_data_raw(NULL, 0)`. IDF treats length 0 as a valid empty
  payload; only a null pointer with nonzero length is `ESP_ERR_INVALID_ARG`.
- **No `onConnParamsUpdateRequest`** — Bluedroid has no pre-accept hook; the API is NimBLE-only.
- **`ESP_GATTS_CONF_EVT` has no notify/indicate bit.** A notify CONF is local TX-complete and
  arrives before the peer indication confirm, so submitted notifications are consumed first;
  an in-flight indication is the fallback. Reservations that have not been handed to the stack
  must not consume a CONF.
