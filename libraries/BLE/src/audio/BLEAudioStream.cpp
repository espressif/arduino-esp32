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

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "audio/BLEAudioStream.h"
#include "audio/BLEAudioStreamInternal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioStream.cpp
 * @brief Backend-agnostic per-stream handle and the engine stream dispatch.
 *
 * Every `BLEAudioStream::Impl` owns one slot of the engine stream pool
 * (`BLEAudioEngineBap.c`) and registers itself as that slot's `owner`. The
 * engine reports stream events through a single callback table
 * (`ble_audio_stream_cbs_t`, installed once by `BLEAudio::begin()`) and hands
 * back the owner pointer, so an event lands on its stream without any lookup.
 *
 * Each event first feeds the internal data-path tap (`BLEAudioStreamTap`,
 * used by `BLEAudioPlayer`/`BLEAudioRecorder`) and then the application
 * callback. The stream state itself (codec, QoS, direction, streaming) lives
 * in the engine slot; the accessors below only read it.
 *
 * API contract is documented on the declarations in `BLEAudioStream.h`; the
 * definitions below carry implementation notes only.
 */

namespace {

using Impl = BLEAudioStream::Impl;

// --------------------------------------------------------------------------
// Engine stream callbacks (Bluetooth host task)
// --------------------------------------------------------------------------

/**
 * @brief Run @p f with the Impl behind @p owner and a temporary public handle to it.
 *
 * The owner is the raw Impl registered with bleAudioStreamAlloc(). The Impl
 * frees its slot in its destructor, so the engine never reports an event for
 * a destroyed Impl; the weak lock only guards the window in which the last
 * handle is being released on another task. The temporary handle keeps the
 * Impl alive while the application callback runs.
 */
template<typename F> void withHandle(void *owner, F &&f) {
  auto *impl = static_cast<Impl *>(owner);
  if (!impl) {
    return;
  }
  auto sp = impl->weak_from_this().lock();
  if (!sp) {
    return;
  }
  BLEAudioStream h = BLEAudioStreamAccess::wrap(std::move(sp));
  f(*impl, h);
}

/**
 * @brief Run @p f on the installed data-path tap, if any.
 *
 * The busy count is raised before the pointer is read, so a concurrent
 * setTap()/clearTap() either sees this call in flight (and waits for it) or
 * this call already reads the new pointer: a removed tap is never entered.
 */
template<typename F> inline void withTap(Impl &impl, F &&f) {
  impl.tapBusy.fetch_add(1);
  if (const BLEAudioStreamTap *t = impl.tap.load()) {
    f(*t);
  }
  impl.tapBusy.fetch_sub(1);
}

/** @brief Wait until no host-task call is inside the tap of @p impl (tap calls are short). */
void drainTap(const Impl &impl) {
  while (impl.tapBusy.load()) {
    vTaskDelay(1);
  }
}

/** @brief Codec configured (unicast Config Codec, broadcast create or sync). */
void onConfigured(void *owner) {
  withHandle(owner, [](Impl &impl, BLEAudioStream &h) {
    if (impl.configuredCb) {
      impl.configuredCb(h);
    }
  });
}

/** @brief Stream entered Streaming: start the data path before the user callback. */
void onStarted(void *owner) {
  withHandle(owner, [](Impl &impl, BLEAudioStream &h) {
    withTap(impl, [](const BLEAudioStreamTap &t) {
      if (t.started) {
        t.started(t.ctx);
      }
    });
    if (impl.startedCb) {
      impl.startedCb(h);
    }
  });
}

/** @brief Stream left Streaming: stop the data path, then tell the application. */
void onStopped(void *owner, uint8_t reason) {
  withHandle(owner, [reason](Impl &impl, BLEAudioStream &h) {
    withTap(impl, [](const BLEAudioStreamTap &t) {
      if (t.stopped) {
        t.stopped(t.ctx);
      }
    });
    if (impl.stoppedCb) {
      impl.stoppedCb(h, reason);
    }
  });
}

/**
 * @brief Endpoint released (back to idle).
 *
 * Normally preceded by stopped(), but an ASE can also be released straight
 * from Enabling/QoS Configured or on ACL loss without a Streaming exit. The
 * data path is told either way so it ends its session immediately; the
 * application already has onStopped() and gets no extra callback.
 */
void onReleased(void *owner) {
  auto *impl = static_cast<Impl *>(owner);
  if (!impl) {
    return;
  }
  withTap(*impl, [](const BLEAudioStreamTap &t) {
    if (t.stopped) {
      t.stopped(t.ctx);
    }
  });
}

/**
 * @brief One received SDU (once per SDU interval per Rx stream).
 *
 * Hot path: the data-path tap is called straight from the raw owner, without
 * the shared_ptr lock, and the public handle is only built when the
 * application registered onReceive().
 */
void onRecv(void *owner, const ble_audio_recv_info_t *info, const uint8_t *data, uint16_t len) {
  auto *impl = static_cast<Impl *>(owner);
  if (!impl) {
    return;
  }
  withTap(*impl, [&](const BLEAudioStreamTap &t) {
    if (t.recv) {
      t.recv(t.ctx, info, data, len);
    }
  });
  if (!impl->receiveCb) {
    return;
  }
  BLEAudioSduInfo sdu;
  if (info) {
    sdu.timestamp = info->ts;
    sdu.seq = info->seq;
    sdu.status = static_cast<BLEAudioSduInfo::Status>(info->status);  // Same values (BLE_AUDIO_SDU_*).
    sdu.timestampValid = info->ts_valid;
  }
  withHandle(owner, [&](Impl &i, BLEAudioStream &h) {
    i.receiveCb(h, sdu, data, len);
  });
}

/** @brief The controller released one SDU of a Tx stream (same hot-path rules as onRecv). */
void onSent(void *owner) {
  auto *impl = static_cast<Impl *>(owner);
  if (!impl) {
    return;
  }
  withTap(*impl, [](const BLEAudioStreamTap &t) {
    if (t.sent) {
      t.sent(t.ctx);
    }
  });
  if (impl->sentCb) {
    withHandle(owner, [](Impl &i, BLEAudioStream &h) {
      i.sentCb(h);
    });
  }
}

/** Single table shared by every stream (the slot stays owned by its Impl across a release). */
const ble_audio_stream_cbs_t kCallbacks = {
  .configured = onConfigured,
  .started = onStarted,
  .stopped = onStopped,
  .released = onReleased,
  .recv = onRecv,
  .sent = onSent,
};

}  // namespace

// --------------------------------------------------------------------------
// Internal access (role handles, data path)
// --------------------------------------------------------------------------

BLEAudioStream BLEAudioStreamAccess::create(ble_audio_stream_kind_t kind) {
  auto impl = std::make_shared<BLEAudioStream::Impl>();
  impl->slot = bleAudioStreamAlloc(kind, impl.get());
  if (!impl->slot) {
    log_e("Stream: pool exhausted (kind=%d); raise the ASE/BIS counts in the LE Audio Kconfig", (int)kind);
    return BLEAudioStream();
  }
  log_d("Stream: allocated slot %p (kind=%d)", impl->slot, (int)kind);
  return BLEAudioStream(std::move(impl));
}

void BLEAudioStreamAccess::installEngineCallbacks() {
  bleAudioStreamSetCallbacks(&kCallbacks);
}

void BLEAudioStreamAccess::setTap(const BLEAudioStream &s, const BLEAudioStreamTap *tap) {
  Impl *impl = s._impl.get();
  if (!impl) {
    return;
  }
  impl->tap.store(tap);
  drainTap(*impl);
}

void BLEAudioStreamAccess::clearTap(const BLEAudioStream &s, const BLEAudioStreamTap *tap) {
  Impl *impl = s._impl.get();
  if (!impl || !tap) {
    return;
  }
  const BLEAudioStreamTap *expected = tap;
  if (impl->tap.compare_exchange_strong(expected, nullptr)) {
    drainTap(*impl);
  }
}

// --------------------------------------------------------------------------
// BLEAudioStream handle: state
// --------------------------------------------------------------------------

BLEAudioStream::BLEAudioStream() = default;

BLEAudioStream::operator bool() const {
  return _impl != nullptr;
}

/** A stream only has a direction once a codec is configured on its slot. */
BLEAudioStream::Direction BLEAudioStream::direction() const {
  ble_audio_codec_t c;
  if (!_impl || !bleAudioStreamGetCodec(_impl->slot, &c)) {
    return Direction::Unknown;
  }
  return bleAudioStreamIsTx(_impl->slot) ? Direction::Tx : Direction::Rx;
}

bool BLEAudioStream::isStreaming() const {
  return _impl && bleAudioStreamIsStreaming(_impl->slot);
}

uint16_t BLEAudioStream::connHandle() const {
  return _impl ? bleAudioStreamConnHandle(_impl->slot) : BLE_AUDIO_CONN_NONE;
}

BLEAudioCodecConfig BLEAudioStream::codecConfig() const {
  ble_audio_codec_t c;
  if (_impl && bleAudioStreamGetCodec(_impl->slot, &c)) {
    return bleAudioCodecFromEngine(c);
  }
  return BLEAudioCodecConfig();
}

BLEAudioQos BLEAudioStream::qos() const {
  ble_audio_qos_t q;
  if (_impl && bleAudioStreamGetQos(_impl->slot, &q)) {
    return bleAudioQosFromEngine(q);
  }
  return BLEAudioQos();
}

// --------------------------------------------------------------------------
// BLEAudioStream handle: data
// --------------------------------------------------------------------------

/**
 * Called once per SDU interval, so only misuse is logged: a write on an Rx
 * stream is a programming error, while "not streaming yet" is expected around
 * start/stop and is left to the returned status.
 */
BTStatus BLEAudioStream::write(const uint8_t *sdu, uint16_t len) {
  if (!_impl || !sdu || !len) {
    return BTStatus::InvalidParam;
  }
  if (!bleAudioStreamIsTx(_impl->slot)) {
    if (direction() == Direction::Rx) {
      log_e("Stream: write() on an Rx stream");
    }
    return BTStatus::InvalidState;
  }
  return bleAudioStatus(bleAudioStreamSend(_impl->slot, sdu, len));
}

// --------------------------------------------------------------------------
// BLEAudioStream handle: callbacks
// --------------------------------------------------------------------------

void BLEAudioStream::onConfigured(Callback cb) {
  if (_impl) {
    _impl->configuredCb = std::move(cb);
  }
}

void BLEAudioStream::onStarted(Callback cb) {
  if (_impl) {
    _impl->startedCb = std::move(cb);
  }
}

void BLEAudioStream::onStopped(StoppedCallback cb) {
  if (_impl) {
    _impl->stoppedCb = std::move(cb);
  }
}

void BLEAudioStream::onReceive(ReceiveCallback cb) {
  if (_impl) {
    _impl->receiveCb = std::move(cb);
  }
}

void BLEAudioStream::onSent(Callback cb) {
  if (_impl) {
    _impl->sentCb = std::move(cb);
  }
}

/** The data-path tap is internal and is left untouched. */
void BLEAudioStream::resetCallbacks() {
  if (_impl) {
    _impl->configuredCb = nullptr;
    _impl->startedCb = nullptr;
    _impl->stoppedCb = nullptr;
    _impl->receiveCb = nullptr;
    _impl->sentCb = nullptr;
  }
}

#endif /* BLE_AUDIO_SUPPORTED */
