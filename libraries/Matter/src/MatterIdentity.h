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
#include <lib/support/CodeUtils.h>
#include <lib/support/Span.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <crypto/CHIPCryptoPAL.h>
#include <setup_payload/SetupPayload.h>

#include <string.h>

// Internal provider wraps used by ArduinoMatter. Not part of the public API.

namespace MatterIdentityInternal {

static constexpr size_t kMaxIdentityLen = 32;
static constexpr size_t kMaxHwStringLen = 64;
static constexpr size_t kMaxSerialLen = 32;

// Copies src into dst (NUL-terminated). src may be a stack/String temporary;
// after return only dst is used. Rejects empty or overflow.
inline bool copyBounded(char *dst, size_t dstSize, const char *src) {
  if (dst == nullptr || src == nullptr || dstSize < 2) {
    return false;
  }
  const size_t n = strlen(src);
  if (n == 0 || n + 1 > dstSize) {
    return false;
  }
  memcpy(dst, src, n + 1);
  return true;
}

class OverrideInstanceInfoProvider : public chip::DeviceLayer::DeviceInstanceInfoProvider {
public:
  // Non-owning views of ArduinoMatter file-static buffers only (never sketch/stack pointers).
  // Bind the current provider, then publish under the CHIP stack lock.
  bool bindBase(chip::DeviceLayer::DeviceInstanceInfoProvider *current) {
    if (current == nullptr) {
      return false;
    }
    if (current == this) {
      return true;
    }
    mBase = current;
    return true;
  }

  void publish() {
    chip::DeviceLayer::SetDeviceInstanceInfoProvider(this);
  }

  void setVendorName(const char *name) {
    mVendorName = name;
  }
  void setProductName(const char *name) {
    mProductName = name;
  }
  void setHardwareVersion(uint16_t version) {
    mHardwareVersion = version;
    mHasHardwareVersion = true;
  }
  void setHardwareVersionString(const char *value) {
    mHardwareVersionString = value;
  }
  void setSerialNumber(const char *value) {
    mSerialNumber = value;
  }

  CHIP_ERROR GetVendorName(char *buf, size_t bufSize) override {
    return getStringOrBase(buf, bufSize, mVendorName, &chip::DeviceLayer::DeviceInstanceInfoProvider::GetVendorName);
  }
  CHIP_ERROR GetVendorId(uint16_t &vendorId) override {
    return requireBase()->GetVendorId(vendorId);
  }
  CHIP_ERROR GetProductName(char *buf, size_t bufSize) override {
    return getStringOrBase(buf, bufSize, mProductName, &chip::DeviceLayer::DeviceInstanceInfoProvider::GetProductName);
  }
  CHIP_ERROR GetProductId(uint16_t &productId) override {
    return requireBase()->GetProductId(productId);
  }
  CHIP_ERROR GetPartNumber(char *buf, size_t bufSize) override {
    return requireBase()->GetPartNumber(buf, bufSize);
  }
  CHIP_ERROR GetProductURL(char *buf, size_t bufSize) override {
    return requireBase()->GetProductURL(buf, bufSize);
  }
  CHIP_ERROR GetProductLabel(char *buf, size_t bufSize) override {
    return requireBase()->GetProductLabel(buf, bufSize);
  }
  CHIP_ERROR GetSerialNumber(char *buf, size_t bufSize) override {
    return getStringOrBase(buf, bufSize, mSerialNumber, &chip::DeviceLayer::DeviceInstanceInfoProvider::GetSerialNumber);
  }
  CHIP_ERROR GetManufacturingDate(uint16_t &year, uint8_t &month, uint8_t &day) override {
    return requireBase()->GetManufacturingDate(year, month, day);
  }
  CHIP_ERROR GetHardwareVersion(uint16_t &hardwareVersion) override {
    if (mHasHardwareVersion) {
      hardwareVersion = mHardwareVersion;
      return CHIP_NO_ERROR;
    }
    return requireBase()->GetHardwareVersion(hardwareVersion);
  }
  CHIP_ERROR GetHardwareVersionString(char *buf, size_t bufSize) override {
    return getStringOrBase(buf, bufSize, mHardwareVersionString, &chip::DeviceLayer::DeviceInstanceInfoProvider::GetHardwareVersionString);
  }
  CHIP_ERROR GetRotatingDeviceIdUniqueId(chip::MutableByteSpan &uniqueIdSpan) override {
    return requireBase()->GetRotatingDeviceIdUniqueId(uniqueIdSpan);
  }

private:
  using StringGetter = CHIP_ERROR (chip::DeviceLayer::DeviceInstanceInfoProvider::*)(char *, size_t);

  static CHIP_ERROR copyToBuf(char *buf, size_t bufSize, const char *src) {
    if (buf == nullptr || src == nullptr) {
      return CHIP_ERROR_INVALID_ARGUMENT;
    }
    const size_t n = strlen(src);
    if (n + 1 > bufSize) {
      return CHIP_ERROR_BUFFER_TOO_SMALL;
    }
    memcpy(buf, src, n + 1);
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR getStringOrBase(char *buf, size_t bufSize, const char *overrideVal, StringGetter getter) {
    if (overrideVal != nullptr && overrideVal[0] != '\0') {
      return copyToBuf(buf, bufSize, overrideVal);
    }
    return (requireBase()->*getter)(buf, bufSize);
  }

  chip::DeviceLayer::DeviceInstanceInfoProvider *requireBase() {
    VerifyOrDie(mBase != nullptr);
    return mBase;
  }

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
  // Bind the current provider so salt/iterations are available, then publish under the CHIP stack lock.
  bool bindBase(chip::DeviceLayer::CommissionableDataProvider *current) {
    if (current == nullptr) {
      return false;
    }
    if (current == this) {
      return true;
    }
    mBase = current;
    return true;
  }

  void publish() {
    chip::DeviceLayer::SetCommissionableDataProvider(this);
  }

  void setDiscriminator(uint16_t discriminator) {
    mDiscriminator = discriminator;
    mHasDiscriminator = true;
  }

  void setPasscode(uint32_t passcode) {
    mPasscode = passcode;
    mHasPasscode = true;
    mHasVerifier = false;
  }

  CHIP_ERROR ensureVerifier() {
    if (!mHasPasscode || mHasVerifier) {
      return CHIP_NO_ERROR;
    }
    uint32_t iterationCount = 0;
    uint8_t saltBytes[chip::Crypto::kSpake2p_Max_PBKDF_Salt_Length] = {};
    chip::MutableByteSpan salt(saltBytes);
    ReturnErrorOnFailure(GetSpake2pIterationCount(iterationCount));
    ReturnErrorOnFailure(GetSpake2pSalt(salt));
    chip::Crypto::Spake2pVerifier verifier;
    ReturnErrorOnFailure(verifier.Generate(iterationCount, salt, mPasscode));
    chip::MutableByteSpan serialized(mVerifier);
    ReturnErrorOnFailure(verifier.Serialize(serialized));
    mHasVerifier = true;
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR GetSetupDiscriminator(uint16_t &setupDiscriminator) override {
    if (mHasDiscriminator) {
      setupDiscriminator = mDiscriminator;
      return CHIP_NO_ERROR;
    }
    return requireBase()->GetSetupDiscriminator(setupDiscriminator);
  }

  CHIP_ERROR SetSetupDiscriminator(uint16_t setupDiscriminator) override {
    if (setupDiscriminator > chip::kMaxDiscriminatorValue) {
      return CHIP_ERROR_INVALID_ARGUMENT;
    }
    setDiscriminator(setupDiscriminator);
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR GetSpake2pIterationCount(uint32_t &iterationCount) override {
    return requireBase()->GetSpake2pIterationCount(iterationCount);
  }

  CHIP_ERROR GetSpake2pSalt(chip::MutableByteSpan &saltBuf) override {
    return requireBase()->GetSpake2pSalt(saltBuf);
  }

  CHIP_ERROR GetSpake2pVerifier(chip::MutableByteSpan &verifierBuf, size_t &outVerifierLen) override {
    if (!mHasPasscode) {
      return requireBase()->GetSpake2pVerifier(verifierBuf, outVerifierLen);
    }
    ReturnErrorOnFailure(ensureVerifier());
    outVerifierLen = chip::Crypto::kSpake2p_VerifierSerialized_Length;
    VerifyOrReturnError(verifierBuf.size() >= outVerifierLen, CHIP_ERROR_BUFFER_TOO_SMALL);
    memcpy(verifierBuf.data(), mVerifier, outVerifierLen);
    verifierBuf.reduce_size(outVerifierLen);
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR GetSetupPasscode(uint32_t &setupPasscode) override {
    if (mHasPasscode) {
      setupPasscode = mPasscode;
      return CHIP_NO_ERROR;
    }
    return requireBase()->GetSetupPasscode(setupPasscode);
  }

  CHIP_ERROR SetSetupPasscode(uint32_t setupPasscode) override {
    if (!chip::PayloadContents::IsValidSetupPIN(setupPasscode)) {
      return CHIP_ERROR_INVALID_ARGUMENT;
    }
    setPasscode(setupPasscode);
    return CHIP_NO_ERROR;
  }

private:
  chip::DeviceLayer::CommissionableDataProvider *requireBase() {
    VerifyOrDie(mBase != nullptr);
    return mBase;
  }

  chip::DeviceLayer::CommissionableDataProvider *mBase = nullptr;
  uint8_t mVerifier[chip::Crypto::kSpake2p_VerifierSerialized_Length] = {};
  uint16_t mDiscriminator = 0;
  uint32_t mPasscode = 0;
  bool mHasDiscriminator = false;
  bool mHasPasscode = false;
  bool mHasVerifier = false;
};

}  // namespace MatterIdentityInternal

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
