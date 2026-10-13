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

/**
 * @file
 * @brief Shared (stack-agnostic) implementation-layer definitions for
 *        @c BLEServerImplCommon. Public @c BLEServer:: methods live in
 *        server/BLEServer.cpp; this file holds the shared Impl helpers (connection
 *        table + callback dispatch) that backend event handlers call.
 *        API contract is documented on the declarations in
 *        server/BLEServerImpl.h.
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "server/BLEServer.h"
#include "core/BLEBackend.h"
#include "core/BLEMutex.h"
#include "esp32-hal-log.h"
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if BLE_GATT_SERVER_SUPPORTED

// --------------------------------------------------------------------------
// Connection helpers (shared between backends)
// --------------------------------------------------------------------------

void BLEServerImplCommon::connSet(uint16_t connHandle, const BLEConnInfo &connInfo) {
  for (auto &entry : connections) {
    if (entry.first == connHandle) {
      entry.second = connInfo;
      return;
    }
  }
  connections.emplace_back(connHandle, connInfo);
}

void BLEServerImplCommon::connErase(uint16_t connHandle) {
  for (auto it = longReads.begin(); it != longReads.end(); ++it) {
    if (it->connHandle == connHandle) {
      longReads.erase(it);
      break;
    }
  }
  for (auto it = connections.begin(); it != connections.end(); ++it) {
    if (it->first == connHandle) {
      connections.erase(it);
      return;
    }
  }
}

bool BLEServerImplCommon::characteristicCanSend(const BLECharacteristic::Impl *chr) const {
  return chr && chr->handle != 0 && chr->service && chr->service->started && !chr->service->removing && chr->service->server == this;
}

BLEConnInfo *BLEServerImplCommon::connFind(uint16_t connHandle) {
  for (auto &entry : connections) {
    if (entry.first == connHandle) {
      return &entry.second;
    }
  }
  return nullptr;
}

const std::vector<uint8_t> &BLEServerImplCommon::selectLongRead(
  uint16_t connHandle, uint16_t attrHandle, uint16_t offset, uint16_t mtu, const std::vector<uint8_t> &current
) {
  if (offset == 0) {
    for (auto it = longReads.begin(); it != longReads.end(); ++it) {
      if (it->connHandle == connHandle) {
        if (mtu <= 1 || current.size() < static_cast<size_t>(mtu - 1)) {
          longReads.erase(it);
          return current;
        }
        it->attrHandle = attrHandle;
        it->value = current;
        return it->value;
      }
    }
    if (mtu <= 1 || current.size() < static_cast<size_t>(mtu - 1)) {
      return current;
    }
    longReads.push_back({connHandle, attrHandle, current});
    return longReads.back().value;
  }
  for (const auto &entry : longReads) {
    if (entry.connHandle == connHandle && entry.attrHandle == attrHandle) {
      return entry.value;
    }
  }
  return current;
}

void BLEServerImplCommon::finishLongRead(uint16_t connHandle, uint16_t offset, uint16_t mtu) {
  for (auto it = longReads.begin(); it != longReads.end(); ++it) {
    if (it->connHandle == connHandle) {
      size_t size = it->value.size();
      if (mtu <= 1 || offset > size || size - offset < static_cast<size_t>(mtu - 1)) {
        longReads.erase(it);
      }
      return;
    }
  }
}

BLEServer BLEServerImplCommon::makeHandle(BLEServer::Impl *impl) {
  return BLEServer(impl->shared_from_this());
}

// --------------------------------------------------------------------------
// Indication queue (shared between backends)
// --------------------------------------------------------------------------

namespace {

// Core Vol 3 Part F §3.3.3: a transaction not completed within 30 seconds times out.
constexpr uint32_t kAttTransactionTimeoutMs = 30000;

struct IndicationReport {
  std::shared_ptr<BLECharacteristic::Impl> chr;
  BLECharacteristic::NotifyStatus status;
  uint32_t code;
};

uint32_t nowMs() {
  return pdTICKS_TO_MS(xTaskGetTickCount());
}

void dispatchReports(const std::vector<IndicationReport> &reports) {
  for (const auto &r : reports) {
    r.chr->dispatchStatus(r.status, r.code);
  }
}

}  // namespace

BLEServerImplCommon::IndicationLink *BLEServerImplCommon::indicationLink(uint16_t connHandle) {
  for (auto &link : indicationLinks) {
    if (link.connHandle == connHandle) {
      return &link;
    }
  }
  return nullptr;
}

BLEServerImplCommon::IndicationLink &BLEServerImplCommon::indicationLinkFor(uint16_t connHandle) {
  if (IndicationLink *link = indicationLink(connHandle)) {
    return *link;
  }
  indicationLinks.push_back({});
  indicationLinks.back().connHandle = connHandle;
  return indicationLinks.back();
}

void BLEServerImplCommon::eraseIndicationLink(uint16_t connHandle) {
  for (auto it = indicationLinks.begin(); it != indicationLinks.end(); ++it) {
    if (it->connHandle == connHandle) {
      indicationLinks.erase(it);
      return;
    }
  }
}

void BLEServerImplCommon::eraseIdleIndicationLink(uint16_t connHandle) {
  IndicationLink *link = indicationLink(connHandle);
  if (link && !link->inFlight && !link->stackBusy && link->queue.empty()) {
    eraseIndicationLink(connHandle);
  }
}

// Marks the queue front as waiting on a stack-owned indication slot. Gives up
// once the front has been refused for the ATT transaction timeout (§3.3.3).
// Caller holds mtx.
bool BLEServerImplCommon::markFrontBusy(IndicationLink &link) {
  uint32_t now = nowMs();
  if (!link.frontBusy) {
    link.frontBusy = true;
    link.frontBusySinceMs = now;
  } else if (now - link.frontBusySinceMs >= kAttTransactionTimeoutMs) {
    link.frontBusy = false;
    return false;
  }
  link.stackBusy = true;
  return true;
}

// Sends queued indications until the stack accepts one, reports busy/deferred,
// or the queue runs dry. Note that queue.front() stays in place until it is
// rejected or confirmed, so busy retries never shift the vector.
bool BLEServerImplCommon::pumpIndications(uint16_t connHandle) {
  auto *server = static_cast<BLEServer::Impl *>(this);
  bool firstOk = true;
  bool first = true;
  for (;;) {
    std::shared_ptr<BLECharacteristic::Impl> chr;
    std::vector<uint8_t> payload;
    uint32_t seq = 0;
    bool valid = true;
    {
      BLELockGuard lock(mtx);
      if (!active) {
        return false;
      }
      IndicationLink *link = indicationLink(connHandle);
      if (!link || link->inFlight || link->stackBusy) {
        return firstOk;
      }
      if (link->queue.empty()) {
        eraseIdleIndicationLink(connHandle);
        return firstOk;
      }
      PendingValue &front = link->queue.front();
      chr = front.chr;
      valid = started && connFind(connHandle) != nullptr && characteristicCanSend(chr.get());
      if (!valid) {
        link->frontBusy = false;
        link->queue.erase(link->queue.begin());
      } else if (bleServerDeferIndication(server, connHandle)) {
        return firstOk;
      } else {
        payload = front.payload;
        seq = ++indicationSeq;
        link->inFlight = chr;
        link->inFlightSeq = seq;
        link->sentAtMs = nowMs();
      }
    }
    if (!valid) {
      if (first) {
        firstOk = false;
      }
      first = false;
      chr->dispatchStatus(BLECharacteristic::ERROR_NO_CLIENT, 0);
      continue;
    }

    BLECharacteristic::NotifyStatus status = BLECharacteristic::ERROR_INDICATE_FAILURE;
    uint32_t code = 0;
    BLEIndicationSend result = bleServerSendIndication(server, connHandle, chr->handle, payload.data(), payload.size(), status, code);
    if (result == BLEIndicationSend::Sent) {
      return firstOk;
    }

    bool shouldDispatch = false;
    {
      BLELockGuard lock(mtx);
      IndicationLink *link = indicationLink(connHandle);
      if (link && link->inFlightSeq == seq && link->inFlight == chr) {
        link->inFlight.reset();
        if (result == BLEIndicationSend::Busy && markFrontBusy(*link)) {
          return firstOk;
        }
        link->frontBusy = false;
        if (!link->queue.empty() && link->queue.front().chr == chr) {
          link->queue.erase(link->queue.begin());
        }
        shouldDispatch = true;
      }
    }
    if (first) {
      firstOk = false;
    }
    first = false;
    if (shouldDispatch) {
      chr->dispatchStatus(status, code);
    }
  }
}

BTStatus BLEServerImplCommon::indicate(uint16_t connHandle, const std::shared_ptr<BLECharacteristic::Impl> &chr, std::vector<uint8_t> payload) {
  bool queued = false;
  bool sendNow = false;
  {
    BLELockGuard lock(mtx);
    if (!active || !started || !characteristicCanSend(chr.get())) {
      return BTStatus::InvalidState;
    }
    IndicationLink &link = indicationLinkFor(connHandle);
    if (link.queue.size() < kMaxQueuedIndications) {
      sendNow = !link.inFlight && !link.stackBusy && link.queue.empty();
      link.queue.push_back({chr, std::move(payload)});
      queued = true;
    }
  }
  if (!queued) {
    log_w("Server: indication queue full on conn=%u", connHandle);
    chr->dispatchStatus(BLECharacteristic::ERROR_INDICATE_FAILURE, 0);
    return BTStatus::Busy;
  }
  if (!sendNow) {
    return BTStatus::OK;
  }
  return pumpIndications(connHandle) ? BTStatus::OK : BTStatus::Fail;
}

bool BLEServerImplCommon::serviceHasPendingSends(const BLEService::Impl *svc) {
  if (!svc) {
    return false;
  }
  BLELockGuard lock(mtx);
  for (const auto &link : indicationLinks) {
    if (link.inFlight && link.inFlight->service == svc) {
      return true;
    }
    for (const auto &item : link.queue) {
      if (item.chr && item.chr->service == svc) {
        return true;
      }
    }
  }
  return false;
}

bool BLEServerImplCommon::indicationDone(uint16_t connHandle, uint16_t attrHandle, BLECharacteristic::NotifyStatus status, uint32_t code) {
  std::vector<IndicationReport> reports;
  bool linkEnded = status == BLECharacteristic::ERROR_NO_CLIENT || status == BLECharacteristic::ERROR_INDICATE_TIMEOUT;
  {
    BLELockGuard lock(mtx);
    IndicationLink *link = indicationLink(connHandle);
    if (!link) {
      return false;
    }
    if (link->inFlight && link->inFlight->handle == attrHandle) {
      reports.push_back({link->inFlight, status, code});
      link->inFlight.reset();
      link->frontBusy = false;
      if (!link->queue.empty() && link->queue.front().chr.get() == reports.back().chr.get()) {
        link->queue.erase(link->queue.begin());
      }
    } else if (!link->inFlight && link->stackBusy) {
      link->stackBusy = false;
    } else {
      return false;
    }
    if (linkEnded) {
      for (auto &item : link->queue) {
        reports.push_back({item.chr, BLECharacteristic::ERROR_NO_CLIENT, 0});
      }
      eraseIndicationLink(connHandle);
    }
  }
  dispatchReports(reports);
  if (!linkEnded) {
    pumpIndications(connHandle);
  }
  return true;
}

bool BLEServerImplCommon::indicationBusy(uint16_t connHandle, uint16_t attrHandle, uint32_t code) {
  std::shared_ptr<BLECharacteristic::Impl> failed;
  bool retry = false;
  {
    BLELockGuard lock(mtx);
    IndicationLink *link = indicationLink(connHandle);
    if (!link || !link->inFlight || link->inFlight->handle != attrHandle) {
      return false;
    }
    std::shared_ptr<BLECharacteristic::Impl> chr = std::move(link->inFlight);
    retry = markFrontBusy(*link);
    if (!retry) {
      if (!link->queue.empty() && link->queue.front().chr == chr) {
        link->queue.erase(link->queue.begin());
      }
      failed = std::move(chr);
    }
  }
  if (failed) {
    log_w("Server: indication on handle %u conn=%u still refused after %lu ms", attrHandle, connHandle, (unsigned long)kAttTransactionTimeoutMs);
    failed->dispatchStatus(BLECharacteristic::ERROR_INDICATE_FAILURE, code);
    pumpIndications(connHandle);
  }
  return retry;
}

void BLEServerImplCommon::retryIndications() {
  std::vector<uint16_t> conns;
  {
    BLELockGuard lock(mtx);
    if (!active) {
      return;
    }
    for (auto &link : indicationLinks) {
      if (link.stackBusy) {
        link.stackBusy = false;
        conns.push_back(link.connHandle);
      }
    }
  }
  for (uint16_t connHandle : conns) {
    pumpIndications(connHandle);
  }
}

void BLEServerImplCommon::dropIndications(uint16_t connHandle) {
  std::vector<IndicationReport> reports;
  {
    BLELockGuard lock(mtx);
    IndicationLink *link = indicationLink(connHandle);
    if (!link) {
      return;
    }
    if (link->inFlight) {
      bool timedOut = nowMs() - link->sentAtMs >= kAttTransactionTimeoutMs;
      reports.push_back({link->inFlight, timedOut ? BLECharacteristic::ERROR_INDICATE_TIMEOUT : BLECharacteristic::ERROR_NO_CLIENT, 0});
      if (!link->queue.empty() && link->queue.front().chr == link->inFlight) {
        link->queue.erase(link->queue.begin());
      }
    }
    for (auto &item : link->queue) {
      reports.push_back({item.chr, BLECharacteristic::ERROR_NO_CLIENT, 0});
    }
    eraseIndicationLink(connHandle);
  }
  dispatchReports(reports);
}

// --------------------------------------------------------------------------
// Callback dispatch (shared between backends)
//
// Each dispatcher copies the handler under mtx and invokes it outside the lock
// so user code can safely re-enter the BLE API without deadlocking.
// --------------------------------------------------------------------------

void BLEServerImplCommon::dispatchConnect(const BLEConnInfo &connInfo) {
  log_i("Server: client connected, handle=%u addr=%s", connInfo.getHandle(), connInfo.getAddress().toString().c_str());
  decltype(onConnectCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onConnectCb;
  }
  BLEServer serverHandle(shared_from_this());
  if (cb) {
    cb(serverHandle, connInfo);
  }
}

void BLEServerImplCommon::dispatchDisconnect(const BLEConnInfo &connInfo, uint8_t reason) {
  log_i("Server: client disconnected, handle=%u addr=%s reason=0x%02x", connInfo.getHandle(), connInfo.getAddress().toString().c_str(), reason);
  decltype(onDisconnectCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onDisconnectCb;
  }
  BLEServer serverHandle(shared_from_this());
  if (cb) {
    cb(serverHandle, connInfo, reason);
  }
}

void BLEServerImplCommon::dispatchMtuChanged(const BLEConnInfo &connInfo, uint16_t mtu) {
  log_d("Server: MTU changed, handle=%u mtu=%u", connInfo.getHandle(), mtu);
  decltype(onMtuChangedCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onMtuChangedCb;
  }
  BLEServer serverHandle(shared_from_this());
  if (cb) {
    cb(serverHandle, connInfo, mtu);
  }
}

void BLEServerImplCommon::dispatchConnParamsUpdate(const BLEConnInfo &connInfo) {
  log_d(
    "Server: conn params updated, handle=%u interval=%u latency=%u timeout=%u", connInfo.getHandle(), connInfo.getInterval(), connInfo.getLatency(),
    connInfo.getSupervisionTimeout()
  );
  decltype(onConnParamsCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onConnParamsCb;
  }
  BLEServer serverHandle(shared_from_this());
  if (cb) {
    cb(serverHandle, connInfo);
  }
}

void BLEServerImplCommon::dispatchIdentityResolved(const BLEConnInfo &connInfo) {
  decltype(onIdentityCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onIdentityCb;
  }
  BLEServer serverHandle(shared_from_this());
  if (cb) {
    cb(serverHandle, connInfo);
  }
}

static void shelveServices(std::vector<std::shared_ptr<BLEService::Impl>> roots) {
  std::vector<BLEService::Impl *> visited;
  while (!roots.empty()) {
    std::shared_ptr<BLEService::Impl> svc = std::move(roots.back());
    roots.pop_back();
    if (!svc || std::find(visited.begin(), visited.end(), svc.get()) != visited.end()) {
      continue;
    }
    visited.push_back(svc.get());

    for (const auto &included : svc->includedServices) {
      roots.push_back(included);
    }
    bleDetachLocalService(svc);
  }
}

void BLEServerImplCommon::releaseGattTree() {
  std::vector<uint16_t> links;
  std::vector<std::shared_ptr<BLEService::Impl>> tree;
  {
    BLELockGuard lock(mtx);
    for (const auto &link : indicationLinks) {
      links.push_back(link.connHandle);
    }
    tree.swap(services);
    active = false;
    started = false;
    std::vector<std::pair<uint16_t, BLEConnInfo>>().swap(connections);
    std::vector<LongReadState>().swap(longReads);
  }
  for (uint16_t connHandle : links) {
    dropIndications(connHandle);
  }
  shelveServices(std::move(tree));
  {
    BLELockGuard lock(mtx);
    std::vector<IndicationLink>().swap(indicationLinks);
    advertiseOnDisconnect = true;
    onConnectCb = nullptr;
    onDisconnectCb = nullptr;
    onMtuChangedCb = nullptr;
    onConnParamsCb = nullptr;
    onIdentityCb = nullptr;
  }
}

void bleDetachLocalDescriptor(const std::shared_ptr<BLEDescriptor::Impl> &desc) {
  if (!desc) {
    return;
  }
  BLELockGuard lock(desc->mtx);
  desc->handle = 0;
  desc->chr = nullptr;
  std::vector<uint8_t>().swap(desc->value);
  desc->onReadCb = nullptr;
  desc->onWriteCb = nullptr;
}

void bleDetachLocalCharacteristic(const std::shared_ptr<BLECharacteristic::Impl> &chr) {
  if (!chr) {
    return;
  }
  std::vector<std::shared_ptr<BLEDescriptor::Impl>> descs;
  {
    BLELockGuard lock(chr->mtx);
    chr->handle = 0;
    chr->service = nullptr;
    std::vector<std::pair<uint16_t, uint16_t>>().swap(chr->subscribers);
    std::vector<uint8_t>().swap(chr->value);
    chr->onReadCb = nullptr;
    chr->onWriteCb = nullptr;
    chr->onNotifyCb = nullptr;
    chr->onSubscribeCb = nullptr;
    chr->onStatusCb = nullptr;
    descs.swap(chr->descriptors);
  }
  for (auto &desc : descs) {
    bleDetachLocalDescriptor(desc);
  }
}

void bleDetachLocalService(const std::shared_ptr<BLEService::Impl> &svc) {
  if (!svc) {
    return;
  }
  std::vector<std::shared_ptr<BLEService::Impl>>().swap(svc->includedServices);
  std::vector<std::shared_ptr<BLECharacteristic::Impl>> chrs;
  chrs.swap(svc->characteristics);
  svc->handle = 0;
  svc->started = false;
  svc->removing = false;
  svc->server = nullptr;
  for (auto &chr : chrs) {
    bleDetachLocalCharacteristic(chr);
  }
}

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_ENABLED */
