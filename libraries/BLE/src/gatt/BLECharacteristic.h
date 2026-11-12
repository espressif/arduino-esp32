/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
 * Copyright 2017 Neil Kolban
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

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include <vector>
#include "BTStatus.h"
#include "types/BLEUUID.h"
#include "types/BLEProperty.h"
#include "types/BLEConnInfo.h"
#include <memory>
#include <functional>

class BLEService;
class BLEDescriptor;

/**
 * @brief GATT Characteristic handle.
 *
 * Lightweight shared handle wrapping a BLECharacteristic::Impl.
 * Create via service.createCharacteristic(uuid, properties).
 *
 */
class BLECharacteristic {
public:
  BLECharacteristic();
  ~BLECharacteristic() = default;
  BLECharacteristic(const BLECharacteristic &) = default;
  BLECharacteristic &operator=(const BLECharacteristic &) = default;
  BLECharacteristic(BLECharacteristic &&) = default;
  BLECharacteristic &operator=(BLECharacteristic &&) = default;

  /**
   * @brief Check whether this handle refers to a valid characteristic.
   * @return true if the handle is backed by a live Impl, false otherwise.
   */
  explicit operator bool() const;

  /**
   * @brief Status codes for @c onStatus() after @c notify() or @c indicate().
   */
  enum NotifyStatus {
    SUCCESS_INDICATE,         ///< Indication sent and confirmed.
    SUCCESS_NOTIFY,           ///< Notification sent.
    ERROR_INDICATE_DISABLED,  ///< Target client has not enabled indications.
    ERROR_NOTIFY_DISABLED,    ///< Target client has not enabled notifications.
    ERROR_GATT,               ///< Low-level GATT error.
    ERROR_NO_CLIENT,          ///< No client connected, target handle invalid, or the link dropped before delivery.
    ERROR_NO_SUBSCRIBER,      ///< Clients are connected but none is subscribed.
    ERROR_INDICATE_TIMEOUT,   ///< Indication not confirmed within 30 s (the link is then closed).
    ERROR_INDICATE_FAILURE,   ///< Indication could not be sent, was rejected, or the queue was full.
  };

  // Handler types

  /**
   * @brief Callback invoked when a client reads this characteristic.
   * @param chr The characteristic being read. Calling @c chr.setValue() here
   *            sets the value returned in this read response.
   * @param conn Information about the connected client.
   * @note Invoked once per client read. A long read (value larger than
   *       MTU - 1) is served from the value set here; the follow-up Read Blob
   *       requests do not invoke the handler again. The library keeps a
   *       per-connection copy of that value until the read completes, so a
   *       concurrent setValue() cannot mix two values in one long read.
   * @note The mutable @p chr reference is borrowed for the callback. Copy the
   *       handle to retain it; do not retain a pointer or reference to the
   *       callback parameter itself. This rule applies to every handler below.
   */
  using ReadHandler = std::function<void(BLECharacteristic &chr, const BLEConnInfo &conn)>;

  /**
   * @brief Callback invoked when a client writes to this characteristic.
   * @param chr The characteristic being written (value already updated).
   * @param conn Information about the connected client.
   */
  using WriteHandler = std::function<void(BLECharacteristic &chr, const BLEConnInfo &conn)>;

  /**
   * @brief Callback invoked once per @c notify() / @c indicate() call, right
   *        before the payload is built, when there is at least one recipient.
   * @param chr The characteristic about to be sent. When @c notify() /
   *            @c indicate() was called without a payload, a value set here
   *            with @c chr.setValue() is the one sent to every recipient.
   */
  using NotifyHandler = std::function<void(BLECharacteristic &chr)>;

  /**
   * @brief Callback invoked when a client subscribes to or unsubscribes from
   *        notifications/indications (CCCD write).
   * @param chr The characteristic whose subscription state changed.
   * @param conn Information about the connected client.
   * @param subValue Bitmask: bit 0 = notifications, bit 1 = indications.
   *                 0 means the client unsubscribed.
   */
  using SubscribeHandler = std::function<void(BLECharacteristic &chr, const BLEConnInfo &conn, uint16_t subValue)>;

  /**
   * @brief Callback invoked with the delivery status of a notification or indication.
   * @param chr The characteristic that sent the notification/indication.
   * @param status Outcome of the operation: @c SUCCESS_NOTIFY once a
   *               notification is sent, @c SUCCESS_INDICATE once the client
   *               confirms an indication, an @c ERROR_* value otherwise.
   *               A broadcast @c notify() / @c indicate() with no recipient
   *               reports @c ERROR_NO_CLIENT (nobody connected) or
   *               @c ERROR_NO_SUBSCRIBER (nobody subscribed); the per-connection
   *               overloads report @c ERROR_NO_CLIENT or
   *               @c ERROR_NOTIFY_DISABLED / @c ERROR_INDICATE_DISABLED.
   * @param code Backend-specific error code (0 on success).
   * @note May run on the calling task (NimBLE reports notification results
   *       before @c notify() returns) or on the BLE stack task.
   */
  using StatusHandler = std::function<void(BLECharacteristic &chr, NotifyStatus status, uint32_t code)>;

  /**
   * @brief Register a callback invoked when a client reads this characteristic.
   * @param handler The read callback, or nullptr to clear.
   */
  void onRead(ReadHandler handler);

  /**
   * @brief Register a callback invoked when a client writes to this characteristic.
   * @param handler The write callback, or nullptr to clear.
   * @note On NimBLE, the stack typically sends the ATT write response after this
   *       handler returns; keep the callback short so peers are not stalled.
   */
  void onWrite(WriteHandler handler);

  /**
   * @brief Register a callback invoked once per notify() / indicate() call, before sending.
   * @param handler The notify callback, or nullptr to clear.
   */
  void onNotify(NotifyHandler handler);

  /**
   * @brief Register a callback invoked when a client subscribes or unsubscribes.
   * @param handler The subscribe callback, or nullptr to clear.
   */
  void onSubscribe(SubscribeHandler handler);

  /**
   * @brief Register a callback invoked with notification/indication delivery status.
   * @param handler The status callback, or nullptr to clear.
   */
  void onStatus(StatusHandler handler);

  /**
   * @brief Remove all previously registered callbacks from this characteristic.
   */
  void resetCallbacks();

  // Value access

  /**
   * @brief Set the characteristic value from a raw byte buffer.
   * @param data Pointer to the data bytes.
   * @param length Number of bytes to copy.
   * @note Thread-safe: the internal value buffer is guarded by a mutex.
   *       The convenience overloads store their argument in host (little-endian)
   *       byte order with no endian conversion; the String overload copies the
   *       raw bytes and does not store a null terminator.
   */
  void setValue(const uint8_t *data, size_t length);
  void setValue(const String &value);
  void setValue(uint16_t value);
  void setValue(uint32_t value);
  void setValue(int value);
  void setValue(float value);
  void setValue(double value);

  /**
   * @brief Set the characteristic value from an arbitrary trivially-copyable type.
   * @tparam T A trivially-copyable type.
   * @param value The value to store (copied byte-for-byte).
   */
  template<typename T> void setValue(const T &value) {
    setValue(reinterpret_cast<const uint8_t *>(&value), sizeof(T));
  }

  /**
   * @brief Get a pointer to the raw characteristic value.
   * @param length If non-null, receives the length of the value in bytes (0 when invalid or empty).
   * @return Pointer to the internal value buffer, or nullptr if the handle is invalid or the value is empty.
   * @note Thread-safe. The returned pointer is invalidated by a concurrent setValue().
   */
  const uint8_t *getValue(size_t *length = nullptr) const;

  /**
   * @brief Get the characteristic value as an Arduino String.
   * @return The value interpreted as a UTF-8 string.
   */
  String getStringValue() const;

  /**
   * @brief Get the characteristic value reinterpreted as type T.
   * @tparam T A trivially-copyable type to reinterpret the value as.
   * @return The value, or a value-initialized T if the stored data is shorter than sizeof(T).
   */
  template<typename T> T getValue() const {
    size_t len = 0;
    const uint8_t *data = getValue(&len);
    T result{};
    if (data && len >= sizeof(T)) {
      memcpy(&result, data, sizeof(T));
    }
    return result;
  }

  // Notifications / Indications

  /**
   * @brief Send a notification to all clients that enabled notifications.
   * @param data Optional payload. If nullptr, the current stored value is sent.
   * @param length Number of bytes in @p data (ignored when @p data is nullptr).
   * @return BTStatus::OK when the call was accepted, including when there is no
   *         recipient (reported through @c onStatus()); per-client delivery
   *         results are reported through @c onStatus().
   */
  BTStatus notify(const uint8_t *data = nullptr, size_t length = 0);

  /**
   * @brief Send a notification to a specific client.
   * @param connHandle Connection handle of the target client.
   * @param data Optional payload. If nullptr, the current stored value is sent.
   * @param length Number of bytes in @p data (ignored when @p data is nullptr).
   * @return BTStatus::OK if sent, BTStatus::NotConnected if @p connHandle is
   *         not connected, BTStatus::InvalidState if that client has not
   *         enabled notifications, or another error status.
   */
  BTStatus notify(uint16_t connHandle, const uint8_t *data = nullptr, size_t length = 0);

  /**
   * @brief Send an indication to all clients that enabled indications.
   * @param data Optional payload. If nullptr, the current stored value is sent.
   * @param length Number of bytes in @p data (ignored when @p data is nullptr).
   * @return BTStatus::OK when every recipient accepted or queued the call
   *         (including when there is no recipient), or @c BTStatus::Busy if any
   *         client's queue is full; outcomes are reported through @c onStatus().
   * @note Does not block. ATT allows one unconfirmed indication per client, so
   *       an indication sent while another is awaiting confirmation is queued
   *       behind it (1 in-flight + up to 8 queued per client, 9 accepted in
   *       total before @c BTStatus::Busy) and sent in order. @c onStatus() reports
   *       @c SUCCESS_INDICATE once each one is confirmed. An indication the
   *       stack refuses because it is still waiting on one it sent itself
   *       (such as Service Changed) is retried until the ATT transaction
   *       timeout (30 s) passes.
   */
  BTStatus indicate(const uint8_t *data = nullptr, size_t length = 0);

  /**
   * @brief Send an indication to a specific client.
   * @param connHandle Connection handle of the target client.
   * @param data Optional payload. If nullptr, the current stored value is sent.
   * @param length Number of bytes in @p data (ignored when @p data is nullptr).
   * @return BTStatus::OK if sent or queued, BTStatus::NotConnected if
   *         @p connHandle is not connected, BTStatus::InvalidState if that
   *         client has not enabled indications, BTStatus::Busy if its queue
   *         is full, or another error status.
   * @note Queued like the broadcast overload.
   */
  BTStatus indicate(uint16_t connHandle, const uint8_t *data = nullptr, size_t length = 0);

  // Properties and permissions

  /**
   * @brief Get the GATT properties of this characteristic.
   * @return Bitmask of BLEProperty flags (Read, Write, Notify, etc.).
   */
  BLEProperty getProperties() const;

  /**
   * @brief Set the ATT permissions for this characteristic value.
   * @param permissions Bitmask of BLEPermission flags.
   */
  void setPermissions(BLEPermission permissions);

  /**
   * @brief Get the ATT permissions for this characteristic value.
   * @return Bitmask of BLEPermission flags.
   */
  BLEPermission getPermissions() const;

  // Descriptor management

  /**
   * @brief Create and add a new descriptor to this characteristic.
   * @param uuid UUID of the descriptor.
   * @param perms ATT permissions for the descriptor.
   * @param maxLen Maximum value length in bytes.
   * @return Handle to the newly created descriptor.
   */
  BLEDescriptor createDescriptor(const BLEUUID &uuid, BLEPermission perms = BLEPermission::ReadOpen, size_t maxLen = 100);

  /**
   * @brief Find a descriptor by UUID.
   * @param uuid UUID to search for.
   * @return Handle to the descriptor, or an invalid handle if not found.
   */
  BLEDescriptor getDescriptor(const BLEUUID &uuid) const;

  /**
   * @brief Get all descriptors attached to this characteristic.
   * @return Vector of descriptor handles.
   */
  std::vector<BLEDescriptor> getDescriptors() const;

  /**
   * @brief Remove a descriptor from this characteristic.
   * @param desc Handle to the descriptor to remove.
   * @note This is a pre-start database-building operation. It is a no-op after
   *       the owning service starts. A retained descriptor handle becomes a
   *       detached shell.
   */
  void removeDescriptor(const BLEDescriptor &desc);

  // Subscription state

  /**
   * @brief Get the number of clients currently subscribed (notify or indicate).
   * @return Subscriber count.
   */
  size_t getSubscribedCount() const;

  /**
   * @brief Get the connection handles of all subscribed clients.
   * @return Vector of connection handles.
   */
  std::vector<uint16_t> getSubscribedConnections() const;

  /**
   * @brief Check whether a specific client is subscribed.
   * @param connHandle Connection handle to query.
   * @return true if the client is subscribed for notifications or indications.
   */
  bool isSubscribed(uint16_t connHandle) const;

  /**
   * @brief Get the UUID of this characteristic.
   * @return The characteristic UUID.
   */
  BLEUUID getUUID() const;

  /**
   * @brief Get the ATT attribute handle of this characteristic.
   * @return The 16-bit attribute handle.
   */
  uint16_t getHandle() const;

  /**
   * @brief Get the parent service that owns this characteristic.
   * @return Handle to the parent BLEService.
   */
  BLEService getService() const;

  /**
   * @brief Set the Characteristic User Description (0x2901 descriptor).
   * @param desc Human-readable description string.
   * @note Creates or updates the 0x2901 descriptor automatically.
   */
  void setDescription(const String &desc);

  /**
   * @brief Get a human-readable string representation of this characteristic.
   * @return String containing UUID, handle, and properties.
   */
  String toString() const;

  struct Impl;

private:
  explicit BLECharacteristic(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;
  friend class BLEServer;
  friend class BLEService;
  friend class BLEDescriptor;
  friend struct BLECharacteristicImplCommon;
};

#endif /* BLE_ENABLED */
