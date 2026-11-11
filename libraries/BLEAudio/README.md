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
  audio.begin();

  BLEAudioUnicastServer server = audio.createUnicastServer();
  server.configureMediaSink();

  audio.start();
  BLE.getAdvertising().start();
}
```

`BLE.getAudioController()` is declared in `<BLE.h>` so the core can build
without this library; include `<BLEAudio.h>` before using the returned handle.

## Examples

The examples are grouped by real, ESP32-sized products instead of by API role.
Pick the device you are building and use the matching counterpart for the
other end of the link. All of them use I2S for audio and are written for the
**ESP32-S31 Function CoreBoard 1**: the speaker examples drive its ES8311 DAC
and NS4150B amplifier, the microphone examples capture from its onboard
microphone through the ES8311 ADC, and the pin macros (`I2S_MCLK`, `I2S_SCLK`,
`I2S_LRCK`, `I2S_DOUT`, `I2S_DIN`, `PA_CTRL`) come from the board variant.
BLEAudio owns the I2S data path; the ES8311 register setup over I2C is
board-support code. The variant exposes `ES8311_I2C_ADDR`, `SDA1`, `SCL1`,
`I2S_*` and `PA_CTRL` for that layer.

| Example | Device | What it shows |
| --- | --- | --- |
| [`LEAudio_UnicastSpeaker`](examples/LEAudio_UnicastSpeaker) | Unicast speaker | CAP acceptor, stereo-capable unicast sink, VCP volume, LC3 player -> I2S DAC |
| [`LEAudio_WirelessMic`](examples/LEAudio_WirelessMic) | Wireless microphone | BAP unicast client, LC3 recorder from an I2S mic -> CIS |
| [`LEAudio_AuracastSource`](examples/LEAudio_AuracastSource) | Auracast transmitter | Broadcast source, Public Broadcast Announcement, LC3 recorder from an I2S input |
| [`LEAudio_AuracastSink`](examples/LEAudio_AuracastSink) | Auracast speaker | Broadcast sink, BASS Scan Delegator, LC3 player -> I2S DAC |

Typical pairs:

- `LEAudio_WirelessMic` -> `LEAudio_UnicastSpeaker`
- `LEAudio_AuracastSource` -> `LEAudio_AuracastSink`
- A phone/PC with LE Audio -> `LEAudio_UnicastSpeaker` (unicast)
- A phone/PC with Auracast -> `LEAudio_AuracastSink` (broadcast)

The examples require an LE Audio-capable target (ESP32-S31) and the LC3 codec
(`esp_audio_codec`). Advertising appearance values are named constants, e.g.
`BLE_APPEARANCE_AUDIO_SINK_STANDALONE_SPEAKER` and
`BLE_APPEARANCE_AUDIO_SOURCE_MICROPHONE`, so a sketch does not hard-code the
Assigned Numbers value.

## Stream topologies and `attach()`

`BLEAudioPlayer` and `BLEAudioRecorder` bind to `BLEAudioStream` handles, and
BAP can deliver one stereo program in two different shapes. Pick the `attach`
form that matches the role that created the streams:

**One stream carrying one or two channels**

```cpp
// One CIS/BIS, either mono or interleaved stereo.
player.attach(stream);
recorder.attach(stream);
```

Use this for a single ASE or a single BIS. `LEAudio_UnicastSpeaker` uses it
because `configureMediaSink()` publishes one sink ASE; `LEAudio_WirelessMic`
uses it because a microphone sends one mono stream.

**Two streams, each carrying one mono channel**

```cpp
// Left and right travel on separate CISes/BISes.
player.attach(left, right);
recorder.attach(left, right);
```

Use this for two mono ASEs/BISes, such as a stereo Auracast layout with two
BISes (front left + front right). `LEAudio_AuracastSink` and
`LEAudio_AuracastSource` use it when `STREAMS`/`CHANNELS` is 2.

The data path checks this at stream start: one stream may carry 1–2 channels;
two streams must both be mono. Any other combination logs `unsupported channel
layout` and does not start playback/capture.

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
- **Custom QoS and codec settings.** Every role accepts `setQos()` and a
  `BLEAudioCodecPreset`; `BLEAudioCodecConfig::fromPreset()` builds a custom
  configuration when a product needs a rate/duration outside the BAP table.
- **No I2S hardware.** `BLEAudioPlayer::setPCMSink()` and
  `BLEAudioRecorder::setPCMSource()` replace the I2S endpoints with callbacks,
  which is how the validation suite checks the LC3 path without a codec board.
- **Custom I2S transports.** The built-in I2S port is internal: it sizes one
  DMA buffer per LC3 frame, preloads the TX ring and clears on underrun so the
  codec task stays paced. To use `I2SClass`, TDM/PDM or a codec-specific
  driver, use `setPCMSink()`/`setPCMSource()` and drive that transport from the
  callback. If `ESP_I2S` later exposes DMA buffer sizing, TX preload and
  auto-clear, moving `BLEAudio` onto `I2SClass` would remove this private port;
  worth revisiting then.
- **Top-level identity only.** TMAP/GMAP publish roles; they move no audio.
  Use `audio.createTmap()` / `audio.createGmap()` and `discover()` to read a
  peer's roles.

## Relationship to `BLE`

- `<BLE.h>` is the core BLE API and works alone.
- `<BLEAudio.h>` adds LE Audio and depends on `BLE`.
- `BLE.end()` still tears down an active audio controller through a weak hook,
  so a sketch that forgets `audio.end()` is safe and a sketch without audio
  links no audio code.

The maintainer-facing architecture, per-role guards and spec obligations are in
[`../BLE/AUDIO.md`](../BLE/AUDIO.md).
