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

#include <access/SubjectDescriptor.h>
#include <app/AttributeValueDecoder.h>
#include <app/ConcreteAttributePath.h>
#include <app/clusters/basic-information/BasicInformationCluster.h>
#include <app/data-model-provider/ActionReturnStatus.h>
#include <app/data-model-provider/OperationTypes.h>
#include <app/server/Server.h>
#include <lib/core/TLV.h>
#include <platform/CHIPDeviceLayer.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>
#include <transport/SecureSession.h>
#include <transport/Session.h>
#include <transport/SessionManager.h>

using chip::SessionHandle;

using namespace esp_matter;
using namespace MatterIdentityInternal;

static constexpr size_t kMaxQrCodeLen = chip::QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1;
static constexpr size_t kMaxQrUrlLen = 768;
static constexpr size_t kMaxManualCodeLen = chip::kManualSetupLongCodeCharLength + 2;

static OverrideInstanceInfoProvider sInstanceProvider;
static OverrideCommissionableDataProvider sCommissionableProvider;

static char sVendorName[kMaxIdentityLen + 1] = {};
static char sProductName[kMaxIdentityLen + 1] = {};
static char sDeviceName[kMaxIdentityLen + 1] = {};
static char sHardwareVersionString[kMaxHwStringLen + 1] = {};
static char sSerialNumber[kMaxSerialLen + 1] = {};
static char sManualCode[kMaxManualCodeLen] = {};
static char sQrUrl[kMaxQrUrlLen] = {};
static uint16_t sHardwareVersion = 0;
static uint16_t sDiscriminator = 0;
static uint32_t sPasscode = 0;
static bool sHasHardwareVersion = false;
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

void ArduinoMatter::applyIdentityBeforeStart() {
  if (!needsOptionalBasicInfoAttrs()) {
    return;
  }
  endpoint_t *ep = endpoint::get(node::get(), chip::kRootEndpointId);
  cluster_t *cluster = (ep != nullptr) ? cluster::get(ep, chip::app::Clusters::BasicInformation::Id) : nullptr;
  if (cluster == nullptr) {
    log_e("Basic Information cluster missing on the root endpoint; optional identity attributes were not created.");
    return;
  }
  using namespace chip::app::Clusters::BasicInformation::Attributes;
  if (sSerialNumber[0] != '\0') {
    ensureBasicInfoAttr(cluster, SerialNumber::Id, cluster::basic_information::attribute::create_serial_number, "SerialNumber");
  }
}

static bool writeNodeLabel(const char *label) {
  esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);

  uint8_t tlvBuf[64] = {};
  chip::TLV::TLVWriter writer;
  writer.Init(tlvBuf);
  CHIP_ERROR err = writer.PutString(chip::TLV::AnonymousTag(), label);
  if (err != CHIP_NO_ERROR) {
    log_e("NodeLabel TLV encode failed: %" CHIP_ERROR_FORMAT, err.Format());
    return false;
  }

  chip::TLV::TLVReader reader;
  reader.Init(tlvBuf, writer.GetLengthWritten());
  err = reader.Next();
  if (err != CHIP_NO_ERROR) {
    log_e("NodeLabel TLV read failed: %" CHIP_ERROR_FORMAT, err.Format());
    return false;
  }

  chip::Access::SubjectDescriptor subject{};
  chip::app::AttributeValueDecoder decoder(reader, subject);
  chip::app::DataModel::WriteAttributeRequest request;
  request.path = chip::app::ConcreteDataAttributePath(
    chip::kRootEndpointId, chip::app::Clusters::BasicInformation::Id, chip::app::Clusters::BasicInformation::Attributes::NodeLabel::Id
  );
  request.operationFlags.Set(chip::app::DataModel::OperationFlags::kInternal);

  const chip::app::DataModel::ActionReturnStatus status = chip::app::Clusters::BasicInformationCluster::Instance().WriteAttribute(request, decoder);
  if (!status.IsSuccess()) {
    log_e("NodeLabel write failed");
    return false;
  }
  log_i("NodeLabel written: %s", label);
  return true;
}

static void refreshOnboardingCodes() {
  sManualCode[0] = '\0';
  sQrUrl[0] = '\0';
  sCodesGenerated = false;

  chip::RendezvousInformationFlags flags(chip::RendezvousInformationFlag::kOnNetwork);
#if CONFIG_ENABLE_CHIPOBLE
  flags.Set(chip::RendezvousInformationFlag::kBLE);
#endif

  char qrBuf[kMaxQrCodeLen] = {};
  chip::MutableCharSpan qr(qrBuf, sizeof(qrBuf));
  chip::MutableCharSpan manual(sManualCode, sizeof(sManualCode));
  CHIP_ERROR qrErr = CHIP_NO_ERROR;
  CHIP_ERROR manualErr = CHIP_NO_ERROR;
  {
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    qrErr = GetQRCode(qr, flags);
    if (qrErr == CHIP_NO_ERROR) {
      manualErr = GetManualPairingCode(manual, flags);
    }
  }
  if (qrErr != CHIP_NO_ERROR) {
    log_e("GetQRCode failed: %" CHIP_ERROR_FORMAT, qrErr.Format());
    return;
  }
  if (manualErr != CHIP_NO_ERROR) {
    log_e("GetManualPairingCode failed: %" CHIP_ERROR_FORMAT, manualErr.Format());
    sManualCode[0] = '\0';
    return;
  }
  sCodesGenerated = true;
  const CHIP_ERROR urlErr = GetQRCodeUrl(sQrUrl, sizeof(sQrUrl), qr);
  if (urlErr != CHIP_NO_ERROR) {
    log_e("GetQRCodeUrl failed: %" CHIP_ERROR_FORMAT, urlErr.Format());
    sQrUrl[0] = '\0';
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

  // Server::Init already opened a window and snapshotted the factory SPAKE2+ verifier.
  // Reopen so AdvertiseAndListenForPASE() copies the wrap. Skip if a fabric exists or
  // fail-safe is armed (an in-progress PASE already passed SPAKE). // codespell:ignore
  if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
    if (!chip::Server::GetInstance().GetFailSafeContext().IsFailSafeFullyDisarmed()) {
      log_w("Commissioning already in progress; custom PIN takes effect on the next window.");
    } else {
      chip::CommissioningWindowManager &mgr = chip::Server::GetInstance().GetCommissioningWindowManager();
      if (mgr.IsCommissioningWindowOpen()) {
        mgr.CloseCommissioningWindow();
      }
      const CHIP_ERROR err = mgr.OpenBasicCommissioningWindow();
      if (err != CHIP_NO_ERROR) {
        log_e("Failed to reopen commissioning window after custom PIN: %" CHIP_ERROR_FORMAT, err.Format());
        return false;
      }
    }
  }
  return true;
}

static bool applyInstanceInfoWrap() {
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

  esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
  if (!sInstanceProvider.bindBase(chip::DeviceLayer::GetDeviceInstanceInfoProvider())) {
    log_e("DeviceInstanceInfoProvider is not available.");
    return false;
  }
  sInstanceProvider.publish();
  return true;
}

void ArduinoMatter::applyIdentityAfterStart() {
  if (sHasDiscriminator || sHasPasscode) {
    if (!applyCommissionable()) {
      log_e("Custom discriminator/passcode were not applied; pairing codes use the factory values.");
    }
  }

  if (needsInstanceInfoWrap() && !applyInstanceInfoWrap()) {
    log_e("Instance identity wrap was not installed.");
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
  chip::Server::GetInstance().GetSecureSessionManager().ForEachSessionHandle(&online, sessionIsActiveCase);
  return online;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
