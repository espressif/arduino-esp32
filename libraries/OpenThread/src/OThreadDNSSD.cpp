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

void stripLeadingUnderscore(char *label) {
  if (label[0] == '_') {
    memmove(label, label + 1, strlen(label));
  }
}

#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
constexpr const char *kDnssdDomain = "default.service.arpa";

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
  : _started(false),
    _announceComplete(false),
    _lastError(OT_ERROR_NONE),
    _instanceNameSet(false),
    _eventCb(nullptr),
    _eventCtx(nullptr),
    _announceSem(nullptr)
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
    ,
    _queryResultCount(0),
    _dnsSem(nullptr),
    _dnsDone(false),
    _dnsOpError(OT_ERROR_NONE),
    _dnsServiceResolveIdx(-1),
    _queryInProgress(false),
    _queryAsync(false),
    _queryAbandoned(false),
    _queryKind(OT_DNSSD_QUERY_SERVICE),
    _queryCb(nullptr),
    _queryCtx(nullptr)
#endif
{
  _hostName[0] = '\0';
  _instanceName[0] = '\0';
  resetSlots();
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
  resetQueryResults();
  _dnsServiceFqdn[0] = '\0';
  _dnsResolvedAddr = IPAddress(IPv6);
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
    while (xSemaphoreTake(_announceSem, 0) == pdTRUE) {
    }
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
  stripLeadingUnderscore(svc);
  stripLeadingUnderscore(prt);
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
    e.mValue = slot.txt[i].value;
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
    return false;
  }

  resetSlots();
  _instanceNameSet = false;
  _instanceName[0] = '\0';
  clearAnnounceState();

  OtLock lock;
  if (!lock) {
    log_e("OThreadDNSSD: failed to acquire OT lock");
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
    return false;
  }

  err = otSrpClientEnableAutoHostAddress(inst);
  if (err != OT_ERROR_NONE) {
    log_e("OThreadDNSSD: EnableAutoHostAddress failed (%d)", (int)err);
    otSrpClientClearHostAndServices(inst);
    otSrpClientSetCallback(inst, nullptr, nullptr);
    _lastError = err;
    return false;
  }

  otSrpClientEnableAutoStartMode(inst, nullptr, nullptr);
  _started = true;
  return true;
}

void OThreadDNSSDClass::end() {
  if (!_started) {
    return;
  }
  otInstance *inst = OThread.getInstance();
  // Ask the SRP server to forget us when possible, then clear local client
  // tracking so OpenThread drops pointers into our slots before we zero them.
  if (inst) {
    OtLock lock;
    if (lock) {
      (void)otSrpClientRemoveHostAndServices(inst, true, true);
      otSrpClientClearHostAndServices(inst);
      otSrpClientStop(inst);
      otSrpClientSetCallback(inst, nullptr, nullptr);
    }
  }
  resetSlots();
  _started = false;
  _announceComplete = false;
  _hostName[0] = '\0';
  _instanceName[0] = '\0';
  _instanceNameSet = false;
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
  resetQueryResults();
  _queryAbandoned = true;
  _queryInProgress = false;
  _queryAsync = false;
  _dnsServiceResolveIdx = -1;
  if (_dnsSem) {
    xSemaphoreGive(_dnsSem);
  }
#endif
  notifyEvent(OT_DNSSD_EVENT_REMOVED, OT_ERROR_NONE);
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

  TxtSlot &t = slot.txt[freeTxt];
  if (!copyCString(t.key, sizeof(t.key), key)) {
    return false;
  }
  t.valueLen = 0;
  if (value && value[0] != '\0') {
    size_t vlen = strlen(value);
    if (vlen > OT_DNSSD_TXT_VALUE_MAX) {
      return false;
    }
    memcpy(t.value, value, vlen);
    t.valueLen = (uint16_t)vlen;
  }
  t.used = true;

  return updateServiceOnOt(slot);
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
  if (slot.numSubtypes >= OT_DNSSD_MAX_SUBTYPES) {
    log_w("OThreadDNSSD: subtype storage full (%u)", (unsigned)OT_DNSSD_MAX_SUBTYPES);
    return false;
  }
  char *dst = slot.subtypes[slot.numSubtypes];
  if (!copyCString(dst, OT_DNSSD_SUBTYPE_MAX + 1, subtype)) {
    return false;
  }
  stripLeadingUnderscore(dst);
  slot.numSubtypes++;
  return updateServiceOnOt(slot);
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
      // ClearService drops OT's pointer immediately — safe to free the slot.
      (void)otSrpClientClearService(inst, &slot.otService);
      memset(&slot, 0, sizeof(slot));
      return true;
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

bool OThreadDNSSDClass::evaluateAnnounceComplete(
  const otSrpClientHostInfo *hostInfo, const otSrpClientService *services
) const {
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
  _announceComplete =
    evaluateAnnounceComplete(otSrpClientGetHostInfo(inst), otSrpClientGetServices(inst));
  return _announceComplete;
}

bool OThreadDNSSDClass::isTerminalSrpError(otError error) {
  // Name already owned by another SRP key (cleared NVS or another device), or
  // signature/key reject. Retrying the same labels will not succeed — sketch must act.
  return error == OT_ERROR_DUPLICATED || error == OT_ERROR_SECURITY;
}

void OThreadDNSSDClass::handleSrpCallback(
  otError aError, const otSrpClientHostInfo *aHostInfo, const otSrpClientService *aServices,
  const otSrpClientService *aRemovedServices, void *aContext
) {
  static_cast<OThreadDNSSDClass *>(aContext)->onSrpCallback(aError, aHostInfo, aServices, aRemovedServices);
}

void OThreadDNSSDClass::onSrpCallback(
  otError aError, const otSrpClientHostInfo *aHostInfo, const otSrpClientService *aServices,
  const otSrpClientService *aRemovedServices
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

  if (aHostInfo && aHostInfo->mState == OT_SRP_CLIENT_ITEM_STATE_REMOVED) {
    notifyEvent(OT_DNSSD_EVENT_REMOVED, OT_ERROR_NONE);
    return;
  }

  if (aRemovedServices != nullptr) {
    notifyEvent(OT_DNSSD_EVENT_REMOVED, OT_ERROR_NONE);
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
  static_cast<OThreadDNSSDClass *>(aContext)->onDnsBrowseCallback(aError, aResponse);
}

void OThreadDNSSDClass::onDnsBrowseCallback(otError aError, const otDnsBrowseResponse *aResponse) {
  // end() may have abandoned the query while this browse was in flight; do not
  // repopulate results or clobber lastError after teardown.
  if (_queryAbandoned || !_started) {
    _dnsDone = true;
    if (_dnsSem) {
      xSemaphoreGive(_dnsSem);
    }
    return;
  }

  _dnsOpError = aError;
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
      uint8_t txtBuf[128];
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
        fillTxtFromDnsData(slot, txtBuf, info.mTxtDataSize);
      }

      _queryResultCount++;
    }
  }

  if (_queryAbandoned) {
    _dnsDone = true;
    if (_dnsSem) {
      xSemaphoreGive(_dnsSem);
    }
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
    finishAsyncQuery(OT_ERROR_NONE);
    return;
  }

  _dnsDone = true;
  if (_dnsSem) {
    xSemaphoreGive(_dnsSem);
  }
}

void OThreadDNSSDClass::handleDnsAddressCallback(otError aError, const otDnsAddressResponse *aResponse, void *aContext) {
  static_cast<OThreadDNSSDClass *>(aContext)->onDnsAddressCallback(aError, aResponse);
}

void OThreadDNSSDClass::onDnsAddressCallback(otError aError, const otDnsAddressResponse *aResponse) {
  if (_queryAbandoned || !_started) {
    _dnsDone = true;
    if (_dnsSem) {
      xSemaphoreGive(_dnsSem);
    }
    return;
  }

  _dnsOpError = aError;
  _lastError = aError;
  _dnsResolvedAddr = IPAddress(IPv6);
  if (aError == OT_ERROR_NONE && aResponse) {
    otIp6Address addr;
    if (otDnsAddressResponseGetAddress(aResponse, 0, &addr, nullptr) == OT_ERROR_NONE) {
      _dnsResolvedAddr = otToIp(addr);
    }
  }

  if (_queryAbandoned) {
    _dnsDone = true;
    if (_dnsSem) {
      xSemaphoreGive(_dnsSem);
    }
    return;
  }

  if (_queryAsync && _queryKind == OT_DNSSD_QUERY_HOST) {
    finishAsyncQuery(aError);
    return;
  }

  _dnsDone = true;
  if (_dnsSem) {
    xSemaphoreGive(_dnsSem);
  }
}

void OThreadDNSSDClass::handleDnsServiceCallback(otError aError, const otDnsServiceResponse *aResponse, void *aContext) {
  static_cast<OThreadDNSSDClass *>(aContext)->onDnsServiceCallback(aError, aResponse);
}

void OThreadDNSSDClass::onDnsServiceCallback(otError aError, const otDnsServiceResponse *aResponse) {
  if (_queryAbandoned || !_started) {
    _dnsDone = true;
    if (_dnsSem) {
      xSemaphoreGive(_dnsSem);
    }
    return;
  }

  _dnsOpError = aError;
  _lastError = aError;
  int finishedIdx = _dnsServiceResolveIdx;
  if (aError == OT_ERROR_NONE && aResponse && finishedIdx >= 0 && finishedIdx < (int)_queryResultCount) {
    QueryResultSlot &slot = _queryResults[finishedIdx];
    char hostBuf[OT_DNS_MAX_NAME_SIZE];
    uint8_t txtBuf[128];
    otDnsServiceInfo info;
    memset(&info, 0, sizeof(info));
    info.mHostNameBuffer = hostBuf;
    info.mHostNameBufferSize = sizeof(hostBuf);
    info.mTxtData = txtBuf;
    info.mTxtDataSize = sizeof(txtBuf);

    char instLabel[OT_DNSSD_INSTANCE_NAME_MAX + 1];
    char svcName[OT_DNS_MAX_NAME_SIZE];
    if (otDnsServiceResponseGetServiceName(aResponse, instLabel, sizeof(instLabel), svcName, sizeof(svcName))
        == OT_ERROR_NONE) {
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
      if (info.mTxtDataSize > 0) {
        fillTxtFromDnsData(slot, txtBuf, info.mTxtDataSize);
      }
    }
  }
  _dnsServiceResolveIdx = -1;

  if (_queryAbandoned) {
    _dnsDone = true;
    if (_dnsSem) {
      xSemaphoreGive(_dnsSem);
    }
    return;
  }

  if (_queryAsync && _queryKind == OT_DNSSD_QUERY_SERVICE) {
    uint8_t next = (finishedIdx >= 0) ? (uint8_t)(finishedIdx + 1) : 0;
    if (startDetailResolveAt(next)) {
      return;
    }
    finishAsyncQuery(OT_ERROR_NONE);
    return;
  }

  _dnsDone = true;
  if (_dnsSem) {
    xSemaphoreGive(_dnsSem);
  }
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
    _dnsServiceResolveIdx = (int)i;
    otError err = otDnsClientResolveServiceAndHostAddress(
      inst, slot.instanceName, _dnsServiceFqdn, handleDnsServiceCallback, this, nullptr
    );
    if (err != OT_ERROR_NONE) {
      _lastError = err;
      _dnsServiceResolveIdx = -1;
      continue;
    }
    return true;
  }
  return false;
}

void OThreadDNSSDClass::clearQueryOp() {
  _queryInProgress = false;
  _queryAsync = false;
  _dnsServiceResolveIdx = -1;
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
  _dnsResolvedAddr = IPAddress(IPv6);
  if (!_started || !fqdn || fqdn[0] == '\0') {
    return _dnsResolvedAddr;
  }
  otInstance *inst = OThread.getInstance();
  if (!inst || !ensureDnsSem()) {
    return _dnsResolvedAddr;
  }

  while (xSemaphoreTake(_dnsSem, 0) == pdTRUE) {
  }
  _dnsDone = false;
  _dnsOpError = OT_ERROR_NONE;
  _queryAbandoned = false;

  {
    OtLock lock;
    if (!lock) {
      return _dnsResolvedAddr;
    }
    otError err = otDnsClientResolveAddress(inst, fqdn, handleDnsAddressCallback, this, nullptr);
    if (err != OT_ERROR_NONE) {
      _lastError = err;
      _dnsOpError = err;
      return _dnsResolvedAddr;
    }
  }

  TickType_t ticks = (timeoutMs == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
  if (xSemaphoreTake(_dnsSem, ticks) != pdTRUE) {
    _queryAbandoned = true;
    _lastError = OT_ERROR_RESPONSE_TIMEOUT;
    return IPAddress(IPv6);
  }
  return _dnsResolvedAddr;
}

bool OThreadDNSSDClass::resolveMissingServiceDetails(const char *serviceFqdn) {
  otInstance *inst = OThread.getInstance();
  if (!inst || !serviceFqdn || !ensureDnsSem()) {
    return false;
  }
  if (serviceFqdn != _dnsServiceFqdn) {
    (void)copyCString(_dnsServiceFqdn, sizeof(_dnsServiceFqdn), serviceFqdn);
  }

  for (uint8_t i = 0; i < _queryResultCount; ++i) {
    if (!slotNeedsDetailResolve(_queryResults[i])) {
      continue;
    }

    while (xSemaphoreTake(_dnsSem, 0) == pdTRUE) {
    }
    _dnsDone = false;
    _dnsOpError = OT_ERROR_NONE;
    _queryAbandoned = false;

    {
      OtLock lock;
      if (!lock) {
        _dnsServiceResolveIdx = -1;
        return false;
      }
      _dnsServiceResolveIdx = (int)i;
      otError err = otDnsClientResolveServiceAndHostAddress(
        inst, _queryResults[i].instanceName, _dnsServiceFqdn, handleDnsServiceCallback, this, nullptr
      );
      if (err != OT_ERROR_NONE) {
        _lastError = err;
        _dnsServiceResolveIdx = -1;
        continue;
      }
    }

    if (xSemaphoreTake(_dnsSem, pdMS_TO_TICKS(dnsResponseWaitMs(inst))) != pdTRUE) {
      _queryAbandoned = true;
      _lastError = OT_ERROR_RESPONSE_TIMEOUT;
      _dnsServiceResolveIdx = -1;
      break;
    }
    _dnsServiceResolveIdx = -1;
  }
  return true;
}

int OThreadDNSSDClass::queryService(const char *service, const char *proto) {
  resetQueryResults();
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
  if (snprintf(_dnsServiceFqdn, sizeof(_dnsServiceFqdn), "%s.%s", shortName, kDnssdDomain) < 0) {
    return 0;
  }

  otInstance *inst = OThread.getInstance();
  if (!inst || !ensureDnsSem()) {
    return 0;
  }

  while (xSemaphoreTake(_dnsSem, 0) == pdTRUE) {
  }
  _dnsDone = false;
  _dnsOpError = OT_ERROR_NONE;
  _queryAbandoned = false;
  _queryInProgress = true;
  _queryAsync = false;
  _queryKind = OT_DNSSD_QUERY_SERVICE;

  {
    OtLock lock;
    if (!lock) {
      clearQueryOp();
      return 0;
    }
    otError err = otDnsClientBrowse(inst, _dnsServiceFqdn, handleDnsBrowseCallback, this, nullptr);
    if (err != OT_ERROR_NONE) {
      log_e("OThreadDNSSD: Browse failed (%d)", (int)err);
      _lastError = err;
      clearQueryOp();
      return 0;
    }
  }

  if (xSemaphoreTake(_dnsSem, pdMS_TO_TICKS(dnsResponseWaitMs(inst))) != pdTRUE) {
    _queryAbandoned = true;
    _lastError = OT_ERROR_RESPONSE_TIMEOUT;
    log_w("OThreadDNSSD: Browse timeout");
    clearQueryOp();
    return (int)_queryResultCount;
  }

  (void)resolveMissingServiceDetails(_dnsServiceFqdn);
  clearQueryOp();
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

  _queryInProgress = true;
  _queryAsync = false;
  _queryKind = OT_DNSSD_QUERY_HOST;
  IPAddress addr = resolveAddressFqdn(fqdn, timeoutMs);
  clearQueryOp();
  return addr;
}

bool OThreadDNSSDClass::startQueryService(const char *service, const char *proto) {
  resetQueryResults();
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
  if (snprintf(_dnsServiceFqdn, sizeof(_dnsServiceFqdn), "%s.%s", shortName, kDnssdDomain) < 0) {
    return false;
  }

  otInstance *inst = OThread.getInstance();
  if (!inst || !ensureDnsSem()) {
    return false;
  }

  while (xSemaphoreTake(_dnsSem, 0) == pdTRUE) {
  }
  _dnsDone = false;
  _dnsOpError = OT_ERROR_NONE;
  _queryAbandoned = false;
  _queryInProgress = true;
  _queryAsync = true;
  _queryKind = OT_DNSSD_QUERY_SERVICE;

  {
    OtLock lock;
    if (!lock) {
      clearQueryOp();
      return false;
    }
    otError err = otDnsClientBrowse(inst, _dnsServiceFqdn, handleDnsBrowseCallback, this, nullptr);
    if (err != OT_ERROR_NONE) {
      log_e("OThreadDNSSD: Browse failed (%d)", (int)err);
      _lastError = err;
      clearQueryOp();
      return false;
    }
  }
  return true;
}

bool OThreadDNSSDClass::startQueryHost(const char *host) {
  _dnsResolvedAddr = IPAddress(IPv6);
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

  while (xSemaphoreTake(_dnsSem, 0) == pdTRUE) {
  }
  _dnsDone = false;
  _dnsOpError = OT_ERROR_NONE;
  _queryAbandoned = false;
  _queryInProgress = true;
  _queryAsync = true;
  _queryKind = OT_DNSSD_QUERY_HOST;

  {
    OtLock lock;
    if (!lock) {
      clearQueryOp();
      return false;
    }
    otError err = otDnsClientResolveAddress(inst, fqdn, handleDnsAddressCallback, this, nullptr);
    if (err != OT_ERROR_NONE) {
      _lastError = err;
      clearQueryOp();
      return false;
    }
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
