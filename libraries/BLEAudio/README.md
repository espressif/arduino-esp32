# BLEAudio -- LE Audio for ESP32 Arduino

`BLEAudio` is the LE Audio (Generic Audio Framework) library for the ESP32
Arduino core. It builds on the core [`BLE`](../BLE) library and adds:

- BAP unicast server/client, broadcast source/sink (Auracast) and the
  Broadcast Assistant
- CAP acceptor/initiator/commander and CSIP
- VCP, MICP, MCP (GMCS) and CCP (GTBS) control profiles
- TMAP, GMAP, HAS and the PBP announcement
- Turnkey LC3 playback/recording to I2S or a PCM callback

It is a **separate Arduino library on purpose**: a sketch that only includes
`<BLE.h>` does not compile the audio engine. Include `<BLEAudio.h>` when you
need audio, and the library is compiled in by the Arduino build system.

```cpp
#include <BLE.h>
#include <BLEAudio.h>

BLEAudio audio;

void setup() {
  BLE.begin("My speaker");
  audio = BLE.getAudioController();

  BLEAudioUnicastServer server = audio.createUnicastServer();
  if (!server) { /* factory failed: role disabled or engine down */ }
  else if (!server.configureMediaSink()) { /* invalid state */ }

  audio.begin();
  BLE.getAdvertising().start();
}
```

`BLE.getAudioController()` is declared in `<BLE.h>` so the core can build
without this library; include `<BLEAudio.h>` before using the returned handle.

## Object model for sketch authors

Three kinds of objects, three rules:

- **Controller** — `BLEAudio audio = BLE.getAudioController();` is a cheap
  shared handle to the single LE Audio engine, valid after `BLE.begin()`.
  There is no second `BLE`-style global on purpose (see `DESIGN.md`): one
  engine, one handle type, no static-init ordering to reason about.
- **Roles** — `audio.createUnicastServer()`, `createBroadcastSink()`,
  `createVolumeRenderer()`, … are the *only* way to a live role. A
  default-constructed role (`BLEAudioBroadcastSink sink;`) is an empty
  handle (`operator bool` is false) that exists so sketches can hold roles
  as globals and assign them in `setup()`; every method on it reports
  `InvalidState` until the factory fills it. Handles are copyable values —
  copies reference the same role.
- **Endpoints** — `BLEAudioPlayer` and `BLEAudioRecorder` are constructed
  directly like Arduino `Serial`, then `begin()` / `attach()` /
  `startPlayback()` / `startCapture()`. They own the local LC3 + I2S (or
  PCM-callback) data path, not a radio resource, so they need no factory.

## Examples

The examples are grouped by real, ESP32-sized products instead of by API role.
Pick the device you are building and use the matching counterpart for the
other end of the link. The I2S examples are written for the
**ESP32-S31 Function CoreBoard 1**: the speaker examples drive its ES8311 DAC
and NS4150B amplifier, the microphone examples capture from its onboard
microphone through the ES8311 ADC, and the pin macros (`I2S_MCLK`, `I2S_SCLK`,
`I2S_LRCK`, `I2S_DOUT`, `I2S_DIN`, `PA_CTRL`) come from the board variant. The
USB examples use a PCM callback instead and need no I2S hardware.
BLEAudio owns the I2S data path; the ES8311 register setup over I2C is
board-support code. The variant provides `es8311Begin()` and
`es8311SetVolume()` for that layer (plus the `ES8311_I2C_ADDR`, `SDA1`,
`SCL1`, `I2S_*` and `PA_CTRL` macros), callable with no extra include.

| Example | Device | What it shows |
| --- | --- | --- |
| [`UnicastSpeaker`](examples/UnicastSpeaker) | Unicast speaker | CAP acceptor, stereo-capable unicast sink, VCP volume, LC3 player -> I2S DAC |
| [`UnicastMic`](examples/UnicastMic) | Unicast microphone | BAP unicast client, LC3 recorder from an I2S mic -> CIS |
| [`AuracastSource`](examples/AuracastSource) | Auracast transmitter | Broadcast source, Public Broadcast Announcement, LC3 recorder from an I2S input |
| [`AuracastSink`](examples/AuracastSink) | Auracast speaker | Broadcast sink, BASS Scan Delegator, LC3 player -> I2S DAC |
| [`UnicastUSBReceiver`](examples/UnicastUSBReceiver) | USB microphone | Unicast sink like the speaker, but LC3 decodes to a PCM callback and the audio goes to the PC as a USB microphone (no I2S hardware needed) |
| [`UnicastUSBTransmitter`](examples/UnicastUSBTransmitter) | USB speaker | Unicast client like the wireless mic, but the LC3 recorder is fed from a USB speaker instead of an I2S mic (no I2S hardware needed) |

Typical pairs:

- `UnicastMic` -> `UnicastSpeaker`
- `AuracastSource` -> `AuracastSink`
- A phone/PC with LE Audio -> `UnicastSpeaker` (unicast)
- A phone/PC with Auracast -> `AuracastSink` (broadcast)
- A phone/PC with LE Audio -> `UnicastUSBReceiver` (unicast). The PC records or listens to the USB microphone
- A PC playing audio -> `UnicastUSBTransmitter` (USB speaker) -> `UnicastUSBReceiver` over LE Audio (unicast). All-USB end-to-end test with two boards and no I2S hardware (point the transmitter at `LE Audio Speaker` instead to use a DAC speaker)

The examples require an LE Audio-capable target (ESP32-S31) and the LC3 codec
(`esp_audio_codec`). Advertising appearance values are named constants, e.g.
`BLE_APPEARANCE_AUDIO_SINK_STANDALONE_SPEAKER` and
`BLE_APPEARANCE_AUDIO_SOURCE_MICROPHONE`, so a sketch does not hard-code the
Assigned Numbers value.

## Profiles and acronyms

LE Audio names are dense. This is every abbreviation a sketch meets, grouped
by the job it does. The per-role headers (`BLEAudioUnicastServer.h`, …) carry
the exact API; here is what the words mean.

**BAP — the audio data plane** (everything streams through this):

| Term | What it is |
| --- | --- |
| `PACS` | Published Audio Capabilities Service. What a device *can* do: sink/source PAC records (codec, rates, channels), supported/available audio *contexts* (`Media`, `Conversational`, …) and sink/source audio *locations* (`FrontLeft`, `FrontRight`, …). Read by the peer before configuring. |
| `ASCS` | Audio Stream Control Service. The control point of unicast: the client walks each **ASE** through Codec Configured → QoS Configured → Enabling → Streaming (and back via Disable/Release). |
| `ASE` | Audio Stream Endpoint: one configurable unicast endpoint. A sink ASE carries audio the client sends; a source ASE carries audio the client receives. Bound to one CIS when streaming. |
| `CIS` / `CIG` | Connected Isochronous Stream / Group. The actual over-the-air audio packets (SDUs) of a unicast link; a group keeps its streams (e.g. left + right) in sync. |
| `SDU` | Service Data Unit: one chunk of audio per interval (e.g. 10 ms of LC3), the unit the codec tasks produce/consume. |
| `QoS` | Per-stream transport contract: SDU interval, max SDU size (framed or not), retransmission number (RTN), max transport latency, PHY. `BLEAudioCodecPreset` bundles codec + low-latency QoS; `setQos()` overrides it. |
| Presentation delay | Microseconds between the SDU reference anchor and rendering. Every device of a group renders at the same instant because of it (`audio.setPresentationDelay()` default 40 ms). |

**Broadcast (Auracast)** — one transmitter, any number of receivers, no connection:

| Term | What it is |
| --- | --- |
| `BIS` / `BIG` | Broadcast Isochronous Stream / Group. Mono = one BIS; stereo = two BISes (front left + front right), each one `BLEAudioStream`. |
| Announcement | Extended-advertising payload receivers scan for: Broadcast Audio Announcement (the Broadcast ID) plus, for Auracast, the Public Broadcast Announcement (`PBP`) and the Broadcast Name. |
| `BASE` | Broadcast Audio Source Endpoint, carried in periodic advertising: codec, subgroups, BIS layout, streaming context, encryption flag. The sink reads it after PA sync to pick BISes. |
| Broadcast Code | Up to 16 characters that encrypt the BIG (zero-padded per spec; empty = open). Receivers need the same code; it can arrive preconfigured, from a Broadcast Assistant, or over PAST. |
| `BASS` / Scan Delegator | Broadcast Audio Scan Service on the sink: receive states the assistant adds, modifies, and removes remotely, including code distribution. |
| `PAST` | Periodic Advertising Sync Transfer: the assistant hands its PA sync to the sink so the sink skips scanning. |

**CAP / CSIP — coordination** (several devices acting as one):

| Term | What it is |
| --- | --- |
| `CAS` | Common Audio Profile Service, published by a CAP acceptor with an included CSIS. It is what makes a device drivable by a phone's CAP initiator. Optional: raw BAP links work without it. |
| `CSIS` | Coordinated Set Identification Service: the set identity of one member. |
| `SIRK` | Set Identity Resolving Key: the 16-octet secret shared by every member of a set. The library ships a fixed demo value — products must call `setSirk()` with their own. |
| `RSI` | Resolvable Set Identifier, derived from the SIRK and advertised, so a coordinator finds members without connecting first. |
| Size / Rank | `setSetSize()` = total devices in the set; `setRank()` = this device's 1-based position. A standalone speaker is a set of one. |
| Lock | A coordinator can lock a set for ordered access (`setLockable()`); the CAP commander uses it when every driven acceptor sits in one discovered set. |

**Control and top-level profiles** — no audio flows through these; they observe and drive it:

| Term | What it is |
| --- | --- |
| `VCP` (`VCS` + `VOCS` + `AICS`) | Volume Control: absolute volume 0–255 and mute (`VCS`), plus the first Volume Offset and Audio Input instances the controller can drive. |
| `MICP` (`MICS` + `AICS`) | Microphone mute, tri-state: unmuted / muted / disabled. |
| `MCP` (`MCS`/`GMCS`) | Media control: playback state, track info, play/pause/seek on the generic player. |
| `CCP` (`TBS`/`GTBS`) | Call control: incoming/dialing/active calls on the generic telephone bearer. |
| `TMAP` (`TMAS`) / `GMAP` (`GMAS`) | Telephony / Gaming role badges: "I am a headset" so peers pick compatible behavior. Identity only. |
| `HAS` | Hearing Access Service: named presets on a hearing aid, read and switched remotely. |

## Lifecycle: `audio.begin()` vs `startPlayback()` / `startCapture()`

LE Audio separates configuration from the transport, and the examples always use this order:

1. `BLE.begin()` — bring up the Bluetooth host.
2. Create and configure the roles (`createCapAcceptor()`,
   `createUnicastServer()`, `createBroadcastSink()`, …). The first factory
   initializes the engine lazily. Plain-GATT `BLEServer` services and audio
   roles can be staged in any order; a single `audio.begin()` publishes
   everything into one GATT table.
3. `audio.begin()` — register the roles and commit their GATT services. This
   does **not** open I2S or start an LC3 task.
4. Configure the PCM side **after** `audio.begin()`:
   - `player.begin(...)` then `player.attach(stream)` and
     `player.startPlayback()`;
   - `recorder.begin(...)` then `recorder.attach(stream)` and
     `recorder.startCapture()`.
5. Start the transport: advertising, `source.start()`, `sink.start()`, or
   connect to a peer. For unicast, the stream is configured later by the
   remote client, so `attach()`/`startPlayback()` also happen in the
   `onConfigured()` callback.

`audio.begin()` is the controller/GATT commit; `startPlayback()` and
`startCapture()` start the local LC3 codec tasks. They are deliberately
separate because the codec task is not needed until a stream exists.

## Stream topologies and `attach()`

`BLEAudioPlayer` is the speaker side: it plays **Rx streams** (audio this
device receives) through LC3 decode to I2S or a PCM sink. `BLEAudioRecorder`
is the microphone side: it encodes I2S or PCM-source audio into **Tx streams**
(audio this device sends), one SDU per SDU interval. The lifecycle is always
`begin()` (endpoint) → `attach()` (streams) → `startPlayback()` /
`startCapture()` (codec task, follows the streams; stopping keeps the
attachments, `end()` releases everything).

BAP can deliver one stereo program in two different shapes. Pick the `attach`
form that matches the role that created the streams:

**One stream carrying one or two channels**

```cpp
// One CIS/BIS, either mono or interleaved stereo.
player.attach(stream);
recorder.attach(stream);
```

Use this for a single ASE or a single BIS. `UnicastSpeaker` uses it
because `configureMediaSink()` publishes one sink ASE. `UnicastMic`
uses it because a microphone sends one mono stream.

**Two streams, each carrying one mono channel**

```cpp
// Left and right travel on separate CISes/BISes.
player.attach(left, right);
recorder.attach(left, right);
```

Use this for two mono ASEs/BISes, such as a stereo Auracast layout with two
BISes (front left + front right). `AuracastSink` and
`AuracastSource` use it when `CHANNELS` is 2.

The data path checks this at stream start: one stream may carry 1–2 channels;
two streams must both be mono and describe the same codec/QoS (rate, frame
duration, octets, frames per SDU, SDU interval). Any other combination logs
`unsupported channel layout` and does not start playback/capture.

## Advanced uses (documented, not separate examples)

The low-level roles remain public; the examples above combine them for common
products. These configurations are intentionally left to the API docs instead
of adding more sketches:

- **Raw BAP roles.** `BLEAudio::createUnicastServer()` /
  `createUnicastClient()` expose the PACS/ASCS data plane without CAP. Use them
  when you need full control over discovery and the ASCS procedure; see
  `BLEAudioUnicastServer.h` and `BLEAudioUnicastClient.h`.
- **Phone/PC media source.** A phone or PC is the usual unicast client: use
  `BLEAudioUnicastClient` for the data path and `BLEAudioCapInitiator` when it
  must start several acceptors as one set. The wireless microphone example
  shows the same client-side stream setup with a local recorder.
- **Broadcast Assistant (BASS client).** `audio.createBroadcastAssistant()`
  performs a remote scan, adds/removes a source on a sink, distributes the
  Broadcast Code and hands over PAST. See `BLEAudioBroadcastAssistant.h`.
- **Hearing Access Service (HAS).** `audio.createHearingAidDevice()` publishes
  named presets on top of a CAP acceptor; `createHearingAidController()` reads
  and switches them. See `BLEAudioHearingAid.h`.
- **CAP handover.** `BLEAudioCapInitiator::handoverToBroadcast()` and
  `handoverToUnicast()` move a running session between CIS and BIS without
  restarting the streams. The acceptor needs a unicast server plus a broadcast
  sink with a Scan Delegator; see `BLEAudioCap.h`.
- **Coordinated sets / multiple peers.** Call `addPeer()` for each earbud and
  `start()` once both are discovered (TMAP 2P_2CIS), or use a CAP initiator,
  which starts every discovered acceptor together. `setSetSize()` / `setRank()`
  on the acceptor publish the set identity.
- **Encrypted Auracast.** `setBroadcastCode()` on the source and sink, or
  `BLEAudioBroadcastAssistant::setBroadcastCode()` when a remote sink requests
  the code. Encrypted broadcasts also set the PBP encryption flag.
- **PAST.** The Broadcast Assistant hands a local periodic-advertising sync to
  the sink when the sink asks for SyncInfo; NimBLE and Bluedroid use different
  native paths and the library hides both.
- **Custom QoS and codec settings.** The roles that originate audio
  (`BLEAudioUnicastClient`, `BLEAudioBroadcastSource`, `BLEAudioCapInitiator`)
  accept `setQos()` and a `BLEAudioCodecPreset`; the unicast server and the
  broadcast sink instead publish a preset mask via `setSupportedPresets()`.
  `BLEAudioCodecConfig::fromPreset()` builds a custom configuration when a
  product needs a rate/duration outside the BAP table.
- **No I2S hardware.** `BLEAudioPlayer::setPCMSink()` and
  `BLEAudioRecorder::setPCMSource()` replace the I2S endpoints with callbacks,
  which is how the validation suite checks the LC3 path without a codec board.
- **Custom I2S transports.** The built-in I2S port is internal: it sizes one
  DMA buffer per LC3 frame, primes one TX buffer and clears on underrun so the
  codec task stays paced. To use `I2SClass`, TDM/PDM or a codec-specific
  driver, use `setPCMSink()`/`setPCMSource()` and drive that transport from the
  callback. If `ESP_I2S` later exposes DMA buffer sizing, TX preload and
  auto-clear, moving `BLEAudio` onto `I2SClass` would remove this private port;
  worth revisiting then.
- **Top-level identity only.** TMAP/GMAP publish roles; they move no audio.
  Use `audio.createTmap()` / `audio.createGmap()` and `discover()` to read a
  peer's roles.

## Errors and callbacks

Configuration methods return `BTStatus` synchronously: `InvalidParam` for a
bad value, `InvalidState` when the engine cannot initialize, before
`audio.begin()`, or while a role is running, and the stack status when a request cannot be queued. Where a
profile defines a completion callback (`onResult()`, `onError()`,
`onHandover()`, `onSyncFailed()`, ...), an asynchronous failure is reported
there.

The control-profile setters (`BLEAudioVolumeController`,
`BLEAudioMicController`, `BLEAudioMediaController`, `BLEAudioCallController`,
`BLEAudioHearingAidController`) return the status of the write request. A
queued write always terminates exactly once: success arrives as the next state
notification, a rejected write as `onResult()` (best-effort op attribution when
several writes share the notification channel). A pending write whose link drops
first is reported as `NotConnected`, so no operation waits forever.

Role callbacks run on the Bluetooth host task. They are copied under a lock
before invocation, so the application may set or clear them from another task,
but keep them short and do not block. `BLEAudioPlayer`/`BLEAudioRecorder` PCM
callbacks run on the codec task; a PCM sink is paced to the stream's real-time
frame rate.

## Relationship to `BLE`

- `<BLE.h>` is the core BLE API and works alone.
- `<BLEAudio.h>` adds LE Audio and depends on `BLE`.
- `BLE.end()` still tears down an active audio controller through a weak hook,
  so a sketch that forgets `audio.end()` is safe and a sketch without audio
  links no audio code.

The maintainer-facing architecture, per-role guards and spec obligations are in
[`DESIGN.md`](DESIGN.md) (complementing [`../BLE/DESIGN.md`](../BLE/DESIGN.md)).
