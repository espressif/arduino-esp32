# BLE Validation Test

Validates BLE secure connection between a server and client using Numeric Comparison pairing, characteristic read/write operations, and IRK (Identity Resolving Key) retrieval. This is a **multi-DUT** test supporting both Bluedroid and NimBLE stacks.

## Architecture

```
 ┌──────────────┐          BLE              ┌──────────────┐
 │    Server    │◄──── advertising ─────────│    Client    │
 │  (device0)   │     scan/connect          │  (device1)   │
 └──────┬───────┘                           └──────┬───────┘
        │ serial                                   │ serial
        ▼                                          ▼
 ┌─────────────────────────────────────────────────────────┐
 │                    pytest (test_ble.py)                 │
 └─────────────────────────────────────────────────────────┘
```

## Test Cases

The suite runs as a sequence of numbered phases driven by `test_ble.py`. Each
phase is gated by a `START_PHASE_<n>` handshake and recorded individually. BLE5-
and L2CAP-dependent phases self-skip on targets/builds without support.

| Phase | Label | What it covers |
|---|---|---|
| 1 | basic_lifecycle | init / deinit / reinit / final deinit + **heap canary integrity around the first `begin()`** + **callback-context `BLE.end()` rejects same-callback `begin()` and recovers after a failed `ble-end` worker** + **idle `end(true)` after `end(false)` must not latch into a later `end(false)`** + release of an unlimited `startBlocking(0)` scan by concurrent host shutdown |
| 2 | ble5_ext_periodic_adv | extended + periodic advertising and sync (BLE5) + **clearExtended() while live must Fail and leave advertising on** + **clearExtended() then startExtended() restart** + **reset() while extended+periodic then restart** + **leftover EXT_ADV_STOP_COMPLETE / PERIODIC_ADV_STOP_COMPLETE after start must not clear flags** (Bluedroid BLE5; skip on NimBLE / classic) |
| 3 | gatt_setup_connect | server start, scan, connect, service discovery, heap tracking + broadcast notify/indicate before anyone connects reports `ERROR_NO_CLIENT` + GATT tree empty after `begin()` |
| 4 | gatt_read_write | characteristic read / write / read-back + value supplied per read by `onRead` via `setValue()` (short and 512-byte long reads, one `onRead` per read) + long reads of a value rewritten every few ms stay uniform (per-connection snapshot) + concurrent same-client blocking reads/writes keep distinct completion ownership + a long read abandoned by disconnect then a uniform re-read + write through a handle from a const lookup |
| 5 | notifications_indications | CCCD subscribe/malformed-write rejection, notify, indicate, unsubscribe, subscriber count + `onNotify` supplying the payload and `onStatus` delivery results + 4 back-to-back indications queued and delivered in order + 12 further indications filling the 8-deep queue (`BTStatus::Busy`) + notifications interleaved with indications on one characteristic, each status reported as the right kind (`ERROR_NO_SUBSCRIBER` checked at the start of Phase 6) |
| 6 | large_att_write | 512-byte write + read integrity |
| 7 | descriptor_read_write | user description + presentation format descriptors + descriptor value supplied per read by `onRead` (short and 512-byte long reads) |
| 8 | write_no_response | write-without-response path + **queued burst payload integrity** |
| 9 | server_disconnect | server-initiated disconnect + client reconnect |
| 10 | security | Numeric Comparison pairing, matching passkey, secure read |
| 11 | ble5_phy_dle | PHY update + data-length extension (BLE5) |
| 12 | reconnect | repeated connect/disconnect + Bluedroid app-id boundary stress + NimBLE `nimbleRef` keepalive vs `disconnectSync.give()` (Bluedroid reports `supported=0`) |
| 13 | blestream | NUS ownership rejection, failed-client-begin paired connect/disconnect callbacks, writeTo/onData/peers, ping/pong, peek, bulk, full `end()` cleanup + re-begin, disconnect-race terminal handling, callback-context `end()` vs in-flight `writeChunks` handle reset, callback-context `end()` deferral/restart, final-public-owner destruction from a stack callback, deterministic worker/teardown failure recovery with admission held closed, and partial-start cleanup ownership/recovery |
| 14 | l2cap_coc | L2CAP CoC connect + echo |
| 15 | introspection_and_permissions | client introspection, fail-closed masking, descriptor perms |
| 16 | conninfo_and_params | `BLEConnInfo` matrix + MTU / conn-params callbacks + ordering |
| 17 | address_types | every `BTAddress::Type` + Public/Random connect roundtrip |
| 18 | authorization | app-level authorization approve/deny of writes |
| 19 | encrypted_perm_enforcement | encrypted characteristic read/write over paired link |
| 20 | adv_data_and_scan | full `BLEAdvertisementData` payload + fresh-primary/cached-scan-response merge + **31-octet cap enforcement** + Slave Connection Interval Range AD (0x12) round trip + **malformed AD parser rejection** (issue #12801, truncated UUID lists, zero-length AD terminator) + **reset() while advertising then restart** + **start() with setScanResponse(false)** + **duration overflow / high-duty directed forever / directed without peer is InvalidParam** + **failed native stop leaves isAdvertising set** + **onComplete may call start()** + **NimBLE rebuild-fail then start() retry** + **leftover ADV_STOP_COMPLETE after start must not clear isAdvertising** (classic Bluedroid; skip on BLE5) |
| 21 | beacon_and_eddystone | iBeacon / Eddystone URL / plaintext Eddystone TLM frames + encrypted eTLM rejection; FEAA service-data starting with `0x01` is not classified as TLM (eTLM is frame `0x20` / version `0x01`) |
| 22 | bond_and_whitelist | bond enumeration, whitelist, IRK cross-check |
| 23 | error_paths_and_misc | unknown UUIDs, typed reads, UUID algebra, cached-value/parent getter teardown race, concurrent lazy aggregate/targeted characteristic and descriptor discovery against disconnect invalidation, immediate recursive remote-tree detachment with retained parents, stale-handle rejection after reconnecting the same client generation, write-after-disconnect + `notify(conn)` / `indicate(conn)` refused for an unsubscribed client (`*_DISABLED`) or unconnected handle (`ERROR_NO_CLIENT`) + NimBLE advertising `stop()` during preemption-resume, remaining-duration resume, and `advertiseOnDisconnect` surviving a preemption resume that does not restart (Bluedroid reports `supported=0`) |
| 24 | ble5_advanced | default PHY, coded-PHY scan, per-connection PHY + DLE (BLE5) |
| 25 | hid_smoke | HID-over-GATT compliance + detached attribute shells/factory rejection after removal and an empty GATT tree after `end()`/`begin()` |
| 26 | ble5_legacy_over_ext | legacy advertising routed over the extended engine (BLE5) |
| 27 | ble5_legacy_plus_ext | concurrent legacy + user extended set on the reserved instance |
| 28 | l2cap_bulk_reconnect | L2CAP CoC late-connect cleanup + bulk/credit-stall reconnect + peer-disconnect operation race + 64 retained-shell event-pool churn/final-handle drops + `end()` during a stalled write and exact `NotConnected` shell status after rebegin |
| 29 | conn_params_reject | central rejects the peripheral's conn-param update from its pre-accept hook (NimBLE) |
| 30 | deletebond_roundtrip | `deleteBond` during a finite scan (completion stays near the original deadline, neither early nor fully restarted), superseded-session completion isolation → verify empty → re-pair round trip + GATT tree empty after `end()`/`begin()` |
| 31 | nc_reject | central rejects Numeric Comparison; pairing aborts, link stays unencrypted/unbonded |
| 32 | passkey_entry | Passkey-entry pairing (peripheral DisplayOnly + static passkey, central KeyboardOnly) |
| 33 | indication_busy_retry | Service Changed invalidates stale remote handles, client rediscovery restores subscriptions, and indications sent behind the stack indication are retried in order |
| 34 | periodic_adv_lifecycle | periodic-adv terminate + supervision-timeout sync-lost transitions (BLE5) |
| 35 | pending_send_edges | CCCD notify-vs-indicate bit routing on one ACL, indication confirmation not stolen by a reserved-but-unsubmitted notification, `removeService` of a service with in-flight indications returns `Busy`, concurrent long-read/service-removal parent retention, late completion isolation after read/confirmed-write/write-command timeouts, notify flood + disconnect terminal-status ordering, BlueDroid terminal-close final-owner release serialized before host deinit (including worker-start failure), and BLEStream client-role final-owner release serialized against concurrent `BLE.end()` |
| 36 | directed_adv_and_oob | directed high-duty advertising with a non-zero InitA (`setDirectedAddress`) connects within 1.28 s; directed `start()` without a peer is `InvalidParam`; legacy OOB (`BLE_SM_IOACT_OOB`) and OOB-SC pairing are rejected (no encryption, no bond) |
| 37 | memory_release | heap reclaimed on release; reinit blocked while released |

Phase 20 also runs a single-device regression guard for the advertisement-data
payload limits: a too-long complete name must degrade to a Shortened Local Name
(0x08), an oversized AD field must be dropped whole rather than corrupt the
payload (legacy 31-octet cap), and an extended-capacity payload must accept the
full name. It additionally guards the Slave Connection Interval Range AD (0x12):
`setPreferredParams` (the field `setMinPreferred` / `setMaxPreferred` feed) must
encode `[04 12 min_lo min_hi max_lo max_hi]` in little-endian 1.25 ms units, and
the client parses the same field back out of the received payload to confirm the
min/max survived the over-the-air round trip.

Two over-the-air windows also guard the advertisement parser against malformed AD
structures appended with `addRaw`. The client concatenates the advertisement and
the scan response into one buffer before parsing, so any structure that halts
parsing hides every structure after it. That allows exactly one halting probe per
window, kept last, and it is why the two probes below are split across windows.

The `ADV_<name>` scan response ends with a manufacturer AD whose length byte
claims 20 octets when only 2 follow (issue #12801). Rejecting it must leave the
fields before it untouched, so the client asserts the genuine manufacturer field
still reads back verbatim as `mfgHex=e502455350` (company `0x02E5` little-endian
followed by `"ESP"`; `getManufacturerDataString()` hex-encodes the whole field,
company id included). A parser missing the bounds check instead reads a 19-octet
manufacturer field and overwrites that data with out-of-bounds bytes.

The `OSZ_<name>` scan response carries the non-halting probes followed by the
terminator probe:

- 16-bit, 32-bit and 128-bit service-UUID structures that are each one octet
  group short of a whole UUID, reported as `malformedUuidsRejected=1`, which holds
  only when the device ends up with no service UUID at all;
- a zero-length AD, which terminates the payload (Core Spec Vol 3, Part C, §11),
  hiding a deliberately well-formed manufacturer AD placed behind it. Nothing but
  correct terminator handling can keep that field out of the parsed data, so the
  client must report `terminatorHidMfg=1`. A parser missing the check reads the
  following length byte as the AD type and underflows its own length to 255, then
  walks a 32-bit UUID list far past the buffer — which the `svcCount` in
  `malformedUuidsRejected` catches as a burst of garbage UUIDs.

Phase 1 checks heap integrity (`heap_caps_check_integrity_all`) immediately before
and after the first `BLE.begin()` on both devices. A stack init that overruns a
heap block corrupts a canary silently and only panics much later in unrelated
code, so the check has to sit right around the call. It then calls `BLE.end()`
from scan `onComplete` (BTC / NimBLE host) and asserts that a same-callback
`begin()` returns `InvalidState`, including when worker creation is injected to
fail; a later user-thread `end()`/`begin()` recovers the stack. An idle
`end(true)` after `end(false)` must not latch into the next session: a
following `end(false)`/`begin()` still succeeds.

Phase 8 follows the single write-without-response with a burst of three packets
sent back to back, with no delay and no ATT response in between so they queue up
in the stack. The server must observe `AAAAAA`, `BBBBBB` and `CCCCCC` in order; a
value buffer shared across queued packets shows up as three copies of the last
payload.

## Requirements

- **Hardware**: Two boards with BLE support (e.g. ESP32, ESP32-S3)
- **Wokwi/QEMU**: Not supported (multi-device test)
- **CI Runner**: `two_duts`
- **SoC Config**: `CONFIG_SOC_BLE_SUPPORTED=y`

## Serial Protocol

1. Both devices print `Device ready for server name`
2. pytest generates a unique server name and sends it to both devices
3. Server begins advertising; client scans for the server name
4. Client connects, discovers service/characteristics
5. Client reads insecure characteristic (no auth)
6. Client reads secure characteristic (triggers Numeric Comparison)
7. Both devices display and confirm the same PIN
8. Authentication completes; both devices retrieve peer IRK
9. Client performs write/read on both characteristics

## Notes

- Race injectors live in `libraries/BLE/src/core/BLEValidationHooks.{h,cpp}`
  and are compiled in by `-DBLE_VALIDATION_HOOKS` in each sketch's `build_opt.h`.
  Library code and both sketches share the `BLEHooks` singleton; do not redeclare
  hook atomics in the `.ino` files.
- NVS is erased at startup to ensure fresh pairing on every run.
- Authentication is triggered on-demand (when reading the secure characteristic) rather than on connect, ensuring consistent ordering across Bluedroid and NimBLE.
- The test verifies PIN match between server and client via assertion.
- BLE5 phases (2, 11, 24, 26, 27, 34) and L2CAP (14, 28) self-skip on targets or builds without support, so the suite runs unchanged on non-BLE5 silicon.
- The conn-param reject phase (29) self-skips on Bluedroid, which negotiates connection-parameter updates inside L2CAP and exposes no application pre-accept hook.

## Coverage gaps (HITL-pending)

The following remain to be added as multi-device cases and require extra
hardware or stack support that this 2-DUT suite cannot exercise (see also
known gaps in `libraries/BLE/DESIGN.md`):

- BLEStream / BluetoothSerial **multi-client server** (two simultaneous centrals,
  per-peer routing under load) — needs a third DUT or two external phones.
  Single-peer `peerCount()`, `peers()`, `writeTo()`, and `onPeerData` are
  regression-guarded in phases 2 (bt_classic) and 13 (blestream). See `libraries/BLE/examples/MultiClientUART` and
  `libraries/BluetoothSerial/examples/MultiClientSerial` for manual multi-central
  testing.
- **EATT** (Enhanced ATT, multiple ATT bearers on one ACL) has no public API.
  Two `BLEClient` instances on one central also share a single ACL with this
  peripheral (one local address), so overlapping cross-client snapshots and
  per-peer CCCD need a third DUT. Phase 4 covers abandon-and-reconnect on one
  link; phase 35 covers CCCD notify-vs-indicate bit routing on one link.
- **Withheld Handle Value Confirmation** / the 30 s ATT indication timeout: both
  hosts auto-confirm indications, so a peer cannot be made to withhold CONF.
  Phase 35 covers a short `readValue` timeout then destroy/re-read instead.
