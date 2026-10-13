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
 * @brief Stack-agnostic shared (common) implementation state for @c BLEServer::Impl.
 *
 * Holds every member that is identical across NimBLE and Bluedroid. The active
 * backend defines @c BLEServer::Impl in @c impl/<stack>/ inheriting this base,
 * adding only the stack-specific state. This header must stay stack-agnostic: it
 * may not include any NimBLE/Bluedroid or native BT header.
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "server/BLEServer.h"
#include "gatt/BLECharacteristic.h"
#include "core/BLEMutex.h"
#include <vector>
#include <memory>
#include <utility>

/**
 * @brief Shared, stack-agnostic state and helpers for a GATT server.
 *
 * Dispatch helpers and connection bookkeeping live here so both backends share
 * one implementation (items 4, 15). @c BLEServer::Impl inherits this base, so
 * backend code accesses these members cast-free as @c impl.member.
 */
struct BLEServerImplCommon : std::enable_shared_from_this<BLEServer::Impl> {
  // True only while this singleton belongs to an initialized BLE session.
  // Guarded by mtx. end() clears it before dispatching teardown statuses so a
  // re-entrant callback cannot populate a GATT tree during shutdown.
  bool active = false;
  bool started = false;
  bool advertiseOnDisconnect = true;

  std::vector<std::shared_ptr<BLEService::Impl>> services;

  BLEServer::ConnectHandler onConnectCb = nullptr;
  BLEServer::DisconnectHandler onDisconnectCb = nullptr;
  BLEServer::MtuChangedHandler onMtuChangedCb = nullptr;
  BLEServer::ConnParamsHandler onConnParamsCb = nullptr;
  BLEServer::IdentityHandler onIdentityCb = nullptr;

  std::vector<std::pair<uint16_t, BLEConnInfo>> connections;

  // Per-connection snapshot of an attribute value across a multi-packet long
  // read (ATT_READ_REQ followed by ATT_READ_BLOB_REQs; Core Vol 3 Part F §3.2.9).
  // Because ATT is strictly sequential per bearer, at most one long read can be
  // in progress per connection at any time. Guarded by mtx.
  struct LongReadState {
    uint16_t connHandle = 0;
    uint16_t attrHandle = 0;
    std::vector<uint8_t> value;
  };
  std::vector<LongReadState> longReads;

  // Outgoing indications, per connection. ATT allows one unconfirmed indication
  // per bearer (Core Vol 3 Part F §3.3.2) and neither stack queues the next one,
  // so later indications wait here and go out in call order as each one is
  // confirmed. Guarded by mtx.
  static constexpr size_t kMaxQueuedIndications = 8;
  struct PendingValue {
    std::shared_ptr<BLECharacteristic::Impl> chr;
    std::vector<uint8_t> payload;
  };
  struct IndicationLink {
    uint16_t connHandle = 0;
    std::shared_ptr<BLECharacteristic::Impl> inFlight;  // Sent, awaiting confirmation.
    uint32_t sentAtMs = 0;
    uint32_t inFlightSeq = 0;
    bool stackBusy = false;  // Stack holds an indication this queue did not send.
    bool frontBusy = false;  // Queue front has been refused as busy since frontBusySinceMs.
    uint32_t frontBusySinceMs = 0;
    std::vector<PendingValue> queue;
  };
  uint32_t indicationSeq = 0;
  std::vector<IndicationLink> indicationLinks;

  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  ~BLEServerImplCommon() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  // Connection table helpers. Caller must hold mtx.

  /**
   * @brief Insert or update a connection entry, keyed by connection handle.
   * @param connHandle Connection handle used as the lookup key.
   * @param connInfo   Connection information to store.
   * @note Updates in place if connHandle already exists, otherwise appends.
   */
  void connSet(uint16_t connHandle, const BLEConnInfo &connInfo);

  /**
   * @brief Remove a connection entry by handle (no-op if absent).
   * @param connHandle Connection handle to remove.
   */
  void connErase(uint16_t connHandle);

  /**
   * @brief Find a connection entry by handle.
   * @param connHandle Connection handle to look up.
   * @return Pointer to the stored BLEConnInfo, or nullptr if not found.
   * @note The pointer is invalidated by any later mutation of the connection list.
   */
  BLEConnInfo *connFind(uint16_t connHandle);

  /** @brief Whether @p chr is still attached to a started, non-removing service on this server. Caller holds mtx. */
  bool characteristicCanSend(const BLECharacteristic::Impl *chr) const;

  /**
   * @brief Select the attribute bytes to serve a read at @p offset, caching a snapshot on offset 0 when needed.
   * @note Caller holds @c mtx.
   */
  const std::vector<uint8_t> &selectLongRead(
    uint16_t connHandle, uint16_t attrHandle, uint16_t offset, uint16_t mtu, const std::vector<uint8_t> &current
  );

  /**
   * @brief Drop the long-read snapshot for @p connHandle once the final Read Blob is served.
   * @note Caller holds @c mtx.
   */
  void finishLongRead(uint16_t connHandle, uint16_t offset, uint16_t mtu);

  // Indication queue. These take mtx themselves and report outcomes through the
  // characteristic's onStatus outside the lock.

  /**
   * @brief Send an indication to @p connHandle now, or queue it behind the one awaiting confirmation.
   * @return OK when sent or queued; Busy when the queue is full; Fail when the stack rejected it.
   */
  BTStatus indicate(uint16_t connHandle, const std::shared_ptr<BLECharacteristic::Impl> &chr, std::vector<uint8_t> payload);

  /**
   * @brief Backend: an indication on @p connHandle reached its final state; sends the next one.
   */
  bool indicationDone(uint16_t connHandle, uint16_t attrHandle, BLECharacteristic::NotifyStatus status, uint32_t code);

  /**
   * @brief Backend: the stack asynchronously refused the in-flight indication as busy (Bluedroid).
   */
  bool indicationBusy(uint16_t connHandle, uint16_t attrHandle, uint32_t code);

  /** @brief Backend: retry indications deferred by stackBusy. */
  void retryIndications();

  /**
   * @brief Backend: @p connHandle closed. Reports any in-flight and queued indications.
   */
  void dropIndications(uint16_t connHandle);

  /**
   * @brief Whether removing @p svc would race an in-flight or queued send.
   */
  bool serviceHasPendingSends(const BLEService::Impl *svc);

  // Handle factory for backend code that holds a raw Impl* (e.g. a GAP callback's
  // void* arg) and is not a friend of BLEServer: rebuilds the owning shared_ptr via
  // shared_from_this() and mints the handle through the private ctor, which only this
  // befriended ImplCommon can call. Keeps the friend surface to one type per class.
  // See DESIGN.md "Handle construction: makeHandle vs. the private constructor".
  static BLEServer makeHandle(BLEServer::Impl *impl);

  /**
   * @brief Drop the GATT tree, connections, and indication queues.
   * @note Called from @c BLE.end(). The server singleton stays so existing
   *       @c BLEServer handles remain the same object after @c begin(); user-held
   *       service/characteristic/descriptor handles become shells (DESIGN.md).
   */
  void releaseGattTree();

  // Stack-agnostic callback dispatch (copies the handler under mtx, invokes it
  // outside the lock). Shared by both backends.
  void dispatchConnect(const BLEConnInfo &connInfo);
  void dispatchDisconnect(const BLEConnInfo &connInfo, uint8_t reason);
  void dispatchMtuChanged(const BLEConnInfo &connInfo, uint16_t mtu);
  void dispatchConnParamsUpdate(const BLEConnInfo &connInfo);
  void dispatchIdentityResolved(const BLEConnInfo &connInfo);

  // Forward a GAP event to the server's private handler on behalf of backend
  // advertising code, which is not itself a friend of BLEServer. Keeps the
  // friend surface to this one agnostically-named type (see makeHandle above).
  static int forwardGapEvent(BLEServer &server, void *event) {
    return server.handleGapEvent(event);
  }

  IndicationLink *indicationLink(uint16_t connHandle);
  IndicationLink &indicationLinkFor(uint16_t connHandle);
  void eraseIndicationLink(uint16_t connHandle);
  bool pumpIndications(uint16_t connHandle);

private:
  void eraseIdleIndicationLink(uint16_t connHandle);
  bool markFrontBusy(IndicationLink &link);
};

/** @brief Outcome of handing one indication to the stack (see @c bleServerSendIndication). */
enum class BLEIndicationSend : uint8_t {
  Sent,    // Accepted; the confirmation is reported through indicationDone().
  Busy,    // The stack is still waiting on an indication this queue did not send.
  Failed,  // Rejected; status/code say why.
};

/**
 * @brief Whether an indication on @p connHandle must wait for in-flight notifications to complete (Bluedroid).
 * @note Called with @c impl->mtx held.
 */
bool bleServerDeferIndication(BLEServer::Impl *impl, uint16_t connHandle);

/**
 * @brief Hand one indication to the native stack.
 * @param status Set on @c Failed: the onStatus code to report.
 * @param code   Set on @c Failed: the stack error.
 *
 * Implemented per-backend (BLEServer.nimble.cpp / BLEServer.bluedroid.cpp) and called by the
 * indication queue without mtx held.
 */
BLEIndicationSend bleServerSendIndication(
  BLEServer::Impl *impl, uint16_t connHandle, uint16_t attrHandle, const uint8_t *data, size_t length, BLECharacteristic::NotifyStatus &status, uint32_t &code
);

/**
 * @brief Send one notification to @p connHandle on the active backend.
 */
BTStatus bleCharacteristicSendNotify(BLECharacteristic::Impl &impl, uint16_t connHandle, const uint8_t *data, size_t length);

#if BLE_GATT_SERVER_SUPPORTED
/**
 * @brief Unregister a GATT service from the native stack.
 * @param impl  Server implementation that owns the GATT database.
 * @param svc   Service implementation to remove.
 * @return      @c BTStatus::OK on success, or an error if removal/rebuild failed.
 * @note        Bluedroid deletes the service in place; NimBLE may rebuild the full GATT table.
 *
 * Implemented per-backend (BLEServer.nimble.cpp / BLEServer.bluedroid.cpp). Declared here
 * so callers get it alongside the shared server Impl base without a selector header.
 */
BTStatus bleServerRemoveService(BLEServer::Impl *impl, std::shared_ptr<BLEService::Impl> svc);
#endif

#endif /* BLE_ENABLED */
