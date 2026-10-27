/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

/**
 * @file
 * @brief LE Audio controller handle, the entry point of the audio component.
 *
 * `BLEAudio` is NOT a second global singleton. It is a shared handle minted
 * from the `BLE` singleton via `BLE.getAudioController()` (the same model as
 * `BLE.getScan()` / `BLE.getSecurity()`): the first call allocates the single
 * engine-backed implementation, later calls return a handle to the same
 * instance. It is only valid after `BLE.begin()`.
 *
 * The controller owns the LE Audio engine (`esp_ble_audio` from ESP-IDF), the
 * single event bridge from the engine to the role handles, and the lifetime
 * of every role created through it: roles stay registered until end(), even
 * if the sketch drops its handles.
 *
 * This header is also the audio component's aggregator: `src/BLE.h` includes
 * it so the whole audio API reaches sketches through `<BLE.h>` (like
 * `stream/BLEStream.h` and `l2cap/BLEL2CAP.h`). It must stay backend-agnostic:
 * no `esp_ble_audio_*` type appears here.
 *
 * Lifecycle:
 * @code
 * BLE.begin("Headset");
 * BLEAudio audio = BLE.getAudioController();  // handle to the single engine
 * audio.setPresentationDelay(40000);          // defaults, before creating roles
 * audio.begin();                              // engine init + event bridge
 * auto server = audio.createUnicastServer();  // create and configure roles
 * audio.start();                              // register roles, single GATT commit
 * // ... stream ...
 * audio.end();                                // or BLE.end()
 * @endcode
 *
 * Every role factory is compiled only when the matching Kconfig role is
 * enabled in the packaged libraries (see the `BLE_AUDIO_*_SUPPORTED` guards in
 * `core/BLEGuards.h`). Create roles after begin() and before start(): the
 * factories log an error and return an empty handle when called too early,
 * and a role that registers services is never registered when created after
 * start(). The broadcast source, which has no service, is the exception.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <functional>
#include <memory>
#include "BTStatus.h"
#include "BTAddress.h"
#include "audio/BLEAudioTypes.h"

class BLEAudioUnicastServer;
class BLEAudioUnicastClient;
class BLEAudioBroadcastSource;
class BLEAudioBroadcastSink;
class BLEAudioBroadcastAssistant;
class BLEAudioVolumeRenderer;
class BLEAudioVolumeController;
class BLEAudioMicDevice;
class BLEAudioMicController;
class BLEAudioCoordinatedSetMember;
class BLEAudioCoordinatedSetCoordinator;
class BLEAudioCapAcceptor;
class BLEAudioCapInitiator;
class BLEAudioCapCommander;
class BLEAudioMediaPlayer;
class BLEAudioMediaController;
class BLEAudioCallServer;
class BLEAudioCallController;
class BLEAudioTmap;
class BLEAudioGmap;
class BLEAudioHearingAidDevice;
class BLEAudioHearingAidController;

class BLEAudio {
public:
  /** @brief Link event for the ACL connection @p connHandle. */
  using LinkCallback = std::function<void(uint16_t connHandle)>;

  BLEAudio();
  ~BLEAudio() = default;
  BLEAudio(const BLEAudio &) = default;
  BLEAudio &operator=(const BLEAudio &) = default;
  BLEAudio(BLEAudio &&) = default;
  BLEAudio &operator=(BLEAudio &&) = default;

  /** @brief Whether this handle references the live controller (minted after BLE.begin()). */
  explicit operator bool() const;

  // --- Lifecycle ---

  /**
   * @brief Initialize the LE Audio engine and the event bridge.
   *
   * Runs the engine's common init, which takes over the host GAP/GATT
   * callbacks; the rest of the library keeps working through forwarding.
   * Does NOT commit the GATT database; that happens in start(). Calling it
   * again while active is a no-op.
   *
   * @return BTStatus::OK on success; InvalidState on a null handle or while a
   *         standalone `BLEIso` session owns the host; another error when the
   *         engine fails to initialize.
   */
  BTStatus begin();

  /**
   * @brief Register every configured role and commit the GATT database.
   *
   * Runs each role's staged registration (PACS/ASCS/BASS/VCS/...), registers
   * the merged PACS records, then performs the single coordinated GATT
   * commit, exposing the audio services and any `BLEServer` services at once.
   * Create and configure every role before calling it. Calling it again
   * after success is a no-op.
   *
   * @return BTStatus::OK on success; InvalidState before begin(); the error of
   *         the first role or engine step that failed otherwise.
   */
  BTStatus start();

  /**
   * @brief Release every audio profile, ISO resource and all engine state.
   *
   * Refused while a stream is still up: stop the streams first. After a
   * successful end(), begin() may be called again with a new set of roles.
   * Also run by `BLE.end()`. Does not shut down the host stack.
   *
   * @return BTStatus::OK (also when not active); InvalidState while streams
   *         are active.
   */
  BTStatus end();

  /** @brief Whether begin() succeeded and end() has not been called. */
  bool isActive() const;
  /** @brief Whether start() succeeded (roles registered, GATT committed). */
  bool isStarted() const;

  /**
   * @brief Connect to a unicast server / CAP acceptor as central (non-blocking).
   *
   * Use this instead of `BLEClient::connect()` for audio peers: on Bluedroid
   * the link must be opened through the audio engine's GATT client. Once the
   * ACL is up the link is encrypted (with the `BLESecurity` settings) as
   * PACS/ASCS require, the ATT MTU is exchanged and the peer's services are
   * discovered; onLinkReady() then reports the link.
   *
   * @param address Peer address (type included).
   * @return BTStatus::OK when the connection attempt started; InvalidState
   *         before start(); another error when the host refused the attempt.
   * @note One connection attempt at a time: wait for onLinkReady() or
   *       onDisconnected() before connecting to the next peer.
   */
  BTStatus connect(const BTAddress &address);

  // --- Link events (Bluetooth host task; keep callbacks short) ---

  /**
   * @brief A peer's GATT database was discovered.
   *
   * Fires for every audio link, whether opened by connect() or by the peer.
   * Client roles (unicast client, volume controller, ...) may use
   * @p connHandle from this point on.
   */
  BLEAudio &onLinkReady(LinkCallback cb);
  /** @brief An ACL link used by LE Audio went down. */
  BLEAudio &onDisconnected(LinkCallback cb);
  /** @brief Clear the onLinkReady() and onDisconnected() callbacks. */
  void resetCallbacks();

  // --- Defaults (before creating roles) ---

  /**
   * @brief Presentation delay the roles advertise (servers) or request (clients, sources).
   * @param delayUs Delay in microseconds (default 40000; BAP presets use 40 ms).
   * @return This handle, for chaining.
   */
  BLEAudio &setPresentationDelay(uint32_t delayUs);
  /** @brief Current default presentation delay in microseconds. */
  uint32_t getPresentationDelay() const;

  // --- BAP roles (after begin(), before start()) ---

  /**
   * @brief BAP Unicast Server (ASCS + PACS): accepts unicast streams from a client.
   *
   * Configure the ASE counts, capabilities and contexts before start(); the
   * peer then configures the server's streams (see `BLEAudioStream::onConfigured`).
   */
  BLEAudioUnicastServer createUnicastServer();
  /**
   * @brief BAP Unicast Client: discovers a connected server and sets up streams on it.
   *
   * Connect with connect(), then configure streams once onLinkReady() fires.
   */
  BLEAudioUnicastClient createUnicastClient();
  /**
   * @brief BAP Broadcast Source (Auracast transmitter).
   *
   * Configure the preset, channels and broadcast metadata; the source's own
   * start() brings up the extended/periodic advertising and the BIG.
   */
  BLEAudioBroadcastSource createBroadcastSource();
  /**
   * @brief BAP Broadcast Sink (Auracast receiver), optionally a BASS Scan Delegator.
   *
   * Scans for announcements and syncs to a source; with the delegator enabled
   * a Broadcast Assistant can drive it remotely.
   */
  BLEAudioBroadcastSink createBroadcastSink();
  /**
   * @brief BAP Broadcast Assistant: finds sources for a remote sink (BASS client).
   *
   * Scans for broadcast sources and adds, modifies or removes them on a
   * connected Scan Delegator, including the Broadcast Code and PAST.
   */
  BLEAudioBroadcastAssistant createBroadcastAssistant();

  // --- CAP / CSIP (after begin(), before start()) ---

  /** @brief CAP Acceptor: publishes CAS (with an included CSIS when part of a set). */
  BLEAudioCapAcceptor createCapAcceptor();
  /** @brief CAP Initiator: starts, updates and stops audio on one or more acceptors. */
  BLEAudioCapInitiator createCapInitiator();
  /** @brief CAP Commander: coordinated volume, mute and broadcast reception control. */
  BLEAudioCapCommander createCapCommander();
  /** @brief CSIP Set Member: publishes CSIS (SIRK, set size, rank, lock). */
  BLEAudioCoordinatedSetMember createCoordinatedSetMember();
  /** @brief CSIP Set Coordinator: discovers and locks the members of a coordinated set. */
  BLEAudioCoordinatedSetCoordinator createCoordinatedSetCoordinator();

  // --- Control profiles (after begin(), before start()) ---

  /** @brief VCP Volume Renderer: publishes VCS (plus the compiled-in VOCS/AICS). */
  BLEAudioVolumeRenderer createVolumeRenderer();
  /** @brief VCP Volume Controller: drives a connected renderer's volume and mute. */
  BLEAudioVolumeController createVolumeController();
  /** @brief MICP Microphone Device: publishes MICS (plus its AICS). */
  BLEAudioMicDevice createMicDevice();
  /** @brief MICP Microphone Controller: drives a connected device's mute state. */
  BLEAudioMicController createMicController();
  /** @brief MCP Media Player: publishes the engine's media player as MCS/GMCS. */
  BLEAudioMediaPlayer createMediaPlayer();
  /** @brief MCP Media Controller: drives playback on a connected media player. */
  BLEAudioMediaController createMediaController();
  /** @brief CCP Call Server: publishes a Generic Telephone Bearer (GTBS). */
  BLEAudioCallServer createCallServer();
  /** @brief CCP Call Controller: observes and drives calls on a connected call server. */
  BLEAudioCallController createCallController();
  /** @brief HAP Hearing Aid: publishes HAS with named presets. */
  BLEAudioHearingAidDevice createHearingAidDevice();
  /** @brief HAP Hearing Aid Controller: reads and switches a connected hearing aid's presets. */
  BLEAudioHearingAidController createHearingAidController();

  // --- Top-level profiles (after begin(), before start()) ---

  /**
   * @brief TMAP identity: publishes TMAS with the local roles, or discovers a peer's.
   *
   * The audio itself still flows through CAP, BAP and the control profiles.
   */
  BLEAudioTmap createTmap();
  /** @brief GMAP identity: publishes GMAS with the local gaming roles, or discovers a peer's. */
  BLEAudioGmap createGmap();

  struct Impl;

private:
  explicit BLEAudio(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEClass;
};

// Audio component aggregator: pull the value types and role handles in so the
// whole audio API reaches sketches through the <BLE.h> umbrella. Each header
// guards itself on its own BLE_AUDIO_*_SUPPORTED symbol.
#include "audio/BLEAudioStream.h"
#include "audio/BLEAudioUnicastServer.h"
#include "audio/BLEAudioUnicastClient.h"
#include "audio/BLEAudioBroadcastSource.h"
#include "audio/BLEAudioBroadcastSink.h"
#include "audio/BLEAudioBroadcastAssistant.h"
#include "audio/BLEAudioCap.h"
#include "audio/BLEAudioCoordinatedSet.h"
#include "audio/BLEAudioVolume.h"
#include "audio/BLEAudioMic.h"
#include "audio/BLEAudioMedia.h"
#include "audio/BLEAudioCall.h"
#include "audio/BLEAudioHearingAid.h"
#include "audio/BLEAudioProfiles.h"

// Turnkey LC3 + I2S data path (guarded on BLE_AUDIO_LC3_SUPPORTED, i.e. the
// esp_audio_codec managed component being compiled in).
#include "audio/BLEAudioPlayer.h"
#include "audio/BLEAudioRecorder.h"

#endif /* BLE_AUDIO_SUPPORTED */
