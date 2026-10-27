# LE Audio Layer Design

## Purpose

This document is for maintainers and AI tools working on the LE Audio subsystem
of the BLE library (`src/audio/`). It describes the architecture, the engine
boundary, the event and stream paths, the lifecycle contract and the per-role
spec-compliance obligations. It complements [`DESIGN.md`](DESIGN.md) (the
library-wide architecture) — read that first. Where the two overlap (GATT
coexistence, PIMPL, callback model), this document defers to `DESIGN.md` and
only adds audio-specific detail.

## Scope and status

LE Audio is built on Espressif's **ESP-BLE-AUDIO** engine (`esp_ble_audio`,
the Generic Audio Framework: PACS/ASCS/BAP/CAP and the control/top-level
profiles). The engine API is **host-agnostic**, so almost the whole subsystem is
shared between NimBLE and Bluedroid. Host-specific code is limited to the glue
files (`BLEAudioEngine.nimble.cpp` / `BLEAudioEngine.bluedroid.cpp`, which
forward host GAP/GATT events to the engine) and a few `BLE_NIMBLE` /
`BLE_BLUEDROID` branches for the calls the engine leaves to the host: link
encryption and the Bluedroid GATT client open in `BLEAudioEngine.c`, and the
PAST send of the CAP handover in `BLEAudioCap.cpp`.

The packaged Arduino libs track ESP-IDF **`release/v6.1`**. Only APIs present
on that branch are used; features that exist only on `master` are not wired
up yet.

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
- **Re-initialization:** `audio.end()` runs `esp_ble_audio_common_deinit`, so
  `audio.begin()` may run again in the same boot with a different set of roles.

### Not exposed / known limits

| Surface | Status |
| --- | --- |
| **Discrete TBS / MCS** | Not exposed. `release/v6.1` builds them on both hosts when `CONFIG_BT_TBS_BEARER_COUNT` / `CONFIG_BT_MCS_INSTANCE_COUNT` is non-zero (both default to 0), but the Arduino API drives only the generic GTBS / GMCS instances. |
| **OTS (Object Transfer)** | Not exposed (`CONFIG_BT_OTS` builds on both hosts in `release/v6.1`). MCP has no object transfer. |
| **CSIP ordered access** | The coordinator discovers and locks set members; the CAP initiator does not run the CSIP ordered-access procedure across members. |
| **BASS receive-state cleanup** | The Scan Delegator does not remove stale receive states on its own; the assistant must remove them. |
| **One PA sync at a time** | The broadcast sink follows one source; an assistant request is refused while a PA sync is in use. |
| **GATT frozen after `start()`** | Roles that register services must be created before `audio.start()`; swapping roles needs `audio.end()` → `audio.begin()`. |
| **Preview status** | The LE Audio APIs are still marked preview in IDF. |

## Layering

```
sketch
  │  <BLE.h>  (audio/BLEAudio.h is the audio aggregator, pulled in by the umbrella)
  ▼
BLEAudio              controller handle (BLEAudio.{h,cpp}, BLEAudioImpl.h)
  │  create*() factories mint …
  ▼
role handles          BLEAudioUnicastServer, …, BLEAudioHearingAidController
  │  value-type PIMPL handles; no esp_ble_audio_* types
  │  BLEAudioStream handles carry the audio of every role
  ▼
C engine units        BLEAudioEngine*.{h,c}  (extern "C"; the ONLY code that
  │                   includes esp_ble_audio_* headers)
  ▼
ESP-BLE-AUDIO engine  (ESP-IDF component; host-agnostic)
```

Two rules keep this clean:

1. **Handle headers are backend-agnostic.** No `esp_ble_audio_*`, `bt_*`,
   `<zephyr/...>` or host type appears in a header a sketch can reach through
   `<BLE.h>`. `tests/check_backend_isolation.sh` enforces this for the umbrella
   and every header it includes directly (which includes `audio/BLEAudio.h`).
2. **The engine boundary is C.** The `esp_ble_audio_*` headers pull in Zephyr
   headers that do not parse as C++, so every engine call lives in a C unit
   behind a narrow `extern "C"` header with plain-C payload structs.

### File map

| Files | Role |
| --- | --- |
| `BLEAudio.{h,cpp}`, `BLEAudioImpl.h` | Controller: lifecycle, role factories, link events, the event dispatcher. |
| `BLEAudio<Role>.{h,cpp}` | Public role handles (UnicastServer/Client, BroadcastSource/Sink/Assistant, Cap, CoordinatedSet, Volume, Mic, Media, Call, HearingAid, Profiles). |
| `BLEAudioStream.{h,cpp}`, `BLEAudioStreamInternal.h` | Stream handle, engine stream callbacks, public↔engine value conversions, `bleAudioStatus()` error mapping. |
| `BLEAudioTypes.h` | Value types: codec config, QoS, presets, locations, contexts. |
| `BLEAudioControlLink.h` | Internal: the peer a control client role drives; defers discovery until the link is GATT-discovered. |
| `BLEAudioPlayer`, `BLEAudioRecorder`, `BLEAudioPipeline`, `BLEAudioI2s.h` | LC3 data plane. |
| `BLEAudioEngine.{h,c}` | Engine lifecycle, event bridge, GAP/GATT plumbing, link bring-up, BAP presets. |
| `BLEAudioEngineBap.{h,c}` | Stream slot pool, PACS, unicast server/client, broadcast source/sink, scan delegator. |
| `BLEAudioEngineAssistant.{h,c}` | BAP Broadcast Assistant (BASS client), PAST. |
| `BLEAudioEngineCap.{h,c}` | CAP acceptor/initiator/commander/handover, CSIP. |
| `BLEAudioEngineControl.{h,c}` | VCP, MICP, MCP, CCP, HAS. |
| `BLEAudioEngineProfiles.{h,c}` | TMAP, GMAP, PBP. |
| `BLEAudioEngine.nimble.{h,cpp}` / `.bluedroid.{h,cpp}` | Host glue: forward the host's GAP/GATT events to the engine. |

The raw isochronous channel API (`BLEIso`, public through `<BLE.h>`, examples
`ISO_*`, tests `iso_cis` / `iso_bis`) lives in `src/iso/`. It carries
transparent SDUs with no audio concept and is documented in `BLEIso.h`. It
never initializes ISO twice: `audio.begin()` is refused while a standalone
`BLEIso` session owns the host, and `BLEIso` attaches to the audio-owned host
when the engine is up. Its roles have their own guards
(`BLE_ISO_CIS_CENTRAL_SUPPORTED`, `BLE_ISO_CIS_PERIPHERAL_SUPPORTED`,
`BLE_ISO_BROADCASTER_SUPPORTED`, `BLE_ISO_SYNC_RECEIVER_SUPPORTED`), which the
LE Audio roles pull in through Kconfig.

## Controller model: stage, then commit once

`BLEAudio` is **not** a second global singleton. It is a shared handle minted
from the `BLE` singleton via `BLE.getAudioController()` — the same model as
`BLE.getScan()`. `BLEAudio::Impl` (`BLEAudioImpl.h`) is defined once and
compiled into every build.

It holds three pieces of state that every factory feeds:

- `roleApplies`: each `create*()` pushes an "apply" lambda that performs the
  role's engine registration (PACS records, ASCS, BASS, CAS/CSIS, VCS, …).
  `audio.start()` runs them in creation order, registers the **merged PACS
  records** (a unicast server and a broadcast sink share one sink PAC), then
  performs the **single coordinated GATT commit** through the core GATT
  coordinator, and clears the list.
- `roles`: a strong reference to every role Impl, kept until `audio.end()`.
  Registered services point into their roles' streams, so a role must outlive
  the sketch's handle. Handlers and apply lambdas capture a `weak_ptr` to avoid
  reference cycles.
- `handlers[]`: one event handler per event group (see below).

This "stage between `begin()` and `start()`, then commit once" model is what
lets many audio profiles and a coexisting classic `BLEServer` land in **one**
committed GATT table. See `DESIGN.md` → "Service staging and startup".

## Lifecycle and ordering contract

```cpp
BLE.begin("Headset");                        // 1. host stack up
BLEAudio audio = BLE.getAudioController();
audio.setPresentationDelay(40000);           // 2. defaults, before creating roles
audio.begin();                               // 3. engine common_init + event bridge

BLEAudioCapAcceptor cap = audio.createCapAcceptor();   // 4. create + configure roles
cap.setSetSize(2).setRank(1);
BLEAudioUnicastServer us = audio.createUnicastServer();
us.setSinkStreams(1).setSourceStreams(1);
// (optionally) create/stage classic BLEServer services here too

audio.start();                               // 5. run roleApplies, merged PACS, single GATT commit
BLE.getAdvertising().start();                // 6. advertise
// ... runtime ...
audio.end();                                 // 7. common_deinit (host stays up)
BLE.end();                                   // 8. host teardown (also runs audio.end())
```

Invariants:

- `create*()` before `begin()` logs an error and returns an empty handle. A
  role that registers services and is created after `start()` is never
  registered (the broadcast source, which has no service, is the exception).
- Server-side configuration (stream counts, presets, locations, identity
  roles) is read by `start()`; changing it afterwards does not change the
  registered services (the setters that are easy to misuse warn). Available
  audio contexts are the exception: they may change at runtime.
- `audio.end()` is refused (`InvalidState`) while a stream is still up; stop
  the streams first. After a successful `end()`, `begin()` may run again.
- Client roles (`*Controller`, `*Initiator`, `UnicastClient`, the assistant)
  still need `begin()`/`start()` to register their engine callbacks, and act
  only once a link is ready.

## Links

Central links to audio peers are opened with `audio.connect(address)`, not
`BLEClient::connect()`: on Bluedroid the link must be opened through the
engine's GATT client, and on both hosts the engine then encrypts the link (with
the `BLESecurity` settings), exchanges the MTU and discovers the peer.
`audio.onLinkReady(conn)` fires when that discovery finished, for links opened
by either side; client roles may use `conn` from then on.

Control client roles bind their peer through `BLEAudioControlLink`: calling
`discover(conn)` before the link is ready is accepted, and the service
discovery runs as soon as the core `GATT_DISCOVERED` event arrives.

## Event bridge (control plane)

The engine callbacks are plain C function pointers. Every C unit translates
its callbacks into one tagged envelope and posts it to a single C++ sink:

```c
typedef struct {
  uint16_t type;         /* BLE_AUDIO_EVT(group, n) */
  uint16_t conn_handle;  /* or BLE_AUDIO_CONN_NONE */
  int err;
  const void *data;      /* payload struct declared in the unit's header */
} ble_audio_evt_t;
```

`BLEAudio::Impl::dispatch()` routes it on the Bluetooth host task:

- **Core events** (`BLE_AUDIO_GRP_CORE`: ACL up/down, security, GATT
  discovered) update the link table, reach **every** registered role handler,
  then the application's `onLinkReady()` / `onDisconnected()`.
- **Every other group** reaches the single handler its role registered. There
  is one group per role (unicast client, broadcast sink, CAP initiator, VCP
  controller, …), so each role handle registers exactly one handler.

Rules:

- Payloads are only valid during the call; units pass stack structs.
- Handlers run on the host task: keep them short and never block. Long
  procedures (starting streams, handovers) are started from `loop()`.
- **One live role instance per type.** A second handle of the same role
  replaces the group handler, matching the engine, which registers one
  callback set per profile.
- Addresses crossing the bridge are **LSB-first** on both hosts (the order
  `BTAddress(const uint8_t *, Type)` expects). GAP event addresses keep the
  host's byte order, so units convert them with `bleAudioAddrFromGap()`.

## Streams and the data path

Audio of every role flows through `BLEAudioStream` handles; stream events do
**not** go through the event bridge.

- **Slot pool.** `BLEAudioEngineBap.c` owns a static pool of stream slots
  sized from Kconfig (sink + source ASE counts, unicast client group streams,
  broadcast source and sink stream counts). Each slot embeds the engine's
  `esp_ble_audio_bap_stream_t`; the engine callbacks recover the slot with
  `container_of`. Unicast and broadcast streams coexist.
- **Ownership.** Each `BLEAudioStream::Impl` owns exactly one slot for its
  whole lifetime and registers itself as the slot's `owner`, so an engine event
  lands on its stream without any lookup, and the destructor returns the slot.
  A unicast server stream is bound to an ASE when the client configures it;
  that fixes its direction (a sink ASE becomes `Rx`, a source ASE `Tx`).
- **Callbacks.** One `ble_audio_stream_cbs_t` table serves every stream.
  Each event first feeds the internal data-path **tap**, then the application
  callback. `recv` and `sent` run once per SDU interval: the tap is called from
  the raw owner, and a public handle is only built when the application
  registered `onReceive()` / `onSent()`.
- **Taps.** The Player/Recorder install a `BLEAudioStreamTap` (plain function
  pointers + context) through `BLEAudioStreamAccess::setTap()`. The tap is an
  atomic pointer paired with a busy count, so removing or replacing it waits
  for host-task calls already inside it; the data path can then free its
  buffers safely. A release of the endpoint is forwarded to the tap as a stop,
  so the data path ends its session even when no Streaming exit was reported.
- **Sending.** `BLEAudioStream::write()` sends one SDU. The slot keeps the
  packet sequence number: it restarts at 0 each time the stream starts and only
  advances when the send succeeds.

### LC3 pipeline

`BLEAudioPipeline` is shared by the player and the recorder: stream
attachment (one stream carrying 1–2 channels, or two mono streams), the codec
task (pinned to the core that does not run the host) and the I2S port.

- **Player:** taps copy each SDU into a per-stream ring and wake the codec
  task. It buffers to a jitter depth derived from the presentation delay (or
  `setJitterDepth()`), then decodes one SDU per interval; lost or missing
  frames are concealed with LC3 PLC. Statistics: received, dropped, lost, PLC
  frames, underruns.
- **Recorder:** a periodic `esp_timer` at the SDU interval wakes the codec
  task, which reads one frame of PCM per channel, encodes it and writes the
  SDU. Statistics: sent, send errors.
- Both follow the attached streams: each time a stream starts, the task reads
  its codec configuration and (re)builds its session. The LC3 codec config
  itself (frame length, octets) comes from the negotiated stream; 44.1 kHz uses
  the 48 kHz frame length, as LC3 specifies (`BLEAudioCodecConfig::samplesPerFrame()`).

## Feature guards

All guards are defined in `core/BLEGuards.h` and are only ever `1` when
`BLE_AUDIO_SUPPORTED` is `1`. Each maps to the engine Kconfig symbol that gates
the role's linkable entry point, so a handle / factory / example self-excludes
when its role is not built. Check the **specific** guard, never the broad
`BLE_AUDIO_SUPPORTED`, when gating a role. Engine units provide stub
definitions in the `#else` branch of each guard so the component links on any
configuration; a role "not enabled" error names the missing Kconfig symbol.

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
| `BLE_AUDIO_MCP_SERVER_SUPPORTED` | `CONFIG_BT_MCS` | MCP Media Player |
| `BLE_AUDIO_MCP_CLIENT_SUPPORTED` | `CONFIG_BT_MCC` | MCP Media Controller |
| `BLE_AUDIO_CCP_SERVER_SUPPORTED` | `CONFIG_BT_CCP_CALL_CONTROL_SERVER` | CCP Call Server |
| `BLE_AUDIO_CCP_CLIENT_SUPPORTED` | `CONFIG_BT_CCP_CALL_CONTROL_CLIENT` | CCP Call Controller |
| `BLE_AUDIO_TMAP_SUPPORTED` | `CONFIG_BT_TMAP` | TMAP identity |
| `BLE_AUDIO_GMAP_SUPPORTED` | `CONFIG_BT_GMAP` | GMAP identity |
| `BLE_AUDIO_PBP_SUPPORTED` | `CONFIG_BT_PBP` | PBP announcement helper |
| `BLE_AUDIO_HAS_SUPPORTED` | `CONFIG_BT_HAS` | HAS device (server) |
| `BLE_AUDIO_HAS_CLIENT_SUPPORTED` | `CONFIG_BT_HAS_CLIENT` | HAS controller (client) |
| `BLE_AUDIO_LC3_SUPPORTED` | `esp_audio_codec` header presence | LC3 data plane |

> **Guard-vs-linkable-surface note.** `BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED` keys on
> `CONFIG_BT_CAP_ACCEPTOR_SET_MEMBER`, not `CONFIG_BT_CAP_ACCEPTOR`: the engine
> only compiles `esp_ble_audio_cap_acceptor_register` (which instantiates
> CAS + an included CSIS) when the acceptor is also a set member. Always guard on
> the symbol that gates the actual linkable engine entry point, not the
> "conceptual" one — verify against the engine source when adding a role.
> Likewise, Kconfig makes `CONFIG_BT_CAP_HANDOVER` depend on the commander,
> initiator, broadcast assistant, broadcast source and a unicast client sink ASE.

The raw ISO roles of `BLEIso` follow the same rule with their own guards, which
only need `BLE_ISO_SUPPORTED` (not the audio engine):

| Guard | Kconfig | Role |
| --- | --- | --- |
| `BLE_ISO_SUPPORTED` | `CONFIG_BT_ISO` | host ISO transport present |
| `BLE_ISO_CIS_CENTRAL_SUPPORTED` | `CONFIG_BT_ISO_CENTRAL` | `BLEIso::connectCis` |
| `BLE_ISO_CIS_PERIPHERAL_SUPPORTED` | `CONFIG_BT_ISO_PERIPHERAL` | `BLEIso::listenCis` |
| `BLE_ISO_BROADCASTER_SUPPORTED` | `CONFIG_BT_ISO_BROADCASTER` | `BLEIso::createBig` |
| `BLE_ISO_SYNC_RECEIVER_SUPPORTED` | `CONFIG_BT_ISO_SYNC_RECEIVER` | `BLEIso::syncBig` |

## Per-role spec-compliance checklist

When adding or reviewing a role, confirm the mandatory service/announcement/
preset obligations. Non-exhaustive, but these are the ones that bite:

- **CAP Acceptor** must publish the **Common Audio Service (CAS)** with an
  **included CSIS** — a bare PACS/ASCS server is a BAP unicast server but *not*
  a CAP acceptor. This is why the acceptor guard requires the set-member symbol.
  The default SIRK is a fixed demo value; products must set their own.
- **Unicast Server** must expose **PACS** (published sink/source capabilities +
  available/supported contexts) and **ASCS** (one ASE per stream). The PAC
  record is the union of the supported presets; advertise the correct
  **Available Audio Contexts** or centrals won't route.
- **Broadcast Source** must emit the **Broadcast Audio Announcement** (0x1852)
  in the extended adv and the **BASE** in periodic adv. The source assembles
  that payload (plus a PBA `0x1856` and Broadcast Name `0x30` so phone Auracast
  UIs can find and title it) through `BLEAdvertising`, so it behaves the same
  on both hosts, and binds the BIG to the adv instance.
- **Broadcast Sink** must publish **PACS** (sink caps) and, to be remotely
  assisted, the **Scan Delegator / BASS**. The sink does not advertise by
  itself: a sketch that wants assistants to reach it advertises connectably
  (see `LEAudio_BroadcastSink`). Auto-sync works without an assistant; with
  auto-sync off and the sink not started it only follows BASS requests (the
  CAP handover acceptor case).
- **VCP Renderer** must publish **VCS** (and any compiled VOCS/AICS instances);
  **MICP Device** must publish **MICS** (+ AICS).
- **CSIP Set Member** must expose **CSIS** with SIRK / Set Size / Rank, and (for
  discoverability) the **RSI** in advertising.
- **TMAP** publishes the **TMAS** identity service with a role bitmask; the
  media/call audio still flows through CAP + BAP + the control profiles. A pure
  client sets no local roles and registers nothing.
- **GMAP** publishes **GMAS** with the role bitmask and one feature field per
  role. Each role needs its `CONFIG_BT_GMAP_<role>_SUPPORTED`, and the features
  must match the ASE counts (UGT needs Sink and/or Source); otherwise
  `audio.start()` fails with InvalidParam.
- **Broadcast Assistant** drives a remote sink's BASS over one ACL.
  `startRemoteScan()` runs `BLE.getScan()` as a passive extended scan; when a
  receive state requests SyncInfo and this device is synced to that source's
  periodic advertising, the sync is sent with PAST (needs the host's PAST
  sender: `CONFIG_BT_NIMBLE_PERIODIC_ADV_SYNC_TRANSFER` /
  `CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER`). Operations before
  `discover()` return `NotConnected`.
- **CAP Handover** moves a running unicast session to a broadcast on the same
  sink stream handles (the acceptors are told to receive it through BASS) and
  back. The acceptors must be broadcast sinks with the Scan Delegator.
- **CCP** is **GTBS-only** and **MCP** is **GMCS-only** in the Arduino API
  (see [Not exposed / known limits](#not-exposed--known-limits)).
- **HAS device** must register a non-empty **preset list** (needs
  `CONFIG_BT_HAS_PRESET_COUNT > 0`) and set an active preset; the client only
  discovers the Active Index + Control Point when its `preset_switch` callback is
  set.
- **PBP** (`BLEAudioPublicBroadcast`) builds/parses PBA service data. The
  `BLEAudioBroadcastSource` embeds a PBA (`0x1856`, SQ or HQ matching the
  preset, plus Program Info / Media context) and a Broadcast Name AD (`0x30`)
  in extended adv so independent Auracast UIs (e.g. Galaxy S23/S24) can
  discover and title it. Complete Local Name (`0x09`) alone shows as "Unknown"
  on those UIs.

## GATT coexistence

An ordinary `BLEServer` custom service and the audio profiles live in **one**
committed GATT table. The audio engine performs the service inits and the single
`ble_gatts_start`; the classic server only *stages* its services. The required
ordering is `BLE.begin()` → `audio.begin()` → create/stage server services →
`audio.start()`. This is a NimBLE-side concern (Bluedroid's incremental
multi-app GATTS registration already allows coexistence). The full contract —
including the `BLEGattDatabase` coordinator's standalone vs. audio mode and why
the dependency points one way (NimBLE coordinator observes the host-agnostic
engine) — is documented in `DESIGN.md` → "Service staging and startup".

## Testing

Audio behavior is exercised by:

- Dual-DUT: `tests/validation/ble_audio/` (server + client): `BLEIso` CIS
  (bidirectional) / BIS, BAP
  unicast/broadcast, LC3 loopback, control profiles, TMAP/GMAP, HAS, stereo
  broadcast, broadcast assistant, CAP handover, memory release.
- Host-interop: `tests/validation/ble_audio_host/` (ESP DUT + Linux Bumble).

Phases self-skip when a role or host tool is unavailable. When a role's
on-air behavior changes, update the suite — the suites are the executable
contract.

## Maintainer rules (audio-specific)

In addition to the library-wide rules in `DESIGN.md`:

1. **Never leak engine types across the C++ boundary.** `esp_ble_audio_*` /
   `bt_*` / `<zephyr/...>` stay inside the `BLEAudioEngine*.c` units.
2. **Always provide stubs** in the `#else` of each role guard so the component
   links on builds without that Kconfig.
3. **Stage in `roleApplies`, commit in `start()`.** Do not call
   `ble_gatts_start` (or an equivalent per-role commit) from a factory — it
   breaks single-commit coexistence.
4. **Guard on the linkable engine symbol**, not the conceptual profile name
   (see the CAP acceptor note).
5. **Keep host code minimal and explicit.** Host event forwarding lives in
   `BLEAudioEngine.nimble.*` / `BLEAudioEngine.bluedroid.*`; any other host
   call sits in a `BLE_NIMBLE` / `BLE_BLUEDROID` branch with an equivalent for
   the other host (or a documented "not supported" error).
6. **Events through the bridge, audio through streams.** A new unit posts
   `ble_audio_evt_t` envelopes with its own group; per-SDU work never goes
   through the bridge.
7. **Use only `release/v6.1` engine APIs** until the packaged libs move.
8. **Match new Kconfig in the lib-builder defconfig.** A new role needs its
   `CONFIG_BT_*` symbol in the target defconfig (e.g.
   `esp32-arduino-lib-builder/configs/defconfig.esp32s31`) before the real
   (non-stub) path can link.
9. **Treat the validation suite as part of the design** — add a self-skipping
   phase for any new on-air behavior.

## Reference material

- ESP-BLE-AUDIO component: ESP-IDF `components/bt/esp_ble_audio`
- Bluetooth LE Audio specifications (GAF, BAP, CAP, CSIP, VCP, MICP, MCP, CCP,
  TMAP, GMAP, HAS, PBP): <https://www.bluetooth.com/specifications/specs/>
- Library-wide architecture: [`DESIGN.md`](DESIGN.md)
