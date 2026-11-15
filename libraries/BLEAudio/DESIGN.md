# BLEAudio Library Design

## Purpose

This document is for maintainers and AI tools working on the **`BLEAudio`**
library (`libraries/BLEAudio/src/`), the LE Audio (Generic Audio Framework)
subsystem of the Arduino BLE stack. Keeping it a separate Arduino library means
a sketch that only includes `<BLE.h>` does not compile the audio engine.

This document complements [`../BLE/DESIGN.md`](../BLE/DESIGN.md) (the core BLE
architecture) — **read that first**. `BLEAudio` follows the exact same public
object model, two-layer PIMPL / shared-handle lifecycle, `BLE_CHECK_IMPL`
null-guard discipline, callback copy-under-lock rule, `on<Event>Cb` naming,
`BTStatus` error model, and backend isolation rules. Where the two overlap, this
document defers to [`../BLE/DESIGN.md`](../BLE/DESIGN.md) and documents only the
audio-specific design, engine boundary, event/stream paths, per-role guards, and
spec-compliance obligations.

## Scope and status

LE Audio is built on Espressif's **ESP-BLE-AUDIO** engine (`esp_ble_audio`, the
Generic Audio Framework: PACS/ASCS/BAP/CAP and the control/top-level profiles).
Because the engine API is **host-agnostic**, every shared `.cpp` and `.c` file
is completely free of `BLE_NIMBLE`, `BLE_BLUEDROID`, and host stack headers.

Host-specific code is strictly isolated into two `.nimble.*` / `.bluedroid.*`
backend subsystem pairs, selected through `BLEAudioBackend.h` (matching
`core/BLEBackend.h` in `BLE`):

1. **`BLEAudio.{nimble,bluedroid}.{h,cpp}`** — concrete `BLEAudio::Impl :
   BLEAudioImplCommon` (verified at compile time by `static_assert` in
   `BLEAudioBackend.h`), NimBLE's `BLEClient` central-connect fallback,
   Bluedroid's `audioGapHook` registration, and NimBLE's weak GATT coordinator
   hooks (`bleAudioGattActive`, `bleAudioGattFrozen`,
   `bleAudioForwardHostGattEvent`). `BLEClass::getAudioController()` and
   `bleAudioPreEndHook()` are shared once in `BLEAudio.cpp`.
2. **`engine/BLEAudioEngine.{nimble,bluedroid}.{h,cpp}`** — host GAP/GATT/SMP
   event forwarding and the C host hooks (`bleAudioHost*`,
   `bleAudioEngineSendPast*`, `bleAudioEngineReceivePeriodicSync`,
   `bleAudioEngineStopPeriodicSyncReceive`, `bleAudioEngineDisconnect`,
   `bleAudioAddrFromGap`) called by the host-agnostic C engine units.

`libraries/BLE/tests/check_backend_isolation.sh` enforces both public-header
isolation and zero `BLE_NIMBLE` / `BLE_BLUEDROID` leaks in shared `BLE` and
`BLEAudio` translation units.

The packaged Arduino libs track ESP-IDF **`release/v6.1`**. Only APIs present
on that branch are used; features that exist only on `master` are not wired up
yet.

### Supported (each behind its own guard)

See [Feature guards](#feature-guards).

- **BAP data plane:** Unicast Server (multiple sink/source ASEs) and Unicast
  Client (multiple streams, one or more peers), Broadcast Source (1–2 BIS) and
  Broadcast Sink (1–2 BIS, optional Scan Delegator / BASS), Broadcast Assistant
  (BASS client, with PAST hand-over of a local PA sync).
- **Coordination:** CAP Acceptor / Initiator (unicast start/update/stop,
  broadcast, unicast↔broadcast handover) / Commander, CSIP Set Member /
  Coordinator.
- **Control profiles:** VCP (Renderer / Controller), MICP (Device /
  Controller), MCP via **GMCS** (Media Player / Controller), CCP via **GTBS**
  (Call Server / Controller).
- **Top-level profiles:** TMAP (TMAS), GMAP (GMAS), HAS (device / controller),
  PBP announcement build / parse (`BLEAudioPublicBroadcast`).
- **LC3 data plane:** `BLEAudioPlayer` (Rx streams → LC3 decode with PLC → I2S
  or a PCM callback) and `BLEAudioRecorder` (I2S or a PCM callback → LC3 encode
  → Tx streams), when the `esp_audio_codec` managed component is present.
- **Re-initialization:** `audio.end()` runs `esp_ble_audio_common_deinit`, and
  the next `audio.begin()` prepares the host (running `ble_gatts_reset()` on
  NimBLE when GAP is idle, or returning `Busy` if GAP/ACL is active), so
  `audio.begin()` may run again in the same boot with a different set of roles.
- **Core-owned terminal teardown:** `BLE.end()` invokes the audio and raw-ISO
  pre-end hooks while the host is still running. It releases an active audio
  controller even when the sketch omitted `audio.end()` and releases a
  standalone `BLEIso` owner before host deinitialization. Native deinit that
  refuses (a stream still up) aborts `BLE.end()`: Arduino keeps ownership, clears
  any `endReleaseMemory` latch, and leaves the host up.

### Not exposed / known limits

| Surface | Status |
| --- | --- |
| **Discrete TBS / MCS** | Not exposed. `release/v6.1` builds them on both hosts when `CONFIG_BT_TBS_BEARER_COUNT` / `CONFIG_BT_MCS_INSTANCE_COUNT` is non-zero (both default to 0), but the Arduino API drives only the generic GTBS / GMCS instances. |
| **OTS (Object Transfer)** | Not exposed (`CONFIG_BT_OTS` builds on both hosts in `release/v6.1`). MCP has no object transfer. |
| **CSIP ordered access** | The coordinator discovers and locks set members; the CAP initiator does not run the CSIP ordered-access procedure across members. |
| **BASS receive-state cleanup** | The Scan Delegator does not remove stale receive states on its own; the assistant must remove them. |
| **One PA sync at a time** | The broadcast sink follows one source; an assistant request is refused while a PA sync is in use. |
| **GATT frozen after `start()`** | Roles that register services must be created before `audio.start()`; swapping roles needs `audio.end()` → `audio.begin()`. |
| **Standalone `BLEIso` CIS on Bluedroid** | The ISO transport adopts the physical ACL at GATTC CONNECT, independent of which GATTC app opened it; raw CIS may use a `BLEClient` ACL. LE Audio central links still use `audio.connect()` so the audio engine owns GATT discovery and security. |
| **Preview status** | The LE Audio APIs are still marked preview in IDF. |

## Layering and alignment with `BLE`

```
sketch
  │  <BLE.h> core BLE        <BLEAudio.h> LE Audio (separate library)
  ▼
BLEAudio              controller handle (BLEAudio.{h,cpp}, BLEAudioImpl.h,
  │                   BLEAudioBackend.h -> BLEAudio.{nimble,bluedroid}.{h,cpp})
  │  create*() factories mint …
  ▼
role handles          BLEAudioUnicastServer, …, BLEAudioHearingAidController
  │  value-type PIMPL handles (all 5 special members defaulted); BLE_CHECK_IMPL
  │  BLEAudioStream handles carry the audio of every role
  ▼
C engine units        BLEAudioEngine*.{h,c}  (extern "C"; host-agnostic; ONLY
  │                   code that includes esp_ble_audio_* headers)
  ├──► Host glue      BLEAudioEngine.{nimble,bluedroid}.{h,cpp}
  ▼
ESP-BLE-AUDIO engine  (ESP-IDF component; host-agnostic)
```

Three invariants keep the boundary clean:

1. **Public headers are backend- and engine-agnostic.** No `esp_ble_audio_*`,
   `bt_*`, `<zephyr/...>`, NimBLE, or Bluedroid type appears in any header
   reachable from `<BLEAudio.h>`.
2. **Shared C/C++ units are host-agnostic.** No `BLE_NIMBLE`, `BLE_BLUEDROID`,
   or raw host `CONFIG_BT_NIMBLE_*` / `CONFIG_BT_BLUEDROID_*` macro appears
   outside `BLEAudioBackend.h`, `BLEAudio.{nimble,bluedroid}.*`, and
   `engine/BLEAudioEngine.{nimble,bluedroid}.*`. Host-pinning and link-table
   bounds come from `core/BLEGuards.h` (`BLE_MAX_CONN`, `BLE_HOST_PINNED_CORE`).
3. **The engine boundary is C.** The `esp_ble_audio_*` headers pull in Zephyr
   headers that do not parse as C++, so every engine call lives in a C unit
   behind a narrow `extern "C"` header with plain-C payload structs.

### Handle semantics and intentional exceptions

- **Controller and role handles** (`BLEAudio`, `BLEAudioStream`, and every role
  minted by `audio.create*()`) are value-type PIMPL handles with all five
  special members defaulted (`~T`, copy ctor/assign, move ctor/assign), private
  `shared_ptr<Impl>` constructors, `BLE_CHECK_IMPL` entry guards, and
  `on<Event>Cb` callback storage guarded by `BLESharedMutex mtx`. Because roles
  are completely host-agnostic (their backend is the C engine), a role's
  `struct Impl` lives directly in its `BLEAudio<Role>.cpp` file rather than
  splitting into empty `.nimble.h` / `.bluedroid.h` shells.
- **`BLEAudioPlayer` and `BLEAudioRecorder`** are user-constructed local codec
  endpoints (analogous to `BLEStream` in `BLE`): sketches instantiate them
  directly, configure `setPCMSink()` / `setPCMSource()` or I2S pins, call
  `begin()`, and `operator bool()` reports whether the session has been begun.
  Shared setup/teardown lives on `BLEAudioDataPath` (`BLEAudioPipeline.h`).

### Engine patches the Arduino layer depends on

The library builds against stock `release/v6.1` plus a small, reviewed keep-list
of engine fixes in the `esp-idf` tree pointed to by the lib-builder:

- **`esp_ble_iso/.../adapter/bluedroid/gatt/gatt.c`, `gattc_disc_chrc_desc`**:
  a characteristic with no descriptor (e.g. read-only VCS *Volume Flags*) must
  end discovery with a NULL attr and `rc == 0`, matching the NimBLE adapter
  (`gatt.db.c`, `handle_gattc_disc_all_dscs`) and Zephyr's `bt_gatt_discover()`
  contract. Returning `-ENODEV` caused `libble_audio` to abort VCP/CSIS/CAP
  setup (`VcpVolCtlrSubFail[..][-19]`).
- **`esp_ble_iso/.../adapter/bluedroid/gatt/gatt.c`, GATTC ACL registration**:
  publish the physical ACL to the ISO host at GATTC CONNECT, not only at the
  engine app's GATTC OPEN, so `BLEIso::connectCis()` can use an ACL opened by
  Arduino `BLEClient`. An `acl_connect_posted` bit suppresses duplicate OPEN
  notifications for engine-owned links.
- **`esp_ble_iso/host/common/app/gap.c` + `host/iso/iso.c`, ACL-loss cleanup**:
  if ACL disconnect arrives while a CIS is still attached, complete the CIS
  channel locally before deleting the ACL slot so `BLEIso::end()` cannot time
  out waiting for a missing local CIS event.
- **`esp_ble_audio/host/adapter/bluedroid/server.c` + `esp_ble_iso/.../gatt.c`,
  GATT service delete completion**: `BTA_GATTS_DeleteService()` is asynchronous;
  deinit waits for `BTA_GATTS_DELELTE_EVT` before releasing the service handle
  so a subsequent session cannot overlap stale BTA handles.
- **`esp_ble_audio/host/adapter/bluedroid/server.c` + `esp_ble_iso/.../gatt.c`,
  unregistered-service proxy fallback**: Bluedroid creates PACS at
  `audio.begin()` even when no PAC records are registered, so the GATTS
  read/write proxy falls back to the adapter's created-service table, matching
  NimBLE's always-published PACS.
- **`BLEAudioEngineControl.c`, TBS URI-scheme list trailing-comma workaround**:
  `uri_scheme_in_list()` in the prebuilt `libble_audio.a` (`tbs.c.o`) only
  compares tokens terminated by `','` and misses the final `'\0'`-terminated
  token, rejecting client `Originate` writes with `BT_TBS_RESULT_CODE_INVALID_URI`
  when `uri_schemes_supported` is `"tel"`. `bleAudioCcpSrvInit()` appends a
  trailing `','` when space permits so every configured scheme matches.

### File map

All paths are relative to `libraries/BLEAudio/src/`.

| Files | Role |
| --- | --- |
| `BLEAudio.{h,cpp}`, `BLEAudioImpl.h`, `BLEAudioBackend.h` | Controller public handle, `BLEClass::getAudioController()`, `bleAudioPreEndHook()`, `BLEAudioImplCommon` (`createStagedRole`, event dispatcher), and single backend selector. |
| `BLEAudio.{nimble,bluedroid}.{h,cpp}` | Single backend pair for `BLEAudio`: `BLEAudio::Impl` methods, weak GATT coordinator hooks (`BLEAudioHooks.h`), Bluedroid `audioGapHook` & deferred SMP timer, central GATTC open, NimBLE GATT reset/staging hooks, PAST send/receive, and ACL disconnect. |
| `BLEAudio<Role>.{h,cpp}` | Public role handles (UnicastServer/Client, BroadcastSource/Sink/Assistant, Cap, CoordinatedSet, Volume, Mic, Media, Call, HearingAid, Profiles). |
| `BLEAudioStream.{h,cpp}`, `BLEAudioStreamImpl.h` | Stream handle, `BLEAudioStream::Impl`, engine stream callbacks, public↔engine value conversions, `bleAudioStatus()` error mapping. |
| `BLEAudioTypes.h` | Value types: codec config, QoS, presets, locations, contexts. |
| `BLEAudioAuracast.{h,cpp}` | Internal: extended advertising payload shared by `BroadcastSource` and `CapInitiator` broadcasts (including UTF-8-safe 32-octet Broadcast Name / PBA Program Info truncation). |
| `BLEAudioControlLink.h` | Internal: peer link tracker for control client roles (lock-free engine discovery hand-off), plus shared `bleAudioWaitUntilIdle`, `bleAudioNthLocation`, and `bleAudioPackBroadcastCode` helpers. |
| `BLEAudioPlayer.{h,cpp}`, `BLEAudioRecorder.{h,cpp}`, `BLEAudioPipeline.{h,cpp}`, `BLEAudioPCM.h` | LC3 data plane and I2S port; `BLEAudioPCM.h` holds optional PCM sink/source callbacks. |
| `engine/BLEAudioEngine.{h,c}` | Pure-C host-agnostic engine lifecycle, event bridge, link bring-up, BAP presets, LE wire helpers (`src/engine/` contains `.c` and `.h` files only). |
| `engine/BLEAudioEngineBap.{h,c}` | Pure-C stream slot pool (with spinlock-guarded `snap_codec`/`snap_qos`), PACS, unicast server/client, broadcast source/sink, scan delegator. |
| `engine/BLEAudioEngineAssistant.{h,c}` | Pure-C BAP Broadcast Assistant (BASS client), PAST deduplication. |
| `engine/BLEAudioEngineCap.{h,c}` | Pure-C CAP acceptor/initiator/commander/handover, CSIP. |
| `engine/BLEAudioEngineControl.{h,c}` | Pure-C VCP, MICP, MCP, CCP, HAS. |
| `engine/BLEAudioEngineProfiles.{h,c}` | Pure-C TMAP, GMAP, PBP. |

**Engine error-return convention.** Engine C functions return `esp_err_t` /
errno values that C++ maps through `bleAudioStatus()`. Three host-glue entry
points return numeric `BTStatus::Code` values directly
(`bleAudioEngineDisconnect`, `bleAudioEngineReceivePeriodicSync`,
`bleAudioEngineStopPeriodicSyncReceive`); their callers convert them directly to
`BTStatus` without re-mapping through `bleAudioStatus()`.

The raw isochronous channel API (`BLEIso`, public through `<BLE.h>`, examples
`ISO_*`, tests `iso_cis` / `iso_bis`) lives in `libraries/BLE/src/iso/`
(`BLEIso.{h,cpp}`, `BLEIsoHost.h`, `BLEIso.{nimble,bluedroid}.{h,cpp}`). It
carries transparent SDUs with no audio concept, refuses `audio.begin()` while a
standalone `BLEIso` session owns the host, and attaches to the audio-owned host
when the engine is up.

## Controller model: stage, then commit once

`BLEAudio` is **not** a second global singleton. It is a shared handle minted
from the `BLE` singleton via `BLE.getAudioController()` — the same model as
`BLE.getScan()`. `BLEAudioImplCommon` (`BLEAudioImpl.h`) holds the shared state
that `BLEAudioImplCommon::createStagedRole` and the role factories populate:

- `roleApplies`: each `create*()` pushes an "apply" lambda that performs the
  role's engine registration (PACS records, ASCS, BASS, CAS/CSIS, VCS, …).
  `audio.start()` consumes them from the front in creation order, registers the
  **merged PACS records** (a unicast server and a broadcast sink share one sink
  PAC), then performs the **single coordinated GATT commit** through the core
  GATT coordinator.
- `roles`: a strong reference to every role `Impl`, kept until `audio.end()`.
  Registered services point into their roles' streams, so a role must outlive
  the sketch's handle. Handlers and apply lambdas capture a `weak_ptr` to avoid
  reference cycles.
- `roleStops`: transport-stop hooks for roles that own advertising/scanning or
  broadcast groups (`BroadcastSource`, `BroadcastSink`, `BroadcastAssistant`,
  `CapInitiator`), retained across a refused `audio.end()` so a retry still
  stops them.
- `handlers[]`: one event handler per event group.

This "stage between `begin()` and `start()`, then commit once" model lets audio
profiles and a coexisting classic `BLEServer` land in **one** committed GATT
table. See [`../BLE/DESIGN.md`](../BLE/DESIGN.md#service-staging-and-startup).

## Public examples and convenience helpers

The `libraries/BLEAudio/examples/` sketches are grouped by **real,
ESP32-sized products** (unicast speaker, wireless microphone, Auracast
transmitter, Auracast speaker), not one sketch per API role:

- `BLEAudioUnicastServer::configureMediaSink()` — one stereo-capable sink ASE,
  media + conversational contexts, front-left/front-right locations.
- `BLEAudioUnicastServer::configureHeadset()` — one sink ASE plus one source
  ASE for the microphone, with the usual contexts and locations.
- `BLEAudioBroadcastSource::configureAuracast(name, channels, preset)` — a
  ready-to-start Auracast transmitter with a Broadcast Name, PBP announcement
  and the 48 kHz stereo preset by default.
- `BLEAudioBroadcastSink::configureAuracast(targetName, streams)` — a
  ready-to-scan Auracast receiver with one or two BISes and auto-sync.

Raw BAP roles, phone/PC sources, CAP handover, multi-peer coordinated sets,
Broadcast Assistant, HAS, custom QoS, and PCM sinks/sources are documented in
the headers and in [`README.md`](README.md).

## Lifecycle and ordering contract

```cpp
#include <BLE.h>
#include <BLEAudio.h>

BLE.begin("Headset");                        // 1. host stack up
BLEAudio audio = BLE.getAudioController();
audio.setPresentationDelay(40000);           // 2. defaults, before creating roles
audio.begin();                               // 3. host prep + engine common_init + event bridge

BLEAudioCapAcceptor cap = audio.createCapAcceptor();   // 4. create + configure roles
cap.setSetSize(2).setRank(1);
BLEAudioUnicastServer us = audio.createUnicastServer();
us.configureHeadset();
// (optionally) create/stage classic BLEServer services here too

audio.start();                               // 5. run roleApplies, merged PACS, single GATT commit
BLE.getAdvertising().start();                // 6. advertise
// ... runtime ...
audio.end();                                 // 7. common_deinit (host stays up)
BLE.end();                                   // 8. host teardown (also runs audio.end())
```

Invariants:

- `create*()` before `begin()` or after `start()` logs an error and returns an
  empty handle (except `createBroadcastSource()`, which registers no GATT
  service and may be created after `start()`).
- Server-side configuration (stream counts, presets, locations, identity roles)
  is read by `start()`; changing it afterwards does not alter the registered
  services. Available audio contexts (`setAvailableContexts`) may change at
  runtime.
- `audio.end()` runs `roleStops`, disconnects any NimBLE fallback clients, stops
  leftover CAP/BAP transports, and waits for ISO channels to disconnect
  (`BLESync`, 5 s) before `common_deinit`. Called from a stack callback, `end()`
  returns `BTStatus::Busy` immediately. If a CONNECTED ISO channel remains,
  `end()` fails and retains ownership. After a successful `end()`, `begin()` may
  run again in the same `BLE.begin()` session.
- `BroadcastSource::stop()` gates its `BLESync` wait on `big_pending` so stopping
  a source whose `Create_BIG` never started returns immediately (0 ms) rather
  than stalling for 5 s.
- `CapInitiator::stopBroadcast()` waits for `CAP_BC_STOPPED` before deleting the
  source (`source_delete` while STREAMING is refused and racing Terminate BIG
  fails HCI Command Disallowed `0x20C`).
- `BLE.end()` calls `audio.end()` and `BLEIso::end()` while the host is still
  usable; if either fails, host shutdown is aborted cleanly.

## Links

Central links to audio peers are opened with `audio.connect(address)`, not
`BLEClient::connect()`: on Bluedroid `bleAudioHostOpenLink` opens the link
through the engine's GATTC interface (`esp_ble_gattc_aux_open`), while on NimBLE
`BLEAudio::Impl::backendConnect` opens the ACL via an owned `BLEClient`. On both
hosts the engine then encrypts the link, exchanges the MTU (explicitly on
NimBLE, automatically on Bluedroid), and discovers the peer's GATT database;
`audio.onLinkReady(conn)` fires once the link is usable.

`BLEAudioEngine.c` records the peer address of **every** live audio ACL (central
or peripheral) so `audio.disconnect(conn)` works on both NimBLE and Bluedroid
even in pure-audio peripheral sketches that never call `BLE.createServer()`.
Peripheral links are GATT-discovered only when at least one client role is
registered (`s_peer_discovery`).

Discovery is gated on security completion as well as MTU: PACS/ASCS carry
encrypted-only permissions on Bluedroid, so `audio.begin()` applies Just Works
(`NoInputNoOutput`, bonding, Secure Connections, no MITM) when `BLESecurity` has
not been configured by the sketch. On Bluedroid, `bleAudioHostSecureLink` defers
`esp_ble_set_encryption` to `esp_timer` (`sEncTimer`, guarded by `sEncMux` and
canceled on `ACL_DISCONNECT`) so BTM has finished registering the peer; if starting encryption fails, a synthetic failed `AUTH_CMPL` is posted back onto `iso_task` so discovery still proceeds unencrypted without touching engine state from the timer task.

Control client roles bind their peer through `BLEAudioControlLink`: `link.mtx`
is released before calling engine discovery functions so Bluedroid's cache-backed
inline discovery callbacks can invoke controller operations (such as
`vc.setVolume(...)` inside `onDiscovered`) without AB-BA lock inversion against
`bt_le_host_lock()`.

**MICP "mute disabled" collapses to "unmuted".** MICS reports mute as a single
bit on air; the engine emits `onMuteChanged(false)` for a disabled mute.

## Event bridge (control plane)

Every C engine unit translates stack callbacks into one tagged envelope posted to
`BLEAudioImplCommon::dispatch()` on the Bluetooth host task:

```c
typedef struct {
  uint16_t type;         /* BLE_AUDIO_EVT(group, n) */
  uint16_t conn_handle;  /* or BLE_AUDIO_CONN_NONE */
  int err;
  const void *data;      /* stack-allocated payload valid during the call */
} ble_audio_evt_t;
```

- **Core events** (`BLE_AUDIO_GRP_CORE`: ACL up/down, security, GATT discovered)
  update `gattReady`, reach **every** registered role handler, and finally fire
  `onLinkReady` / `onDisconnected`.
- **Role group events** reach the single handler registered for that group.
- Addresses crossing the bridge are normalized to **LSB-first** (`BTAddress`
  byte order) via `bleAudioAddrFromGap()`.

## Role callbacks and operation results

Role handles follow the `BLE` callback rules: per-event `std::function` members
(`on<Event>Cb`), `resetCallbacks()`, copy-under-lock (`bleAudioCopyCallback`)
and unlocked invocation on the host task. Setters return `BTStatus` for
validation and queue admission; multi-operation control points (`BroadcastAssistant`,
`CapCommander`, `CallController`) report asynchronous completion through a single
`onResult(op, status)` callback, while state-characteristic profiles confirm
writes through their state notification callbacks.

## Streams and the data path

Audio of every role flows through `BLEAudioStream` handles, bypassing the event
bridge:

- **Slot pool.** `BLEAudioEngineBap.c` owns a static pool of `ble_audio_slot_t`
  entries embedding `esp_ble_audio_bap_stream_t`. Each `BLEAudioStream::Impl`
  owns one slot for its lifetime and returns it in `~Impl()`.
- **Callbacks & taps.** Engine callbacks feed the lock-free data-path tap
  (`BLEAudioStreamTap`, synchronized via `tap` + `tapBusy`) first, then promote
  `weak_from_this()` to invoke the user callback (`onConfiguredCb`,
  `onStartedCb`, `onStoppedCb`, `onReceiveCb`, `onSentCb`) outside `mtx`.
- **Cross-core safety.** `BLEAudioStream::write()` may run on the LC3 codec task
  (pinned via `bleAudioCodecCore()` to the core opposite `BLE_HOST_PINNED_CORE`),
  serialized inside `esp_ble_audio_bap_stream_send()` by `bt_le_host_lock()`.
  Shared slot scalars (`streaming`, `seq`, `bound`, `tx`, `in_use`,
  `conn_handle`) are `_Atomic`, and `codecConfig()` / `qos()` copy cached
  `snap_codec` / `snap_qos` snapshots under `s_slot_mux` rather than
  dereferencing engine pointers off the host task.
- **LC3 pipeline.** `BLEAudioDataPath::start()` synchronizes task publication
  with `entry()` via `ulTaskNotifyTake` / `xTaskNotifyGive` so an early task exit
  cannot race `task` storage. `BLEAudioPlayer` decodes with LC3 PLC over a
  presentation-delay jitter buffer; `BLEAudioRecorder` paces capture with a
  periodic `esp_timer` and controller TX-complete credits.

## Feature guards

All guards live in `libraries/BLE/src/core/BLEGuards.h` and are `1` only when
`BLE_AUDIO_SUPPORTED` is `1`. Gate roles on their **specific** guard (which maps
to the linkable engine Kconfig entry point), never on broad `BLE_AUDIO_SUPPORTED`,
and keep `#else` stubs in the C engine units so partial Kconfig builds link cleanly.

| Guard | Kconfig | Role |
| --- | --- | --- |
| `BLE_AUDIO_SUPPORTED` | `CONFIG_BT_AUDIO` (+ `BLE_ISO_SUPPORTED`) | engine present |
| `BLE_AUDIO_UNICAST_SERVER_SUPPORTED` | `CONFIG_BT_BAP_UNICAST_SERVER` | Unicast Server |
| `BLE_AUDIO_UNICAST_CLIENT_SUPPORTED` | `CONFIG_BT_BAP_UNICAST_CLIENT` | Unicast Client |
| `BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED` | `CONFIG_BT_BAP_BROADCAST_SOURCE` | Broadcast Source |
| `BLE_AUDIO_BROADCAST_SINK_SUPPORTED` | `CONFIG_BT_BAP_BROADCAST_SINK` | Broadcast Sink |
| `BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED` | `CONFIG_BT_BAP_SCAN_DELEGATOR` | BASS (on sink) |
| `BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED` | `CONFIG_BT_BAP_BROADCAST_ASSISTANT` | Broadcast Assistant |
| `BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED` | `CONFIG_BT_CAP_ACCEPTOR_SET_MEMBER` | CAP Acceptor (CAS+CSIS) |
| `BLE_AUDIO_CAP_INITIATOR_SUPPORTED` | `CONFIG_BT_CAP_INITIATOR` | CAP Initiator |
| `BLE_AUDIO_CAP_COMMANDER_SUPPORTED` | `CONFIG_BT_CAP_COMMANDER` | CAP Commander |
| `BLE_AUDIO_CAP_HANDOVER_SUPPORTED` | `CONFIG_BT_CAP_HANDOVER` | CAP unicast↔broadcast handover |
| `BLE_AUDIO_CSIP_MEMBER_SUPPORTED` | `CONFIG_BT_CSIP_SET_MEMBER` | CSIP Set Member |
| `BLE_AUDIO_CSIP_COORDINATOR_SUPPORTED` | `CONFIG_BT_CSIP_SET_COORDINATOR` | CSIP Set Coordinator |
| `BLE_AUDIO_VCP_RENDERER_SUPPORTED` | `CONFIG_BT_VCP_VOL_REND` | VCP Renderer |
| `BLE_AUDIO_VCP_CONTROLLER_SUPPORTED` | `CONFIG_BT_VCP_VOL_CTLR` | VCP Controller |
| `BLE_AUDIO_MICP_DEVICE_SUPPORTED` | `CONFIG_BT_MICP_MIC_DEV` | MICP Device |
| `BLE_AUDIO_MICP_CONTROLLER_SUPPORTED` | `CONFIG_BT_MICP_MIC_CTLR` | MICP Controller |
| `BLE_AUDIO_MCP_SERVER_SUPPORTED` | `CONFIG_BT_MCS` + `CONFIG_BT_MCTL_LOCAL_PLAYER_CONTROL` | MCP Media Player |
| `BLE_AUDIO_MCP_CLIENT_SUPPORTED` | `CONFIG_BT_MCC` | MCP Media Controller |
| `BLE_AUDIO_CCP_SERVER_SUPPORTED` | `CONFIG_BT_CCP_CALL_CONTROL_SERVER` | CCP Call Server |
| `BLE_AUDIO_CCP_CLIENT_SUPPORTED` | `CONFIG_BT_CCP_CALL_CONTROL_CLIENT` | CCP Call Controller |
| `BLE_AUDIO_TMAP_SUPPORTED` | `CONFIG_BT_TMAP` | TMAP identity |
| `BLE_AUDIO_GMAP_SUPPORTED` | `CONFIG_BT_GMAP` | GMAP identity |
| `BLE_AUDIO_PBP_SUPPORTED` | `CONFIG_BT_PBP` | PBP announcement helper |
| `BLE_AUDIO_HAS_SUPPORTED` | `CONFIG_BT_HAS` | HAS device (server) |
| `BLE_AUDIO_HAS_CLIENT_SUPPORTED` | `CONFIG_BT_HAS_CLIENT` | HAS controller (client) |
| `BLE_AUDIO_LC3_SUPPORTED` | `esp_audio_codec` header presence | LC3 data plane |

## Per-role spec-compliance checklist

- **CAP Acceptor:** publishes **CAS** with an **included CSIS** (`CONFIG_BT_CAP_ACCEPTOR_SET_MEMBER`); products must replace the default demo SIRK.
- **Unicast Server:** publishes **PACS** + **ASCS** and valid **Available Audio Contexts**.
- **Broadcast Source:** emits **Broadcast Audio Announcement** (`0x1852`) + optional **PBA** (`0x1856`) + UTF-8-truncated **Broadcast Name** (`0x30`, 4–32 octets per PBP v1.0 §3.1.1) in extended adv, and **BASE** (`0x1851`) in periodic adv.
- **Broadcast Sink:** publishes **PACS** (sink caps, `setMaxChannelsPerStream` default 2) and optional **Scan Delegator / BASS**; disarms PAST receive and pending PA-sync creation when BASS cancels PA sync.
- **VCP / MICP / CSIP / TMAP / GMAP / HAS / CCP (GTBS) / MCP (GMCS):** see role headers for mandatory characteristics and Kconfig instance counts.
- **Broadcast Assistant:** drives remote BASS over one ACL and deduplicates PAST transfers per `(conn_handle, source_id)` (supporting `conn_handle == 0` via `CONN_NONE = 0xFFFF` sentinel).

## GATT coexistence

Classic `BLEServer` services and LE Audio profiles share **one** committed GATT
table on NimBLE via `gatt/BLEGattDatabase.nimble.{h,cpp}` (`BLE.begin()` →
`audio.begin()` → create/stage `BLEServer` services → `audio.start()`), while
Bluedroid registers separate BTA GATTS apps natively. Pure-audio and pure-ISO
peripherals also work without calling `BLE.createServer()` on both stacks. Full
details live in [`../BLE/DESIGN.md`](../BLE/DESIGN.md#service-staging-and-startup).

## Testing

- **Dual-DUT HITL:** `tests/validation/ble_audio/` (15 phases covering pure-audio peripheral lifecycle without `BLEServer`, same-host `audio.end()`→`begin()` re-init, classic GATT coexistence & callback-deferred `BLE.end()`, `BLEIso` CIS/BIS, BAP unicast/broadcast, LC3 loopback, VCP/MICP/MCP/CCP, TMAP/GMAP/CSIP/CAP Commander, HAS, stereo broadcast with callback churn, Broadcast Assistant with PAST on handle 0, CAP handover, `BroadcastSource` start/stop race, `CapInitiator` 1/2-BIS interleave, and controller memory release).
- **Host-interop:** `tests/validation/ble_audio_host/` (ESP DUT + Linux Bumble).
- **Static isolation check:** `libraries/BLE/tests/check_backend_isolation.sh`.

## Maintainer rules (audio-specific)

In addition to [`../BLE/DESIGN.md`](../BLE/DESIGN.md#maintainer-rules):

1. **Zero engine types in C++ headers; zero backend macros in shared files.** Keep `esp_ble_audio_*` / `bt_*` / `<zephyr/...>` inside `engine/BLEAudioEngine*.c`, and keep `BLE_NIMBLE` / `BLE_BLUEDROID` inside `BLEAudioBackend.h`, `BLEAudio.{nimble,bluedroid}.*`, and `engine/BLEAudioEngine.{nimble,bluedroid}.*`.
2. **Always provide `#else` stubs** in engine C units for disabled role Kconfigs.
3. **Stage in `roleApplies`, commit in `start()`.** Use `BLEAudioImplCommon::createStagedRole` where applicable and never commit GATT from a role factory.
4. **Use `BLE_CHECK_IMPL`, `on<Event>Cb`, and defaulted move members** on every handle, matching `BLE`.
5. **Events through the bridge, audio through streams.**
6. **Validate on both NimBLE and Bluedroid** (`select-ble-backend.sh`) before declaring any change done.

## Reference material

- Core BLE architecture: [`../BLE/DESIGN.md`](../BLE/DESIGN.md)
- User guide & examples: [`README.md`](README.md)
- ESP-BLE-AUDIO component: ESP-IDF `components/bt/esp_ble_audio`
- Bluetooth LE Audio specifications: <https://www.bluetooth.com/specifications/specs/>
