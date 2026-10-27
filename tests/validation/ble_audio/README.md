# LE Audio Validation Test

Validates the LE Audio (Generic Audio Framework) stack between a server and a
client across two DUTs: the `BLEIso` transport, the BAP unicast/broadcast
data plane, the LC3 codec loopback, the control profiles (VCP/MICP/MCP/CCP via
CAP), the top-level identity profiles (TMAP/GMAP), the Hearing Access Service,
stereo broadcast, the Broadcast Assistant and the CAP handover. This is a
**multi-DUT** test. The sketches are stack-neutral (NimBLE and Bluedroid),
but the packaged `esp32s31` libraries enable LE Audio on NimBLE only, so CI
runs the audio phases on NimBLE. Bluedroid needs libraries rebuilt with
Bluedroid and LE Audio enabled.

It was split out of the combined [`ble`](../ble) suite so that test stays
focused on core GATT/GAP while this one owns the GAF stack. The core-BLE
lifecycle (init/deinit, GATT, security, advertising, L2CAP, etc.) lives in
`ble`; this suite assumes a working stack and drives only the audio layer.

## Architecture

```
 ┌──────────────┐          BLE / ISO         ┌──────────────┐
 │    Server    │◄──── advertising ──────────│    Client    │
 │  (device0)   │   scan/connect + CIS/BIS   │  (device1)   │
 └──────┬───────┘                            └──────┬───────┘
        │ serial                                    │ serial
        ▼                                           ▼
 ┌─────────────────────────────────────────────────────────┐
 │                pytest (test_ble_audio.py)                 │
 └─────────────────────────────────────────────────────────┘
```

## Test Cases

The suite runs as a sequence of numbered phases driven by `test_ble_audio.py`.
Each phase is gated by a `START_PHASE_<n>` handshake and recorded individually.
Every audio phase self-skips on targets/builds without the relevant feature, so
the suite runs unchanged on non-audio silicon.

| Phase | Label | What it covers |
|---|---|---|
| 1 | audio_lifecycle | LE Audio engine bring-up (`common_init`/`start`) + classic-service coexistence committed into one GATT table via `BLEGattDatabase` (fails if `begin()`, `start()` or `end()` fails), then `audio.end()` -> `audio.begin()` -> `audio.start()` re-initialization with a different role set |
| 2 | iso_cis | `BLEIso` CIS transport integrity: server = CIS Peripheral (`listenCis`), client = CIS Central (`connectCis`) on a bidirectional CIS (`returnSduSize`); counts transparent SDUs streamed Central->Peripheral and Peripheral->Central with channel-managed sequence numbers, then `disconnect()`s the CIS (host ISO) |
| 3 | iso_bis | `BLEIso` BIS transport integrity: server = BIS Broadcaster (`createBig` on ext/periodic adv), client = BIS Receiver (`syncBig` via forwarded BIGInfo); counts SDUs Broadcaster->Receiver, then the Broadcaster terminates the BIG with `disconnect()` (host ISO + BLE5) |
| 4 | bap_unicast | BAP unicast transport integrity: server = Unicast Server (PACS/ASCS, coexisting with a classic GATT service), client = Unicast Client running the full discover->config->QoS->enable->CIS->start setup; asserts the discovered sink PACS covers the configured preset (`BLEAudioUnicastPeerInfo::sinkPresets`), that the stream reached Streaming, and counts transparent SDUs streamed Client->Server sink ASE (Unicast Server / Unicast Client) |
| 5 | bap_broadcast | BAP broadcast transport integrity: server = Broadcast Source (ext/periodic adv + BASE, streaming a BIG), client = Broadcast Sink scanning + PA-syncing + BIG-syncing; asserts the BASE was parsed (`onBaseReceived`) and counts transparent SDUs sent by the Source and received by the Sink over the BIS (Broadcast Source / Broadcast Sink) |
| 6 | lc3_loopback | LC3 data plane end-to-end: client = Unicast Client whose stream is attached to a `BLEAudioRecorder` fed by a 1 kHz tone through a PCM source (PCM->LC3->CIS), server = Unicast Server whose sink stream is attached to a `BLEAudioPlayer` with a PCM sink (CIS->LC3->PCM); asserts the recorder sent SDUs and the decoded sample count and mean amplitude prove real audio survived the codec (LC3 codec `esp_audio_codec`) |
| 7 | control_profiles | LE Audio control-profile round-trip over one ACL: server = acceptor exposing CAP (CAS+CSIS), VCP renderer, MICP device, MCP media player (MCS), CCP call server (GTBS); client = controller ("phone") that connects once and drives CAP discovery, VCP setVolume, MICP mute, MCP play, and CCP originate; gated on the VCP write landing on the server's renderer (VCP renderer + CAP acceptor / VCP controller + CAP initiator) |
| 8 | top_profiles | TMAP / GMAP identity round-trip: server publishes TMAS (CallTerminal + UnicastMediaReceiver) and GMAS (UnicastGameTerminal, UGT Sink); client discovers both and asserts the role / feature bitmasks (TMAP/GMAP). On the same link, set control: the client's CSIP Set Coordinator reads the server's one-member set, locks and releases it, and its CAP Commander (CAS + VCP discovery) sets the volume to 77; the server must see both lock changes and the volume (CSIP coordinator + CAP commander + VCP controller on the client) |
| 9 | hearing_aid | Hearing Access Service round-trip: server = binaural hearing aid publishing a 3-preset HAS list and counting select requests; client = controller that discovers the HAS, reads the preset records, and switches the active preset to index 2 (LE Audio engine + HAS server/client) |
| 10 | stereo_broadcast | encrypted stereo Auracast: server = 2-BIS Broadcast Source (front left + front right) with a Broadcast Code sending a distinct pattern per BIS, client = 2-BIS Broadcast Sink counting SDUs per channel and checking the pattern (broadcast source/sink) |
| 11 | broadcast_assistant | BASS control point: server = Broadcast Sink with the Scan Delegator (auto-sync off), client = Broadcast Assistant that discovers BASS, runs Remote Scan Started/Stopped, adds a source description and removes it again, confirming each step through the operation result and the receive-state notifications (scan delegator + assistant) |
| 12 | cap_handover | CAP unicast -> broadcast handover: server = CAP acceptor with a Unicast Server sink and a delegator Broadcast Sink, client = CAP Initiator that starts unicast, streams raw SDUs, then hands the session over to a broadcast on the same stream handle; the server must receive SDUs on both transports (`CONFIG_BT_CAP_HANDOVER`) |
| 13 | memory_release | heap reclaimed on release; reinit blocked while released |

## Requirements

- **Hardware**: Two boards with BLE support. The audio data-plane phases only
  truly run on a dual-audio `esp32s31 <-> esp32s31` bench; on any other pair
  they self-skip (the suite still runs green).
- **Wokwi/QEMU**: Not supported (multi-device test)
- **CI Runner**: `two_duts`
- **SoC Config**: `CONFIG_SOC_BLE_SUPPORTED=y`

## Serial Protocol

1. Both devices print `Device ready for name`
2. pytest generates a unique name and sends it to both devices
3. Each phase is gated by a `START_PHASE_<n>` handshake; the server acts as the
   audio source/acceptor and the client as the sink/controller
4. Both devices report a per-phase result line that the host asserts

## Notes

- Each phase brings up its own fresh audio stack (`BLE.end()`/`begin()`), so a
  failure in one phase does not leak state into the next.
- The audio_lifecycle phase (1) self-skips on builds without the LE Audio engine
  (`BLE_AUDIO_SUPPORTED == 0`), so the suite runs unchanged on non-audio silicon.
  It is a single-device (server) engine test; the client only acknowledges the phase.
- The iso_cis (2) and iso_bis (3) phases exercise the `BLEIso` transport across
  two DUTs and self-skip unless *both* DUTs compile host ISO in (`BLE_ISO_SUPPORTED`;
  iso_bis additionally needs BLE5). In the current target matrix that means they SKIP
  on every pair except a dual-ISO `esp32s31 <-> esp32s31` bench, where they verify
  CIS/BIS SDUs actually flow end-to-end.
- The bap_unicast (4) and bap_broadcast (5) phases exercise the BAP data plane across
  two DUTs and self-skip unless the server compiles the Unicast Server / Broadcast
  Source and the client the Unicast Client / Broadcast Sink. They SKIP on every pair except a dual-audio
  `esp32s31 <-> esp32s31` bench, where transparent SDUs flow over a real CIS
  (unicast) or BIS (broadcast).
- The lc3_loopback (6) phase exercises the turnkey LC3 data plane
  (`BLEAudioRecorder`/`BLEAudioPlayer`) across two DUTs and self-skips unless *both*
  DUTs compile the LC3 codec in (`BLE_AUDIO_LC3_SUPPORTED`, i.e. the `esp_audio_codec`
  managed component). It SKIPs on every pair except a dual-audio
  `esp32s31 <-> esp32s31` bench with the codec, where the client encodes a 1 kHz tone
  into LC3 over the CIS and the server decodes it and asserts the tone survived
  (decoded sample count + mean amplitude).
- The control_profiles (7) phase exercises the LE Audio control surface
  (CAP/VCP/MICP/MCP/CCP) and self-skips unless the server compiles the VCP renderer and
  CAP acceptor and the client the VCP controller and CAP initiator. The hard gate is the
  VCP round-trip (client discovers VCS + writes volume, server observes the write); CAP
  discovery is also asserted, and MICP/MCP/CCP are exercised and logged. The server's
  `calls` count is informational: the reference GTBS may reject a remote originate.
- The top_profiles (8) phase requires TMAP + GMAP compiled on both DUTs
  (`BLE_AUDIO_TMAP_SUPPORTED` + `BLE_AUDIO_GMAP_SUPPORTED`, plus
  `CONFIG_BT_GMAP_UGT_SUPPORTED` on the server). TMAP/TMAS and GMAP/GMAS are
  real identity round-trips: the client asserts CT|UMR and UGT with the UGT
  Sink feature. Media/call/game audio itself still flows through CAP + BAP +
  the control profiles exercised in earlier phases. The set-control part runs
  only when the client also compiles the CSIP coordinator, CAP commander and VCP
  controller; otherwise the phase checks the identity services alone.
- The hearing_aid (9) phase exercises the Hearing Access Service and self-skips unless
  the server compiles the HAS server (`BLE_AUDIO_HAS_SUPPORTED`) and the client compiles
  the HAS client (`BLE_AUDIO_HAS_CLIENT_SUPPORTED`).
- The stereo_broadcast (10) phase needs the broadcast source on the server
  (`BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED`) and the broadcast sink on the client
  (`BLE_AUDIO_BROADCAST_SINK_SUPPORTED`), plus two BIS in the packaged Kconfig.
- The broadcast_assistant (11) phase needs the scan delegator and broadcast sink on the
  server (`BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED`) and the assistant on the client
  (`BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED`). It asks for neither PA nor BIS sync,
  so it verifies the BASS control path without a third, broadcasting device.
- The cap_handover (12) phase needs CAP handover on the client
  (`BLE_AUDIO_CAP_HANDOVER_SUPPORTED`, i.e. `CONFIG_BT_CAP_HANDOVER`) and a CAP acceptor,
  the unicast server, the broadcast sink and the scan delegator on the server. The acceptor's broadcast sink
  does not scan on its own: it only syncs when the initiator adds the broadcast through
  BASS during the handover.
- Every connected audio phase opens its link with `BLEAudio::connect()` and waits for
  `onLinkReady()`, so the engine secures and discovers the link on both NimBLE and
  Bluedroid before any profile procedure starts.
