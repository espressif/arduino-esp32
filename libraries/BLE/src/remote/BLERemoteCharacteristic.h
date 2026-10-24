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
#include <functional>
#include "WString.h"
#include "BTStatus.h"
#include "types/BLEUUID.h"
#include "types/BLESubscription.h"
#include <memory>

class BLERemoteService;
class BLERemoteDescriptor;

/**
 * @brief Remote GATT characteristic discovered on a peer device.
 *
 * Shared handle. Obtained via BLERemoteService::getCharacteristic().
 */
class BLERemoteCharacteristic {
public:
  /**
   * @brief Construct an empty (invalid) handle; obtain a live one via BLERemoteService::getCharacteristic().
   */
  BLERemoteCharacteristic();
  ~BLERemoteCharacteristic() = default;
  BLERemoteCharacteristic(const BLERemoteCharacteristic &) = default;
  BLERemoteCharacteristic &operator=(const BLERemoteCharacteristic &) = default;
  BLERemoteCharacteristic(BLERemoteCharacteristic &&) = default;
  BLERemoteCharacteristic &operator=(BLERemoteCharacteristic &&) = default;

  /**
   * @brief Check whether this handle refers to a valid remote characteristic.
   * @return True if the underlying characteristic data is present, false otherwise.
   */
  explicit operator bool() const;

  /**
   * @brief Read the characteristic value from the remote device.
   * @param timeoutMs Maximum time in milliseconds to wait for a response.
   * @return The value as a String, or an empty String on failure/timeout.
   * @note The call blocks the current task until the GATT read completes or
   *       @p timeoutMs elapses. If encryption is being negotiated, the
   *       implementation may wait for the security procedure before the read.
   */
  String readValue(uint32_t timeoutMs = 3000);

  /**
   * @brief Read the characteristic value and interpret the first byte as uint8_t.
   * @param timeoutMs Maximum time in milliseconds to wait for a response.
   * @return The first byte of the value, or 0 on failure.
   */
  uint8_t readUInt8(uint32_t timeoutMs = 3000);

  /**
   * @brief Read the characteristic value and interpret the first two bytes as uint16_t.
   * @param timeoutMs Maximum time in milliseconds to wait for a response.
   * @return The little-endian 16-bit value, or 0 on failure.
   */
  uint16_t readUInt16(uint32_t timeoutMs = 3000);

  /**
   * @brief Read the characteristic value and interpret the first four bytes as uint32_t.
   * @param timeoutMs Maximum time in milliseconds to wait for a response.
   * @return The little-endian 32-bit value, or 0 on failure.
   */
  uint32_t readUInt32(uint32_t timeoutMs = 3000);

  /**
   * @brief Read the characteristic value and interpret the first four bytes as a float.
   * @param timeoutMs Maximum time in milliseconds to wait for a response.
   * @return The float value, or 0.0f on failure.
   */
  float readFloat(uint32_t timeoutMs = 3000);

  /**
   * @brief Read the characteristic value into a caller-supplied buffer.
   * @param buf       Destination buffer.
   * @param bufLen    Size of @p buf in bytes.
   * @param timeoutMs Maximum time in milliseconds to wait for a response.
   * @return Number of bytes copied into @p buf (may be less than the full value if @p bufLen is too small).
   */
  size_t readValue(uint8_t *buf, size_t bufLen, uint32_t timeoutMs = 3000);

  /**
   * @brief Get a pointer to the most recently read raw value without issuing a new read.
   * @param len If non-null, receives the length of the data in bytes.
   * @return Pointer to the internal buffer, or nullptr if no value has been read yet.
   * @note The pointer is invalidated by any subsequent read or write on this characteristic.
   */
  const uint8_t *readRawData(size_t *len = nullptr) const;

  /**
   * @brief Write a value to the remote characteristic.
   * @param data         Pointer to the data to write.
   * @param len          Number of bytes to write.
   * @param withResponse If true, use Write Request (ATT); if false, use Write Command (no ACK).
   * @return BTStatus indicating success or failure.
   * @note For ATT payloads that exceed one connection event or require prepared
   *       writes, the stack selects long-write semantics; @p withResponse
   *       should be true. Blocking behavior matches @ref readValue.
   */
  BTStatus writeValue(const uint8_t *data, size_t len, bool withResponse = true);
  BTStatus writeValue(const String &value, bool withResponse = true);
  BTStatus writeValue(uint8_t value, bool withResponse = true);

  /**
   * @brief Check if the characteristic supports reads.
   * @return True if the Read property is set.
   */
  bool canRead() const;

  /**
   * @brief Check if the characteristic supports writes with response.
   * @return True if the Write property is set.
   */
  bool canWrite() const;

  /**
   * @brief Check if the characteristic supports writes without response.
   * @return True if the Write Without Response property is set.
   */
  bool canWriteNoResponse() const;

  /**
   * @brief Check if the characteristic supports notifications.
   * @return True if the Notify property is set.
   */
  bool canNotify() const;

  /**
   * @brief Check if the characteristic supports indications.
   * @return True if the Indicate property is set.
   */
  bool canIndicate() const;

  /**
   * @brief Check if the characteristic supports broadcast.
   * @return True if the Broadcast property is set.
   */
  bool canBroadcast() const;

  /**
   * @brief Callback type for characteristic notifications and indications.
   * @param chr    The characteristic that received the value.
   * @param data   Pointer to the payload.
   * @param length Length of the payload in bytes.
   * @param kind   @ref BLESubscription::Notify or @ref BLESubscription::Indicate
   *               for this ATT PDU. Never @ref BLESubscription::None and never both bits.
   * @note Runs on the BLE stack task. Do not call blocking GATT operations
   *       (@c readValue(), confirmed @c writeValue(), @c subscribe()) on @p chr
   *       here: they wait for a response that this same task delivers, so they
   *       stall until their timeout. A read also replaces the buffer @p data
   *       points into. Defer such work to another task.
   * @note For the GATT Service Changed characteristic (0x2A05), the discovered
   *       remote tree is invalidated before this callback. Bluedroid does not
   *       expose the peer's changed-handle range, so its callback payload is
   *       the conservative full range `01 00 FF FF`; rediscover services
   *       before using any prior remote handle.
   */
  using NotifyCallback = std::function<void(BLERemoteCharacteristic &chr, const uint8_t *, size_t, BLESubscription)>;

  /**
   * @brief Subscribe to server-initiated deliveries from this characteristic.
   * @param mode     Deliveries to enable (@ref BLESubscription::Notify,
   *                 @ref BLESubscription::Indicate, or both combined with
   *                 `|`). Must not be @ref BLESubscription::None; use
   *                 @c unsubscribe() to disable deliveries.
   * @param callback Optional callback invoked on each delivery.
   * @return BTStatus indicating success or failure.
   * @note Writes the selected bits to the Client Characteristic Configuration
   *       Descriptor (CCCD); the bits replace any previous subscription.
   */
  BTStatus subscribe(BLESubscription mode, NotifyCallback callback = nullptr);

  /**
   * @brief Unsubscribe from notifications and indications.
   * @return BTStatus indicating success or failure.
   */
  BTStatus unsubscribe();

  /**
   * @brief Discover and return a descriptor by UUID.
   * @param uuid UUID of the descriptor to find.
   * @return The matching descriptor, or an invalid handle if not found.
   * @note Triggers GATT descriptor discovery on first call.
   */
  BLERemoteDescriptor getDescriptor(const BLEUUID &uuid) const;

  /**
   * @brief Return all discovered descriptors of this characteristic.
   * @return Vector of descriptor handles (may be empty).
   * @note Triggers GATT descriptor discovery on first call.
   */
  std::vector<BLERemoteDescriptor> getDescriptors() const;

  /**
   * @brief Get the parent service of this characteristic.
   * @return The parent BLERemoteService handle.
   */
  BLERemoteService getRemoteService() const;

  /**
   * @brief Get the UUID of this characteristic.
   * @return The characteristic UUID.
   */
  BLEUUID getUUID() const;

  /**
   * @brief Get the attribute handle of this characteristic.
   * @return The GATT attribute handle.
   */
  uint16_t getHandle() const;

  /**
   * @brief Get a human-readable representation of this characteristic.
   * @return A String describing the characteristic UUID, handle, and properties.
   */
  String toString() const;

  struct Impl;

private:
  explicit BLERemoteCharacteristic(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}

  // Performs a GATT read into _impl->value; returns true on success. Backend-defined;
  // the String/numeric/buffer readers above are shared wrappers over it (no extra copy).
  bool readInto(uint32_t timeoutMs);

  std::shared_ptr<Impl> _impl;
  friend class BLEClient;
  friend class BLERemoteService;
  friend class BLERemoteDescriptor;
  friend struct BLERemoteCharacteristicImplCommon;
};

#endif /* BLE_ENABLED */
