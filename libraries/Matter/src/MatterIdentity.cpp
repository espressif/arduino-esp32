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

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include "MatterIdentity.h"
#if CONFIG_CUSTOM_DEVICE_INSTANCE_INFO_PROVIDER
#include <esp_matter_providers.h>
#endif

#include <app/server/Server.h>
#include <lib/support/CHIPMem.h>
#include <platform/CHIPDeviceConfig.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/DeviceInfoProvider.h>
#include <platform/ESP32/ConfigurationManagerImpl.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>
#include <transport/SecureSession.h>
#include <transport/Session.h>
#include <transport/SessionManager.h>

#include <lib/support/CodeUtils.h>
#include <esp_app_desc.h>
#include <string.h>

using chip::SessionHandle;

using namespace esp_matter;
using namespace MatterIdentityInternal;

bool MatterIdentityInternal::copyBounded(char *dst, size_t dstSize, const char *src) {
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

bool OverrideInstanceInfoProvider::bindBase(chip::DeviceLayer::DeviceInstanceInfoProvider *current) {
  if (current == nullptr) {
    return false;
  }
  if (current == this) {
    return true;
  }
  mBase = current;
  return true;
}

void OverrideInstanceInfoProvider::publish() {
  chip::DeviceLayer::SetDeviceInstanceInfoProvider(this);
}

void OverrideInstanceInfoProvider::setVendorName(const char *name) {
  mVendorName = name;
}

void OverrideInstanceInfoProvider::setProductName(const char *name) {
  mProductName = name;
}

void OverrideInstanceInfoProvider::setHardwareVersion(uint16_t version) {
  mHardwareVersion = version;
  mHasHardwareVersion = true;
}

void OverrideInstanceInfoProvider::setHardwareVersionString(const char *value) {
  mHardwareVersionString = value;
}

void OverrideInstanceInfoProvider::setSerialNumber(const char *value) {
  mSerialNumber = value;
}

CHIP_ERROR OverrideInstanceInfoProvider::copyToBuf(char *buf, size_t bufSize, const char *src) {
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

chip::DeviceLayer::DeviceInstanceInfoProvider *OverrideInstanceInfoProvider::lazyBindBase() {
  if (mBase == nullptr) {
    chip::DeviceLayer::DeviceInstanceInfoProvider *base = chip::DeviceLayer::GetDeviceInstanceInfoProvider();
    if (base != nullptr && base != this) {
      mBase = base;
    }
  }
  return mBase;
}

chip::DeviceLayer::DeviceInstanceInfoProvider *OverrideInstanceInfoProvider::requireBase() {
  return mBase;
}

CHIP_ERROR OverrideInstanceInfoProvider::getStringOrBase(char *buf, size_t bufSize, const char *overrideVal, StringGetter getter) {
  if (overrideVal != nullptr && overrideVal[0] != '\0') {
    return copyToBuf(buf, bufSize, overrideVal);
  }
  if (mBase != nullptr || lazyBindBase() != nullptr) {
    return (requireBase()->*getter)(buf, bufSize);
  }
  if (buf != nullptr && bufSize > 0) {
    buf[0] = '\0';
    return CHIP_NO_ERROR;
  }
  return CHIP_ERROR_INVALID_ARGUMENT;
}

CHIP_ERROR OverrideInstanceInfoProvider::GetVendorName(char *buf, size_t bufSize) {
  return getStringOrBase(buf, bufSize, mVendorName, &chip::DeviceLayer::DeviceInstanceInfoProvider::GetVendorName);
}

CHIP_ERROR OverrideInstanceInfoProvider::GetVendorId(uint16_t &vendorId) {
  vendorId = static_cast<uint16_t>(CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID);
  return CHIP_NO_ERROR;
}

CHIP_ERROR OverrideInstanceInfoProvider::GetProductName(char *buf, size_t bufSize) {
  return getStringOrBase(buf, bufSize, mProductName, &chip::DeviceLayer::DeviceInstanceInfoProvider::GetProductName);
}

CHIP_ERROR OverrideInstanceInfoProvider::GetProductId(uint16_t &productId) {
  productId = static_cast<uint16_t>(CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID);
  return CHIP_NO_ERROR;
}

CHIP_ERROR OverrideInstanceInfoProvider::GetPartNumber(char *buf, size_t bufSize) {
  (void)buf;
  (void)bufSize;
  return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

CHIP_ERROR OverrideInstanceInfoProvider::GetProductURL(char *buf, size_t bufSize) {
  (void)buf;
  (void)bufSize;
  return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

CHIP_ERROR OverrideInstanceInfoProvider::GetProductLabel(char *buf, size_t bufSize) {
  (void)buf;
  (void)bufSize;
  return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

CHIP_ERROR OverrideInstanceInfoProvider::GetSerialNumber(char *buf, size_t bufSize) {
  return getStringOrBase(buf, bufSize, mSerialNumber, &chip::DeviceLayer::DeviceInstanceInfoProvider::GetSerialNumber);
}

CHIP_ERROR OverrideInstanceInfoProvider::GetManufacturingDate(uint16_t &year, uint8_t &month, uint8_t &day) {
  (void)year;
  (void)month;
  (void)day;
  return CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
}

CHIP_ERROR OverrideInstanceInfoProvider::GetHardwareVersion(uint16_t &hardwareVersion) {
  if (mHasHardwareVersion) {
    hardwareVersion = mHardwareVersion;
    return CHIP_NO_ERROR;
  }
  hardwareVersion = static_cast<uint16_t>(CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION);
  return CHIP_NO_ERROR;
}

CHIP_ERROR OverrideInstanceInfoProvider::GetHardwareVersionString(char *buf, size_t bufSize) {
  return getStringOrBase(buf, bufSize, mHardwareVersionString, &chip::DeviceLayer::DeviceInstanceInfoProvider::GetHardwareVersionString);
}

CHIP_ERROR OverrideInstanceInfoProvider::GetRotatingDeviceIdUniqueId(chip::MutableByteSpan &uniqueIdSpan) {
#if CHIP_ENABLE_ROTATING_DEVICE_ID && defined(CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID)
  constexpr uint8_t uniqueId[] = CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID;
  VerifyOrReturnError(sizeof(uniqueId) <= uniqueIdSpan.size(), CHIP_ERROR_BUFFER_TOO_SMALL);
  memcpy(uniqueIdSpan.data(), uniqueId, sizeof(uniqueId));
  uniqueIdSpan.reduce_size(sizeof(uniqueId));
  return CHIP_NO_ERROR;
#else
  (void)uniqueIdSpan;
  return CHIP_ERROR_WRONG_KEY_TYPE;
#endif
}

bool OverrideCommissionableDataProvider::bindBase(chip::DeviceLayer::CommissionableDataProvider *current) {
  if (current == nullptr) {
    return false;
  }
  if (current == this) {
    return true;
  }
  mBase = current;
  return true;
}

void OverrideCommissionableDataProvider::publish() {
  chip::DeviceLayer::SetCommissionableDataProvider(this);
}

void OverrideCommissionableDataProvider::setDiscriminator(uint16_t discriminator) {
  mDiscriminator = discriminator;
  mHasDiscriminator = true;
}

void OverrideCommissionableDataProvider::setPasscode(uint32_t passcode) {
  mPasscode = passcode;
  mHasPasscode = true;
  mHasVerifier = false;
}

chip::DeviceLayer::CommissionableDataProvider *OverrideCommissionableDataProvider::requireBase() {
  VerifyOrDie(mBase != nullptr);
  return mBase;
}

CHIP_ERROR OverrideCommissionableDataProvider::ensureVerifier() {
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

CHIP_ERROR OverrideCommissionableDataProvider::GetSetupDiscriminator(uint16_t &setupDiscriminator) {
  if (mHasDiscriminator) {
    setupDiscriminator = mDiscriminator;
    return CHIP_NO_ERROR;
  }
  return requireBase()->GetSetupDiscriminator(setupDiscriminator);
}

CHIP_ERROR OverrideCommissionableDataProvider::SetSetupDiscriminator(uint16_t setupDiscriminator) {
  if (setupDiscriminator > chip::kMaxDiscriminatorValue) {
    return CHIP_ERROR_INVALID_ARGUMENT;
  }
  setDiscriminator(setupDiscriminator);
  return CHIP_NO_ERROR;
}

CHIP_ERROR OverrideCommissionableDataProvider::GetSpake2pIterationCount(uint32_t &iterationCount) {
  return requireBase()->GetSpake2pIterationCount(iterationCount);
}

CHIP_ERROR OverrideCommissionableDataProvider::GetSpake2pSalt(chip::MutableByteSpan &saltBuf) {
  return requireBase()->GetSpake2pSalt(saltBuf);
}

CHIP_ERROR OverrideCommissionableDataProvider::GetSpake2pVerifier(chip::MutableByteSpan &verifierBuf, size_t &outVerifierLen) {
  if (!mHasPasscode) {
    return requireBase()->GetSpake2pVerifier(verifierBuf, outVerifierLen);
  }
  ReturnErrorOnFailure(ensureVerifier());
  outVerifierLen = sizeof(mVerifier);
  VerifyOrReturnError(verifierBuf.size() >= outVerifierLen, CHIP_ERROR_BUFFER_TOO_SMALL);
  memcpy(verifierBuf.data(), mVerifier, outVerifierLen);
  verifierBuf.reduce_size(outVerifierLen);
  return CHIP_NO_ERROR;
}

CHIP_ERROR OverrideCommissionableDataProvider::GetSetupPasscode(uint32_t &setupPasscode) {
  if (mHasPasscode) {
    setupPasscode = mPasscode;
    return CHIP_NO_ERROR;
  }
  return requireBase()->GetSetupPasscode(setupPasscode);
}

CHIP_ERROR OverrideCommissionableDataProvider::SetSetupPasscode(uint32_t setupPasscode) {
  if (!chip::PayloadContents::IsValidSetupPIN(setupPasscode)) {
    return CHIP_ERROR_INVALID_ARGUMENT;
  }
  setPasscode(setupPasscode);
  return CHIP_NO_ERROR;
}

static constexpr size_t kMaxQrCodeLen = chip::QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1;
static constexpr size_t kMaxQrUrlLen = 768;
static constexpr size_t kMaxManualCodeLen = chip::kManualSetupLongCodeCharLength + 2;

// SoftwareVersion is ConfigurationManager, not DeviceInstanceInfoProvider.
// Inherit the ESP32 impl so Init()/NVS still run; override only version getters.
class OverrideConfigurationManager : public chip::DeviceLayer::ConfigurationManagerImpl {
public:
  void setSoftwareVersion(uint32_t version) {
    mSoftwareVersion = version;
    mHasSoftwareVersion = true;
  }
  void setSoftwareVersionString(const char *value) { mSoftwareVersionString = value; }

  CHIP_ERROR GetSoftwareVersion(uint32_t &softwareVer) override {
    if (mHasSoftwareVersion) {
      softwareVer = mSoftwareVersion;
      return CHIP_NO_ERROR;
    }
    return chip::DeviceLayer::ConfigurationManagerImpl::GetSoftwareVersion(softwareVer);
  }

  CHIP_ERROR GetSoftwareVersionString(char *buf, size_t bufSize) override {
    if (mSoftwareVersionString != nullptr && mSoftwareVersionString[0] != '\0') {
      if (buf == nullptr) {
        return CHIP_ERROR_INVALID_ARGUMENT;
      }
      const size_t n = strlen(mSoftwareVersionString);
      if (n + 1 > bufSize) {
        return CHIP_ERROR_BUFFER_TOO_SMALL;
      }
      memcpy(buf, mSoftwareVersionString, n + 1);
      return CHIP_NO_ERROR;
    }
    return chip::DeviceLayer::ConfigurationManagerImpl::GetSoftwareVersionString(buf, bufSize);
  }

private:
  const char *mSoftwareVersionString = nullptr;
  uint32_t mSoftwareVersion = 0;
  bool mHasSoftwareVersion = false;
};

static OverrideInstanceInfoProvider sInstanceProvider;
static OverrideCommissionableDataProvider sCommissionableProvider;
static OverrideConfigurationManager sConfigMgr;

static char sVendorName[kMaxIdentityLen + 1] = {};
static char sProductName[kMaxIdentityLen + 1] = {};
static char sDeviceName[kMaxIdentityLen + 1] = {};
static char sHardwareVersionString[kMaxHwStringLen + 1] = {};
static char sSoftwareVersionString[kMaxSwStringLen + 1] = {};
static char sSerialNumber[kMaxSerialLen + 1] = {};
static char sManualCode[kMaxManualCodeLen] = {};
static char sQrUrl[kMaxQrUrlLen] = {};
static uint16_t sHardwareVersion = 0;
static uint16_t sDiscriminator = 0;
static uint32_t sPasscode = 0;
static uint32_t sSoftwareVersion = 0;
static bool sHasHardwareVersion = false;
static bool sHasSoftwareVersion = false;
static bool sHasDiscriminator = false;
static bool sHasPasscode = false;
static bool sCodesGenerated = false;

bool ArduinoMatter::ensureSetBeforeBegin(const char *apiName) {
  if (isStackStarted()) {
    log_w("Matter.%s() has no effect after Matter.begin(); call it before Matter.begin().", apiName);
    return false;
  }
  return true;
}

bool ArduinoMatter::storeIdentityString(char *dst, size_t dstSize, const char *src, const char *apiName) {
  if (!ensureSetBeforeBegin(apiName)) {
    return false;
  }
  if (!copyBounded(dst, dstSize, src)) {
    log_e("Matter.%s() value is empty or longer than %u characters.", apiName, static_cast<unsigned>(dstSize - 1));
    return false;
  }
  return true;
}

bool ArduinoMatter::setVendorName(const char *name) {
  return storeIdentityString(sVendorName, sizeof(sVendorName), name, "setVendorName");
}
bool ArduinoMatter::setProductName(const char *name) {
  return storeIdentityString(sProductName, sizeof(sProductName), name, "setProductName");
}
bool ArduinoMatter::setDeviceName(const char *name) {
  return storeIdentityString(sDeviceName, sizeof(sDeviceName), name, "setDeviceName");
}
bool ArduinoMatter::setSerialNumber(const char *value) {
  return storeIdentityString(sSerialNumber, sizeof(sSerialNumber), value, "setSerialNumber");
}
bool ArduinoMatter::setHardwareVersionString(const char *value) {
  return storeIdentityString(sHardwareVersionString, sizeof(sHardwareVersionString), value, "setHardwareVersionString");
}

bool ArduinoMatter::setHardwareVersion(uint16_t version) {
  if (!ensureSetBeforeBegin("setHardwareVersion")) {
    return false;
  }
  sHardwareVersion = version;
  sHasHardwareVersion = true;
  return true;
}

bool ArduinoMatter::setSoftwareVersion(uint32_t version) {
  if (!ensureSetBeforeBegin("setSoftwareVersion")) {
    return false;
  }
  sSoftwareVersion = version;
  sHasSoftwareVersion = true;
  return true;
}

bool ArduinoMatter::setSoftwareVersionString(const char *value) {
  return storeIdentityString(sSoftwareVersionString, sizeof(sSoftwareVersionString), value, "setSoftwareVersionString");
}

uint32_t ArduinoMatter::getSoftwareVersion() {
  if (sHasSoftwareVersion) {
    return sSoftwareVersion;
  }
  uint32_t version = 0;
  if (chip::DeviceLayer::ConfigurationMgr().GetSoftwareVersion(version) != CHIP_NO_ERROR) {
    version = static_cast<uint32_t>(CONFIG_DEVICE_SOFTWARE_VERSION_NUMBER);
  }
  return version;
}

String ArduinoMatter::getSoftwareVersionString() {
  if (sSoftwareVersionString[0] != '\0') {
    return String(sSoftwareVersionString);
  }
  char buf[chip::DeviceLayer::ConfigurationManager::kMaxSoftwareVersionStringLength + 1] = {};
  if (chip::DeviceLayer::ConfigurationMgr().GetSoftwareVersionString(buf, sizeof(buf)) == CHIP_NO_ERROR && buf[0] != '\0') {
    return String(buf);
  }
  const esp_app_desc_t *appDescription = esp_app_get_description();
  if (appDescription != nullptr && appDescription->version[0] != '\0') {
    return String(appDescription->version);
  }
  return String();
}

bool ArduinoMatter::setSetupDiscriminator(uint16_t discriminator) {
  if (!ensureSetBeforeBegin("setSetupDiscriminator")) {
    return false;
  }
  if (discriminator > chip::kMaxDiscriminatorValue) {
    log_e("Matter.setSetupDiscriminator() must be 0..0xFFF.");
    return false;
  }
  sDiscriminator = discriminator;
  sHasDiscriminator = true;
  return true;
}

bool ArduinoMatter::setSetupPasscode(uint32_t passcode) {
  if (!ensureSetBeforeBegin("setSetupPasscode")) {
    return false;
  }
  if (!chip::PayloadContents::IsValidSetupPIN(passcode)) {
    log_e("Matter.setSetupPasscode() is not a valid Matter setup PIN.");
    return false;
  }
  sPasscode = passcode;
  sHasPasscode = true;
  return true;
}

static bool needsOptionalBasicInfoAttrs() {
  return sSerialNumber[0] != '\0';
}

static bool needsInstanceInfoWrap() {
  return sVendorName[0] != '\0' || sProductName[0] != '\0' || sHasHardwareVersion || sHardwareVersionString[0] != '\0' || sSerialNumber[0] != '\0';
}

static bool ensureBasicInfoAttr(cluster_t *cluster, uint32_t attributeId, attribute_t *(*createFn)(cluster_t *, char *, uint16_t), const char *name) {
  if (attribute::get(cluster, attributeId) != nullptr) {
    return true;
  }
  if (createFn(cluster, nullptr, 0) == nullptr) {
    log_e("Failed to create %s", name);
    return false;
  }
  return true;
}

static void fillInstanceInfoOverrides() {
  if (sVendorName[0] != '\0') {
    sInstanceProvider.setVendorName(sVendorName);
  }
  if (sProductName[0] != '\0') {
    sInstanceProvider.setProductName(sProductName);
  }
  if (sHasHardwareVersion) {
    sInstanceProvider.setHardwareVersion(sHardwareVersion);
  }
  if (sHardwareVersionString[0] != '\0') {
    sInstanceProvider.setHardwareVersionString(sHardwareVersionString);
  }
  if (sSerialNumber[0] != '\0') {
    sInstanceProvider.setSerialNumber(sSerialNumber);
  }
}

// Device instance info provider wrapper for Arduino Matter identity customization.
// The provider must be registered with esp_matter::set_custom_device_instance_info_provider()
// BEFORE esp_matter::start() so that it's used during BasicInformationCluster creation.
// Attribute values are then read dynamically at request time from GetVendorName() etc.
static void prepareInstanceInfoOverrides() {
  fillInstanceInfoOverrides();
}

// CHIP's ESP32DeviceInfoProvider.cpp is not compiled unless factory data is
// enabled (CONFIG_ENABLE_ESP32_DEVICE_INFO_PROVIDER depends on it). Arduino
// defaults to CONFIG_NONE_DEVICE_INFO_PROVIDER, so own a RAM provider that
// satisfies FixedLabel / UserLabel / LocalizationConfiguration / TimeFormat.
namespace {

class ArduinoDeviceInfoProvider : public chip::DeviceLayer::DeviceInfoProvider {
public:
  FixedLabelIterator *IterateFixedLabel(chip::EndpointId) override {
    return chip::Platform::New<EmptyIterator<FixedLabelType>>();
  }

  UserLabelIterator *IterateUserLabel(chip::EndpointId endpoint) override {
    return chip::Platform::New<UserLabelIteratorImpl>(*this, endpoint);
  }

  SupportedLocalesIterator *IterateSupportedLocales() override {
    return chip::Platform::New<LocalesIteratorImpl>();
  }

  SupportedCalendarTypesIterator *IterateSupportedCalendarTypes() override {
    return chip::Platform::New<CalendarIteratorImpl>();
  }

protected:
  CHIP_ERROR SetUserLabelLength(chip::EndpointId endpoint, size_t val) override {
    if (val > chip::DeviceLayer::kMaxUserLabelListLength) {
      return CHIP_ERROR_INVALID_ARGUMENT;
    }
    EndpointLabels *slot = findOrAdd(endpoint);
    if (slot == nullptr) {
      return CHIP_ERROR_NO_MEMORY;
    }
    slot->length = val;
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR GetUserLabelLength(chip::EndpointId endpoint, size_t &val) override {
    EndpointLabels *slot = find(endpoint);
    if (slot == nullptr) {
      return CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
    }
    val = slot->length;
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR SetUserLabelAt(chip::EndpointId endpoint, size_t index, const UserLabelType &userLabel) override {
    EndpointLabels *slot = find(endpoint);
    if (slot == nullptr || index >= slot->length) {
      return CHIP_ERROR_INVALID_KEY_ID;
    }
    if (userLabel.label.size() > chip::DeviceLayer::kMaxLabelNameLength || userLabel.value.size() > chip::DeviceLayer::kMaxLabelValueLength) {
      return CHIP_ERROR_INVALID_ARGUMENT;
    }
    memcpy(slot->names[index], userLabel.label.data(), userLabel.label.size());
    slot->names[index][userLabel.label.size()] = '\0';
    memcpy(slot->values[index], userLabel.value.data(), userLabel.value.size());
    slot->values[index][userLabel.value.size()] = '\0';
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR DeleteUserLabelAt(chip::EndpointId endpoint, size_t index) override {
    EndpointLabels *slot = find(endpoint);
    if (slot == nullptr || index >= slot->length) {
      return CHIP_ERROR_INVALID_KEY_ID;
    }
    slot->names[index][0] = '\0';
    slot->values[index][0] = '\0';
    return CHIP_NO_ERROR;
  }

private:
  static constexpr size_t kMaxEndpoints = 8;

  struct EndpointLabels {
    bool used = false;
    chip::EndpointId id = 0;
    size_t length = 0;
    char names[chip::DeviceLayer::kMaxUserLabelListLength][chip::DeviceLayer::kMaxLabelNameLength + 1] = {};
    char values[chip::DeviceLayer::kMaxUserLabelListLength][chip::DeviceLayer::kMaxLabelValueLength + 1] = {};
  };

  template <typename T>
  class EmptyIterator : public Iterator<T> {
  public:
    size_t Count() override {
      return 0;
    }
    bool Next(T &) override {
      return false;
    }
    void Release() override {
      chip::Platform::Delete(this);
    }
  };

  class UserLabelIteratorImpl : public UserLabelIterator {
  public:
    UserLabelIteratorImpl(ArduinoDeviceInfoProvider &provider, chip::EndpointId endpoint) : mProvider(provider), mEndpoint(endpoint) {
      (void)mProvider.GetUserLabelLength(endpoint, mTotal);
    }
    size_t Count() override {
      return mTotal;
    }
    bool Next(UserLabelType &output) override {
      EndpointLabels *slot = mProvider.find(mEndpoint);
      if (slot == nullptr || mIndex >= slot->length) {
        return false;
      }
      output.label = chip::CharSpan::fromCharString(slot->names[mIndex]);
      output.value = chip::CharSpan::fromCharString(slot->values[mIndex]);
      mIndex++;
      return true;
    }
    void Release() override {
      chip::Platform::Delete(this);
    }

  private:
    ArduinoDeviceInfoProvider &mProvider;
    chip::EndpointId mEndpoint = 0;
    size_t mIndex = 0;
    size_t mTotal = 0;
  };

  class LocalesIteratorImpl : public SupportedLocalesIterator {
  public:
    size_t Count() override {
      return 1;
    }
    bool Next(chip::CharSpan &output) override {
      if (mDone) {
        return false;
      }
      output = chip::CharSpan::fromCharString("en-US");
      mDone = true;
      return true;
    }
    void Release() override {
      chip::Platform::Delete(this);
    }

  private:
    bool mDone = false;
  };

  class CalendarIteratorImpl : public SupportedCalendarTypesIterator {
  public:
    size_t Count() override {
      return 1;
    }
    bool Next(CalendarType &output) override {
      if (mDone) {
        return false;
      }
      output = CalendarType::kGregorian;
      mDone = true;
      return true;
    }
    void Release() override {
      chip::Platform::Delete(this);
    }

  private:
    bool mDone = false;
  };

  EndpointLabels *find(chip::EndpointId endpoint) {
    for (auto &slot : mEndpoints) {
      if (slot.used && slot.id == endpoint) {
        return &slot;
      }
    }
    return nullptr;
  }

  EndpointLabels *findOrAdd(chip::EndpointId endpoint) {
    EndpointLabels *existing = find(endpoint);
    if (existing != nullptr) {
      return existing;
    }
    for (auto &slot : mEndpoints) {
      if (!slot.used) {
        slot.used = true;
        slot.id = endpoint;
        slot.length = 0;
        return &slot;
      }
    }
    return nullptr;
  }

  EndpointLabels mEndpoints[kMaxEndpoints];
};

ArduinoDeviceInfoProvider sDeviceInfoProvider;

}  // namespace

void ArduinoMatter::applyIdentityBeforeStart() {
  // FixedLabel / UserLabel (Generic Switch) and LocalizationConfiguration
  // VerifyOrDie / dereference GetDeviceInfoProvider(). ESP-Matter
  // CONFIG_NONE_DEVICE_INFO_PROVIDER leaves it null, and CHIP's
  // ESP32DeviceInfoProvider is not linked without factory data.
  // Factory/custom providers overwrite this in setup_providers().
  if (chip::DeviceLayer::GetDeviceInfoProvider() == nullptr) {
    chip::DeviceLayer::SetDeviceInfoProvider(&sDeviceInfoProvider);
    log_i("Registered Arduino DeviceInfoProvider for FixedLabel/UserLabel.");
  }

  if (needsOptionalBasicInfoAttrs()) {
    endpoint_t *ep = endpoint::get(node::get(), chip::kRootEndpointId);
    cluster_t *cluster = (ep != nullptr) ? cluster::get(ep, chip::app::Clusters::BasicInformation::Id) : nullptr;
    if (cluster == nullptr) {
      log_e("Basic Information cluster missing on the root endpoint; optional identity attributes were not created.");
    } else {
      using namespace chip::app::Clusters::BasicInformation::Attributes;
      if (sSerialNumber[0] != '\0') {
        ensureBasicInfoAttr(cluster, SerialNumber::Id, cluster::basic_information::attribute::create_serial_number, "SerialNumber");
      }
    }
  }

  if (sHasSoftwareVersion || sSoftwareVersionString[0] != '\0') {
    if (sHasSoftwareVersion) {
      sConfigMgr.setSoftwareVersion(sSoftwareVersion);
    }
    if (sSoftwareVersionString[0] != '\0') {
      sConfigMgr.setSoftwareVersionString(sSoftwareVersionString);
    }
    chip::DeviceLayer::SetConfigurationMgr(&sConfigMgr);
    log_i("Custom ConfigurationManager registered (SoftwareVersion=%lu)", static_cast<unsigned long>(sHasSoftwareVersion ? sSoftwareVersion : 0));
  }

  // Register custom device instance info provider BEFORE esp_matter::start()
  // Binding to base provider will happen after stack is initialized in applyIdentityAfterStart()
  if (needsInstanceInfoWrap()) {
    prepareInstanceInfoOverrides();
#if CONFIG_CUSTOM_DEVICE_INSTANCE_INFO_PROVIDER
    esp_matter::set_custom_device_instance_info_provider(&sInstanceProvider);
    log_i("Custom device instance info provider registered (Vendor=%s, Product=%s)", sVendorName, sProductName);
#else
    log_w("CONFIG_CUSTOM_DEVICE_INSTANCE_INFO_PROVIDER not enabled; device identity will use factory defaults");
#endif
  }
#if defined(CONFIG_FACTORY_DEVICE_INSTANCE_INFO_PROVIDER) || defined(CONFIG_SEC_CERT_DEVICE_INSTANCE_INFO_PROVIDER)
  if (needsInstanceInfoWrap()) {
    log_w(
      "Factory or secure-cert instance-info provider takes priority. "
      "Arduino VendorName/ProductName/SerialNumber/HardwareVersion may not be used by Matter stack."
    );
  }
#endif
}

static bool writeNodeLabel(const char *label) {
  // Matter 1.6: WriteAttributeRequest is constructed (path, subject) and
  // BasicInformationCluster is no longer a singleton. Writable attributes go
  // through the data-model provider into the registered cluster.
  char *writableLabel = const_cast<char *>(label);
  const uint16_t len = static_cast<uint16_t>(strlen(label));
  esp_matter_attr_val_t val = esp_matter_char_str(writableLabel, len);
  if (attribute::update(
        chip::kRootEndpointId, chip::app::Clusters::BasicInformation::Id, chip::app::Clusters::BasicInformation::Attributes::NodeLabel::Id, &val
      ) != ESP_OK) {
    log_e("NodeLabel write failed");
    return false;
  }
  log_i("NodeLabel written: %s", label);
  return true;
}

static chip::RendezvousInformationFlags onboardingRendezvousFlags() {
  chip::RendezvousInformationFlags flags;
#if CONFIG_ENABLE_MATTER_OVER_THREAD
  if (ArduinoMatter::getSelectedNetwork() == MATTER_NETWORK_THREAD) {
    flags.Set(chip::RendezvousInformationFlag::kThread);
  } else
#endif
  {
    flags.Set(chip::RendezvousInformationFlag::kOnNetwork);
  }
  if (ArduinoMatter::isBLECommissioningEnabled()) {
    flags.Set(chip::RendezvousInformationFlag::kBLE);
  }
  return flags;
}

static void refreshOnboardingCodes() {
  sManualCode[0] = '\0';
  sQrUrl[0] = '\0';
  sCodesGenerated = false;

  char qrBuf[kMaxQrCodeLen] = {};
  chip::MutableCharSpan qr(qrBuf, sizeof(qrBuf));
  chip::MutableCharSpan manual(sManualCode, sizeof(sManualCode));
  CHIP_ERROR qrErr = CHIP_ERROR_INCORRECT_STATE;
  CHIP_ERROR manualErr = CHIP_ERROR_INCORRECT_STATE;
  {
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    const chip::RendezvousInformationFlags flags = onboardingRendezvousFlags();
    qrErr = GetQRCode(qr, flags);
    manualErr = GetManualPairingCode(manual, flags);
    if ((qrErr != CHIP_NO_ERROR || manualErr != CHIP_NO_ERROR) && ArduinoMatter::isBLECommissioningEnabled()) {
      if (qrErr != CHIP_NO_ERROR) {
        qr = chip::MutableCharSpan(qrBuf, sizeof(qrBuf));
        const CHIP_ERROR retry = GetQRCode(qr, chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
        if (retry == CHIP_NO_ERROR) {
          qrErr = retry;
        }
      }
      if (manualErr != CHIP_NO_ERROR) {
        manual = chip::MutableCharSpan(sManualCode, sizeof(sManualCode));
        const CHIP_ERROR retry = GetManualPairingCode(manual, chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
        if (retry == CHIP_NO_ERROR) {
          manualErr = retry;
        }
      }
    }
  }
  if (qrErr != CHIP_NO_ERROR) {
    log_e("GetQRCode failed: %" CHIP_ERROR_FORMAT, qrErr.Format());
    qr = chip::MutableCharSpan();
  }
  if (manualErr != CHIP_NO_ERROR) {
    log_e("GetManualPairingCode failed: %" CHIP_ERROR_FORMAT, manualErr.Format());
    sManualCode[0] = '\0';
  }
  if (qrErr != CHIP_NO_ERROR && manualErr != CHIP_NO_ERROR) {
    return;
  }
  sCodesGenerated = true;
  if (qrErr == CHIP_NO_ERROR) {
    const CHIP_ERROR urlErr = GetQRCodeUrl(sQrUrl, sizeof(sQrUrl), qr);
    if (urlErr != CHIP_NO_ERROR) {
      log_e("GetQRCodeUrl failed: %" CHIP_ERROR_FORMAT, urlErr.Format());
      sQrUrl[0] = '\0';
    }
  }
}

static bool applyCommissionable() {
  esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);

  if (!sCommissionableProvider.bindBase(chip::DeviceLayer::GetCommissionableDataProvider())) {
    log_e("CommissionableDataProvider is not available.");
    return false;
  }
  if (sHasDiscriminator) {
    const CHIP_ERROR err = sCommissionableProvider.SetSetupDiscriminator(sDiscriminator);
    if (err != CHIP_NO_ERROR) {
      log_e("SetSetupDiscriminator failed: %" CHIP_ERROR_FORMAT, err.Format());
      return false;
    }
  }
  if (sHasPasscode) {
    const CHIP_ERROR err = sCommissionableProvider.SetSetupPasscode(sPasscode);
    if (err != CHIP_NO_ERROR) {
      log_e("SetSetupPasscode failed: %" CHIP_ERROR_FORMAT, err.Format());
      return false;
    }
    const CHIP_ERROR verifierErr = sCommissionableProvider.ensureVerifier();
    if (verifierErr != CHIP_NO_ERROR) {
      log_e("SPAKE2+ verifier generate failed: %" CHIP_ERROR_FORMAT, verifierErr.Format());
      return false;
    }
  }

  sCommissionableProvider.publish();
  return true;
}

static bool reopenCommissioningWindow(chip::CommissioningWindowAdvertisement advertisement) {
  esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
  if (chip::Server::GetInstance().GetFabricTable().FabricCount() != 0) {
    return true;
  }
  if (!chip::Server::GetInstance().GetFailSafeContext().IsFailSafeFullyDisarmed()) {
    log_w("Commissioning already in progress; the next window will use the current commissioning settings.");
    return true;
  }
  chip::CommissioningWindowManager &mgr = chip::Server::GetInstance().GetCommissioningWindowManager();
  if (mgr.IsCommissioningWindowOpen()) {
    mgr.CloseCommissioningWindow();
  }
  const CHIP_ERROR err = mgr.OpenBasicCommissioningWindow(chip::System::Clock::Seconds32(CHIP_DEVICE_CONFIG_DISCOVERY_TIMEOUT_SECS), advertisement);
  if (err != CHIP_NO_ERROR) {
    log_e("Failed to reopen commissioning window: %" CHIP_ERROR_FORMAT, err.Format());
    return false;
  }
  return true;
}

void ArduinoMatter::applyIdentityAfterStart() {
  // Bind custom provider to base provider now that stack is initialized
  if (needsInstanceInfoWrap()) {
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    if (!sInstanceProvider.bindBase(chip::DeviceLayer::GetDeviceInstanceInfoProvider())) {
      log_e("Failed to bind custom provider to base DeviceInstanceInfoProvider.");
    } else {
      log_i("Custom device instance info provider bound to base provider.");
    }
  }

  if (sHasDiscriminator || sHasPasscode) {
    if (!applyCommissionable()) {
      log_e("Custom discriminator/passcode were not applied; pairing codes use the factory values.");
    } else {
      // Server::Init opens kAllSupported (BLE + DNS-SD) and snapshots the PASE verifier.  // codespell:ignore
      // Reopen only when the PIN/discriminator wrap was published.
      // Do not close/reopen just because BLE is off — that tears down _matterc._udp on
      // on-network Wi-Fi before the commissioner can find the node (test: MatterOnNetworkWiFi).
      const chip::CommissioningWindowAdvertisement advertisement =
        isBLECommissioningEnabled() ? chip::CommissioningWindowAdvertisement::kAllSupported : chip::CommissioningWindowAdvertisement::kDnssdOnly;
      if (!reopenCommissioningWindow(advertisement)) {
        log_e("Commissioning window was not refreshed.");
      }
    }
  }

  if (sDeviceName[0] != '\0') {
    writeNodeLabel(sDeviceName);
  }

  refreshOnboardingCodes();
}

String ArduinoMatter::getManualPairingCode() {
  if (!isStackStarted()) {
    log_w("Matter.getManualPairingCode() is not available before Matter.begin(); pairing codes are generated after begin().");
    return String();
  }
  if (sCodesGenerated && sManualCode[0] != '\0') {
    return String(sManualCode);
  }
  log_w("Matter.getManualPairingCode() could not generate a pairing code.");
  return String();
}

String ArduinoMatter::getOnboardingQRCodeUrl() {
  if (!isStackStarted()) {
    log_w("Matter.getOnboardingQRCodeUrl() is not available before Matter.begin(); pairing codes are generated after begin().");
    return String();
  }
  if (sCodesGenerated && sQrUrl[0] != '\0') {
    return String(sQrUrl);
  }
  log_w("Matter.getOnboardingQRCodeUrl() could not generate a pairing code URL.");
  return String();
}

static bool sessionIsActiveCase(void *context, chip::SessionHandle &session) {
  chip::Transport::SecureSession *secure = session->AsSecureSession();
  if (secure != nullptr && secure->IsCASESession() && secure->IsActiveSession()) {
    *static_cast<bool *>(context) = true;
  }
  return true;
}

bool ArduinoMatter::isOnline() {
  if (!isStackStarted() || !isDeviceCommissioned()) {
    return false;
  }
  bool online = false;
  esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
  const CHIP_ERROR err = chip::Server::GetInstance().GetSecureSessionManager().ForEachSessionHandle(&online, sessionIsActiveCase);
  if (err != CHIP_NO_ERROR) {
    log_w("isOnline() session walk failed: %" CHIP_ERROR_FORMAT, err.Format());
    return false;
  }
  return online;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
