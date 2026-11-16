##########
BLE Audio
##########

About
-----

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
    us.setSinkStreams(1).setSourceStreams(0);
    us.setSinkContexts(BLEAudioContext::Media | BLEAudioContext::Conversational);
    // us.setSupportedPresets(...) to narrow the published PACS (default: 16/24/32/48 kHz)

Client side (phone/dongle):

.. code-block:: cpp

    BLEAudioUnicastClient uc = audio.createUnicastClient();
    uc.setPreset(BLEAudioCodecPreset::LC3_16_2_1).setSinkStreams(1);
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
    src.setPreset(BLEAudioCodecPreset::LC3_48_4_1).setChannels(2).setName("Stage left");
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
  set size), then ``lock()``/``release()`` serialize access across members.

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

``BLEAudioPlayer`` decodes to I2S (or a PCM sink); ``BLEAudioRecorder`` encodes from I2S
(or a PCM source). ``begin()`` selects the endpoint, ``attach()`` binds Tx/Rx streams,
``startPlayback()``/``startCapture()`` run the codec task while streams stream:

.. code-block:: cpp

    player.begin(bclk, ws, dout);
    player.attach(us.stream(0));
    player.startPlayback();

BAP can deliver one stereo program in two shapes — pick the ``attach()`` form that
matches the role that created the streams:

* **One stream, 1–2 channels** (one ASE or one BIS; interleaved stereo):
  ``player.attach(stream)``.
* **Two mono streams** (two ASEs/BISes, e.g. stereo Auracast front left + front right):
  ``player.attach(left, right)`` — both must be mono with identical codec/QoS.

Any other combination logs ``unsupported channel layout`` and does not start. Statistics
(``sdusSent()`` / ``sendErrors()`` / ``txStalls()`` / ``underruns()`` / ``plcFrames()``)
are the API-visible health signals; a TX stall (controller stops releasing SDUs)
resyncs and counts ``txStalls`` instead of spinning silently. With no I2S hardware,
``player.setPCMSink()`` / ``recorder.setPCMSource()`` replace the endpoints with
callbacks paced to the stream's real-time frame rate.

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

The ``BLEAudio`` examples are grouped by product, not by API role, and are written for
the ESP32-S31 Function CoreBoard 1 (ES8311 DAC/ADC + NS4150B amplifier):

* ``LEAudio_UnicastSpeaker`` — CAP acceptor, stereo-capable unicast sink, VCP volume,
  LC3 player to I2S DAC.
* ``LEAudio_WirelessMic`` — BAP unicast client, LC3 recorder from an I2S mic to CIS.
* ``LEAudio_AuracastSource`` — broadcast source, Public Broadcast Announcement, LC3
  recorder from an I2S input.
* ``LEAudio_AuracastSink`` — broadcast sink, BASS Scan Delegator, LC3 player to I2S DAC.

Typical pairs: WirelessMic → UnicastSpeaker, AuracastSource → AuracastSink, or a
phone/PC with LE Audio ↔ UnicastSpeaker and a phone/PC with Auracast ↔ AuracastSink.
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
