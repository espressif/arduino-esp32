#########
BLE Audio
#########

About
-----

.. warning::

    The BLE Audio library is new and the API might not be stable yet. While
    breaking changes will be avoided where possible, they may still happen in
    future releases. Pin your core version if you need a frozen API.

The ``BLEAudio`` library adds Bluetooth LE Audio (the Generic Audio Framework: BAP, CAP,
and the control/identity profiles) to ESP32 Arduino. It builds on the core :doc:`ble`
library and is a **separate Arduino library on purpose**: a sketch that includes only
``<BLE.h>`` does not compile the audio engine. Include ``<BLEAudio.h>`` when you need audio.

What it provides:

* **BAP unicast** server/client (PACS/ASCS data plane) and **broadcast** source/sink
  (Auracast), plus the Broadcast Assistant (BASS client) and Scan Delegator
* **CAP** acceptor/initiator/commander, unicast-to-broadcast handover, and CSIP
  coordinated sets
* **Control profiles**: VCP (volume), MICP (microphone), MCP (media, GMCS) and
  CCP (calls, GTBS)
* **Identity profiles**: TMAP, GMAP, HAS (Hearing Access Service), PBP announcements
* Turnkey **LC3 playback/recording** to I2S or to a PCM callback (no I2S hardware needed)

The same sketch compiles for both BLE host stacks (**NimBLE** and **Bluedroid**) with no
``#ifdef`` guards; the stack-specific pieces are two small backend pairs selected
automatically.

Requirements
------------

* An LE Audio-capable target: **ESP32-S31** (``esp32s31``). Other targets compile the
  library out feature by feature and the roles self-skip.
* The LC3 codec (``esp_audio_codec``) for ``BLEAudioPlayer`` / ``BLEAudioRecorder``.
  Transport-only roles (unicast/broadcast setup, control profiles) work without it.
* A phone/PC with LE Audio (or a second ESP32-S31) as the peer. A BAP ``preset``
  (``BLEAudioCodecPreset``) selects codec + QoS; both ends must support it.

How LE Audio works in brief
---------------------------

Classic Bluetooth audio (A2DP) sends sound over a point-to-point BR/EDR link.
LE Audio instead runs **on top of BLE** and splits the job in two: a
*control plane* that configures everything, and a *transport* that carries
the sound.

* **Control plane (GATT).** Devices publish what they can do (codecs,
  channels, contexts) and a peer configures, starts, and stops streams with
  GATT procedures. All of this happens over a normal encrypted BLE
  connection (ACL).
* **Audio plane (isochronous channels, "ISO").** The sound itself does *not*
  travel over GATT. It travels in small time-sliced packets (SDUs) on
  **CIS** (Connected Isochronous Streams, one link, e.g. phone → earbuds) or
  **BIS** (Broadcast Isochronous Streams, one transmitter → any number of
  receivers in range, no connection). A **CIG**/**BIG** is just the group of
  streams sent together so left/right stay in sync. Every device renders at
  the same instant thanks to a shared **presentation delay**.

One radio program can therefore take two topologies: **unicast** (a call or
music for specific connected devices) or **broadcast** (one transmitter for
everyone in range — this is **Auracast**, e.g. a TV or venue stream that any
number of receivers, phones, or hearing aids can tune into by syncing to its
advertising).

The profiles are layers on top of that transport, each answering one question:

* **BAP** (Basic Audio Profile) — the foundation everything streams through:
  *what can you play/capture* is published as **PACS** (capabilities: codecs,
  contexts like ``Media``, locations like ``FrontLeft``), and each **ASE**
  (Audio Stream Endpoint: one configurable unicast endpoint, sink or source)
  is walked through **ASCS** (configure codec/QoS, enable, start).
* **CAP** (Common Audio Profile) — *coordinate several things as one*: an
  **acceptor** publishes **CAS** (with an included set-identity service) so a
  phone's **initiator** can start/stop streams on many devices at once, and a
  **commander** sends one volume/mute change to the whole set. Also moves a
  session between unicast and broadcast (**handover**). Raw BAP links work
  without it.
* **CSIP** — *these two earbuds are one set*: every member shares the secret
  **SIRK**, advertises a resolvable **RSI** so coordinators find it without
  connecting, and reports **size** (how many) and **rank** (which one I am);
  sets can be **locked** for ordered access.
* **Broadcast helpers** — **PBP** announces a public broadcast so phones can
  list it; the sink reads the **BASE** (codec, BIS layout, context,
  encryption flag) from periodic advertising to pick its streams; **BASS**
  (Scan Delegator) lets the sink be steered remotely ("receive *that*
  broadcast"); the **assistant** (often a phone) picks the source and hands
  over the **Broadcast Code** and sync info (**PAST**).
* **VCP / MICP** — *volume* (0–255 plus mute, with volume-offset and
  audio-input instances) and *microphone mute* (unmuted/muted/disabled),
  changed locally or by the peer.
* **MCP / CCP** — *media* (play/pause/track on the generic player) and
  *phone calls* (on the generic telephone bearer).
* **TMAP / GMAP** — *role badges* ("I am a headset" / "I am a gaming
  headset"), identity only, so peers pick compatible behavior.
* **HAS** — *hearing aids*: named presets the user switches between.

The **LC3 codec** (mandatory in LE Audio) compresses each channel into small
frames, so even low-power devices stream music-quality audio. Standard
**presets** (e.g. 16 kHz / 10 ms for voice, 48 kHz / 10 ms for music) bundle
codec settings with matching transport QoS — pick one and both ends agree.

Lifecycle: two starts
----------------------

LE Audio separates configuration from the transport, and sketches always use this order:

.. code-block:: cpp

    #include <BLE.h>
    #include <BLEAudio.h>

    void setup() {
        BLE.begin("My speaker");
        BLEAudio audio = BLE.getAudioController();

        BLEAudioUnicastServer server = audio.createUnicastServer();
        server.configureMediaSink();

        audio.begin();              // 1. controller/GATT commit
        // 2. PCM side, AFTER audio.begin():
        //    player.begin(...) -> player.attach(stream) -> player.startPlayback()
        BLE.getAdvertising().start();
    }

1. ``BLE.begin()`` -- bring up the Bluetooth host.
2. Create and configure roles (``createUnicastServer()``, ``createCapAcceptor()``,
   ``createBroadcastSink()``, ...). The first factory initializes the engine
   lazily. Factories return an empty handle when the engine cannot
   initialize or the role is disabled — check with ``if (server)``.
3. ``audio.begin()`` -- register the roles and commit their GATT services. This does
   **not** open I2S or start an LC3 task.
4. Configure the PCM side **after** ``audio.begin()``: ``player.begin(...)`` then
   ``player.attach(stream)`` and ``player.startPlayback()`` (or ``recorder.begin(...)``,
   ``recorder.attach(stream)`` and ``recorder.startCapture()``).
5. Start the transport: advertising, ``source.start()``, ``sink.start()``, or connect
   to a peer. For unicast, the stream is configured later by the remote client, so
   ``attach()``/``startPlayback()`` also happen in the stream's ``onConfigured()``
   callback.

``audio.begin()`` is the controller/GATT commit; ``startPlayback()``/``startCapture()``
start the local LC3 codec tasks. They are deliberately separate because the codec task
is not needed until a stream exists. ``audio.end()`` releases everything; calling it
from a stack callback is refused with ``Busy`` — defer it to ``loop()``.

GATT coexistence: audio profiles and a plain-GATT ``BLEServer`` share **one** committed GATT
table (``audio.begin()`` performs the single coordinated commit). Staging order does
not matter: services staged before the first audio role exists are adopted into the
same commit — afterwards the table is frozen and ``server.start()`` reports
``InvalidState``.

Controller and roles
--------------------

``BLE.getAudioController()`` is declared in ``<BLE.h>`` so the core builds without the
audio library; include ``<BLEAudio.h>`` before using the returned handle.

.. code-block:: cpp

    BLEAudio audio = BLE.getAudioController();
    auto server = audio.createUnicastServer();  // create and configure roles
    auto client = audio.createUnicastClient();
    auto source = audio.createBroadcastSource();
    auto sink   = audio.createBroadcastSink();
    auto asst   = audio.createBroadcastAssistant();
    auto accept = audio.createCapAcceptor();
    auto init   = audio.createCapInitiator();
    auto cmdr   = audio.createCapCommander();

Role handles are cheap shared references; copying aliases the same role. A
default-constructed or failed handle is empty: calls on it do nothing, and calls
returning ``BTStatus`` report ``InvalidState``.

BAP unicast
-----------

The unicast client drives the full discover → Config Codec → Config QoS → Enable →
CIS connect → Start procedure on one or two servers; the server publishes PACS/ASCS
and renders or captures. ``BLEAudioCodecPreset`` selects the codec/QoS (e.g.
``LC3_16_2_1``, ``LC3_48_2_1``); ``setQos()`` replaces the preset QoS with a custom one.
``BLEAudioCodecConfig::fromPreset()`` builds a custom configuration for rates outside
the BAP table (44.1 kHz presets use framed SDUs).

Server side (headset/speaker):

.. code-block:: cpp

    BLEAudioUnicastServer us = audio.createUnicastServer();
    us.setSinkStreams(1);
    us.setSourceStreams(0);
    us.setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);
    // us.setSupportedPresets(...) to narrow the published PACS (default: 16/24/32/48 kHz)

Client side (phone/dongle):

.. code-block:: cpp

    BLEAudioUnicastClient uc = audio.createUnicastClient();
    uc.setPreset(BLEAudioCodecPreset::LC3_16_2_1);
    uc.setSinkStreams(1);
    uc.onDiscovered([&](const BLEAudioUnicastPeerInfo &peer) {
        // peer.sinkPresets: mask of presets the server accepts
    });
    uc.onStarted([]() { /* every stream is streaming */ });
    uc.connectAndStart(connHandle);   // discover, then configure + start

``BLEAudioStream`` handles carry the media: ``uc.stream(Direction::Tx)`` is written by
a recorder (or raw SDUs), the server's ``us.stream(0)`` is read by a player. A second
``discover()`` replaces the peer; ``addPeer()`` + ``start()`` drives two peers
(e.g. left + right earbuds).

BAP broadcast (Auracast)
------------------------

The source advertises (extended + periodic + BASE) and streams over a BIG; sinks
scan, PA-sync, and BIG-sync. One stream carries one or two channels; two mono
streams travel on separate BISes.

.. code-block:: cpp

    BLEAudioBroadcastSource src = audio.createBroadcastSource();
    src.setPreset(BLEAudioCodecPreset::LC3_48_4_1);
    src.setChannels(2);
    src.setName("Stage left");
    // src.setBroadcastCode("...") for encrypted broadcasts
    src.start();   // advertising + BIG come up; onStarted() follows

    BLEAudioBroadcastSink sink = audio.createBroadcastSink();
    sink.start();  // scan -> PA sync -> BIG sync; onSynced() follows

Public Broadcast Announcements (PBP) describe the broadcast to scanners; the
**Broadcast Assistant** (BASS client) performs remote scans, adds/removes sources on a
sink, distributes the Broadcast Code and hands over PAST sync info, all through
``onResult()`` plus receive-state notifications.

CAP: sets, commander, handover
------------------------------

* **Acceptor** (headset): ``createCapAcceptor()`` with ``setSetSize()``/``setRank()``
  publishes CAS + CSIS identity.
* **Initiator** (phone): ``createCapInitiator()`` discovers acceptors, ``startUnicast()``
  brings up every discovered acceptor as one set, ``startBroadcast()`` transmits.
* **Commander**: discovers an acceptor (CAS + VCP) and drives ``setVolume()`` and the
  other receivers procedures; every procedure ends exactly once in ``onResult()``.
* **Handover** (``CONFIG_BT_CAP_HANDOVER``): ``initiator.handoverToBroadcast()`` moves a
  running unicast session to a broadcast on the same stream handles (sink streams keep
  carrying audio as BIS); ``handoverToUnicast()`` moves it back. A failed handover also
  stops unicast so ``startUnicast()`` can rebuild it.
* **Coordinated sets**: the CSIP coordinator reads the set (``onDiscovered`` reports the
  set size), then ``lock()``/``unlock()`` serialize access across members.

Control profiles: VCP / MICP / MCP / CCP
----------------------------------------

Each pair is a renderer/device (server side, exposes the service) plus a controller
(client side, drives one peer at a time):

.. code-block:: cpp

    // Server: expose volume + microphone + media + call surface
    BLEAudioVolumeRenderer vol = audio.createVolumeRenderer();
    vol.setInitialVolume(100);
    BLEAudioMicDevice mic = audio.createMicDevice();
    BLEAudioMediaPlayer media = audio.createMediaPlayer();
    BLEAudioCallServer call = audio.createCallServer();
    call.setProviderName("Phone");

    // Client ("phone"): discover once, then drive
    BLEAudioVolumeController vc = audio.createVolumeController();
    vc.discover(connHandle);
    vc.setVolume(200);   // OK = write queued; success arrives as onStateChanged
    BLEAudioMicController mc = audio.createMicController();
    mc.discover(connHandle);
    mc.mute();           // success arrives as onMuteChanged

Every queued write terminates **exactly once**: success arrives as the next state
notification (``onStateChanged`` / ``onMuteChanged`` / command result), a rejected write
as ``onResult()`` (best-effort op attribution when several writes share the channel).
A pending write whose link drops first is reported as ``NotConnected`` instead of hanging.
Commands issued before ``discover()`` return ``InvalidState``; cached state clears on
disconnect. Only the first VOCS/AICS instance of a peer is driven.

Only the generic instances are exposed: MCP drives the GMCS media player, CCP the GTBS
telephone bearer. Use ``call.originate("tel:...")`` to place a call; the server answers
through ``onOriginate()``. Discrete TBS/MCS bearers need a real phone and are not covered.

Identity profiles: TMAP / GMAP / HAS
------------------------------------

TMAP/GMAP publish role/flag bits and move no audio: ``createTmap()`` /
``createGmap()`` on the publisher, ``discover()`` + ``onDiscovered()`` on the reader.
The Hearing Access Service publishes named presets on top of a CAP acceptor
(``createHearingAidDevice()``: ``addPreset()``/``setActivePreset()``); the controller
(``createHearingAidController()``) reads the preset list and switches the active preset.

LC3 player, recorder, and stream topologies
-------------------------------------------

``BLEAudioPlayer`` is the speaker side: it takes **Rx streams** (audio this
device receives), LC3-decodes them with packet-loss concealment, and plays the
PCM on an I2S DAC (or a PCM sink callback). ``BLEAudioRecorder`` is the
microphone side: it takes PCM from an I2S ADC/mic (or a PCM source callback),
LC3-encodes it, and writes one SDU per SDU interval into **Tx streams** (audio
this device sends). Attaching the wrong direction never plays or captures —
pick streams with ``stream(BLEAudioStream::Direction::Rx)`` for the player and
``Direction::Tx`` for the recorder.

Lifecycle, always in this order:

1. ``begin(...)`` opens the endpoint (I2S pins, or no pins after
   ``setPCMSink()`` / ``setPCMSource()``). ``attach()`` and ``start`` return
   ``InvalidState`` before it.
2. ``attach(stream)`` binds the streams. For unicast this happens later, when
   the peer configures them (``onConfigured()``); for broadcast the streams
   exist as soon as the channel count is set.
3. ``startPlayback()`` / ``startCapture()`` spawn the codec task. Actual
   playback/capture follows the streams: it runs while they stream and pauses
   when they stop. ``stopPlayback()`` / ``stopCapture()`` stop the task but
   keep the attachments, so starting again resumes; ``end()`` releases
   everything.

BAP can deliver one stereo program in two shapes — pick the ``attach()`` form that
matches the role that created the streams:

* **One stream, 1–2 channels** (one ASE or one BIS; interleaved stereo):
  ``player.attach(stream)``.
* **Two mono streams** (two ASEs/BISes, e.g. stereo Auracast front left + front right):
  ``player.attach(left, right)`` — both must be mono with identical codec/QoS.

Any other combination logs ``unsupported channel layout`` and does not start.
Statistics (reset by each start) are the API-visible health signals:

* **Player**: ``sdusReceived()`` (arrived, any status), ``sdusDropped()`` (jitter
  buffer full), ``sdusLost()`` (flagged lost/invalid or wrong length),
  ``plcFrames()`` (concealed frames), ``underruns()`` (nothing queued for a
  streaming interval), ``lateResyncs()`` (PCM-clock restarts, no data lost).
* **Recorder**: ``sdusSent()`` (accepted by the stack, per stream),
  ``sendErrors()`` (encode/stack refusals), ``txStalls()`` (controller
  stopped releasing SDUs; resyncs instead of spinning silently).

The player sizes its jitter buffer from the negotiated QoS (presentation
delay / SDU interval, at least 2 SDUs; ``setJitterDepth()`` overrides it up
to 16). With no I2S hardware, ``player.setPCMSink()`` /
``recorder.setPCMSource()`` replace the endpoints with callbacks paced to the
stream's real-time frame rate.

Errors and callbacks
--------------------

Configuration methods return ``BTStatus`` synchronously: ``InvalidParam`` for a bad value,
``InvalidState`` when the engine cannot initialize, before ``audio.begin()`` or while a
role is running, and
the stack status when a request cannot be queued. Where a profile defines a completion
callback (``onResult()``, ``onError()``, ``onHandover()``, ``onSyncFailed()``, ...), an
asynchronous failure is reported there.

Role callbacks run on the Bluetooth host task. They are copied under a lock before
invocation, so the application may set or clear them from another task — but keep them
short and never block. ``BLEAudioPlayer``/``BLEAudioRecorder`` PCM callbacks run on the
codec task instead, paced to the stream frame rate.

Examples
--------

The ``BLEAudio`` examples are grouped by product, not by API role. The I2S examples
are written for the ESP32-S31 Function CoreBoard 1 (ES8311 DAC/ADC + NS4150B
amplifier), while the USB examples need no I2S hardware:

* ``UnicastSpeaker`` — CAP acceptor, stereo-capable unicast sink, VCP volume,
  LC3 player to I2S DAC.
* ``UnicastMic`` — BAP unicast client, LC3 recorder from an I2S mic to CIS.
* ``AuracastSource`` — broadcast source, Public Broadcast Announcement, LC3
  recorder from an I2S input.
* ``AuracastSink`` — broadcast sink, BASS Scan Delegator, LC3 player to I2S DAC.
* ``UnicastUSBReceiver`` — unicast sink like the speaker, but LC3 decodes to a
  PCM callback and the audio reaches the PC as a USB microphone (no I2S).
* ``UnicastUSBTransmitter`` — unicast client like the wireless mic, but the LC3
  recorder is fed from a USB speaker instead of an I2S mic (no I2S).

Typical pairs: ``UnicastMic`` → ``UnicastSpeaker``, ``AuracastSource`` →
``AuracastSink``, a phone/PC with LE Audio ↔ ``UnicastSpeaker`` or
``UnicastUSBReceiver``, a phone/PC with Auracast ↔ ``AuracastSink``, and
``UnicastUSBTransmitter`` → ``UnicastUSBReceiver`` for an all-USB end-to-end
test (two boards, no I2S hardware).

Advanced uses (raw BAP roles, phone-side client + initiator, HAS, encrypted broadcasts,
custom QoS, no-I2S operation) are covered by the ``Advanced uses`` section of the
library README and the per-role headers.

Relationship to BLE
-------------------

* ``<BLE.h>`` is the core BLE API and works alone.
* ``<BLEAudio.h>`` adds LE Audio and depends on ``BLE``.
* ``BLE.end()`` still tears down an active audio controller through a weak hook, so a
  sketch that forgets ``audio.end()`` is safe and a sketch without audio links no audio
  code.

The maintainer-facing architecture, per-role guards and spec obligations live in the
library ``DESIGN.md`` (complementing the core ``BLE/DESIGN.md`` service-staging rules).

API Reference
-------------

All role handles are cheap shared references (copying aliases the same role).
A default-constructed or failed-factory handle is empty: calls on it do nothing,
and calls returning ``BTStatus`` report ``InvalidState``. Every ``on*`` callback
runs on the Bluetooth host task -- keep it short, never block, and never call
``audio.end()`` from it (defer to ``loop()``). ``BLEAudioPlayer`` /
``BLEAudioRecorder`` PCM callbacks run on the codec task instead.

The whole API is reached through the ``<BLEAudio.h>`` umbrella. Everything below
is guarded by ``BLE_AUDIO_SUPPORTED`` (roles additionally by their
``BLE_AUDIO_*_SUPPORTED`` build flag); ``BLEAudioPlayer`` / ``BLEAudioRecorder``
need ``BLE_AUDIO_LC3_SUPPORTED`` (the ``esp_audio_codec`` component).

BLEAudio (controller)
*********************

Minted from ``BLE.getAudioController()`` after ``BLE.begin()``. Owns the engine,
the event bridge and every role created through it. Create and configure roles
before ``begin()``; the broadcast source (no GATT service) and the broadcast
assistant additionally accept creation after the commit.

.. code-block:: cpp

    #include <BLE.h>
    #include <BLEAudio.h>

    BLEAudio audio = BLE.getAudioController();
    explicit operator bool() const;
    bool isActive() const;    // engine up, end() not called
    bool isCommitted() const; // begin() committed the GATT table

``isStarted()`` is kept as a deprecated alias for ``isCommitted()``.

Lifecycle:

.. code-block:: cpp

    BTStatus begin();
    BTStatus end();
    BTStatus setAutoSecurityPolicy(bool enable);
    bool autoSecurityPolicy() const;

``begin()`` initializes the engine, registers every staged role, merges the PACS
records and performs the single coordinated GATT commit (audio plus any staged
plain ``BLEServer`` services). Calling it again after success is a no-op. When
the sketch did not configure ``BLESecurity``, ``begin()`` applies the default
audio policy (Just Works, NoInputNoOutput, bonding, Secure Connections, no
MITM). ``end()`` stops leftover transports, waits for ISO idle and releases
everything; refused with ``Busy`` from a stack callback, or with
``Fail``/``InvalidState`` while a connected ISO channel is still up.

Link management (use instead of ``BLEClient::connect()`` for audio peers; the
link is opened through the engine, encrypted, MTU-exchanged and discovered):

.. code-block:: cpp

    using LinkCallback = std::function<void(uint16_t connHandle)>;
    BTStatus connect(const BTAddress &address);
    BTStatus disconnect(uint16_t connHandle);
    void onLinkReady(LinkCallback cb);
    void onDisconnected(LinkCallback cb);
    void resetCallbacks();

Defaults (before creating roles):

.. code-block:: cpp

    BTStatus setPresentationDelay(uint32_t delayUs);  // default 40000
    uint32_t getPresentationDelay() const;

Role factories (each returns an empty handle when the engine cannot initialize,
when called after the commit -- except broadcast source / assistant -- or when
the role is disabled in Kconfig):

.. code-block:: cpp

    BLEAudioUnicastServer createUnicastServer();
    BLEAudioUnicastClient createUnicastClient();
    BLEAudioBroadcastSource createBroadcastSource();
    BLEAudioBroadcastSink createBroadcastSink();
    BLEAudioBroadcastAssistant createBroadcastAssistant();
    BLEAudioCapAcceptor createCapAcceptor();
    BLEAudioCapInitiator createCapInitiator();
    BLEAudioCapCommander createCapCommander();
    BLEAudioCoordinatedSetMember createCoordinatedSetMember();
    BLEAudioCoordinatedSetCoordinator createCoordinatedSetCoordinator();
    BLEAudioVolumeRenderer createVolumeRenderer();
    BLEAudioVolumeController createVolumeController();
    BLEAudioMicDevice createMicDevice();
    BLEAudioMicController createMicController();
    BLEAudioMediaPlayer createMediaPlayer();
    BLEAudioMediaController createMediaController();
    BLEAudioCallServer createCallServer();
    BLEAudioCallController createCallController();
    BLEAudioHearingAidDevice createHearingAidDevice();
    BLEAudioHearingAidController createHearingAidController();
    BLEAudioTmap createTmap();
    BLEAudioGmap createGmap();

Value types: contexts, locations, presets, codec, QoS
********************************************************

.. code-block:: cpp

    enum class BLEAudioContext : uint16_t {
        Prohibited = 0x0000, Unspecified = 0x0001, Conversational = 0x0002,
        Media = 0x0004, Game = 0x0008, Instructional = 0x0010,
        VoiceAssistants = 0x0020, Live = 0x0040, SoundEffects = 0x0080,
        Notifications = 0x0100, Ringtone = 0x0200, Alerts = 0x0400,
        EmergencyAlarm = 0x0800,
    };
    inline constexpr BLEAudioContext operator|(BLEAudioContext a, BLEAudioContext b);
    inline BLEAudioContext &operator|=(BLEAudioContext &a, BLEAudioContext b);

    enum class BLEAudioLocation : uint32_t {
        Mono = 0x00000000, FrontLeft = 0x00000001, FrontRight = 0x00000002,
        FrontCenter = 0x00000004, LowFreqEffects1 = 0x00000008,
        BackLeft = 0x00000010, BackRight = 0x00000020,
        SideLeft = 0x00000400, SideRight = 0x00000800,
    };
    inline constexpr BLEAudioLocation operator|(BLEAudioLocation a, BLEAudioLocation b);
    inline BLEAudioLocation &operator|=(BLEAudioLocation &a, BLEAudioLocation b);

Contexts describe what a stream carries (PACS supported/available, Enable,
BASE metadata). Locations are one channel per set bit (``Mono`` = single
channel, no position). Combine with ``|``.

.. code-block:: cpp

    enum class BLEAudioCodecPreset : uint8_t {
        LC3_8_1_1, LC3_8_2_1, LC3_16_1_1, LC3_16_2_1,
        LC3_24_1_1, LC3_24_2_1, LC3_32_1_1, LC3_32_2_1,
        LC3_441_1_1, LC3_441_2_1, LC3_48_1_1, LC3_48_2_1,
        LC3_48_3_1, LC3_48_4_1, LC3_48_5_1, LC3_48_6_1,
    };
    inline constexpr uint32_t BLEAudioPresetBit(BLEAudioCodecPreset p);
    inline constexpr uint32_t operator|(BLEAudioCodecPreset a, BLEAudioCodecPreset b);
    inline constexpr uint32_t operator|(uint32_t a, BLEAudioCodecPreset b);
    inline constexpr uint32_t operator|(BLEAudioCodecPreset a, uint32_t b);
    inline constexpr uint32_t operator&(uint32_t a, BLEAudioCodecPreset b);
    inline constexpr uint32_t operator&(BLEAudioCodecPreset a, uint32_t b);

Presets bundle codec settings with matching low-latency QoS (BAP Tables 5.2 /
6.4). Roles take a mask built with ``|`` (e.g.
``LC3_16_2_1 | LC3_48_2_1``); test membership with ``&``
(e.g. ``(peer.sinkPresets & LC3_48_2_1)``).

.. code-block:: cpp

    struct BLEAudioCodecConfig {
        uint32_t samplingRateHz = 16000;
        uint16_t frameDurationUs = 10000;   // 7500 or 10000
        uint16_t octetsPerFrame = 40;
        uint8_t framesPerSdu = 1;
        BLEAudioLocation channelAllocation = BLEAudioLocation::Mono;
        static BLEAudioCodecConfig fromPreset(BLEAudioCodecPreset preset);
        uint8_t channels() const;        // set bits, 1 for Mono
        uint16_t samplesPerFrame() const;
        uint16_t sduOctets() const;      // octetsPerFrame * channels * framesPerSdu
    };

    struct BLEAudioQos {
        uint32_t sduIntervalUs = 10000;
        uint16_t maxSdu = 40;
        uint8_t retransmissions = 2;
        uint16_t maxTransportLatencyMs = 10;
        uint32_t presentationDelayUs = 40000;
        uint8_t phy = 0x02;              // 0x01 1M, 0x02 2M, 0x04 Coded
        bool framed = false;            // required by the 44.1 kHz presets
        static BLEAudioQos fromPreset(BLEAudioCodecPreset preset, bool broadcast = false);
    };

    struct BLEAudioSduInfo {
        enum class Status : uint8_t { Valid = 0, Invalid = 1, Lost = 2 };
        uint32_t timestamp = 0;
        uint16_t seq = 0;
        Status status = Status::Valid;
        bool timestampValid = false;
    };

``BLEAudioSduInfo`` is the per-SDU metadata of ``BLEAudioStream::onReceive``.
``Lost`` means nothing arrived for that interval and the payload is empty.

BLEAudioStream
**************

One unidirectional stream: a unicast ASE on a CIS, or one BIS of a broadcast
group. Roles own the streams and expose them via ``stream()``. Direction is the
local view: ``Tx`` = this device sends, ``Rx`` = it receives. Payloads pass
through unchanged (no LC3); the player/recorder layer the codec on top.

.. code-block:: cpp

    BLEAudioStream();                            // empty handle
    explicit operator bool() const;
    bool operator==(const BLEAudioStream &other) const;
    bool operator!=(const BLEAudioStream &other) const;

    enum class Direction : uint8_t { Unknown, Tx, Rx };

    Direction direction() const;      // Unknown until a unicast server stream is bound
    bool isStreaming() const;
    uint16_t connHandle() const;      // ACL for unicast, 0xFFFF for broadcast/unbound
    BLEAudioCodecConfig codecConfig() const;
    BLEAudioQos qos() const;

    BTStatus write(const uint8_t *sdu, uint16_t len);

Call ``write()`` once per SDU interval with ``codecConfig().sduOctets()`` bytes
on a streaming ``Tx`` stream; the sequence number is managed internally.

.. code-block:: cpp

    using Callback = std::function<void(BLEAudioStream &stream)>;
    using StoppedCallback = std::function<void(BLEAudioStream &stream, uint8_t reason)>;
    using ReceiveCallback = std::function<void(BLEAudioStream &stream, const BLEAudioSduInfo &info,
                                               const uint8_t *sdu, uint16_t len)>;
    void onConfigured(Callback cb);   // codec configured (peer / create / sync)
    void onStarted(Callback cb);
    void onStopped(StoppedCallback cb);
    void onReceive(ReceiveCallback cb);  // payload valid only during the call
    void onSent(Callback cb);            // controller released one queued Tx SDU
    void resetCallbacks();               // leaves the player/recorder tap untouched

BAP unicast
***********

BLEAudioUnicastServer
^^^^^^^^^^^^^^^^^^^^^

Publishes PACS + ASCS (headset/speaker). Configure ASE counts, capabilities,
contexts and locations before ``audio.begin()``; the peer then configures the
streams (see ``BLEAudioStream::onConfigured``).

.. code-block:: cpp

    BLEAudioUnicastServer();                   // empty; via audio.createUnicastServer()
    explicit operator bool() const;

    // Configuration (before audio.begin()):
    BTStatus setSinkStreams(uint8_t count);       // ASEs the client sends to, default 1
    BTStatus setSourceStreams(uint8_t count);     // ASEs the client receives, default 1
    BTStatus setSupportedPresets(uint32_t presetMask);  // default 10 ms @16/24/32/48 kHz
    BTStatus setMaxChannelsPerStream(uint8_t channels);  // 1-2, default 1
    BTStatus setSinkContexts(BLEAudioContext contexts);
    BTStatus setSourceContexts(BLEAudioContext contexts);
    BTStatus setSinkLocation(BLEAudioLocation location);    // default Mono
    BTStatus setSourceLocation(BLEAudioLocation location);
    BTStatus setPresentationDelayRange(uint32_t minUs, uint32_t maxUs);
    BTStatus configureMediaSink();  // 1 sink / 0 source, stereo, Media|Conversational
    BTStatus configureHeadset();    // 1 sink + 1 source, stereo sink, mono source

    // Runtime:
    BTStatus setAvailableContexts(BLEAudioContext sink, BLEAudioContext source);
    size_t streamCount() const;
    BLEAudioStream stream(size_t index) const;  // sink-first allocation order
    BLEAudioStream stream(BLEAudioStream::Direction dir, uint8_t index = 0) const;

BLEAudioUnicastClient
^^^^^^^^^^^^^^^^^^^^^

Discovers PACS + ASCS, then walks Config Codec -> Config QoS -> Enable -> CIS
connect -> Start Ready on one server (``connectAndStart()``) or two
(``addPeer()`` each, then ``start()``). Per-peer streams are sink-first, then
source; ``Direction::Tx`` = local sends.

.. code-block:: cpp

    struct BLEAudioUnicastPeerInfo {
        uint16_t connHandle = 0xFFFF;
        uint8_t sinkEndpoints = 0;      // remote sink ASEs (= local can send)
        uint8_t sourceEndpoints = 0;
        BLEAudioLocation sinkLocation = BLEAudioLocation::Mono;
        BLEAudioLocation sourceLocation = BLEAudioLocation::Mono;
        BLEAudioContext sinkContexts = BLEAudioContext::Prohibited;
        BLEAudioContext sourceContexts = BLEAudioContext::Prohibited;
        uint32_t sinkPresets = 0;       // BLEAudioPresetBit() mask
        uint32_t sourcePresets = 0;
    };

    BLEAudioUnicastClient();                   // empty; via audio.createUnicastClient()
    explicit operator bool() const;

    // Configuration (read at start(), change while stopped):
    BTStatus setPreset(BLEAudioCodecPreset preset);  // default LC3_16_2_1
    BTStatus setQos(const BLEAudioQos &qos);
    BTStatus setContext(BLEAudioContext context);    // default Media
    BTStatus setSinkStreams(uint8_t perPeer);        // default 1
    BTStatus setSourceStreams(uint8_t perPeer);      // default 0

    // Procedures (after audio.begin()):
    BTStatus addPeer(uint16_t connHandle);
    BTStatus start();
    BTStatus connectAndStart(uint16_t connHandle);  // addPeer() + auto-start
    BTStatus stop();
    bool isStreaming() const;

    size_t streamCount() const;
    BLEAudioStream stream(size_t index) const;
    BLEAudioStream stream(BLEAudioStream::Direction dir, uint8_t index = 0) const;

    enum class Step : uint8_t { Config = 1, Qos, Enable, Start, Connect, Group };
    using DiscoveredCallback = std::function<void(const BLEAudioUnicastPeerInfo &peer)>;
    using Callback = std::function<void()>;
    using ErrorCallback = std::function<void(Step step, uint8_t responseCode, uint8_t reason)>;
    void onDiscovered(DiscoveredCallback cb);
    void onStarted(Callback cb);   // every stream streaming
    void onStopped(Callback cb);
    void onError(ErrorCallback cb);
    void resetCallbacks();

BAP broadcast (Auracast)
************************

BLEAudioBroadcastSource
^^^^^^^^^^^^^^^^^^^^^^^

Transmitter: one BIG, one subgroup, one BIS per channel (mono = 1 Tx stream,
stereo = front-left + front-right). ``start()`` owns one extended advertising
instance (announcement + name + PBA) plus periodic advertising (BASE) and the
BIG. Feed each stream one SDU per interval after ``onStarted()``, or attach a
``BLEAudioRecorder``.

.. code-block:: cpp

    BLEAudioBroadcastSource();                 // empty; via audio.createBroadcastSource()
    explicit operator bool() const;

    // Configuration (staged; codec/QoS/channels/code/context mark the source dirty):
    BTStatus setPreset(BLEAudioCodecPreset preset);  // default LC3_16_2_1
    BTStatus setQos(const BLEAudioQos &qos);
    BTStatus setChannels(uint8_t channels);           // 1 mono, 2 stereo
    BTStatus setBroadcastId(uint32_t broadcastId);   // 24-bit, default random
    BTStatus setBroadcastCode(const String &code);    // empty = unencrypted
    BTStatus setName(const String &name);             // trunc 32, default device name
    BTStatus setContext(BLEAudioContext context);     // default Media
    BTStatus setPublicBroadcast(bool enable);         // PBP builds only, default on
    BTStatus configureAuracast(const String &name, uint8_t channels = 2,
                                               BLEAudioCodecPreset preset = BLEAudioCodecPreset::LC3_48_2_1);

    // Control (after audio.begin()):
    BTStatus start(uint8_t advInstance = 0);
    BTStatus stop();              // blocks until BIG down; Busy from a stack callback
    bool isStreaming() const;
    BTStatus updateContext(BLEAudioContext context);
    uint32_t getBroadcastId() const;

    size_t streamCount() const;
    BLEAudioStream stream(size_t index) const;  // 0 = mono/front-left, 1 = front-right

    using Callback = std::function<void()>;
    using StoppedCallback = std::function<void(uint8_t reason)>;
    void onStarted(Callback cb);
    void onStopped(StoppedCallback cb);
    void resetCallbacks();

BLEAudioBroadcastSink
^^^^^^^^^^^^^^^^^^^^^

Receiver: scan (``onSourceFound()``) -> PA sync (``onSynced()`` + BASE via
``onBaseReceived()``) -> BIG sync (``onStarted()``; one Rx stream per joined
BIS). Auto-sync (default) joins the first source matching the target name/ID;
otherwise pick one from ``onSourceFound()`` and call ``syncTo()``. With the
Scan Delegator (BASS, default on) an assistant can steer it remotely.

.. code-block:: cpp

    struct BLEAudioBroadcastBaseInfo {
        uint32_t bisMask = 0;             // BIT(index-1) per BIS in the BASE
        uint8_t subgroups = 0;
        uint32_t presentationDelayUs = 0;
        BLEAudioCodecConfig codec;       // codec of the first subgroup
    };

    BLEAudioBroadcastSink();                   // empty; via audio.createBroadcastSink()
    explicit operator bool() const;

    // Configuration (before audio.begin()):
    BTStatus setStreams(uint8_t count);              // 1 mono, 2 stereo
    BTStatus setSupportedPresets(uint32_t presetMask);
    BTStatus setMaxChannelsPerStream(uint8_t channels);
    BTStatus setLocation(BLEAudioLocation location);
    BTStatus setContexts(BLEAudioContext contexts);
    BTStatus setScanDelegator(bool enable);
    // Source selection (any time):
    BTStatus setBroadcastCode(const String &code);
    BTStatus setTargetName(const String &name);        // empty = any
    BTStatus setTargetBroadcastId(uint32_t broadcastId);  // 0 = any
    BTStatus setBisMask(uint32_t mask);               // 0 = lowest indexes
    BTStatus setAutoSync(bool enable);                // default on
    BTStatus configureAuracast(const String &targetName = "", uint8_t streams = 2);

    // Control (after audio.begin()):
    BTStatus start();
    BTStatus syncTo(const BLEAudioBroadcastSourceInfo &source);
    BTStatus stop();
    bool isStreaming() const;

    size_t streamCount() const;
    BLEAudioStream stream(size_t index) const;

    using SourceFoundCallback = std::function<void(const BLEAudioBroadcastSourceInfo &source)>;
    using BaseCallback = std::function<void(const BLEAudioBroadcastBaseInfo &base)>;
    using Callback = std::function<void()>;
    using ReasonCallback = std::function<void(uint8_t reason)>;
    using ErrorCallback = std::function<void(BTStatus status)>;  // NotFound / PermissionDenied
    void onSourceFound(SourceFoundCallback cb);
    void onSynced(Callback cb);
    void onBaseReceived(BaseCallback cb);
    void onBroadcastCodeReceived(Callback cb);
    void onSyncLost(ReasonCallback cb);
    void onSyncFailed(ErrorCallback cb);
    void onStarted(Callback cb);
    void onStopped(ReasonCallback cb);
    void resetCallbacks();

BLEAudioBroadcastAssistant
^^^^^^^^^^^^^^^^^^^^^^^^^^

BASS client (phone/remote): discovers BASS on a connected sink, scans on its
behalf, adds/modifies/removes sources and hands over the Broadcast Code and
PAST sync info. One sink at a time; calls before ``discover()`` return
``NotConnected``.

.. code-block:: cpp

    struct BLEAudioBroadcastSourceInfo {
        BTAddress address;
        uint8_t sid = 0;
        uint32_t broadcastId = 0;   // 24-bit
        String name;
        int8_t rssi = 0;
        uint16_t paInterval = 0xFFFF;
    };

    struct BLEAudioReceiveState {
        enum class PaState : uint8_t { NotSynced, SyncInfoRequested, Synced, Failed, NoPast };
        enum class Encryption : uint8_t { None, CodeRequired, Decrypting, BadCode };
        static constexpr uint8_t kMaxSubgroups = 4;
        uint8_t sourceId = 0;
        bool removed = false;
        BTAddress address;
        uint8_t sid = 0;
        uint32_t broadcastId = 0;
        PaState paState = PaState::NotSynced;
        Encryption encryption = Encryption::None;
        uint8_t badCode[16] = {};
        uint8_t numSubgroups = 0;
        uint32_t bisSync[kMaxSubgroups] = {};
    };

    BLEAudioBroadcastAssistant();               // empty; via audio.createBroadcastAssistant()
    explicit operator bool() const;

    BTStatus discover(uint16_t connHandle);
    uint16_t getConnHandle() const;             // BLE_AUDIO_CONN_NONE when unbound
    BTStatus startRemoteScan();                 // Busy if another role holds the ext scan
    BTStatus stopRemoteScan();
    BTStatus addSource(const BLEAudioBroadcastSourceInfo &source, bool syncPa = true,
                       uint32_t bisMask = 0xFFFFFFFF);
    BTStatus modifySource(uint8_t sourceId, bool syncPa, uint32_t bisMask);
    BTStatus removeSource(uint8_t sourceId);    // unsync first with modifySource(id, false, 0)
    BTStatus setBroadcastCode(uint8_t sourceId, const String &code);

    enum class Op : uint8_t {
        ScanStart, ScanStop, AddSource, ModifySource, RemoveSource, BroadcastCode, Past,
    };
    using DiscoverCallback = std::function<void(BTStatus status, uint8_t receiveStates)>;
    using SourceFoundCallback = std::function<void(const BLEAudioBroadcastSourceInfo &source)>;
    using ReceiveStateCallback = std::function<void(const BLEAudioReceiveState &state)>;
    using ResultCallback = std::function<void(Op op, BTStatus status)>;
    void onDiscovered(DiscoverCallback cb);
    void onSourceFound(SourceFoundCallback cb);
    void onReceiveState(ReceiveStateCallback cb);
    void onResult(ResultCallback cb);
    void resetCallbacks();

CAP and coordinated sets
************************

BLEAudioCapAcceptor
^^^^^^^^^^^^^^^^^^^

CAS with an included CSIS instance (earbud/speaker). Audio flows through the
BAP roles created next to it.

.. code-block:: cpp

    BLEAudioCapAcceptor();                     // empty; via audio.createCapAcceptor()
    explicit operator bool() const;
    BTStatus setSirk(const uint8_t sirk[BLE_AUDIO_SIRK_SIZE]);
    BTStatus setSetSize(uint8_t size);
    BTStatus setRank(uint8_t rank);   // 1-based
    BTStatus setLockable(bool lockable);
    BLEAudioCoordinatedSetMember coordinatedSet() const;

BLEAudioCapInitiator
^^^^^^^^^^^^^^^^^^^^

Drives every discovered acceptor as one unit. ``discover()`` each acceptor
after ``audio.onLinkReady()``; ``startUnicast()`` brings them all up.

.. code-block:: cpp

    struct BLEAudioCapPeerInfo : BLEAudioUnicastPeerInfo {
        bool coordinatedSet = false;  // CAS includes CSIS
    };

    BLEAudioCapInitiator();                    // empty; via audio.createCapInitiator()
    explicit operator bool() const;

    BTStatus setPreset(BLEAudioCodecPreset preset);  // default LC3_16_2_1
    BTStatus setQos(const BLEAudioQos &qos);
    BTStatus setContext(BLEAudioContext context);    // default Media
    BTStatus setSinkStreams(uint8_t perPeer);        // default 1
    BTStatus setSourceStreams(uint8_t perPeer);      // default 0
    BTStatus setBroadcastId(uint32_t broadcastId);
    uint32_t getBroadcastId() const;
    BTStatus setBroadcastCode(const String &code);
    BTStatus setBroadcastName(const String &name);

    BTStatus discover(uint16_t connHandle);
    BTStatus startUnicast();
    BTStatus updateUnicast(BLEAudioContext context);
    BTStatus stopUnicast();
    bool isUnicastStreaming() const;
    BTStatus startBroadcast(uint8_t channels = 1, uint8_t advInstance = 0);
    BTStatus stopBroadcast();
    bool isBroadcasting() const;
    BTStatus handoverToBroadcast(uint8_t advInstance = 0);  // needs CONFIG_BT_CAP_HANDOVER
    BTStatus handoverToUnicast();

    size_t streamCount() const;
    BLEAudioStream stream(size_t index) const;
    BLEAudioStream stream(BLEAudioStream::Direction dir, uint8_t index = 0) const;

    using DiscoveredCallback = std::function<void(BTStatus status, const BLEAudioCapPeerInfo &peer)>;
    using StatusCallback = std::function<void(BTStatus status)>;
    using Callback = std::function<void()>;
    using StoppedCallback = std::function<void(uint8_t reason)>;
    using HandoverCallback = std::function<void(BTStatus status, bool toBroadcast)>;
    void onDiscovered(DiscoveredCallback cb);
    void onUnicastStarted(StatusCallback cb);
    void onUnicastUpdated(StatusCallback cb);
    void onUnicastStopped(Callback cb);
    void onBroadcastStarted(Callback cb);
    void onBroadcastStopped(StoppedCallback cb);
    void onHandover(HandoverCallback cb);
    void resetCallbacks();

BLEAudioCapCommander
^^^^^^^^^^^^^^^^^^^^

Volume/mute/broadcast-reception control of every discovered acceptor at once.
Each procedure returns once started and reports through ``onResult()``.

.. code-block:: cpp

    BLEAudioCapCommander();                    // empty; via audio.createCapCommander()
    explicit operator bool() const;

    enum class Operation : uint8_t {
        Volume, VolumeMute, VolumeOffset, MicMute, MicGain,
        BroadcastReceptionStart, BroadcastReceptionStop, BroadcastCode,
    };
    struct ReceiveState {
        uint16_t connHandle;
        uint8_t sourceId;
        uint32_t broadcastId;
        uint8_t paSyncState;       // 0 not synced .. 4 no PAST
        uint8_t encryptionState;   // 0 none, 1 code req, 2 decrypting, 3 bad code
        uint32_t bisSync;          // bit n-1 = BIS n
    };

    BTStatus discover(uint16_t connHandle);
    BTStatus setCoordinated(bool enable);   // CSIP ordered access; needs CSIP coordinator
    BTStatus setVolume(uint8_t volume);
    BTStatus setMute(bool mute);
    BTStatus setVolumeOffset(int16_t offset);  // first VOCS, -255..255
    BTStatus setMicMute(bool mute);
    BTStatus setMicGain(int8_t gain);          // first mic AICS
    BTStatus startBroadcastReception(const BTAddress &source, uint8_t sid, uint32_t broadcastId,
                                     uint32_t bisSync = 0, uint16_t paInterval = 0xFFFF);
    BTStatus stopBroadcastReception();
    BTStatus distributeBroadcastCode(const String &code);  // non-empty, up to 16 chars

    using DiscoveredCallback = std::function<void(BTStatus status, uint16_t connHandle,
                                                  bool coordinatedSet, bool broadcastReception)>;
    using ResultCallback = std::function<void(Operation op, BTStatus status)>;
    using ReceiveStateCallback = std::function<void(const ReceiveState &state)>;
    void onDiscovered(DiscoveredCallback cb);
    void onResult(ResultCallback cb);
    void onReceiveState(ReceiveStateCallback cb);
    void resetCallbacks();

Coordinated sets (CSIP)
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    static constexpr size_t BLE_AUDIO_SIRK_SIZE = 16;
    static constexpr size_t BLE_AUDIO_RSI_SIZE = 6;

    // Member (CSIS server). A CAP acceptor owns one via coordinatedSet();
    // create a standalone member only for non-CAP devices.
    BLEAudioCoordinatedSetMember();            // empty; via audio.createCoordinatedSetMember()
    explicit operator bool() const;
    enum class SirkAccess : uint8_t { Accept, AcceptEncrypted, Reject, OobOnly };
    using LockCallback = std::function<void(bool locked, uint16_t connHandle)>;
    using SirkReadCallback = std::function<SirkAccess(uint16_t connHandle)>;
    BTStatus setSirk(const uint8_t sirk[BLE_AUDIO_SIRK_SIZE]);
    BTStatus setSetSize(uint8_t size);   // default 2
    BTStatus setRank(uint8_t rank);      // 1..size, default 1
    BTStatus setLockable(bool lockable); // default true, read at begin()
    BTStatus setName(const String &name);
    BTStatus generateRsi(uint8_t rsi[BLE_AUDIO_RSI_SIZE]) const;  // advertise as AD 0x2E
    BTStatus lock();
    BTStatus unlock(bool force = false);
    bool isLocked() const;
    void onLockChanged(LockCallback cb);
    void onSirkRead(SirkReadCallback cb);  // default: accept
    void resetCallbacks();

    struct BLEAudioCoordinatedSetInfo {
        uint16_t connHandle = 0xFFFF;
        uint8_t setCount = 0;       // CSIS instances (0 = not in a set)
        uint8_t setSize = 0;
        uint8_t rank = 0;
        bool lockable = false;
        uint8_t sirk[BLE_AUDIO_SIRK_SIZE] = {};
    };

    // Coordinator (CSIP client).
    // (Callbacks below are scoped to BLEAudioCoordinatedSetCoordinator.)
    BLEAudioCoordinatedSetCoordinator();       // empty; via audio.createCoordinatedSetCoordinator()
    explicit operator bool() const;
    using DiscoveredCallback = std::function<void(BTStatus status, const BLEAudioCoordinatedSetInfo &info)>;
    using LockCallback = std::function<void(BTStatus status, bool locked)>;
    using LockChangedCallback = std::function<void(bool locked, uint16_t connHandle)>;
    BTStatus discover(uint16_t connHandle);
    bool isSetMember(const uint8_t *advPayload, size_t len) const;
    BTStatus lock();     // every connected discovered member, rank order
    BTStatus unlock();
    BLEAudioCoordinatedSetInfo setInfo() const;
    void onDiscovered(DiscoveredCallback cb);
    void onLock(LockCallback cb);
    void onLockChanged(LockChangedCallback cb);
    void resetCallbacks();

Control profiles: VCP / MICP
****************************

Volume renderer (VCS server) and controller (one peer; only the first
VOCS/AICS driven; second ``discover()`` replaces the peer; cache clears on
disconnect). ``OK`` means the write was queued -- success arrives as the next
state notification, a rejection as ``onResult()``.

.. code-block:: cpp

    BLEAudioVolumeRenderer();                  // empty handle, via audio.createVolumeRenderer()
    explicit operator bool() const;
    enum class Operation : uint8_t { Unknown, SetVolume, Mute, Unmute, VolumeUp, VolumeDown, SetStep };
    using StateCallback = std::function<void(uint8_t volume, bool muted)>;
    using ResultCallback = std::function<void(Operation op, BTStatus status)>;
    BTStatus setInitialVolume(uint8_t volume);  // default 100
    BTStatus setInitialMute(bool muted);
    BTStatus setVolumeStep(uint8_t step);       // 1..255, default 1
    BTStatus setVolume(uint8_t volume);
    BTStatus mute();
    BTStatus unmute();
    BTStatus volumeUp();
    BTStatus volumeDown();
    uint8_t getVolume() const;
    bool isMuted() const;
    void onStateChanged(StateCallback cb);
    void onResult(ResultCallback cb);
    void resetCallbacks();

.. code-block:: cpp

    BLEAudioVolumeController();                // empty handle, via audio.createVolumeController()
    explicit operator bool() const;
    enum class Operation : uint8_t {
        Unknown, SetVolume, Mute, Unmute, VolumeUp, VolumeDown, ReadState, SetOffset, SetInputGain,
    };
    using DiscoveredCallback = std::function<void(BTStatus status, uint8_t offsetCount, uint8_t inputCount)>;
    using StateCallback = std::function<void(uint8_t volume, bool muted)>;
    using OffsetCallback = std::function<void(int16_t offset)>;
    using InputCallback = std::function<void(int8_t gain, bool muted)>;
    using ResultCallback = std::function<void(Operation op, BTStatus status)>;
    BTStatus discover(uint16_t connHandle);
    uint16_t getConnHandle() const;
    BTStatus setVolume(uint8_t volume);
    BTStatus mute();
    BTStatus unmute();
    BTStatus volumeUp();
    BTStatus volumeDown();
    BTStatus readState();
    uint8_t getVolume() const;
    bool isMuted() const;
    BTStatus setVolumeOffset(int16_t offset);   // first VOCS, -255..255
    BTStatus setInputGain(int8_t gain);         // first AICS
    void onDiscovered(DiscoveredCallback cb);
    void onStateChanged(StateCallback cb);
    void onOffsetChanged(OffsetCallback cb);
    void onInputChanged(InputCallback cb);
    void onResult(ResultCallback cb);
    void resetCallbacks();

Microphone device (MICS server) and controller (same one-peer / first-AICS /
queued-write rules as VCP):

.. code-block:: cpp

    BLEAudioMicDevice();                       // empty handle, via audio.createMicDevice()
    explicit operator bool() const;
    enum class Operation : uint8_t { Unknown, Mute, Unmute, DisableMute };
    using MuteCallback = std::function<void(bool muted)>;
    using ResultCallback = std::function<void(Operation op, BTStatus status)>;
    BTStatus setInitialMute(bool muted);
    BTStatus mute();
    BTStatus unmute();
    BTStatus disableMute();      // mute unavailable; controllers cannot change it
    bool isMuted() const;
    bool isMuteDisabled() const;
    void onMuteChanged(MuteCallback cb);
    void onResult(ResultCallback cb);
    void resetCallbacks();

.. code-block:: cpp

    BLEAudioMicController();                   // empty handle, via audio.createMicController()
    explicit operator bool() const;
    enum class Operation : uint8_t { Unknown, Mute, Unmute, ReadMute, SetInputGain };
    using DiscoveredCallback = std::function<void(BTStatus status, uint8_t inputCount)>;
    using MuteCallback = std::function<void(bool muted)>;
    using InputCallback = std::function<void(int8_t gain, bool muted)>;
    using ResultCallback = std::function<void(Operation op, BTStatus status)>;
    BTStatus discover(uint16_t connHandle);
    uint16_t getConnHandle() const;
    BTStatus mute();
    BTStatus unmute();
    BTStatus readMute();
    bool isMuted() const;
    bool isMuteDisabled() const;
    BTStatus setInputGain(int8_t gain);   // first mic AICS
    void onDiscovered(DiscoveredCallback cb);
    void onMuteChanged(MuteCallback cb);
    void onInputChanged(InputCallback cb);
    void onResult(ResultCallback cb);
    void resetCallbacks();

Control profiles: MCP / CCP
***************************

Only the generic instances are exposed: MCP drives GMCS, CCP the GTBS.
Controllers bind one peer (``discover()`` replaces it); commands return
``OK`` once queued and complete in the matching callback.

Media player (GMCS server) and controller:

``play()``, ``pause()``, ``stop()``, ``nextTrack()`` and ``previousTrack()`` send the
corresponding MCC command to the remote player. They are media commands, not object
lifecycle; local audio is controlled by ``BLEAudioPlayer::startPlayback()`` /
``stopPlayback()``.

.. code-block:: cpp

    enum class BLEAudioMediaState : uint8_t { Inactive = 0, Playing = 1, Paused = 2, Seeking = 3 };
    enum class BLEAudioMediaCommand : uint8_t {
        Play = 0x01, Pause = 0x02, FastRewind = 0x03, FastForward = 0x04, Stop = 0x05,
        PreviousTrack = 0x30, NextTrack = 0x31, FirstTrack = 0x32, LastTrack = 0x33,
        PreviousGroup = 0x40, NextGroup = 0x41,
    };

    BLEAudioMediaPlayer();                     // empty handle, via audio.createMediaPlayer()
    explicit operator bool() const;
    using StateCallback = std::function<void(BLEAudioMediaState state)>;
    using CommandCallback = std::function<void(BLEAudioMediaCommand command, BTStatus result)>;
    BTStatus setPlayerName(const String &name);
    BTStatus setTrackTitle(const String &title);
    BTStatus sendCommand(BLEAudioMediaCommand command);  // needs local-control Kconfig, else NotSupported
    BTStatus play();
    BTStatus pause();
    BTStatus stop();
    BTStatus nextTrack();
    BTStatus previousTrack();
    BLEAudioMediaState getState() const;
    void onStateChanged(StateCallback cb);
    void onCommandResult(CommandCallback cb);
    void resetCallbacks();

.. code-block:: cpp

    BLEAudioMediaController();                 // empty handle, via audio.createMediaController(), GMCS only
    explicit operator bool() const;
    using DiscoveredCallback = std::function<void(BTStatus status)>;
    using StateCallback = std::function<void(BLEAudioMediaState state)>;
    using CommandCallback = std::function<void(BLEAudioMediaCommand command, BTStatus result)>;
    using TextCallback = std::function<void(const String &text)>;
    BTStatus discover(uint16_t connHandle);
    uint16_t getConnHandle() const;
    BTStatus sendCommand(BLEAudioMediaCommand command);
    BTStatus play();
    BTStatus pause();
    BTStatus stop();
    BTStatus nextTrack();
    BTStatus previousTrack();
    BTStatus readState();
    BTStatus readPlayerName();
    BTStatus readTrackTitle();
    BLEAudioMediaState getState() const;  // Inactive after disconnect
    void onDiscovered(DiscoveredCallback cb);
    void onStateChanged(StateCallback cb);
    void onCommandResult(CommandCallback cb);
    void onPlayerName(TextCallback cb);
    void onTrackTitle(TextCallback cb);
    void resetCallbacks();

Call server (GTBS phone side) and controller (headset side):

.. code-block:: cpp

    enum class BLEAudioCallOperation : uint8_t { Originate, Accept, Terminate, Hold, Retrieve };
    enum class BLEAudioCallState : uint8_t {
        Incoming, Dialing, Alerting, Active,
        LocallyHeld, RemotelyHeld, LocallyAndRemotelyHeld, Ended = 0xFF,
    };

    // Server (via audio.createCallServer()):
    BLEAudioCallServer();
    explicit operator bool() const;
    using OriginateCallback = std::function<bool(uint8_t callIndex, const String &uri)>;
    using TerminatedCallback = std::function<void(uint8_t callIndex, uint8_t reason)>;
    using CallControlCallback = std::function<void(BLEAudioCallOperation operation, uint8_t callIndex)>;
    BTStatus setProviderName(const String &name);  // default "ESP Phone"
    BTStatus setUci(const String &uci);
    BTStatus setUriSchemes(const String &schemes);  // e.g. "tel,skype", default "tel"
    BTStatus incomingCall(const String &from, uint8_t &callIndex, const String &friendlyName = "");
    BTStatus accept(uint8_t callIndex);
    BTStatus hold(uint8_t callIndex);
    BTStatus retrieve(uint8_t callIndex);
    BTStatus terminate(uint8_t callIndex);
    BTStatus remoteAnswered(uint8_t callIndex);
    BTStatus remoteTerminated(uint8_t callIndex);
    BTStatus updateProviderName(const String &name);
    void onOriginate(OriginateCallback cb);
    void onTerminated(TerminatedCallback cb);
    void onCallControl(CallControlCallback cb);
    void resetCallbacks();

    // Controller (via audio.createCallController()):
    BLEAudioCallController();
    explicit operator bool() const;
    using DiscoveredCallback = std::function<void(BTStatus status, bool gtbsFound)>;
    using ResultCallback = std::function<void(BLEAudioCallOperation operation, BTStatus status, uint8_t callIndex)>;
    using CallStateCallback = std::function<void(uint8_t callIndex, BLEAudioCallState state)>;
    BTStatus discover(uint16_t connHandle);
    uint16_t getConnHandle() const;
    BTStatus originate(const String &uri);   // e.g. "tel:+..."
    BTStatus accept(uint8_t callIndex);
    BTStatus terminate(uint8_t callIndex);
    BTStatus hold(uint8_t callIndex);
    BTStatus retrieve(uint8_t callIndex);
    BTStatus readCalls();                    // one onCallState per call
    void onDiscovered(DiscoveredCallback cb);
    void onResult(ResultCallback cb);
    void onCallState(CallStateCallback cb);
    void resetCallbacks();

Identity profiles and hearing aids
**********************************

TMAP / GMAP publish role bits (identity only, no audio); readers use
``discover()`` + ``onDiscovered()``. HAS publishes named presets on top of a
CAP acceptor.

.. code-block:: cpp

    enum class BLEAudioTmapRole : uint16_t {
        None = 0x0000, CallGateway = 0x0001, CallTerminal = 0x0002,
        UnicastMediaSender = 0x0004, UnicastMediaReceiver = 0x0008,
        BroadcastMediaSender = 0x0010, BroadcastMediaReceiver = 0x0020,
    };
    inline BLEAudioTmapRole operator|(BLEAudioTmapRole a, BLEAudioTmapRole b);
    inline BLEAudioTmapRole &operator|=(BLEAudioTmapRole &a, BLEAudioTmapRole b);
    inline bool operator&(BLEAudioTmapRole a, BLEAudioTmapRole b);

    BLEAudioTmap();                            // empty; via audio.createTmap()
    explicit operator bool() const;
    using DiscoverCallback = std::function<void(uint16_t connHandle, BTStatus status,
                                                BLEAudioTmapRole peerRoles)>;
    BTStatus setRoles(BLEAudioTmapRole roles);   // before audio.begin()
    BLEAudioTmapRole getRoles() const;
    BTStatus discover(uint16_t connHandle);
    void onDiscovered(DiscoverCallback cb);
    void resetCallbacks();

    enum class BLEAudioGmapRole : uint8_t {
        None = 0x00, UnicastGameGateway = 0x01, UnicastGameTerminal = 0x02,
        BroadcastGameSender = 0x04, BroadcastGameReceiver = 0x08,
    };
    inline BLEAudioGmapRole operator|(BLEAudioGmapRole a, BLEAudioGmapRole b);
    inline BLEAudioGmapRole &operator|=(BLEAudioGmapRole &a, BLEAudioGmapRole b);
    inline bool operator&(BLEAudioGmapRole a, BLEAudioGmapRole b);
    struct BLEAudioGmapFeatures {
        uint8_t unicastGateway = 0;
        uint8_t unicastTerminal = 0;
        uint8_t broadcastSender = 0;
        uint8_t broadcastReceiver = 0;
    };

    BLEAudioGmap();                            // empty; via audio.createGmap()
    explicit operator bool() const;
    using DiscoverCallback = std::function<void(uint16_t connHandle, BTStatus status,
                                                BLEAudioGmapRole peerRoles,
                                                const BLEAudioGmapFeatures &peerFeatures)>;
    BTStatus setRoles(BLEAudioGmapRole roles);
    BTStatus setFeatures(const BLEAudioGmapFeatures &features);
    BTStatus setFeatures(uint8_t unicastGateway, uint8_t unicastTerminal,
                              uint8_t broadcastSender, uint8_t broadcastReceiver);
    BLEAudioGmapRole getRoles() const;
    BTStatus discover(uint16_t connHandle);
    void onDiscovered(DiscoverCallback cb);
    void resetCallbacks();

.. code-block:: cpp

    // PBP announcement helpers (static-only; needs BLE_AUDIO_PBP_SUPPORTED):
    enum Feature : uint8_t { Encryption = 0x01, StandardQuality = 0x02, HighQuality = 0x04 };
    static size_t buildAnnouncement(uint8_t features, const uint8_t *metadata, size_t metadataLen,
                                    uint8_t *out, size_t outCap);
    static bool parseAnnouncement(const uint8_t *data, size_t dataLen, uint8_t &featuresOut,
                                  const uint8_t *&metadataOut, size_t &metadataLenOut);

.. code-block:: cpp

    enum class BLEHearingAidType : uint8_t { Binaural = 0, Monaural = 1, Banded = 2 };

    // Device (via audio.createHearingAidDevice()):
    BLEAudioHearingAidDevice();
    explicit operator bool() const;
    using SelectCallback = std::function<void(uint8_t index, bool sync)>;
    using RenameCallback = std::function<void(uint8_t index, const String &name)>;
    BTStatus setType(BLEHearingAidType type);
    BTStatus setPresetSync(bool enabled);
    BTStatus addPreset(uint8_t index, const String &name,
                                        bool available = true, bool writable = false);
    BTStatus removePreset(uint8_t index);
    BTStatus setPresetAvailable(uint8_t index, bool available);
    BTStatus renamePreset(uint8_t index, const String &name);
    BTStatus setActivePreset(uint8_t index);
    uint8_t getActivePreset() const;   // 0 = none
    void onPresetSelected(SelectCallback cb);
    void onPresetRenamed(RenameCallback cb);
    void resetCallbacks();

    // Controller (via audio.createHearingAidController(), one peer at a time):
    BLEAudioHearingAidController();
    explicit operator bool() const;
    using DiscoveredCallback = std::function<void(BTStatus status, BLEHearingAidType type)>;
    using PresetCallback = std::function<void(uint8_t index, bool available,
                                              const String &name, bool isLast)>;
    using SwitchCallback = std::function<void(BTStatus status, uint8_t index)>;
    BTStatus discover(uint16_t connHandle);
    uint16_t getConnHandle() const;
    BTStatus readPresets(uint8_t startIndex = 1, uint8_t maxCount = 255);
    BTStatus setActivePreset(uint8_t index, bool sync = false);
    BTStatus nextPreset(bool sync = false);
    BTStatus previousPreset(bool sync = false);
    BTStatus renamePreset(uint8_t index, const String &name);
    uint8_t getActivePreset() const;
    void onDiscovered(DiscoveredCallback cb);
    void onPreset(PresetCallback cb);
    void onPresetSwitch(SwitchCallback cb);
    void resetCallbacks();

LC3 player and recorder
***********************

``BLEAudioPlayer`` takes **Rx** streams, LC3-decodes them and plays PCM on I2S
(or a PCM sink). ``BLEAudioRecorder`` takes PCM from I2S (or a PCM source),
LC3-encodes it and writes SDUs into **Tx** streams. Order: ``begin()`` (after
``audio.begin()``) -> ``attach()`` (for unicast, in the stream's
``onConfigured()``) -> ``startPlayback()`` / ``startCapture()``. One stereo
program is either one 1-2 channel stream (``attach(stream)``) or two mono
streams (``attach(left, right)``); anything else logs ``unsupported channel
layout``. Needs ``BLE_AUDIO_LC3_SUPPORTED``.

.. code-block:: cpp

    using BLEAudioPCMSink = std::function<void(const int16_t *pcm, size_t samples)>;
    using BLEAudioPCMSource = std::function<size_t(int16_t *pcm, size_t samples)>;

PCM callbacks run on the codec task, paced to the real-time frame rate
(7.5/10 ms, 8.16/10.88 ms at 44.1 kHz). The sink consumes interleaved 16-bit
``samples`` (frames * channels); the source fills ``samples`` wanted and
returns how many were produced (short reads are padded with silence). Never
block on BLE or call ``attach()`` / ``start`` / ``stop`` / ``end()`` from them.

.. code-block:: cpp

    BLEAudioPlayer();                          // empty until begin()
    explicit operator bool() const;            // true between begin() and end()
    BTStatus begin();                          // PCM-sink mode (needs setPCMSink())
    BTStatus begin(int bclk, int ws, int dout, int mclk = -1, int port = 0);
    void end();                                // deferred when called from the sink callback
    BTStatus setPCMSink(BLEAudioPCMSink sink);   // before begin(), wins over I2S
    BTStatus setJitterDepth(uint8_t sdus);       // 0 = from QoS (default), 2..16
    BTStatus attach(BLEAudioStream stream);
    BTStatus attach(BLEAudioStream stream, BLEAudioStream right);
    BTStatus startPlayback();                  // spawns the decode task
    void stopPlayback();                       // keeps attachments
    bool isRunning() const;
    uint8_t channels() const;                  // 0 while idle (I2S always stereo)
    uint32_t sampleRate() const;               // 0 while idle
    // Statistics (reset by startPlayback()):
    uint32_t sdusReceived() const;
    uint32_t sdusDropped() const;              // jitter buffer full
    uint32_t sdusLost() const;                 // flagged lost/invalid or bad length
    uint32_t plcFrames() const;
    uint32_t underruns() const;
    uint32_t lateResyncs() const;

.. code-block:: cpp

    BLEAudioRecorder();
    explicit operator bool() const;
    BTStatus begin();                          // PCM-source mode (needs setPCMSource())
    BTStatus begin(int bclk, int ws, int din, int mclk = -1, int port = 0);
    void end();
    BTStatus setPCMSource(BLEAudioPCMSource source);  // before begin()
    BTStatus attach(BLEAudioStream stream);
    BTStatus attach(BLEAudioStream stream, BLEAudioStream right);
    BTStatus startCapture();
    void stopCapture();
    bool isRunning() const;
    uint8_t channels() const;
    uint32_t sampleRate() const;
    // Statistics (reset by startCapture()):
    uint32_t sdusSent() const;                 // accepted by the stack, per stream
    uint32_t sendErrors() const;
    uint32_t txStalls() const;

Errors and callbacks
********************

Configuration methods return ``BTStatus`` synchronously: ``InvalidParam`` for a
bad value, ``InvalidState`` on an empty handle, before ``audio.begin()``, while
a role runs, or before ``discover()`` on a controller, plus the stack status
when a request cannot be queued. Queued control-point writes terminate exactly
once: success arrives as the next state notification, a rejection as
``onResult()`` (best-effort op attribution when several writes share the
channel). A pending write whose link drops first is reported as
``NotConnected``. A failed ``begin()`` leaves earlier roles registered: run
``end()``, fix the set, then ``begin()`` again.
