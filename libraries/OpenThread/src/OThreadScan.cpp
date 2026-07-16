// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "OThreadScan.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_OPENTHREAD_ENABLED

#include "esp_openthread_lock.h"

#include <cstring>

namespace {

struct OtLock {
  bool held;
  explicit OtLock(TickType_t ticks = portMAX_DELAY) : held(esp_openthread_lock_acquire(ticks)) {}
  ~OtLock() {
    if (held) {
      esp_openthread_lock_release();
    }
  }
  explicit operator bool() const {
    return held;
  }
  OtLock(const OtLock &) = delete;
  OtLock &operator=(const OtLock &) = delete;
};

String formatHexLower(const uint8_t *data, size_t len) {
  String s;
  s.reserve(len * 2);
  char buf[3];
  buf[2] = '\0';
  for (size_t i = 0; i < len; ++i) {
    snprintf(buf, sizeof(buf), "%02x", data[i]);
    s += buf;
  }
  return s;
}

}  // namespace

OThreadScanClass OThreadScan;

OThreadScanClass::OThreadScanClass()
  : _timeoutMs(OT_DISCOVER_DEFAULT_TIMEOUT_MS),
    _channel(0),
    _filters(),
    _resultCb(nullptr),
    _resultCtx(nullptr),
    _completeCb(nullptr),
    _completeCtx(nullptr),
    _async(false),
    _inProgress(false),
    _triggered(false),
    _done(false),
    _startedMs(0),
    _startError(OT_ERROR_NONE),
    _doneSem(nullptr) {}

OThreadScanClass::~OThreadScanClass() {
  if (_doneSem) {
    vSemaphoreDelete(_doneSem);
    _doneSem = nullptr;
  }
}

bool OThreadScanClass::ensureDoneSem() {
  if (_doneSem) {
    return true;
  }
  _doneSem = xSemaphoreCreateBinary();
  if (_doneSem == nullptr) {
    log_e("OThreadScan: failed to create done semaphore");
    return false;
  }
  return true;
}

void OThreadScanClass::prepareResultStorage() {
  if (_results.capacity() < OT_DISCOVER_MAX_RESULTS) {
    _results.reserve(OT_DISCOVER_MAX_RESULTS);
  }
  if (_rawResults.capacity() < OT_DISCOVER_MAX_RESULTS) {
    _rawResults.reserve(OT_DISCOVER_MAX_RESULTS);
  }
}

int OThreadScanClass::findResultByExtendedPanId(const OThreadNetworkInfo &info) const {
  for (size_t i = 0; i < _results.size(); ++i) {
    if (memcmp(_results[i].extendedPanId, info.extendedPanId, OT_EXT_PAN_ID_SIZE) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void OThreadScanClass::setScanTimeout(uint32_t ms) {
  _timeoutMs = ms;
}

void OThreadScanClass::setChannel(uint8_t channel) {
  _channel = channel;
}

void OThreadScanClass::setDiscoverFilters(const OThreadDiscoverFilters &filters) {
  _filters = filters;
}

void OThreadScanClass::onResult(OThreadDiscoverResultCallback callback, void *context) {
  _resultCb = callback;
  _resultCtx = context;
}

void OThreadScanClass::onComplete(OThreadDiscoverCompleteCallback callback, void *context) {
  _completeCb = callback;
  _completeCtx = context;
}

bool OThreadScanClass::discoverChannelMask(uint32_t &mask) const {
  if (_channel == 0) {
    mask = 0;
    return true;
  }
  if (_channel < 11 || _channel > 26) {
    return false;
  }
  mask = (1U << _channel);
  return true;
}

int16_t OThreadScanClass::discoverNetworks(bool async) {
  otError error = OT_ERROR_NONE;
  uint32_t channelMask = 0;

  // Create outside OtLock: heap alloc must not run while holding the OT API lock.
  if (!ensureDoneSem()) {
    return OT_DISCOVER_FAILED;
  }
  prepareResultStorage();

  {
    OtLock lock;
    if (!lock) {
      return OT_DISCOVER_FAILED;
    }

    otInstance *inst = OThread.getInstance();
    if (!inst) {
      return OT_DISCOVER_FAILED;
    }

    if (_inProgress || otThreadIsDiscoverInProgress(inst)) {
      return OT_DISCOVER_RUNNING;
    }

    if (!discoverChannelMask(channelMask)) {
      return OT_DISCOVER_FAILED;
    }

    _results.clear();
    _rawResults.clear();
    _async = async;
    _triggered = true;
    _done = false;
    _inProgress = false;
    _startError = OT_ERROR_NONE;
    _startedMs = millis();

    if (_doneSem) {
      while (xSemaphoreTake(_doneSem, 0) == pdTRUE) {
      }
    }

    error = otThreadDiscover(inst, channelMask, _filters.panIdFilter, _filters.joinerOnly, _filters.eui64Filter,
                             handleDiscoverResult, this);
    _startError = error;
    if (error != OT_ERROR_NONE) {
      _triggered = false;
      return OT_DISCOVER_FAILED;
    }

    _inProgress = true;
  }

  if (async) {
    return OT_DISCOVER_RUNNING;
  }

  if (!_doneSem || xSemaphoreTake(_doneSem, pdMS_TO_TICKS(_timeoutMs)) != pdTRUE) {
    _inProgress = false;
    return OT_DISCOVER_FAILED;
  }

  if (_startError != OT_ERROR_NONE) {
    return OT_DISCOVER_FAILED;
  }

  return (int16_t)_results.size();
}

int16_t OThreadScanClass::scanComplete() {
  if (!_triggered) {
    return OT_DISCOVER_FAILED;
  }

  if (_done) {
    if (_startError != OT_ERROR_NONE) {
      return OT_DISCOVER_FAILED;
    }
    return (int16_t)_results.size();
  }

  // _done is set only in onDiscoverResult(nullptr). Do not infer completion from
  // otThreadIsDiscoverInProgress(): OT may be idle before the final callback runs.
  // Completion is signaled by the final discover callback (aResult == nullptr),
  // which sets _done and releases _doneSem.
  
  if (_inProgress && (millis() - _startedMs) > _timeoutMs) {
    _inProgress = false;
    return OT_DISCOVER_FAILED;
  }

  if (_inProgress) {
    return OT_DISCOVER_RUNNING;
  }

  return OT_DISCOVER_FAILED;
}

void OThreadScanClass::scanDelete() {
  // Treat discovery as active until the final otThreadDiscover callback sets
  // _done. otThreadIsDiscoverInProgress() can already be false in that window,
  // so do not use it to decide whether clearing is safe.
  if (_triggered && !_done) {
    log_d("OThreadScan: scanDelete() deferred (waiting for discover complete)");
    return;
  }

  std::vector<OThreadNetworkInfo>().swap(_results);
  std::vector<otActiveScanResult>().swap(_rawResults);
  _triggered = false;
  _done = false;
  _inProgress = false;
  _startError = OT_ERROR_NONE;
  if (_doneSem) {
    while (xSemaphoreTake(_doneSem, 0) == pdTRUE) {
    }
  }
}

bool OThreadScanClass::isDiscoverInProgress() const {
  if (_inProgress) {
    return true;
  }

  OtLock lock;
  if (!lock) {
    return _inProgress;
  }

  otInstance *inst = OThread.getInstance();
  if (!inst) {
    return false;
  }

  return otThreadIsDiscoverInProgress(inst);
}

uint16_t OThreadScanClass::getResultCount() const {
  if (!resultsAvailable()) {
    return 0;
  }
  const size_t n = _results.size();
  return (n > UINT16_MAX) ? UINT16_MAX : static_cast<uint16_t>(n);
}

const OThreadNetworkInfo &OThreadScanClass::getResult(uint16_t index) const {
  static const OThreadNetworkInfo kEmpty{};
  if (!resultsAvailable() || index >= _results.size()) {
    return kEmpty;
  }
  return _results[index];
}

bool OThreadScanClass::getResult(uint16_t index, OThreadNetworkInfo &info) const {
  if (!resultsAvailable() || index >= _results.size()) {
    return false;
  }
  info = _results[index];
  return true;
}

String OThreadScanClass::networkName(uint16_t index) {
  return getResult(index).networkNameStr();
}

uint16_t OThreadScanClass::panId(uint16_t index) {
  return getResult(index).panId;
}

String OThreadScanClass::extendedPanIdStr(uint16_t index) {
  return getResult(index).extendedPanIdStr();
}

String OThreadScanClass::extAddressStr(uint16_t index) {
  return getResult(index).extAddressStr();
}

bool OThreadScanClass::extAddress(uint16_t index, uint8_t address[OT_EXT_ADDRESS_SIZE]) {
  if (!resultsAvailable() || index >= _results.size()) {
    return false;
  }
  memcpy(address, _results[index].extAddress, OT_EXT_ADDRESS_SIZE);
  return true;
}

bool OThreadScanClass::extendedPanId(uint16_t index, uint8_t extPanId[OT_EXT_PAN_ID_SIZE]) {
  if (!resultsAvailable() || index >= _results.size()) {
    return false;
  }
  memcpy(extPanId, _results[index].extendedPanId, OT_EXT_PAN_ID_SIZE);
  return true;
}

uint8_t OThreadScanClass::channel(uint16_t index) {
  return getResult(index).channel;
}

int8_t OThreadScanClass::rssi(uint16_t index) {
  return getResult(index).rssi;
}

uint8_t OThreadScanClass::lqi(uint16_t index) {
  return getResult(index).lqi;
}

bool OThreadScanClass::isJoinable(uint16_t index) {
  if (!resultsAvailable() || index >= _results.size()) {
    return false;
  }
  return _results[index].joinable;
}

uint8_t OThreadScanClass::threadVersion(uint16_t index) {
  return getResult(index).threadVersion;
}

bool OThreadScanClass::isNativeCommissioner(uint16_t index) {
  if (!resultsAvailable() || index >= _results.size()) {
    return false;
  }
  return _results[index].nativeCommissioner;
}

const otActiveScanResult *OThreadScanClass::getActiveScanResult(uint16_t index) const {
  if (!resultsAvailable() || index >= _rawResults.size()) {
    return nullptr;
  }
  return &_rawResults[index];
}

void OThreadScanClass::handleDiscoverResult(otActiveScanResult *aResult, void *aContext) {
  static_cast<OThreadScanClass *>(aContext)->onDiscoverResult(aResult);
}

void OThreadScanClass::onDiscoverResult(otActiveScanResult *aResult) {
  if (aResult != nullptr) {
    const OThreadNetworkInfo info = fromActiveScanResult(*aResult);

    const int existing = findResultByExtendedPanId(info);
    if (existing >= 0) {
      // Same Thread network from another router/beacon; keep the strongest RSSI.
      if (info.rssi > _results[static_cast<size_t>(existing)].rssi) {
        _results[static_cast<size_t>(existing)] = info;
        _rawResults[static_cast<size_t>(existing)] = *aResult;
      }
      // Still stream every Discovery Response (Matter / OpenThread model).
      if (_resultCb) {
        _resultCb(info, _resultCtx);
      }
      return;
    }

    if (_results.size() >= OT_DISCOVER_MAX_RESULTS) {
      log_w("OThreadScan: result storage full (%u); streaming only", (unsigned)OT_DISCOVER_MAX_RESULTS);
      if (_resultCb) {
        _resultCb(info, _resultCtx);
      }
      return;
    }
    // Vectors were pre-reserved in discoverNetworks(); push_back must not realloc here.
    _results.push_back(info);
    _rawResults.push_back(*aResult);
    if (_resultCb) {
      _resultCb(info, _resultCtx);
    }
    return;
  }

  _inProgress = false;
  _done = true;
  if (_doneSem) {
    xSemaphoreGive(_doneSem);
  }
  if (_completeCb) {
    const int16_t count = (_startError == OT_ERROR_NONE) ? (int16_t)_results.size() : OT_DISCOVER_FAILED;
    _completeCb(count, _startError, _completeCtx);
  }
}

OThreadNetworkInfo OThreadScanClass::fromActiveScanResult(const otActiveScanResult &in) {
  OThreadNetworkInfo out{};
  strncpy(out.networkName, in.mNetworkName.m8, OT_NETWORK_NAME_MAX_SIZE);
  out.networkName[OT_NETWORK_NAME_MAX_SIZE] = '\0';
  memcpy(out.extendedPanId, in.mExtendedPanId.m8, OT_EXT_PAN_ID_SIZE);
  out.panId = in.mPanId;
  memcpy(out.extAddress, in.mExtAddress.m8, OT_EXT_ADDRESS_SIZE);
  out.channel = in.mChannel;
  out.rssi = in.mRssi;
  out.lqi = in.mLqi;
  out.threadVersion = (uint8_t)in.mVersion;
  out.joinable = in.mIsJoinable;
  out.nativeCommissioner = in.mIsNative;
  return out;
}

String OThreadNetworkInfo::extendedPanIdStr() const {
  return formatHexLower(extendedPanId, OT_EXT_PAN_ID_SIZE);
}

String OThreadNetworkInfo::extAddressStr() const {
  return formatHexLower(extAddress, OT_EXT_ADDRESS_SIZE);
}

#endif /* SOC_IEEE802154_SUPPORTED && CONFIG_OPENTHREAD_ENABLED */
