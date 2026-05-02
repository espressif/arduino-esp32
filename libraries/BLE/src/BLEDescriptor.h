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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BTStatus.h"
#include "BLEUUID.h"
#include "BLEProperty.h"
#include "BLEConnInfo.h"
#include <memory>
#include <functional>

class BLECharacteristic;

/**
 * @brief GATT Descriptor handle.
 *
 * Lightweight shared handle wrapping a BLEDescriptor::Impl.
 * Create via characteristic.createDescriptor(uuid, permissions).
 *
 * Standard descriptor factories:
 *   - CCCD (0x2902) is auto-created for Notify/Indicate characteristics
 *   - createUserDescription(text) creates 0x2901
 *   - createPresentationFormat() creates 0x2904
 *
 */
class BLEDescriptor {
public:
  BLEDescriptor();

  /**
   * @brief Construct a standalone descriptor with a given UUID and max value length.
   * @param uuid UUID for the descriptor.
   * @param maxLength Maximum value length in bytes.
   * @note Prefer BLECharacteristic::createDescriptor() to add a descriptor to a
   *       characteristic directly.
   */
  BLEDescriptor(const BLEUUID &uuid, uint16_t maxLength = 100);
  ~BLEDescriptor() = default;
  BLEDescriptor(const BLEDescriptor &) = default;
  BLEDescriptor &operator=(const BLEDescriptor &) = default;
  BLEDescriptor(BLEDescriptor &&) = default;
  BLEDescriptor &operator=(BLEDescriptor &&) = default;

  /**
   * @brief Check whether this handle refers to a valid descriptor.
   * @return true if the handle is backed by a live Impl, false otherwise.
   */
  explicit operator bool() const;

  /**
   * @brief Callback invoked when a client reads this descriptor.
   * @param desc The descriptor being read.
   * @param conn Information about the connected client.
   */
  using ReadHandler = std::function<void(BLEDescriptor desc, const BLEConnInfo &conn)>;

  /**
   * @brief Callback invoked when a client writes to this descriptor.
   * @param desc The descriptor being written (value already updated).
   * @param conn Information about the connected client.
   */
  using WriteHandler = std::function<void(BLEDescriptor desc, const BLEConnInfo &conn)>;

  /**
   * @brief Register a callback invoked when a client reads this descriptor.
   * @param handler The read callback, or nullptr to clear.
   */
  void onRead(ReadHandler handler);

  /**
   * @brief Register a callback invoked when a client writes to this descriptor.
   * @param handler The write callback, or nullptr to clear.
   */
  void onWrite(WriteHandler handler);

  /**
   * @brief Remove all previously registered callbacks from this descriptor.
   */
  void resetCallbacks();

  /**
   * @brief Set the descriptor value from a raw byte buffer.
   * @param data Pointer to the data bytes.
   * @param length Number of bytes to copy.
   */
  void setValue(const uint8_t *data, size_t length);
  void setValue(const String &value);

  /**
   * @brief Get a pointer to the raw descriptor value.
   * @param length If non-null, receives the length of the value in bytes.
   * @return Pointer to the internal value buffer, or nullptr if the handle is invalid.
   */
  const uint8_t *getValue(size_t *length = nullptr) const;

  /**
   * @brief Get the current value length.
   * @return Number of bytes stored in the descriptor value.
   */
  size_t getLength() const;

  /**
   * @brief Set the ATT permissions for this descriptor.
   * @param perms Bitmask of BLEPermission flags.
   */
  void setPermissions(BLEPermission perms);

  /**
   * @brief Get the UUID of this descriptor.
   * @return The descriptor UUID.
   */
  BLEUUID getUUID() const;

  /**
   * @brief Get the ATT attribute handle of this descriptor.
   * @return The 16-bit attribute handle.
   */
  uint16_t getHandle() const;

  /**
   * @brief Get the parent characteristic that owns this descriptor.
   * @return Handle to the parent BLECharacteristic.
   */
  BLECharacteristic getCharacteristic() const;

  /**
   * @brief Get a human-readable string representation of this descriptor.
   * @return String containing UUID, handle, and value summary.
   */
  String toString() const;

  // Standard descriptor factories

  /**
   * @brief Create a Characteristic User Description descriptor (UUID 0x2901).
   * @param description Human-readable description text.
   * @return A new BLEDescriptor configured as a User Description.
   */
  static BLEDescriptor createUserDescription(const String &description);

  /**
   * @brief Create a Client Characteristic Configuration Descriptor (UUID 0x2902).
   * @return A new BLEDescriptor configured as a CCCD.
   * @note A CCCD is auto-created for characteristics with Notify or Indicate properties;
   *       use this factory only when manual control is needed.
   */
  static BLEDescriptor createCCCD();

  /**
   * @brief Create a Characteristic Presentation Format descriptor (UUID 0x2904).
   * @return A new BLEDescriptor configured as a Presentation Format.
   */
  static BLEDescriptor createPresentationFormat();

  // 0x2901 User Description convenience

  /**
   * @brief Set the User Description string (0x2901).
   * @param description Human-readable description text.
   */
  void setUserDescription(const String &description);

  /**
   * @brief Get the User Description string (0x2901).
   * @return The stored description text.
   */
  String getUserDescription() const;

  // 0x2902 CCCD convenience (auto-created for Notify/Indicate characteristics)

  /**
   * @brief Check whether notifications are enabled in this CCCD.
   * @return true if the notifications bit is set.
   */
  bool getNotifications() const;

  /**
   * @brief Check whether indications are enabled in this CCCD.
   * @return true if the indications bit is set.
   */
  bool getIndications() const;

  /**
   * @brief Enable or disable notifications in this CCCD.
   * @param enable true to set the notifications bit, false to clear it.
   */
  void setNotifications(bool enable);

  /**
   * @brief Enable or disable indications in this CCCD.
   * @param enable true to set the indications bit, false to clear it.
   */
  void setIndications(bool enable);

  // 0x2904 Presentation Format convenience

  /**
   * @brief Set the format field of a Presentation Format descriptor.
   * @param format One of the FORMAT_* constants.
   */
  void setFormat(uint8_t format);

  /**
   * @brief Set the exponent field of a Presentation Format descriptor.
   * @param exponent Signed exponent applied to the characteristic value.
   */
  void setExponent(int8_t exponent);

  /**
   * @brief Set the unit field of a Presentation Format descriptor.
   * @param unit Bluetooth Assigned Number for the unit (e.g., 0x2700 for unitless).
   */
  void setUnit(uint16_t unit);

  /**
   * @brief Set the namespace field of a Presentation Format descriptor.
   * @param ns Bluetooth namespace (typically 0x01 for Bluetooth SIG).
   */
  void setNamespace(uint8_t ns);

  /**
   * @brief Set the description field of a Presentation Format descriptor.
   * @param description Bluetooth SIG-defined description enumeration value.
   */
  void setFormatDescription(uint16_t description);

  // Format constants (Bluetooth Assigned Numbers, Format Types)
  static constexpr uint8_t FORMAT_BOOLEAN = 1;   ///< Boolean.
  static constexpr uint8_t FORMAT_UINT8 = 4;     ///< Unsigned 8-bit integer.
  static constexpr uint8_t FORMAT_UINT16 = 6;    ///< Unsigned 16-bit integer.
  static constexpr uint8_t FORMAT_UINT32 = 8;    ///< Unsigned 32-bit integer.
  static constexpr uint8_t FORMAT_SINT8 = 12;    ///< Signed 8-bit integer.
  static constexpr uint8_t FORMAT_SINT16 = 14;   ///< Signed 16-bit integer.
  static constexpr uint8_t FORMAT_SINT32 = 16;   ///< Signed 32-bit integer.
  static constexpr uint8_t FORMAT_FLOAT32 = 20;  ///< IEEE-754 32-bit float.
  static constexpr uint8_t FORMAT_FLOAT64 = 21;  ///< IEEE-754 64-bit float.
  static constexpr uint8_t FORMAT_UTF8 = 25;     ///< UTF-8 string.
  static constexpr uint8_t FORMAT_UTF16 = 26;    ///< UTF-16 string.

  // Type queries

  /**
   * @brief Check whether this descriptor is a User Description (UUID 0x2901).
   * @return true if UUID matches 0x2901.
   */
  bool isUserDescription() const;

  /**
   * @brief Check whether this descriptor is a CCCD (UUID 0x2902).
   * @return true if UUID matches 0x2902.
   */
  bool isCCCD() const;

  /**
   * @brief Check whether this descriptor is a Presentation Format (UUID 0x2904).
   * @return true if UUID matches 0x2904.
   */
  bool isPresentationFormat() const;

  struct Impl;

private:
  explicit BLEDescriptor(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;
  friend class BLECharacteristic;
  friend class BLEServer;
};

#endif /* BLE_ENABLED */
