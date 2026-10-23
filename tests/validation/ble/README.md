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
| 1 | basic_lifecycle | init / deinit / reinit / final deinit + **heap canary integrity around the first `begin()`** |
| 2 | ble5_ext_periodic_adv | extended + periodic advertising and sync (BLE5) |
| 3 | gatt_setup_connect | server start, scan, connect, service discovery, heap tracking |
| 4 | gatt_read_write | characteristic read / write / read-back |
| 5 | notifications_indications | CCCD subscribe, notify, indicate, unsubscribe, subscriber count |
| 6 | large_att_write | 512-byte write + read integrity |
| 7 | descriptor_read_write | user description + presentation format descriptors |
| 8 | write_no_response | write-without-response path + **queued burst payload integrity** |
| 9 | server_disconnect | server-initiated disconnect + client reconnect |
| 10 | security | Numeric Comparison pairing, matching passkey, secure read |
| 11 | ble5_phy_dle | PHY update + data-length extension (BLE5) |
| 12 | reconnect | repeated connect/disconnect + Bluedroid app-id boundary stress |
| 13 | blestream | NUS stream: writeTo/onData/peers, ping/pong, peek, bulk, disconnect |
| 14 | l2cap_coc | L2CAP CoC connect + echo |
| 15 | introspection_and_permissions | client introspection, fail-closed masking, descriptor perms |
| 16 | conninfo_and_params | `BLEConnInfo` matrix + MTU / conn-params callbacks + ordering |
| 17 | address_types | every `BTAddress::Type` + Public/Random connect roundtrip |
| 18 | authorization | app-level authorization approve/deny of writes |
| 19 | encrypted_perm_enforcement | encrypted characteristic read/write over paired link |
| 20 | adv_data_and_scan | full `BLEAdvertisementData` payload + **31-octet cap enforcement** + Slave Connection Interval Range AD (0x12) round trip + **malformed AD parser rejection** (issue #12801, truncated UUID lists, zero-length AD terminator) |
| 21 | beacon_and_eddystone | iBeacon / Eddystone URL / Eddystone TLM frames |
| 22 | bond_and_whitelist | bond enumeration, whitelist, IRK cross-check |
| 23 | error_paths_and_misc | unknown UUIDs, typed reads, UUID algebra, write-after-disconnect |
| 24 | ble5_advanced | default PHY, coded-PHY scan, per-connection PHY + DLE (BLE5) |
| 25 | hid_smoke | HID-over-GATT service/report/boot-keyboard compliance |
| 26 | ble5_legacy_over_ext | legacy advertising routed over the extended engine (BLE5) |
| 27 | ble5_legacy_plus_ext | concurrent legacy + user extended set on the reserved instance |
| 28 | l2cap_bulk_reconnect | L2CAP CoC bulk transfer across the credit-stall boundary + channel reconnect |
| 29 | conn_params_reject | central rejects the peripheral's conn-param update from its pre-accept hook (NimBLE) |
| 30 | deletebond_roundtrip | `deleteBond` delete → verify empty → re-pair round trip |
| 31 | nc_reject | central rejects Numeric Comparison; pairing aborts, link stays unencrypted/unbonded |
| 32 | passkey_entry | Passkey-entry pairing (peripheral DisplayOnly + static passkey, central KeyboardOnly) |
| 33 | periodic_adv_lifecycle | periodic-adv terminate + supervision-timeout sync-lost transitions (BLE5) |
| 34 | memory_release | heap reclaimed on release; reinit blocked while released |

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
code, so the check has to sit right around the call.

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

- NVS is erased at startup to ensure fresh pairing on every run.
- Authentication is triggered on-demand (when reading the secure characteristic) rather than on connect, ensuring consistent ordering across Bluedroid and NimBLE.
- The test verifies PIN match between server and client via assertion.
- BLE5 phases (2, 11, 24, 26, 27, 33) and L2CAP (14, 28) self-skip on targets or builds without support, so the suite runs unchanged on non-BLE5 silicon.
- The conn-param reject phase (29) self-skips on Bluedroid, which negotiates connection-parameter updates inside L2CAP and exposes no application pre-accept hook.

## Coverage gaps (HITL-pending)

The following remain to be added as multi-device cases and require a 2-device
BLE5-capable rig to author and verify (see also known gaps in
`libraries/BLE/DESIGN.md`):

- BLEStream / BluetoothSerial **multi-client server** (two simultaneous centrals,
  per-peer routing under load) — needs a third DUT or two external phones.
  Single-peer `peerCount()`, `peers()`, `writeTo()`, `onPeerData`, and
  `disconnect(addr)` are regression-guarded in phases 2 (bt_classic) and 13
  (blestream). See `libraries/BLE/examples/MultiClientUART` and
  `libraries/BluetoothSerial/examples/MultiClientSerial` for manual multi-central
  testing.
