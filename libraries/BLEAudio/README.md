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

The examples are grouped by real use case instead of by API role. Pick the
device you are building and use the matching counterpart for the other end of
the link.

| Example | Device | What it shows |
| --- | --- | --- |
| [`LEAudio_Headset`](examples/LEAudio_Headset) | Headset / earbud | CAP acceptor, unicast sink + source, VCP, MICP, MCP, CCP, TMAP/GMAP, LC3 player + recorder |
| [`LEAudio_Speaker`](examples/LEAudio_Speaker) | Smart speaker | CAP acceptor, stereo-capable unicast sink, VCP, TMAP, LC3 player |
| [`LEAudio_Phone`](examples/LEAudio_Phone) | Phone / PC | CAP initiator, unicast client, VCP/MICP/MCP/CCP controllers, LC3 recorder from a PCM source |
| [`LEAudio_AuracastSource`](examples/LEAudio_AuracastSource) | Venue transmitter | Broadcast source, Public Broadcast Announcement, LC3 recorder from I2S microphones |
| [`LEAudio_AuracastSink`](examples/LEAudio_AuracastSink) | Portable receiver | Broadcast sink, BASS Scan Delegator, LC3 player |
| [`LEAudio_BroadcastAssistant`](examples/LEAudio_BroadcastAssistant) | Phone / remote | BASS client: remote scan, add/remove a source, Broadcast Code, PAST |
| [`LEAudio_HearingAid`](examples/LEAudio_HearingAid) | Hearing aid | HAS presets on top of a CAP acceptor and unicast server |

Typical pairs:

- `LEAudio_Phone` -> `LEAudio_Headset` or `LEAudio_Speaker`
- `LEAudio_AuracastSource` -> `LEAudio_AuracastSink`
- `LEAudio_BroadcastAssistant` -> `LEAudio_AuracastSink`

## Advanced uses (documented, not separate examples)

The low-level roles remain public; the examples above combine them for common
products. These configurations are intentionally left to the API docs instead
of adding more sketches:

- **Raw BAP roles.** `BLEAudio::createUnicastServer()` /
  `createUnicastClient()` expose the PACS/ASCS data plane without CAP. Use them
  when you need full control over discovery and the ASCS procedure; see
  `BLEAudioUnicastServer.h` and `BLEAudioUnicastClient.h`.
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
- **No I2S hardware.** `BLEAudioPlayer::setPcmSink()` and
  `BLEAudioRecorder::setPcmSource()` replace the I2S endpoints with callbacks,
  which is how the validation suite checks the LC3 path without a codec board.
- **Top-level identity only.** TMAP/GMAP publish roles; they move no audio.
  `LEAudio_Headset` shows the usual local roles, and `discover()` reads a
  peer's roles.

## Relationship to `BLE`

- `<BLE.h>` is the core BLE API and works alone.
- `<BLEAudio.h>` adds LE Audio and depends on `BLE`.
- `BLE.end()` still tears down an active audio controller through a weak hook,
  so a sketch that forgets `audio.end()` is safe and a sketch without audio
  links no audio code.

The maintainer-facing architecture, per-role guards and spec obligations are in
[`../BLE/AUDIO.md`](../BLE/AUDIO.md).
