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

#include "OThreadDNSSD.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED
#if CONFIG_OPENTHREAD_SRP_CLIENT

#include "esp_openthread_lock.h"
#include <cstring>
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
#include <openthread/dns.h>
#include <openthread/ip6.h>
#endif

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

void stripLeadingUnderscores(char *label) {
  size_t i = 0;
  while (label[i] == '_') {
    ++i;
  }
  if (i > 0) {
    memmove(label, label + i, strlen(label + i) + 1);
  }
}

#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
constexpr const char *kDnssdDomain = "default.service.arpa";
// Length-prefixed TXT rdata for the stored pool (key + '=' + value per entry).
constexpr size_t kTxtScratchSize = OT_DNSSD_MAX_TXT_ENTRIES * (OT_DNSSD_TXT_KEY_MAX + OT_DNSSD_TXT_VALUE_MAX + 2) + 16;

inline IPAddress otToIp(const otIp6Address &in) {
  return IPAddress(IPv6, in.mFields.m8);
}

inline bool isUnspecified(const otIp6Address &addr) {
  for (int i = 0; i < OT_IP6_ADDRESS_SIZE; ++i) {
    if (addr.mFields.m8[i] != 0) {
      return false;
    }
  }
  return true;
}
#endif

}  // namespace

OThreadDNSSDClass OThreadDNSSD;

OThreadDNSSDClass::OThreadDNSSDClass()
  : _started(false), _announceComplete(false), _lastError(OT_ERROR_NONE), _instanceNameSet(false), _eventCb(nullptr), _eventCtx(nullptr), _announceSem(nullptr)
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
    ,
    _queryResultCount(0), _dnsSem(nullptr), _dnsDone(false), _dnsServiceResolveIdx(-1), _queryInProgress(false), _queryAsync(false),
    _queryKind(OT_DNSSD_QUERY_SERVICE), _queryGen(0), _queryActiveGen(0), _queryCb(nullptr), _queryCtx(nullptr)
#endif
{
  _hostName[0] = '\0';
  _instanceName[0] = '\0';
  resetSlots();
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
  resetQueryResults();
  _dnsServiceFqdn[0] = '\0';
  _dnsResolvedAddr = IPAddress(IPv6);
  memset(_dnsCbCtx, 0, sizeof(_dnsCbCtx));
#endif
}

OThreadDNSSDClass::~OThreadDNSSDClass() {
  if (_started) {
    end();
  }
  if (_announceSem) {
    vSemaphoreDelete(_announceSem);
    _announceSem = nullptr;
  }
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
  if (_dnsSem) {
    vSemaphoreDelete(_dnsSem);
    _dnsSem = nullptr;
  }
#endif
}

bool OThreadDNSSDClass::ensureAnnounceSem() {
  if (_announceSem) {
    return true;
  }
  _announceSem = xSemaphoreCreateBinary();
  if (!_announceSem) {
    log_e("OThreadDNSSD: failed to create announce semaphore");
    return false;
  }
  return true;
}

void OThreadDNSSDClass::resetSlots() {
  for (uint8_t i = 0; i < OT_DNSSD_MAX_SERVICES; ++i) {
    ServiceSlot &s = _services[i];
    memset(&s, 0, sizeof(s));
  }
}

void OThreadDNSSDClass::clearAnnounceState() {
  _announceComplete = false;
  _lastError = OT_ERROR_NONE;
  if (_announceSem) {
    while (xSemaphoreTake(_announceSem, 0) == pdTRUE) {}
  }
}

bool OThreadDNSSDClass::copyCString(char *dst, size_t dstSize, const char *src) const {
  if (!dst || dstSize == 0 || !src || src[0] == '\0') {
    return false;
  }
  size_t n = strlen(src);
  if (n >= dstSize) {
    return false;
  }
  memcpy(dst, src, n + 1);
  return true;
}

bool OThreadDNSSDClass::buildServiceName(char *dst, size_t dstSize, const char *service, const char *proto) {
  if (!dst || !service || !proto || service[0] == '\0' || proto[0] == '\0') {
    return false;
  }
  char svc[OT_DNSSD_LABEL_MAX + 1];
  char prt[OT_DNSSD_LABEL_MAX + 1];
  if (!copyCString(svc, sizeof(svc), service) || !copyCString(prt, sizeof(prt), proto)) {
    return false;
  }
  stripLeadingUnderscores(svc);
  stripLeadingUnderscores(prt);
  if (svc[0] == '\0' || prt[0] == '\0') {
    return false;
  }
  int n = snprintf(dst, dstSize, "_%s._%s", svc, prt);
  return n > 0 && (size_t)n < dstSize;
}

int OThreadDNSSDClass::findServiceIndex(const char *serviceName) const {
  for (uint8_t i = 0; i < OT_DNSSD_MAX_SERVICES; ++i) {
    if (_services[i].used && strcmp(_services[i].serviceName, serviceName) == 0) {
      return (int)i;
    }
  }
  return -1;
}

int OThreadDNSSDClass::allocServiceIndex() {
  for (uint8_t i = 0; i < OT_DNSSD_MAX_SERVICES; ++i) {
    if (!_services[i].used) {
      return (int)i;
    }
  }
  return -1;
}

void OThreadDNSSDClass::wireOtService(ServiceSlot &slot) {
  memset(&slot.otService, 0, sizeof(slot.otService));
  slot.otService.mName = slot.serviceName;
  slot.otService.mInstanceName = slot.instanceName;
  slot.otService.mPort = slot.port;
  slot.otService.mPriority = 0;
  slot.otService.mWeight = 0;
  slot.otService.mLease = 0;
  slot.otService.mKeyLease = 0;

  if (slot.numSubtypes > 0) {
    for (uint8_t i = 0; i < slot.numSubtypes; ++i) {
      slot.subtypePtrs[i] = slot.subtypes[i];
    }
    slot.subtypePtrs[slot.numSubtypes] = nullptr;
    slot.otService.mSubTypeLabels = slot.subtypePtrs;
  } else {
    slot.otService.mSubTypeLabels = nullptr;
  }

  slot.numTxt = 0;
  for (uint8_t i = 0; i < OT_DNSSD_MAX_TXT_ENTRIES; ++i) {
    if (!slot.txt[i].used) {
      continue;
    }
    otDnsTxtEntry &e = slot.txtEntries[slot.numTxt];
    e.mKey = slot.txt[i].key;
    // NULL mValue => boolean TXT "key"; non-NULL + length 0 => "key=".
    e.mValue = (slot.txt[i].valueLen > 0) ? slot.txt[i].value : nullptr;
    e.mValueLength = slot.txt[i].valueLen;
    slot.numTxt++;
  }
  slot.otService.mTxtEntries = (slot.numTxt > 0) ? slot.txtEntries : nullptr;
  slot.otService.mNumTxtEntries = slot.numTxt;
}

bool OThreadDNSSDClass::pushServiceToOt(ServiceSlot &slot) {
  otInstance *inst = OThread.getInstance();
  if (!inst) {
    return false;
  }
  wireOtService(slot);
  otError err = otSrpClientAddService(inst, &slot.otService);
  if (err != OT_ERROR_NONE) {
    log_e("OThreadDNSSD: AddService failed (%d)", (int)err);
    _lastError = err;
    return false;
  }
  slot.registeredWithOt = true;
  slot.pendingRemove = false;
  clearAnnounceState();
  return true;
}

bool OThreadDNSSDClass::updateServiceOnOt(ServiceSlot &slot) {
  otInstance *inst = OThread.getInstance();
  if (!inst) {
    return false;
  }
  if (slot.registeredWithOt) {
    otError cerr = otSrpClientClearService(inst, &slot.otService);
    if (cerr != OT_ERROR_NONE && cerr != OT_ERROR_NOT_FOUND) {
      log_w("OThreadDNSSD: ClearService (%d)", (int)cerr);
    }
    slot.registeredWithOt = false;
  }
  return pushServiceToOt(slot);
}

bool OThreadDNSSDClass::begin(const char *hostName) {
  if (_started) {
    end();
  }
  if (OThread.isAttachedToExternalStack()) {
    log_e("OThreadDNSSD: begin() is not supported while attached to an external OpenThread stack (Matter/CHIP owns SRP)");
    return false;
  }
  otInstance *inst = OThread.getInstance();
  if (!inst) {
    log_e("OThreadDNSSD: OpenThread not started");
    return false;
  }
  if (!hostName || hostName[0] == '\0') {
    return false;
  }
  if (!copyCString(_hostName, sizeof(_hostName), hostName)) {
    log_e("OThreadDNSSD: host name too long (max %u)", (unsigned)OT_DNSSD_HOST_NAME_MAX);
    return false;
  }
  if (!ensureAnnounceSem()) {
    _hostName[0] = '\0';
    return false;
  }

  resetSlots();
  _instanceNameSet = false;
  _instanceName[0] = '\0';
  clearAnnounceState();

  OtLock lock;
  if (!lock) {
    log_e("OThreadDNSSD: failed to acquire OT lock");
    _hostName[0] = '\0';
    return false;
  }

  otSrpClientSetCallback(inst, handleSrpCallback, this);
  // Drop any local client state left from a previous session.
  otSrpClientClearHostAndServices(inst);

  otError err = otSrpClientSetHostName(inst, _hostName);
  if (err != OT_ERROR_NONE) {
    log_e("OThreadDNSSD: SetHostName failed (%d)", (int)err);
    otSrpClientSetCallback(inst, nullptr, nullptr);
    _lastError = err;
    _hostName[0] = '\0';
    return false;
  }

  err = otSrpClientEnableAutoHostAddress(inst);
  if (err != OT_ERROR_NONE) {
    log_e("OThreadDNSSD: EnableAutoHostAddress failed (%d)", (int)err);
    otSrpClientClearHostAndServices(inst);
    otSrpClientSetCallback(inst, nullptr, nullptr);
    _lastError = err;
    _hostName[0] = '\0';
    return false;
  }

  // SRP auto-start also updates the DNS client default server when DNS is built in.
  otSrpClientEnableAutoStartMode(inst, nullptr, nullptr);
  _started = true;
  return true;
}

void OThreadDNSSDClass::end() {
  // Must run before OThread::end() clears the attach flag.
  teardown(!OThread.isAttachedToExternalStack());
}

void OThreadDNSSDClass::teardown(bool ownSrpClient) {
  if (!_started) {
    return;
  }
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
  bool abortAsync = false;
  ot_dnssd_query_kind_t abortKind = OT_DNSSD_QUERY_SERVICE;
#endif
  {
    OtLock lock;
    otInstance *inst = OThread.getInstance();
    // Hold the OT lock through DNS invalidation so an in-flight DNS callback
    // cannot commit after we have torn down.
    if (inst && lock) {
      if (ownSrpClient) {
        // Arduino owns this instance: unregister toward the SRP server and
        // stop the client so OpenThread drops pointers into our slots.
        (void)otSrpClientRemoveHostAndServices(inst, true, true);
        otSrpClientClearHostAndServices(inst);
        otSrpClientStop(inst);
      }
      // Always drop our callback so OT cannot call into a dead sketch object.
      otSrpClientSetCallback(inst, nullptr, nullptr);
    } else if (inst && !lock) {
      log_e("OThreadDNSSD: end() failed to acquire OT lock; clearing local state anyway");
    }
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
    abortAsync = _queryInProgress && _queryAsync;
    abortKind = _queryKind;
#endif
    resetSlots();
    _started = false;
    _announceComplete = false;
    _hostName[0] = '\0';
    _instanceName[0] = '\0';
    _instanceNameSet = false;
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
    clearQueryOp();
    resetQueryResults();
    _dnsResolvedAddr = IPAddress(IPv6);
    _dnsServiceFqdn[0] = '\0';
#endif
  }
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
  if (abortAsync) {
    _queryKind = abortKind;
    notifyQueryEvent(OT_DNSSD_QUERY_ERROR, OT_ERROR_ABORT, 0);
  } else if (_dnsSem) {
    xSemaphoreGive(_dnsSem);
  }
#endif
  if (ownSrpClient) {
    notifyEvent(OT_DNSSD_EVENT_REMOVED, OT_ERROR_NONE);
  }
}

void OThreadDNSSDClass::setInstanceName(const char *name) {
  if (!name || name[0] == '\0') {
    _instanceNameSet = false;
    _instanceName[0] = '\0';
    return;
  }
  if (!copyCString(_instanceName, sizeof(_instanceName), name)) {
    log_e("OThreadDNSSD: instance name too long (max %u)", (unsigned)OT_DNSSD_INSTANCE_NAME_MAX);
    return;
  }
  _instanceNameSet = true;
}

bool OThreadDNSSDClass::addService(const char *service, const char *proto, uint16_t port) {
  if (!_started) {
    log_e("OThreadDNSSD: addService without begin()");
    return false;
  }
  char serviceName[OT_DNSSD_SERVICE_NAME_MAX + 1];
  if (!buildServiceName(serviceName, sizeof(serviceName), service, proto)) {
    return false;
  }

  OtLock lock;
  if (!lock) {
    return false;
  }

  int idx = findServiceIndex(serviceName);
  bool updating = (idx >= 0);
  if (updating && _services[idx].pendingRemove) {
    log_w("OThreadDNSSD: addService while remove is in progress");
    return false;
  }
  if (!updating) {
    idx = allocServiceIndex();
    if (idx < 0) {
      log_w("OThreadDNSSD: service storage full (%u)", (unsigned)OT_DNSSD_MAX_SERVICES);
      return false;
    }
  }

  ServiceSlot &slot = _services[idx];
  if (!updating) {
    memset(&slot, 0, sizeof(slot));
    slot.used = true;
    memcpy(slot.serviceName, serviceName, strlen(serviceName) + 1);
  }

  const char *instName = _instanceNameSet ? _instanceName : _hostName;
  if (!copyCString(slot.instanceName, sizeof(slot.instanceName), instName)) {
    if (!updating) {
      slot.used = false;
    }
    return false;
  }
  slot.port = port;

  if (updating) {
    return updateServiceOnOt(slot);
  }
  return pushServiceToOt(slot);
}

bool OThreadDNSSDClass::addServiceTxt(const char *service, const char *proto, const char *key, const char *value) {
  if (!_started || !key || key[0] == '\0') {
    return false;
  }
  char serviceName[OT_DNSSD_SERVICE_NAME_MAX + 1];
  if (!buildServiceName(serviceName, sizeof(serviceName), service, proto)) {
    return false;
  }

  OtLock lock;
  if (!lock) {
    return false;
  }

  int idx = findServiceIndex(serviceName);
  if (idx < 0) {
    log_e("OThreadDNSSD: addServiceTxt: service not found (addService first)");
    return false;
  }
  ServiceSlot &slot = _services[idx];
  if (slot.pendingRemove) {
    log_w("OThreadDNSSD: addServiceTxt while remove is in progress");
    return false;
  }

  int freeTxt = -1;
  for (uint8_t i = 0; i < OT_DNSSD_MAX_TXT_ENTRIES; ++i) {
    if (slot.txt[i].used && strcmp(slot.txt[i].key, key) == 0) {
      freeTxt = (int)i;  // update existing key
      break;
    }
    if (!slot.txt[i].used && freeTxt < 0) {
      freeTxt = (int)i;
    }
  }
  if (freeTxt < 0) {
    log_w("OThreadDNSSD: TXT storage full (%u)", (unsigned)OT_DNSSD_MAX_TXT_ENTRIES);
    return false;
  }

  size_t vlen = 0;
  if (value && value[0] != '\0') {
    vlen = strlen(value);
    if (vlen > OT_DNSSD_TXT_VALUE_MAX) {
      return false;
    }
  }

  TxtSlot &t = slot.txt[freeTxt];
  TxtSlot saved = t;
  if (!copyCString(t.key, sizeof(t.key), key)) {
    return false;
  }
  t.valueLen = 0;
  if (vlen > 0) {
    memcpy(t.value, value, vlen);
    t.valueLen = (uint16_t)vlen;
  }
  t.used = true;

  if (!updateServiceOnOt(slot)) {
    t = saved;
    return false;
  }
  return true;
}

bool OThreadDNSSDClass::addServiceSubtype(const char *service, const char *proto, const char *subtype) {
  if (!_started || !subtype || subtype[0] == '\0') {
    return false;
  }
  char serviceName[OT_DNSSD_SERVICE_NAME_MAX + 1];
  if (!buildServiceName(serviceName, sizeof(serviceName), service, proto)) {
    return false;
  }

  OtLock lock;
  if (!lock) {
    return false;
  }

  int idx = findServiceIndex(serviceName);
  if (idx < 0) {
    return false;
  }
  ServiceSlot &slot = _services[idx];
  if (slot.pendingRemove) {
    log_w("OThreadDNSSD: addServiceSubtype while remove is in progress");
    return false;
  }

  char normalized[OT_DNSSD_SUBTYPE_MAX + 1];
  if (!copyCString(normalized, sizeof(normalized), subtype)) {
    return false;
  }
  stripLeadingUnderscores(normalized);
  if (normalized[0] == '\0') {
    return false;
  }
  for (uint8_t i = 0; i < slot.numSubtypes; ++i) {
    if (strcmp(slot.subtypes[i], normalized) == 0) {
      return true;  // already present
    }
  }
  if (slot.numSubtypes >= OT_DNSSD_MAX_SUBTYPES) {
    log_w("OThreadDNSSD: subtype storage full (%u)", (unsigned)OT_DNSSD_MAX_SUBTYPES);
    return false;
  }
  char *dst = slot.subtypes[slot.numSubtypes];
  memcpy(dst, normalized, strlen(normalized) + 1);
  slot.numSubtypes++;
  if (!updateServiceOnOt(slot)) {
    slot.numSubtypes--;
    dst[0] = '\0';
    return false;
  }
  return true;
}

bool OThreadDNSSDClass::removeService(const char *service, const char *proto) {
  if (!_started) {
    return false;
  }
  char serviceName[OT_DNSSD_SERVICE_NAME_MAX + 1];
  if (!buildServiceName(serviceName, sizeof(serviceName), service, proto)) {
    return false;
  }

  OtLock lock;
  if (!lock) {
    return false;
  }

  int idx = findServiceIndex(serviceName);
  if (idx < 0) {
    return false;
  }
  ServiceSlot &slot = _services[idx];
  if (slot.pendingRemove) {
    return true;  // already queued
  }
  otInstance *inst = OThread.getInstance();
  if (!inst) {
    return false;
  }

  clearAnnounceState();
  if (slot.registeredWithOt) {
    otError err = otSrpClientRemoveService(inst, &slot.otService);
    if (err != OT_ERROR_NONE) {
      log_e("OThreadDNSSD: RemoveService failed (%d)", (int)err);
      _lastError = err;
      // Drop the client-list entry without a server update. OT documents only
      // NONE (pointer released) or NOT_FOUND (never tracked). Either is safe
      // to memset; any other error means the stack may still hold the slot.
      otError cerr = otSrpClientClearService(inst, &slot.otService);
      if (cerr != OT_ERROR_NONE && cerr != OT_ERROR_NOT_FOUND) {
        log_e("OThreadDNSSD: ClearService failed (%d)", (int)cerr);
        _lastError = cerr;
        return false;
      }
      memset(&slot, 0, sizeof(slot));
      return true;  // cleared locally (see removeService() contract)
    }
    // RemoveService is asynchronous: OT keeps using name/TXT pointers in this
    // slot until the service appears in aRemovedServices. Do not memset yet.
    slot.pendingRemove = true;
    return true;
  }

  memset(&slot, 0, sizeof(slot));
  return true;
}

void OThreadDNSSDClass::onServiceEvent(OThreadDNSSDEventCallback callback, void *context) {
  _eventCb = callback;
  _eventCtx = context;
}

bool OThreadDNSSDClass::isAnnounceComplete() {
  if (!_started) {
    return false;
  }
  return syncAnnounceCompleteFromOt();
}

bool OThreadDNSSDClass::waitForAnnounce(uint32_t timeoutMs) {
  if (!_started) {
    return false;
  }
  if (!ensureAnnounceSem()) {
    return false;
  }

  // Poll live SRP item state while waiting so a late Registered transition is
  // visible even if the announce semaphore was already consumed (e.g. after a
  // prior terminal error in a previous wait — sketches typically wait once).
  const uint32_t startMs = millis();
  for (;;) {
    if (syncAnnounceCompleteFromOt()) {
      return true;
    }
    if (isTerminalSrpError(_lastError)) {
      return false;
    }

    uint32_t elapsed = millis() - startMs;
    if (timeoutMs != UINT32_MAX && elapsed >= timeoutMs) {
      return syncAnnounceCompleteFromOt();
    }

    uint32_t sliceMs = 100;
    if (timeoutMs != UINT32_MAX) {
      uint32_t remain = timeoutMs - elapsed;
      if (remain < sliceMs) {
        sliceMs = remain ? remain : 1;
      }
    }
    (void)xSemaphoreTake(_announceSem, pdMS_TO_TICKS(sliceMs));
  }
}

void OThreadDNSSDClass::notifyEvent(ot_dnssd_event_t event, otError error) {
  if (_eventCb) {
    _eventCb(event, error, _eventCtx);
  }
}

void OThreadDNSSDClass::reclaimRemovedServices(const otSrpClientService *removedServices) {
  for (const otSrpClientService *s = removedServices; s != nullptr; s = s->mNext) {
    for (uint8_t i = 0; i < OT_DNSSD_MAX_SERVICES; ++i) {
      ServiceSlot &slot = _services[i];
      if (slot.used && &slot.otService == s) {
        memset(&slot, 0, sizeof(slot));
        break;
      }
    }
  }
}

bool OThreadDNSSDClass::evaluateAnnounceComplete(const otSrpClientHostInfo *hostInfo, const otSrpClientService *services) const {
  if (!hostInfo || hostInfo->mState != OT_SRP_CLIENT_ITEM_STATE_REGISTERED) {
    return false;
  }
  bool anyService = false;
  bool allRegistered = true;
  for (const otSrpClientService *s = services; s != nullptr; s = s->mNext) {
    anyService = true;
    if (s->mState != OT_SRP_CLIENT_ITEM_STATE_REGISTERED) {
      allRegistered = false;
      break;
    }
  }
  // Active local services only (pending removes still occupy a slot but are not
  // required to be Registered for announce-complete).
  bool haveLocal = false;
  for (uint8_t i = 0; i < OT_DNSSD_MAX_SERVICES; ++i) {
    if (_services[i].used && !_services[i].pendingRemove) {
      haveLocal = true;
      break;
    }
  }
  if (!haveLocal) {
    return false;
  }
  return anyService && allRegistered;
}

void OThreadDNSSDClass::refreshAnnounceFlag(const otSrpClientHostInfo *hostInfo, const otSrpClientService *services) {
  _announceComplete = evaluateAnnounceComplete(hostInfo, services);
}

bool OThreadDNSSDClass::syncAnnounceCompleteFromOt() {
  if (!_started) {
    _announceComplete = false;
    return false;
  }
  otInstance *inst = OThread.getInstance();
  if (!inst) {
    _announceComplete = false;
    return false;
  }
  OtLock lock;
  if (!lock) {
    return _announceComplete;
  }
  _announceComplete = evaluateAnnounceComplete(otSrpClientGetHostInfo(inst), otSrpClientGetServices(inst));
  return _announceComplete;
}

bool OThreadDNSSDClass::isTerminalSrpError(otError error) {
  // Name already owned by another SRP key (cleared NVS or another device), or
  // signature/key reject. Retrying the same labels will not succeed — sketch must act.
  return error == OT_ERROR_DUPLICATED || error == OT_ERROR_SECURITY;
}

void OThreadDNSSDClass::handleSrpCallback(
  otError aError, const otSrpClientHostInfo *aHostInfo, const otSrpClientService *aServices, const otSrpClientService *aRemovedServices, void *aContext
) {
  static_cast<OThreadDNSSDClass *>(aContext)->onSrpCallback(aError, aHostInfo, aServices, aRemovedServices);
}

void OThreadDNSSDClass::onSrpCallback(
  otError aError, const otSrpClientHostInfo *aHostInfo, const otSrpClientService *aServices, const otSrpClientService *aRemovedServices
) {
  // OT is done with removed entries — reclaim slot storage (name/TXT buffers).
  reclaimRemovedServices(aRemovedServices);

  _lastError = aError;

  if (aError != OT_ERROR_NONE) {
    _announceComplete = false;
    notifyEvent(OT_DNSSD_EVENT_ERROR, aError);
    if (isTerminalSrpError(aError)) {
      if (aError == OT_ERROR_DUPLICATED) {
        log_e(
          "OThreadDNSSD: SRP name conflict for host '%s' (OT_ERROR_DUPLICATED). "
          "Use a unique hostname (e.g. include MAC), keep NVS across reflash, "
          "or clear the stale host on the OTBR.",
          _hostName
        );
      } else {
        log_e("OThreadDNSSD: SRP security error for host '%s' (%d)", _hostName, (int)aError);
      }
      // Unblock waitForAnnounce with failure; do not rename or retry policy here.
      // OpenThread may still reach Registered later (e.g. after OTBR clears the
      // name); sketches can poll isAnnounceComplete() which reads live OT state.
      if (_announceSem) {
        xSemaphoreGive(_announceSem);
      }
    }
    // Transient errors (e.g. RESPONSE_TIMEOUT): OT client retries; keep waiting.
    return;
  }

  bool wasComplete = _announceComplete;
  refreshAnnounceFlag(aHostInfo, aServices);

  const bool hostRemoved = aHostInfo && aHostInfo->mState == OT_SRP_CLIENT_ITEM_STATE_REMOVED;
  bool haveLocal = false;
  for (uint8_t i = 0; i < OT_DNSSD_MAX_SERVICES; ++i) {
    if (_services[i].used && !_services[i].pendingRemove) {
      haveLocal = true;
      break;
    }
  }
  // REMOVED means the host (or the last local service) is gone — not a
  // single-service delete that still leaves others registered (that would
  // otherwise be followed by ANNOUNCED in the same callback).
  if (hostRemoved || (aRemovedServices != nullptr && !haveLocal)) {
    notifyEvent(OT_DNSSD_EVENT_REMOVED, OT_ERROR_NONE);
    if (hostRemoved) {
      return;
    }
  }

  if (_announceComplete && !wasComplete) {
    notifyEvent(OT_DNSSD_EVENT_ANNOUNCED, OT_ERROR_NONE);
    if (_announceSem) {
      xSemaphoreGive(_announceSem);
    }
  }
}

#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT

bool OThreadDNSSDClass::ensureDnsSem() {
  if (_dnsSem) {
    return true;
  }
  _dnsSem = xSemaphoreCreateBinary();
  if (!_dnsSem) {
    log_e("OThreadDNSSD: failed to create DNS semaphore");
    return false;
  }
  return true;
}

void OThreadDNSSDClass::resetQueryResults() {
  for (uint8_t i = 0; i < OT_DNSSD_MAX_QUERY_RESULTS; ++i) {
    QueryResultSlot &slot = _queryResults[i];
    slot.used = false;
    slot.instanceName[0] = '\0';
    slot.hostName[0] = '\0';
    slot.address = IPAddress(IPv6);
    slot.port = 0;
    slot.numTxt = 0;
    for (uint8_t t = 0; t < OT_DNSSD_MAX_TXT_ENTRIES; ++t) {
      slot.txt[t].used = false;
      slot.txt[t].key[0] = '\0';
      slot.txt[t].value[0] = '\0';
    }
  }
  _queryResultCount = 0;
}

void OThreadDNSSDClass::copyHostLabel(char *dst, size_t dstSize, const char *fqdnOrLabel) {
  if (!dst || dstSize == 0) {
    return;
  }
  dst[0] = '\0';
  if (!fqdnOrLabel || fqdnOrLabel[0] == '\0') {
    return;
  }
  size_t n = 0;
  while (fqdnOrLabel[n] != '\0' && fqdnOrLabel[n] != '.' && n + 1 < dstSize) {
    dst[n] = fqdnOrLabel[n];
    n++;
  }
  dst[n] = '\0';
}

void OThreadDNSSDClass::fillTxtFromDnsData(QueryResultSlot &slot, const uint8_t *txtData, uint16_t txtLen) {
  slot.numTxt = 0;
  for (uint8_t i = 0; i < OT_DNSSD_MAX_TXT_ENTRIES; ++i) {
    slot.txt[i].used = false;
    slot.txt[i].key[0] = '\0';
    slot.txt[i].value[0] = '\0';
  }
  if (!txtData || txtLen == 0) {
    return;
  }
  otDnsTxtEntryIterator iter;
  otDnsInitTxtEntryIterator(&iter, txtData, txtLen);
  otDnsTxtEntry entry;
  while (otDnsGetNextTxtEntry(&iter, &entry) == OT_ERROR_NONE && slot.numTxt < OT_DNSSD_MAX_TXT_ENTRIES) {
    QueryTxtSlot &t = slot.txt[slot.numTxt];
    if (!entry.mKey) {
      continue;
    }
    if (!copyCString(t.key, sizeof(t.key), entry.mKey)) {
      continue;
    }
    t.value[0] = '\0';
    if (entry.mValue && entry.mValueLength > 0) {
      size_t n = entry.mValueLength;
      if (n > OT_DNSSD_TXT_VALUE_MAX) {
        n = OT_DNSSD_TXT_VALUE_MAX;
      }
      memcpy(t.value, entry.mValue, n);
      t.value[n] = '\0';
    }
    t.used = true;
    slot.numTxt++;
  }
}

void OThreadDNSSDClass::handleDnsBrowseCallback(otError aError, const otDnsBrowseResponse *aResponse, void *aContext) {
  DnsCbCtx *ctx = static_cast<DnsCbCtx *>(aContext);
  if (!ctx || !ctx->used || !ctx->self) {
    return;
  }
  OThreadDNSSDClass *self = ctx->self;
  const uint32_t gen = ctx->gen;
  self->freeDnsCbCtx(ctx);
  self->onDnsBrowseCallback(aError, aResponse, gen);
}

void OThreadDNSSDClass::onDnsBrowseCallback(otError aError, const otDnsBrowseResponse *aResponse, uint32_t gen) {
  if (!isQueryCallbackCurrent(gen)) {
    return;
  }

  _lastError = aError;
  if (aError == OT_ERROR_NONE && aResponse) {
    for (uint16_t i = 0; i < OT_DNSSD_MAX_QUERY_RESULTS; ++i) {
      char label[OT_DNSSD_INSTANCE_NAME_MAX + 1];
      otError err = otDnsBrowseResponseGetServiceInstance(aResponse, i, label, sizeof(label));
      if (err == OT_ERROR_NOT_FOUND) {
        break;
      }
      if (err != OT_ERROR_NONE) {
        continue;
      }

      QueryResultSlot &slot = _queryResults[_queryResultCount];
      slot.used = false;
      slot.instanceName[0] = '\0';
      slot.hostName[0] = '\0';
      slot.address = IPAddress(IPv6);
      slot.port = 0;
      slot.numTxt = 0;
      for (uint8_t t = 0; t < OT_DNSSD_MAX_TXT_ENTRIES; ++t) {
        slot.txt[t].used = false;
        slot.txt[t].key[0] = '\0';
        slot.txt[t].value[0] = '\0';
      }
      if (!copyCString(slot.instanceName, sizeof(slot.instanceName), label)) {
        continue;
      }
      slot.used = true;

      char hostBuf[OT_DNS_MAX_NAME_SIZE];
      uint8_t txtBuf[kTxtScratchSize];
      otDnsServiceInfo info;
      memset(&info, 0, sizeof(info));
      info.mHostNameBuffer = hostBuf;
      info.mHostNameBufferSize = sizeof(hostBuf);
      info.mTxtData = txtBuf;
      info.mTxtDataSize = sizeof(txtBuf);

      err = otDnsBrowseResponseGetServiceInfo(aResponse, label, &info);
      if (err == OT_ERROR_NONE) {
        slot.port = info.mPort;
        copyHostLabel(slot.hostName, sizeof(slot.hostName), hostBuf);
        if (!isUnspecified(info.mHostAddress)) {
          slot.address = otToIp(info.mHostAddress);
        }
        if (info.mTxtDataTruncated) {
          log_w("OThreadDNSSD: TXT truncated in browse response");
        }
        fillTxtFromDnsData(slot, txtBuf, info.mTxtDataSize);
      }

      _queryResultCount++;
    }
  }

  if (!isQueryCallbackCurrent(gen)) {
    return;
  }

  if (_queryAsync && _queryKind == OT_DNSSD_QUERY_SERVICE) {
    if (aError != OT_ERROR_NONE) {
      finishAsyncQuery(aError);
      return;
    }
    if (startDetailResolveAt(0)) {
      return;
    }
    finishAsyncQuery(anySlotNeedsDetailResolve() ? ((_lastError != OT_ERROR_NONE) ? _lastError : OT_ERROR_FAILED) : OT_ERROR_NONE);
    return;
  }

  wakeDnsWaiter();
}

void OThreadDNSSDClass::handleDnsAddressCallback(otError aError, const otDnsAddressResponse *aResponse, void *aContext) {
  DnsCbCtx *ctx = static_cast<DnsCbCtx *>(aContext);
  if (!ctx || !ctx->used || !ctx->self) {
    return;
  }
  OThreadDNSSDClass *self = ctx->self;
  const uint32_t gen = ctx->gen;
  self->freeDnsCbCtx(ctx);
  self->onDnsAddressCallback(aError, aResponse, gen);
}

void OThreadDNSSDClass::onDnsAddressCallback(otError aError, const otDnsAddressResponse *aResponse, uint32_t gen) {
  if (!isQueryCallbackCurrent(gen)) {
    return;
  }

  _lastError = aError;
  _dnsResolvedAddr = IPAddress(IPv6);
  if (aError == OT_ERROR_NONE && aResponse) {
    otIp6Address addr;
    if (otDnsAddressResponseGetAddress(aResponse, 0, &addr, nullptr) == OT_ERROR_NONE) {
      _dnsResolvedAddr = otToIp(addr);
    }
  }

  if (!isQueryCallbackCurrent(gen)) {
    return;
  }

  if (_queryAsync && _queryKind == OT_DNSSD_QUERY_HOST) {
    finishAsyncQuery(aError);
    return;
  }

  wakeDnsWaiter();
}

void OThreadDNSSDClass::handleDnsServiceCallback(otError aError, const otDnsServiceResponse *aResponse, void *aContext) {
  DnsCbCtx *ctx = static_cast<DnsCbCtx *>(aContext);
  if (!ctx || !ctx->used || !ctx->self) {
    return;
  }
  OThreadDNSSDClass *self = ctx->self;
  const uint32_t gen = ctx->gen;
  self->freeDnsCbCtx(ctx);
  self->onDnsServiceCallback(aError, aResponse, gen);
}

void OThreadDNSSDClass::onDnsServiceCallback(otError aError, const otDnsServiceResponse *aResponse, uint32_t gen) {
  if (!isQueryCallbackCurrent(gen)) {
    return;
  }

  _lastError = aError;
  int finishedIdx = _dnsServiceResolveIdx;
  if (aError == OT_ERROR_NONE && aResponse && finishedIdx >= 0 && finishedIdx < (int)_queryResultCount) {
    QueryResultSlot &slot = _queryResults[finishedIdx];
    char hostBuf[OT_DNS_MAX_NAME_SIZE];
    uint8_t txtBuf[kTxtScratchSize];
    otDnsServiceInfo info;
    memset(&info, 0, sizeof(info));
    info.mHostNameBuffer = hostBuf;
    info.mHostNameBufferSize = sizeof(hostBuf);
    info.mTxtData = txtBuf;
    info.mTxtDataSize = sizeof(txtBuf);

    char instLabel[OT_DNSSD_INSTANCE_NAME_MAX + 1];
    char svcName[OT_DNS_MAX_NAME_SIZE];
    if (otDnsServiceResponseGetServiceName(aResponse, instLabel, sizeof(instLabel), svcName, sizeof(svcName)) == OT_ERROR_NONE) {
      (void)svcName;
      if (slot.instanceName[0] == '\0') {
        (void)copyCString(slot.instanceName, sizeof(slot.instanceName), instLabel);
      }
    }

    if (otDnsServiceResponseGetServiceInfo(aResponse, &info) == OT_ERROR_NONE) {
      slot.port = info.mPort;
      copyHostLabel(slot.hostName, sizeof(slot.hostName), hostBuf);
      if (!isUnspecified(info.mHostAddress)) {
        slot.address = otToIp(info.mHostAddress);
      }
      if (info.mTxtDataTruncated) {
        log_w("OThreadDNSSD: TXT truncated in service response");
      }
      if (info.mTxtDataSize > 0) {
        fillTxtFromDnsData(slot, txtBuf, info.mTxtDataSize);
      }
    }
  }

  if (!isQueryCallbackCurrent(gen)) {
    return;
  }
  _dnsServiceResolveIdx = -1;

  if (_queryAsync && _queryKind == OT_DNSSD_QUERY_SERVICE) {
    uint8_t next = (finishedIdx >= 0) ? (uint8_t)(finishedIdx + 1) : 0;
    if (startDetailResolveAt(next)) {
      return;
    }
    finishAsyncQuery(anySlotNeedsDetailResolve() ? ((_lastError != OT_ERROR_NONE) ? _lastError : OT_ERROR_FAILED) : OT_ERROR_NONE);
    return;
  }

  wakeDnsWaiter();
}

uint32_t OThreadDNSSDClass::dnsResponseWaitMs(otInstance *inst) const {
  // Floor: OT_DNSSD_QUERY_TIMEOUT_MS. Also stay above the OT DNS client's
  // configured response timeout so Discovery Proxy empty answers (~6 s) complete
  // before we abort the semaphore wait.
  uint32_t ms = OT_DNSSD_QUERY_TIMEOUT_MS;
  if (inst) {
    const otDnsQueryConfig *cfg = otDnsClientGetDefaultConfig(inst);
    if (cfg && cfg->mResponseTimeout > 0) {
      uint32_t need = cfg->mResponseTimeout + 1000;
      if (need > ms) {
        ms = need;
      }
    }
  }
  return ms;
}

uint32_t OThreadDNSSDClass::hostResolveWaitMs(otInstance *inst, uint32_t timeoutMs) const {
  if (timeoutMs == UINT32_MAX) {
    return timeoutMs;
  }
  // Default only: stay above the OT DNS client wait (same floor as browse).
  // Explicit timeouts are fail-fast / caller-owned and are not raised.
  if (timeoutMs == OT_DNSSD_QUERY_TIMEOUT_MS) {
    return dnsResponseWaitMs(inst);
  }
  return timeoutMs;
}

bool OThreadDNSSDClass::buildHostFqdn(char *dst, size_t dstSize, const char *host) const {
  if (!dst || dstSize == 0 || !host || host[0] == '\0') {
    return false;
  }
  if (strchr(host, '.') != nullptr) {
    return copyCString(dst, dstSize, host);
  }
  int n = snprintf(dst, dstSize, "%s.%s", host, kDnssdDomain);
  return n > 0 && (size_t)n < dstSize;
}

bool OThreadDNSSDClass::buildServiceFqdn(char *dst, size_t dstSize, const char *shortName) const {
  if (!dst || dstSize == 0 || !shortName || shortName[0] == '\0') {
    return false;
  }
  int n = snprintf(dst, dstSize, "%s.%s", shortName, kDnssdDomain);
  return n > 0 && (size_t)n < dstSize;
}

bool OThreadDNSSDClass::anySlotNeedsDetailResolve() const {
  for (uint8_t i = 0; i < _queryResultCount; ++i) {
    if (slotNeedsDetailResolve(_queryResults[i])) {
      return true;
    }
  }
  return false;
}

bool OThreadDNSSDClass::slotNeedsDetailResolve(const QueryResultSlot &slot) const {
  if (!slot.used) {
    return false;
  }
  bool addrEmpty = true;
  for (int b = 0; b < 16; ++b) {
    if (slot.address[b] != 0) {
      addrEmpty = false;
      break;
    }
  }
  return (slot.port == 0 || addrEmpty);
}

bool OThreadDNSSDClass::startDetailResolveAt(uint8_t startIdx) {
  otInstance *inst = OThread.getInstance();
  if (!inst || _dnsServiceFqdn[0] == '\0') {
    return false;
  }

  for (uint8_t i = startIdx; i < _queryResultCount; ++i) {
    QueryResultSlot &slot = _queryResults[i];
    if (!slotNeedsDetailResolve(slot)) {
      continue;
    }

    OtLock lock;
    if (!lock) {
      return false;
    }
    DnsCbCtx *ctx = allocDnsCbCtx();
    if (!ctx) {
      return false;
    }
    _dnsServiceResolveIdx = (int)i;
    otError err = otDnsClientResolveServiceAndHostAddress(inst, slot.instanceName, _dnsServiceFqdn, handleDnsServiceCallback, ctx, nullptr);
    if (err != OT_ERROR_NONE) {
      freeDnsCbCtx(ctx);
      _lastError = err;
      _dnsServiceResolveIdx = -1;
      continue;
    }
    return true;
  }
  return false;
}

void OThreadDNSSDClass::armQueryOp(bool async, ot_dnssd_query_kind_t kind) {
  _queryGen++;
  if (_queryGen == 0) {
    _queryGen = 1;
  }
  _queryActiveGen = _queryGen;
  _queryInProgress = true;
  _queryAsync = async;
  _queryKind = kind;
  _dnsServiceResolveIdx = -1;
}

void OThreadDNSSDClass::clearQueryOp() {
  _queryActiveGen = 0;
  _queryGen++;
  if (_queryGen == 0) {
    _queryGen = 1;
  }
  _queryInProgress = false;
  _queryAsync = false;
  _dnsServiceResolveIdx = -1;
}

bool OThreadDNSSDClass::isQueryCallbackCurrent(uint32_t gen) const {
  return _started && gen != 0 && gen == _queryActiveGen;
}

OThreadDNSSDClass::DnsCbCtx *OThreadDNSSDClass::allocDnsCbCtx() {
  if (_queryActiveGen == 0) {
    return nullptr;
  }
  for (uint8_t i = 0; i < kDnsCbCtxCount; ++i) {
    if (!_dnsCbCtx[i].used) {
      _dnsCbCtx[i].used = true;
      _dnsCbCtx[i].self = this;
      _dnsCbCtx[i].gen = _queryActiveGen;
      return &_dnsCbCtx[i];
    }
  }
  log_e("OThreadDNSSD: DNS callback context pool full");
  _lastError = OT_ERROR_NO_BUFS;
  return nullptr;
}

void OThreadDNSSDClass::freeDnsCbCtx(DnsCbCtx *ctx) {
  if (!ctx) {
    return;
  }
  ctx->used = false;
  ctx->self = nullptr;
  ctx->gen = 0;
}

void OThreadDNSSDClass::wakeDnsWaiter() {
  _dnsDone = true;
  if (_dnsSem) {
    xSemaphoreGive(_dnsSem);
  }
}

bool OThreadDNSSDClass::abandonIfBlockingWaitTimedOut() {
  // Serialize with OT-task DNS callbacks (they run with the OT lock held).
  OtLock lock;
  if (lock && _dnsDone) {
    return false;
  }
  _lastError = OT_ERROR_RESPONSE_TIMEOUT;
  clearQueryOp();
  return true;
}

void OThreadDNSSDClass::notifyQueryEvent(ot_dnssd_query_event_t event, otError error, int count) {
  if (_queryCb) {
    _queryCb(_queryKind, event, error, count, _queryCtx);
  }
}

void OThreadDNSSDClass::finishAsyncQuery(otError error) {
  int count = 0;
  if (_queryKind == OT_DNSSD_QUERY_HOST) {
    bool empty = true;
    for (int b = 0; b < 16; ++b) {
      if (_dnsResolvedAddr[b] != 0) {
        empty = false;
        break;
      }
    }
    count = empty ? 0 : 1;
  } else {
    count = (int)_queryResultCount;
  }

  ot_dnssd_query_event_t event = (error == OT_ERROR_NONE) ? OT_DNSSD_QUERY_DONE : OT_DNSSD_QUERY_ERROR;
  clearQueryOp();
  notifyQueryEvent(event, error, count);
}

void OThreadDNSSDClass::onQueryEvent(OThreadDNSSDQueryCallback callback, void *context) {
  _queryCb = callback;
  _queryCtx = context;
}

IPAddress OThreadDNSSDClass::resolveAddressFqdn(const char *fqdn, uint32_t timeoutMs) {
  IPAddress empty(IPv6);
  // Caller must armQueryOp() first (blocking queryHost).
  if (!_started || !fqdn || fqdn[0] == '\0' || _queryActiveGen == 0) {
    return empty;
  }
  otInstance *inst = OThread.getInstance();
  if (!inst || !ensureDnsSem()) {
    clearQueryOp();
    return empty;
  }

  while (xSemaphoreTake(_dnsSem, 0) == pdTRUE) {}
  _dnsDone = false;

  {
    OtLock lock;
    if (!lock) {
      clearQueryOp();
      return empty;
    }
    DnsCbCtx *ctx = allocDnsCbCtx();
    if (!ctx) {
      clearQueryOp();
      return empty;
    }
    otError err = otDnsClientResolveAddress(inst, fqdn, handleDnsAddressCallback, ctx, nullptr);
    if (err != OT_ERROR_NONE) {
      freeDnsCbCtx(ctx);
      _lastError = err;
      clearQueryOp();
      return empty;
    }
    _dnsResolvedAddr = empty;
  }

  uint32_t waitMs = hostResolveWaitMs(inst, timeoutMs);
  TickType_t ticks = (waitMs == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(waitMs);
  if (xSemaphoreTake(_dnsSem, ticks) != pdTRUE) {
    if (abandonIfBlockingWaitTimedOut()) {
      _dnsResolvedAddr = empty;
      return empty;
    }
  }
  return _dnsResolvedAddr;
}

bool OThreadDNSSDClass::resolveMissingServiceDetails(const char *serviceFqdn) {
  otInstance *inst = OThread.getInstance();
  if (!inst || !serviceFqdn || !ensureDnsSem() || _queryActiveGen == 0) {
    return false;
  }
  if (serviceFqdn != _dnsServiceFqdn) {
    (void)copyCString(_dnsServiceFqdn, sizeof(_dnsServiceFqdn), serviceFqdn);
  }

  for (uint8_t i = 0; i < _queryResultCount; ++i) {
    if (!slotNeedsDetailResolve(_queryResults[i])) {
      continue;
    }

    while (xSemaphoreTake(_dnsSem, 0) == pdTRUE) {}
    _dnsDone = false;

    {
      OtLock lock;
      if (!lock) {
        _dnsServiceResolveIdx = -1;
        return false;
      }
      DnsCbCtx *ctx = allocDnsCbCtx();
      if (!ctx) {
        _dnsServiceResolveIdx = -1;
        return false;
      }
      _dnsServiceResolveIdx = (int)i;
      otError err = otDnsClientResolveServiceAndHostAddress(inst, _queryResults[i].instanceName, _dnsServiceFqdn, handleDnsServiceCallback, ctx, nullptr);
      if (err != OT_ERROR_NONE) {
        freeDnsCbCtx(ctx);
        _lastError = err;
        _dnsServiceResolveIdx = -1;
        continue;
      }
    }

    if (xSemaphoreTake(_dnsSem, pdMS_TO_TICKS(dnsResponseWaitMs(inst))) != pdTRUE) {
      if (abandonIfBlockingWaitTimedOut()) {
        break;
      }
    }
    _dnsServiceResolveIdx = -1;
  }
  return true;
}

int OThreadDNSSDClass::queryService(const char *service, const char *proto) {
  if (!_started) {
    log_e("OThreadDNSSD: queryService without begin()");
    return 0;
  }
  if (_queryInProgress) {
    log_w("OThreadDNSSD: query already in progress");
    return 0;
  }
  char shortName[OT_DNSSD_SERVICE_NAME_MAX + 1];
  if (!buildServiceName(shortName, sizeof(shortName), service, proto)) {
    return 0;
  }
  char serviceFqdn[OT_DNS_MAX_NAME_SIZE];
  if (!buildServiceFqdn(serviceFqdn, sizeof(serviceFqdn), shortName)) {
    return 0;
  }

  otInstance *inst = OThread.getInstance();
  if (!inst || !ensureDnsSem()) {
    return 0;
  }

  while (xSemaphoreTake(_dnsSem, 0) == pdTRUE) {}
  _dnsDone = false;
  armQueryOp(false, OT_DNSSD_QUERY_SERVICE);

  {
    OtLock lock;
    if (!lock) {
      clearQueryOp();
      return 0;
    }
    memcpy(_dnsServiceFqdn, serviceFqdn, strlen(serviceFqdn) + 1);
    DnsCbCtx *ctx = allocDnsCbCtx();
    if (!ctx) {
      clearQueryOp();
      return 0;
    }
    otError err = otDnsClientBrowse(inst, _dnsServiceFqdn, handleDnsBrowseCallback, ctx, nullptr);
    if (err != OT_ERROR_NONE) {
      freeDnsCbCtx(ctx);
      log_e("OThreadDNSSD: Browse failed (%d)", (int)err);
      _lastError = err;
      clearQueryOp();
      return 0;
    }
    resetQueryResults();
  }

  if (xSemaphoreTake(_dnsSem, pdMS_TO_TICKS(dnsResponseWaitMs(inst))) != pdTRUE) {
    if (abandonIfBlockingWaitTimedOut()) {
      log_w("OThreadDNSSD: Browse timeout");
      resetQueryResults();
      return 0;
    }
  }

  (void)resolveMissingServiceDetails(_dnsServiceFqdn);
  if (_queryActiveGen != 0) {
    clearQueryOp();
  }
  return (int)_queryResultCount;
}

IPAddress OThreadDNSSDClass::queryHost(const char *host, uint32_t timeoutMs) {
  IPAddress empty(IPv6);
  if (!_started) {
    log_e("OThreadDNSSD: queryHost without begin()");
    return empty;
  }
  if (_queryInProgress) {
    log_w("OThreadDNSSD: query already in progress");
    return empty;
  }
  if (!host || host[0] == '\0') {
    return empty;
  }

  char fqdn[OT_DNS_MAX_NAME_SIZE];
  if (!buildHostFqdn(fqdn, sizeof(fqdn), host)) {
    return empty;
  }

  armQueryOp(false, OT_DNSSD_QUERY_HOST);
  IPAddress addr = resolveAddressFqdn(fqdn, timeoutMs);
  if (_queryActiveGen != 0) {
    clearQueryOp();
  }
  return addr;
}

bool OThreadDNSSDClass::startQueryService(const char *service, const char *proto) {
  if (!_started) {
    log_e("OThreadDNSSD: startQueryService without begin()");
    return false;
  }
  if (_queryInProgress) {
    log_w("OThreadDNSSD: query already in progress");
    return false;
  }
  char shortName[OT_DNSSD_SERVICE_NAME_MAX + 1];
  if (!buildServiceName(shortName, sizeof(shortName), service, proto)) {
    return false;
  }
  char serviceFqdn[OT_DNS_MAX_NAME_SIZE];
  if (!buildServiceFqdn(serviceFqdn, sizeof(serviceFqdn), shortName)) {
    return false;
  }

  otInstance *inst = OThread.getInstance();
  if (!inst || !ensureDnsSem()) {
    return false;
  }

  while (xSemaphoreTake(_dnsSem, 0) == pdTRUE) {}
  _dnsDone = false;
  armQueryOp(true, OT_DNSSD_QUERY_SERVICE);

  {
    OtLock lock;
    if (!lock) {
      clearQueryOp();
      return false;
    }
    memcpy(_dnsServiceFqdn, serviceFqdn, strlen(serviceFqdn) + 1);
    DnsCbCtx *ctx = allocDnsCbCtx();
    if (!ctx) {
      clearQueryOp();
      return false;
    }
    otError err = otDnsClientBrowse(inst, _dnsServiceFqdn, handleDnsBrowseCallback, ctx, nullptr);
    if (err != OT_ERROR_NONE) {
      freeDnsCbCtx(ctx);
      log_e("OThreadDNSSD: Browse failed (%d)", (int)err);
      _lastError = err;
      clearQueryOp();
      return false;
    }
    resetQueryResults();
  }
  return true;
}

bool OThreadDNSSDClass::startQueryHost(const char *host) {
  if (!_started) {
    log_e("OThreadDNSSD: startQueryHost without begin()");
    return false;
  }
  if (_queryInProgress) {
    log_w("OThreadDNSSD: query already in progress");
    return false;
  }
  if (!host || host[0] == '\0') {
    return false;
  }

  char fqdn[OT_DNS_MAX_NAME_SIZE];
  if (!buildHostFqdn(fqdn, sizeof(fqdn), host)) {
    return false;
  }

  otInstance *inst = OThread.getInstance();
  if (!inst || !ensureDnsSem()) {
    return false;
  }

  while (xSemaphoreTake(_dnsSem, 0) == pdTRUE) {}
  _dnsDone = false;
  armQueryOp(true, OT_DNSSD_QUERY_HOST);

  {
    OtLock lock;
    if (!lock) {
      clearQueryOp();
      return false;
    }
    DnsCbCtx *ctx = allocDnsCbCtx();
    if (!ctx) {
      clearQueryOp();
      return false;
    }
    otError err = otDnsClientResolveAddress(inst, fqdn, handleDnsAddressCallback, ctx, nullptr);
    if (err != OT_ERROR_NONE) {
      freeDnsCbCtx(ctx);
      _lastError = err;
      clearQueryOp();
      return false;
    }
    _dnsResolvedAddr = IPAddress(IPv6);
  }
  return true;
}

const char *OThreadDNSSDClass::hostname(int idx) const {
  if (idx < 0 || idx >= (int)_queryResultCount || !_queryResults[idx].used) {
    return "";
  }
  return _queryResults[idx].hostName;
}

const char *OThreadDNSSDClass::instanceName(int idx) const {
  if (idx < 0 || idx >= (int)_queryResultCount || !_queryResults[idx].used) {
    return "";
  }
  return _queryResults[idx].instanceName;
}

IPAddress OThreadDNSSDClass::address(int idx) const {
  if (idx < 0 || idx >= (int)_queryResultCount || !_queryResults[idx].used) {
    return IPAddress(IPv6);
  }
  return _queryResults[idx].address;
}

uint16_t OThreadDNSSDClass::port(int idx) const {
  if (idx < 0 || idx >= (int)_queryResultCount || !_queryResults[idx].used) {
    return 0;
  }
  return _queryResults[idx].port;
}

int OThreadDNSSDClass::numTxt(int idx) const {
  if (idx < 0 || idx >= (int)_queryResultCount || !_queryResults[idx].used) {
    return 0;
  }
  return (int)_queryResults[idx].numTxt;
}

bool OThreadDNSSDClass::hasTxt(int idx, const char *key) const {
  if (!key || idx < 0 || idx >= (int)_queryResultCount || !_queryResults[idx].used) {
    return false;
  }
  const QueryResultSlot &slot = _queryResults[idx];
  for (uint8_t i = 0; i < slot.numTxt; ++i) {
    if (slot.txt[i].used && strcmp(slot.txt[i].key, key) == 0) {
      return true;
    }
  }
  return false;
}

String OThreadDNSSDClass::txt(int idx, const char *key) const {
  if (!key || idx < 0 || idx >= (int)_queryResultCount || !_queryResults[idx].used) {
    return String();
  }
  const QueryResultSlot &slot = _queryResults[idx];
  for (uint8_t i = 0; i < slot.numTxt; ++i) {
    if (slot.txt[i].used && strcmp(slot.txt[i].key, key) == 0) {
      return String(slot.txt[i].value);
    }
  }
  return String();
}

String OThreadDNSSDClass::txt(int idx, int txtIdx) const {
  if (idx < 0 || idx >= (int)_queryResultCount || !_queryResults[idx].used) {
    return String();
  }
  const QueryResultSlot &slot = _queryResults[idx];
  if (txtIdx < 0 || txtIdx >= (int)slot.numTxt || !slot.txt[txtIdx].used) {
    return String();
  }
  return String(slot.txt[txtIdx].value);
}

String OThreadDNSSDClass::txtKey(int idx, int txtIdx) const {
  if (idx < 0 || idx >= (int)_queryResultCount || !_queryResults[idx].used) {
    return String();
  }
  const QueryResultSlot &slot = _queryResults[idx];
  if (txtIdx < 0 || txtIdx >= (int)slot.numTxt || !slot.txt[txtIdx].used) {
    return String();
  }
  return String(slot.txt[txtIdx].key);
}

#endif /* CONFIG_OPENTHREAD_DNS_CLIENT */

#endif /* CONFIG_OPENTHREAD_SRP_CLIENT */
#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
