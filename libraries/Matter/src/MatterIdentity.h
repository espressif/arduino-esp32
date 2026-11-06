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

#pragma once

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <crypto/CHIPCryptoPAL.h>

#include <stddef.h>
#include <stdint.h>

// Internal provider wraps used by ArduinoMatter. Not part of the public API.

namespace MatterIdentityInternal {

static constexpr size_t kMaxIdentityLen = 32;
static constexpr size_t kMaxHwStringLen = 64;
static constexpr size_t kMaxSwStringLen = 64;
static constexpr size_t kMaxSerialLen = 32;

bool copyBounded(char *dst, size_t dstSize, const char *src);

class OverrideInstanceInfoProvider : public chip::DeviceLayer::DeviceInstanceInfoProvider {
public:
  bool bindBase(chip::DeviceLayer::DeviceInstanceInfoProvider *current);
  void publish();
  void setVendorName(const char *name);
  void setProductName(const char *name);
  void setHardwareVersion(uint16_t version);
  void setHardwareVersionString(const char *value);
  void setSerialNumber(const char *value);

  CHIP_ERROR GetVendorName(char *buf, size_t bufSize) override;
  CHIP_ERROR GetVendorId(uint16_t &vendorId) override;
  CHIP_ERROR GetProductName(char *buf, size_t bufSize) override;
  CHIP_ERROR GetProductId(uint16_t &productId) override;
  CHIP_ERROR GetPartNumber(char *buf, size_t bufSize) override;
  CHIP_ERROR GetProductURL(char *buf, size_t bufSize) override;
  CHIP_ERROR GetProductLabel(char *buf, size_t bufSize) override;
  CHIP_ERROR GetSerialNumber(char *buf, size_t bufSize) override;
  CHIP_ERROR GetManufacturingDate(uint16_t &year, uint8_t &month, uint8_t &day) override;
  CHIP_ERROR GetHardwareVersion(uint16_t &hardwareVersion) override;
  CHIP_ERROR GetHardwareVersionString(char *buf, size_t bufSize) override;
  CHIP_ERROR GetRotatingDeviceIdUniqueId(chip::MutableByteSpan &uniqueIdSpan) override;

private:
  using StringGetter = CHIP_ERROR (chip::DeviceLayer::DeviceInstanceInfoProvider::*)(char *, size_t);

  static CHIP_ERROR copyToBuf(char *buf, size_t bufSize, const char *src);
  CHIP_ERROR getStringOrBase(char *buf, size_t bufSize, const char *overrideVal, StringGetter getter);
  chip::DeviceLayer::DeviceInstanceInfoProvider *lazyBindBase();
  chip::DeviceLayer::DeviceInstanceInfoProvider *requireBase();

  chip::DeviceLayer::DeviceInstanceInfoProvider *mBase = nullptr;
  const char *mVendorName = nullptr;
  const char *mProductName = nullptr;
  const char *mHardwareVersionString = nullptr;
  const char *mSerialNumber = nullptr;
  uint16_t mHardwareVersion = 0;
  bool mHasHardwareVersion = false;
};

class OverrideCommissionableDataProvider : public chip::DeviceLayer::CommissionableDataProvider {
public:
  bool bindBase(chip::DeviceLayer::CommissionableDataProvider *current);
  void publish();
  void setDiscriminator(uint16_t discriminator);
  void setPasscode(uint32_t passcode);
  CHIP_ERROR ensureVerifier();

  CHIP_ERROR GetSetupDiscriminator(uint16_t &setupDiscriminator) override;
  CHIP_ERROR SetSetupDiscriminator(uint16_t setupDiscriminator) override;
  CHIP_ERROR GetSpake2pIterationCount(uint32_t &iterationCount) override;
  CHIP_ERROR GetSpake2pSalt(chip::MutableByteSpan &saltBuf) override;
  CHIP_ERROR GetSpake2pVerifier(chip::MutableByteSpan &verifierBuf, size_t &outVerifierLen) override;
  CHIP_ERROR GetSetupPasscode(uint32_t &setupPasscode) override;
  CHIP_ERROR SetSetupPasscode(uint32_t setupPasscode) override;

private:
  chip::DeviceLayer::CommissionableDataProvider *requireBase();

  chip::DeviceLayer::CommissionableDataProvider *mBase = nullptr;
  chip::Crypto::Spake2pVerifierSerialized mVerifier = {};
  uint16_t mDiscriminator = 0;
  uint32_t mPasscode = 0;
  bool mHasDiscriminator = false;
  bool mHasPasscode = false;
  bool mHasVerifier = false;
};

}  // namespace MatterIdentityInternal

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
