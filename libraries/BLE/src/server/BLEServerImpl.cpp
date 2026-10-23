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

BLEServerImplCommon::BLEServerImplCommon() = default;

BLEServerImplCommon::~BLEServerImplCommon() {
  if (mtx) {
    vSemaphoreDelete(mtx);
  }
}

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
  for (auto &lr : longReads) {
    if (!lr.value.empty() && lr.connHandle == connHandle) {
      lr.value.clear();
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
  if (!chr) {
    return false;
  }
  auto svc = chr->serviceOwner.lock();
  if (!svc || !svc->started || svc->removing || svc->server != this) {
    return false;
  }
  bool owned = std::any_of(svc->characteristics.begin(), svc->characteristics.end(), [chr](const auto &entry) {
    return entry.get() == chr;
  });
  if (!owned) {
    return false;
  }
  return chr->handle != 0;
}

void BLEServerImplCommon::addPendingNotify(uint16_t connHandle, uint16_t attrHandle) {
  for (auto &entry : pendingNotifies) {
    if (entry.connHandle == connHandle && entry.attrHandle == attrHandle) {
      entry.reserved++;
      return;
    }
  }
  pendingNotifies.push_back({connHandle, attrHandle, 1, 0});
}

void BLEServerImplCommon::markNotifySubmitted(uint16_t connHandle, uint16_t attrHandle) {
  for (auto &entry : pendingNotifies) {
    if (entry.connHandle == connHandle && entry.attrHandle == attrHandle && entry.reserved > 0) {
      entry.reserved--;
      entry.submitted++;
      return;
    }
  }
}

bool BLEServerImplCommon::consumePendingNotify(uint16_t connHandle, uint16_t attrHandle) {
  for (auto it = pendingNotifies.begin(); it != pendingNotifies.end(); ++it) {
    if (it->connHandle == connHandle && it->attrHandle == attrHandle && it->submitted > 0) {
      if (--it->submitted == 0 && it->reserved == 0) {
        pendingNotifies.erase(it);
      }
      return true;
    }
  }
  return false;
}

void BLEServerImplCommon::erasePendingNotifies(uint16_t connHandle) {
  for (auto it = pendingNotifies.begin(); it != pendingNotifies.end();) {
    if (it->connHandle == connHandle) {
      it = pendingNotifies.erase(it);
    } else {
      ++it;
    }
  }
}

bool BLEServerImplCommon::hasPendingNotifications(const BLEService::Impl *svc) const {
  if (!svc) {
    return false;
  }
  for (const auto &entry : pendingNotifies) {
    if (entry.total() > 0) {
      for (const auto &chr : svc->characteristics) {
        if (chr->handle == entry.attrHandle) {
          return true;
        }
      }
    }
  }
  return false;
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
    bool isLong = (mtu > 1 && current.size() >= static_cast<size_t>(mtu - 1));
    LongReadState *connSlot = nullptr;
    LongReadState *freeSlot = nullptr;
    for (auto &lr : longReads) {
      if (!lr.value.empty() && lr.connHandle == connHandle && !connSlot) {
        connSlot = &lr;
      }
      if (lr.value.empty() && !freeSlot) {
        freeSlot = &lr;
      }
    }
    // Never steal a live slot: another connection's Read Blob sequence would
    // then be served bytes from this read. Fall back to the current value.
    LongReadState *slot = connSlot ? connSlot : freeSlot;
    if (isLong && !slot) {
      return current;
    }
    if (isLong) {
      slot->connHandle = connHandle;
      slot->attrHandle = attrHandle;
      slot->value = current;
    } else if (connSlot) {
      connSlot->value.clear();
    }
    return current;
  }
  for (auto &lr : longReads) {
    if (!lr.value.empty() && lr.connHandle == connHandle) {
      if (lr.attrHandle == attrHandle) {
        return lr.value;
      }
      lr.value.clear();
    }
  }
  return current;
}

void BLEServerImplCommon::finishLongRead(uint16_t connHandle, uint16_t attrHandle, uint16_t offset, uint16_t mtu) {
  for (auto &lr : longReads) {
    if (!lr.value.empty() && lr.connHandle == connHandle && lr.attrHandle == attrHandle) {
      size_t size = lr.value.size();
      if (mtu <= 1 || offset > size || size - offset < static_cast<size_t>(mtu - 1)) {
        lr.value.clear();
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

uint32_t nowMs() {
  return pdTICKS_TO_MS(xTaskGetTickCount());
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
      } else {
        // Keep front.payload intact while in flight: on Bluedroid a busy refusal
        // arrives asynchronously via ESP_GATTS_CONF_EVT (indicationBusy()), which
        // retries front.payload directly without re-entering this local scope.
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
    const size_t maxEntries = kMaxQueuedIndications + ((link.inFlight || link.stackBusy) ? 1 : 0);
    if (link.queue.size() < maxEntries) {
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
  if (hasPendingNotifications(svc)) {
    return true;
  }
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
  std::shared_ptr<BLECharacteristic::Impl> completedChr;
  bool linkEnded = status == BLECharacteristic::ERROR_NO_CLIENT || status == BLECharacteristic::ERROR_INDICATE_TIMEOUT;
  {
    BLELockGuard lock(mtx);
    IndicationLink *link = indicationLink(connHandle);
    if (!link) {
      return false;
    }
    if (link->inFlight && link->inFlight->handle == attrHandle) {
      completedChr = std::move(link->inFlight);
      link->frontBusy = false;
      if (!link->queue.empty() && link->queue.front().chr == completedChr) {
        link->queue.erase(link->queue.begin());
      }
    } else if (!link->inFlight && link->stackBusy) {
      link->stackBusy = false;
    } else {
      return false;
    }
  }
  if (completedChr) {
    completedChr->dispatchStatus(status, code);
  }
  if (linkEnded) {
    dropIndications(connHandle);
  } else {
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
  for (uint16_t conn : conns) {
    pumpIndications(conn);
  }
}

void BLEServerImplCommon::dropIndications(uint16_t connHandle) {
  IndicationLink removed;
  bool found = false;
  {
    BLELockGuard lock(mtx);
    for (auto &lr : longReads) {
      if (lr.connHandle == connHandle) {
        lr.value.clear();
      }
    }
    for (auto it = indicationLinks.begin(); it != indicationLinks.end(); ++it) {
      if (it->connHandle == connHandle) {
        removed = std::move(*it);
        indicationLinks.erase(it);
        found = true;
        break;
      }
    }
  }
  if (!found) {
    return;
  }
  if (removed.inFlight) {
    bool timedOut = nowMs() - removed.sentAtMs >= kAttTransactionTimeoutMs;
    removed.inFlight->dispatchStatus(timedOut ? BLECharacteristic::ERROR_INDICATE_TIMEOUT : BLECharacteristic::ERROR_NO_CLIENT, 0);
  }
  for (auto &item : removed.queue) {
    if (item.chr == removed.inFlight) {
      removed.inFlight.reset();
      continue;
    }
    item.chr->dispatchStatus(BLECharacteristic::ERROR_NO_CLIENT, 0);
  }
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
    if (!onConnectCb) {
      return;
    }
    cb = onConnectCb;
  }
  BLEServer serverHandle(shared_from_this());
  cb(serverHandle, connInfo);
}

void BLEServerImplCommon::dispatchDisconnect(const BLEConnInfo &connInfo, uint8_t reason) {
  log_i("Server: client disconnected, handle=%u addr=%s reason=0x%02x", connInfo.getHandle(), connInfo.getAddress().toString().c_str(), reason);
  decltype(onDisconnectCb) cb;
  {
    BLELockGuard lock(mtx);
    if (!onDisconnectCb) {
      return;
    }
    cb = onDisconnectCb;
  }
  BLEServer serverHandle(shared_from_this());
  cb(serverHandle, connInfo, reason);
}

void BLEServerImplCommon::dispatchMtuChanged(const BLEConnInfo &connInfo, uint16_t mtu) {
  log_d("Server: MTU changed, handle=%u mtu=%u", connInfo.getHandle(), mtu);
  decltype(onMtuChangedCb) cb;
  {
    BLELockGuard lock(mtx);
    if (!onMtuChangedCb) {
      return;
    }
    cb = onMtuChangedCb;
  }
  BLEServer serverHandle(shared_from_this());
  cb(serverHandle, connInfo, mtu);
}

void BLEServerImplCommon::dispatchConnParamsUpdate(const BLEConnInfo &connInfo) {
  log_d(
    "Server: conn params updated, handle=%u interval=%u latency=%u timeout=%u", connInfo.getHandle(), connInfo.getInterval(), connInfo.getLatency(),
    connInfo.getSupervisionTimeout()
  );
  decltype(onConnParamsCb) cb;
  {
    BLELockGuard lock(mtx);
    if (!onConnParamsCb) {
      return;
    }
    cb = onConnParamsCb;
  }
  BLEServer serverHandle(shared_from_this());
  cb(serverHandle, connInfo);
}

void BLEServerImplCommon::dispatchIdentityResolved(const BLEConnInfo &connInfo) {
  decltype(onIdentityCb) cb;
  {
    BLELockGuard lock(mtx);
    if (!onIdentityCb) {
      return;
    }
    cb = onIdentityCb;
  }
  BLEServer serverHandle(shared_from_this());
  cb(serverHandle, connInfo);
}

static void shelveServices(std::vector<std::shared_ptr<BLEService::Impl>> roots) {
  while (!roots.empty()) {
    std::shared_ptr<BLEService::Impl> svc = std::move(roots.back());
    roots.pop_back();
    if (!svc) {
      continue;
    }
    for (auto &included : svc->includedServices) {
      roots.push_back(std::move(included));
    }
    bleDetachLocalService(svc);
  }
}

void BLEServerImplCommon::releaseGattTree() {
  std::vector<uint16_t> links;
  std::vector<std::shared_ptr<BLEService::Impl>> tree;
  {
    BLELockGuard lock(mtx);
    links.reserve(indicationLinks.size());
    for (const auto &link : indicationLinks) {
      links.push_back(link.connHandle);
    }
    pendingNotifies.clear();
    tree.swap(services);
    active = false;
    started = false;
    connections.clear();
    for (auto &lr : longReads) {
      lr.value.clear();
    }
  }
  shelveServices(std::move(tree));
  for (uint16_t link : links) {
    dropIndications(link);
  }
  {
    BLELockGuard lock(mtx);
    indicationLinks.clear();
    advertiseOnDisconnect = true;
    onConnectCb = nullptr;
    onDisconnectCb = nullptr;
    onMtuChangedCb = nullptr;
    onConnParamsCb = nullptr;
    onIdentityCb = nullptr;
  }
  pendingDisconnectHandle.store(0xFFFF);
  disconnectSync.give(BTStatus::Fail);
}

void bleDetachLocalDescriptor(const std::shared_ptr<BLEDescriptor::Impl> &desc) {
  if (!desc) {
    return;
  }
  BLELockGuard lock(desc->mtx);
  desc->handle = 0;
  desc->chr = nullptr;
  desc->value.clear();
  desc->onReadCb = nullptr;
  desc->onWriteCb = nullptr;
}

void bleDetachLocalCharacteristic(const std::shared_ptr<BLECharacteristic::Impl> &chr) {
  if (!chr) {
    return;
  }
  auto svc = chr->serviceOwner.lock();
  auto server = svc ? svc->serverOwner.lock() : std::shared_ptr<BLEServer::Impl>();
  if (server) {
    BLELockGuard serverLock(server->mtx);
    chr->subscribers.clear();
  }
  std::vector<std::shared_ptr<BLEDescriptor::Impl>> descs;
  {
    BLELockGuard lock(chr->mtx);
    chr->handle = 0;
    chr->service = nullptr;
    if (!server) {
      chr->subscribers.clear();
    }
    chr->value.clear();
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
  std::vector<std::shared_ptr<BLECharacteristic::Impl>> chrs;
  {
    // Service-tree fields are guarded by the owning server's mtx (the same
    // lock createService/createCharacteristic take). Snapshot and clear them
    // under it, then release it before detaching characteristics, each of
    // which takes its own chr->mtx in bleDetachLocalCharacteristic().
    auto server = svc->serverOwner.lock();
    BLELockGuard lock(server ? server->mtx : nullptr);
    svc->includedServices.clear();
    chrs.swap(svc->characteristics);
    svc->handle = 0;
    svc->started = false;
    svc->removing = false;
    svc->server = nullptr;
  }
  for (auto &chr : chrs) {
    bleDetachLocalCharacteristic(chr);
  }
}

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_ENABLED */
