/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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
 * @file NimBLECharacteristic.cpp
 * @brief NimBLE GATT server characteristic access, registration, and notify or indicate (BLE_GATT_SERVER_SUPPORTED).
 *
 * Spec references:
 *  - BT Core Spec v5.x, Vol 3, Part G (GATT) — server-side characteristic procedures.
 *  - §3.3.1.1 — Characteristic Properties bit-field (Read, Write, Notify, Indicate…).
 *  - §3.3.3.3 — Client Characteristic Configuration Descriptor (CCCD, UUID 0x2902):
 *               Bit 0 = Notifications Enabled, Bit 1 = Indications Enabled.
 *  - §4.10    — Notifications: server sends ATT_HANDLE_VALUE_NTF; no client confirmation.
 *  - §4.11    — Indications: server sends ATT_HANDLE_VALUE_IND; client replies with
 *               ATT_HANDLE_VALUE_CONFIRM before the server may send another indication.
 *  - BT Core Spec v5.x, Vol 3, Part F (ATT) — attribute protocol for GATT server:
 *  - §3.4.1.1 — ATT error codes (e.g. BLE_ATT_ERR_INSUFFICIENT_RES = 0x11,
 *               BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN = 0x0D).
 *  - §3.2.9   — ATT_MTU: default 23 bytes; max 517 bytes after MTU exchange.
 */

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "NimBLECharacteristic.h"
#include "NimBLEServer.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLECharacteristicValidation.h"
#include "esp32-hal-log.h"

#if BLE_GATT_SERVER_SUPPORTED

// --------------------------------------------------------------------------
// UUID conversion: BLEUUID (big-endian) <-> NimBLE ble_uuid_any_t (LE for 128)
// --------------------------------------------------------------------------

/**
 * @brief Converts a @c BLEUUID to NimBLE's @c ble_uuid_any_t (reorders 128-bit UUIDs to the stack's byte order).
 * @param uuid Source UUID in Arduino BLE representation.
 * @param out Destination NimBLE union written in place.
 */
void uuidToNimble(const BLEUUID &uuid, ble_uuid_any_t &out) {
  switch (uuid.bitSize()) {
    case 16:
    {
      out.u.type = BLE_UUID_TYPE_16;
      out.u16.value = uuid.toUint16();
      break;
    }
    case 32:
    {
      out.u.type = BLE_UUID_TYPE_32;
      out.u32.value = uuid.toUint32();
      break;
    }
    case 128:
    {
      out.u.type = BLE_UUID_TYPE_128;
      const uint8_t *be = uuid.data();
      for (int i = 0; i < 16; i++) {
        out.u128.value[15 - i] = be[i];
      }
      break;
    }
    default:
    {
      BLEUUID u128 = uuid.to128();
      out.u.type = BLE_UUID_TYPE_128;
      const uint8_t *be = u128.data();
      for (int i = 0; i < 16; i++) {
        out.u128.value[15 - i] = be[i];
      }
      break;
    }
  }
}

// --------------------------------------------------------------------------
// GATT access callback for characteristics
// --------------------------------------------------------------------------

/**
 * @brief NimBLE GATT read/write access handler for a characteristic: invokes user read/write hooks and copies value to or from the mbuf.
 * @param conn_handle Connection handle, or @c BLE_HS_CONN_HANDLE_NONE when not connection-scoped.
 * @param attr_handle GATT attribute handle (unused; reserved for diagnostics).
 * @param ctxt Read/write context with opcode, mbuf, and other stack fields.
 * @param arg Pointer to @c BLECharacteristic::Impl.
 * @return 0 and ATT error codes (e.g. @c BLE_ATT_ERR_INSUFFICIENT_RES) for the stack to send to the peer.
 * @note For write operations, the application onWrite callback runs before this function returns; on NimBLE, that can delay the ATT write response.
 */
int BLECharacteristic::Impl::accessCallback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  auto *impl = static_cast<BLECharacteristic::Impl *>(arg);
  if (!impl) {
    return BLE_ATT_ERR_UNLIKELY;
  }
  (void)attr_handle;

  // Note on authorization: when the characteristic is registered with
  // BLEPermission::ReadAuthorized / WriteAuthorized, NimBLE forwards every
  // ATT read/write through BLE_GAP_EVENT_AUTHORIZE (handled in
  // NimBLEServer::gapEventHandler). The stack only invokes this access
  // callback if the application returned BLE_GAP_AUTHORIZE_ACCEPT, so by
  // the time we land here the request is already authorized.

  switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
    {
      log_d("Characteristic %s: read by conn=%u", impl->uuid.toString().c_str(), conn_handle);

      BLECharacteristic::ReadHandler readCb;
      {
        BLELockGuard lock(impl->valueMtx);
        readCb = impl->onReadCb;
      }
      if (conn_handle != BLE_HS_CONN_HANDLE_NONE && readCb) {
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(conn_handle, &desc) == 0) {
          BLECharacteristic chr{impl->shared_from_this()};
          readCb(chr, BLEConnInfoImpl::fromDesc(desc));
        }
      }

      BLELockGuard lock(impl->valueMtx);
      int rc = os_mbuf_append(ctxt->om, impl->value.data(), impl->value.size());
      return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
    {
      uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
      if (len > BLE_ATT_ATTR_MAX_LEN) {
        log_w("Characteristic %s: write len=%u exceeds max", impl->uuid.toString().c_str(), len);
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      }
      log_d("Characteristic %s: write by conn=%u len=%u", impl->uuid.toString().c_str(), conn_handle, len);
      uint8_t buf[BLE_ATT_ATTR_MAX_LEN];
      os_mbuf_copydata(ctxt->om, 0, len, buf);
      BLECharacteristic::WriteHandler writeCb;
      {
        BLELockGuard lock(impl->valueMtx);
        impl->value.assign(buf, buf + len);
        writeCb = impl->onWriteCb;
      }

      if (writeCb) {
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(conn_handle, &desc) == 0) {
          BLECharacteristic chr{impl->shared_from_this()};
          writeCb(chr, BLEConnInfoImpl::fromDesc(desc));
        }
      }
      return 0;
    }

    default: return BLE_ATT_ERR_UNLIKELY;
  }
}

// --------------------------------------------------------------------------
// GATT access callback for descriptors
// --------------------------------------------------------------------------

/**
 * @brief NimBLE GATT read/write access handler for a descriptor, mirroring the characteristic path for user callbacks and stored value.
 * @param conn_handle Active connection, or the none handle for host-only access.
 * @param attr_handle Descriptor attribute handle.
 * @param ctxt Access context from NimBLE.
 * @param arg Pointer to @c BLEDescriptor::Impl.
 * @return 0 on success, or a NimBLE ATT error code.
 * @note For descriptor writes, onWrite runs before this returns; on NimBLE the ATT response may be delayed until the callback completes.
 */
int BLECharacteristic::Impl::descAccessCallback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  auto *desc = static_cast<BLEDescriptor::Impl *>(arg);
  if (!desc) {
    return BLE_ATT_ERR_UNLIKELY;
  }

  switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_DSC:
    {
      BLEDescriptor::ReadHandler readCb;
      {
        BLELockGuard lock(desc->mtx);
        readCb = desc->onReadCb;
      }
      if (readCb && conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        struct ble_gap_conn_desc conn_desc;
        if (ble_gap_conn_find(conn_handle, &conn_desc) == 0) {
          BLEDescriptor dsc{desc->shared_from_this()};
          readCb(dsc, BLEConnInfoImpl::fromDesc(conn_desc));
        }
      }
      BLELockGuard lock(desc->mtx);
      int rc = os_mbuf_append(ctxt->om, desc->value.data(), desc->value.size());
      return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    case BLE_GATT_ACCESS_OP_WRITE_DSC:
    {
      uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
      uint8_t buf[BLE_ATT_ATTR_MAX_LEN];
      if (len > BLE_ATT_ATTR_MAX_LEN) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      }
      os_mbuf_copydata(ctxt->om, 0, len, buf);
      BLEDescriptor::WriteHandler writeCb;
      {
        BLELockGuard lock(desc->mtx);
        desc->value.assign(buf, buf + len);
        writeCb = desc->onWriteCb;
      }
      if (writeCb && conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        struct ble_gap_conn_desc conn_desc;
        if (ble_gap_conn_find(conn_handle, &conn_desc) == 0) {
          BLEDescriptor dsc{desc->shared_from_this()};
          writeCb(dsc, BLEConnInfoImpl::fromDesc(conn_desc));
        }
      }
      return 0;
    }

    default: return BLE_ATT_ERR_UNLIKELY;
  }
}

// --------------------------------------------------------------------------
// BLECharacteristic public API
// --------------------------------------------------------------------------

/**
 * @brief Sends a notification to every connection that has notifications enabled in the CCCD.
 * @param data Optional payload; if null or empty, the current characteristic value is sent.
 * @param length Number of bytes at @a data, or 0 to use the stored value.
 * @return @c OK when the fan-out completes, or a prior error if the attribute is not registered; individual connections may log failures.
 * @note Notifications are enabled when CCCD bit 0 is set by the client (0x0001).
 *       GATT §4.10: notifications have no ATT-layer acknowledgement from the client.
 *       Delivers through NimBLE @c ble_gatts_notify_custom; stack limits and MTU apply.
 */
BTStatus BLECharacteristic::notify(const uint8_t *data, size_t length) {
  if (!_impl || _impl->handle == 0) {
    log_w("Characteristic: notify called but not registered (handle=0)");
    return BTStatus::InvalidState;
  }

  std::vector<std::pair<uint16_t, uint16_t>> subs;
  {
    SemaphoreHandle_t serverMtx = (_impl->service && _impl->service->server) ? _impl->service->server->mtx : nullptr;
    BLELockGuard lock(serverMtx);
    subs = _impl->subscribers;
  }
  for (auto &[connHandle, subVal] : subs) {
    // CCCD bit 0 = Notifications Enabled (BT Core Spec v5.x, Vol 3, Part G, §3.3.3.3).
    if (subVal & 0x0001) {
      notify(connHandle, data, length);
    }
  }
  return BTStatus::OK;
}

/**
 * @brief Sends a notification on a specific connection if that connection has notification enabled.
 * @param connHandle Target connection handle.
 * @param data Optional payload; if null or length 0, the stored value is used.
 * @param length Byte length of @a data, or 0 to use the stored value.
 * @return @c OK on success, or an error if registration, memory, or the stack call fails.
 * @note Uses NimBLE @c ble_gatts_notify_custom; large payloads are subject to MTU and host buffering.
 */
BTStatus BLECharacteristic::notify(uint16_t connHandle, const uint8_t *data, size_t length) {
  if (!_impl || _impl->handle == 0) {
    log_w("Characteristic: notify(conn) called but not registered (handle=0)");
    return BTStatus::InvalidState;
  }

  log_d("Characteristic %s: notify conn=%u len=%u", _impl->uuid.toString().c_str(), connHandle, length);
  struct os_mbuf *om = nullptr;
  if (data && length > 0) {
    om = ble_hs_mbuf_from_flat(data, length);
  } else {
    BLELockGuard lock(_impl->valueMtx);
    om = ble_hs_mbuf_from_flat(_impl->value.data(), _impl->value.size());
  }
  if (!om) {
    log_e("Characteristic %s: notify - no memory for mbuf", _impl->uuid.toString().c_str());
    return BTStatus::NoMemory;
  }

  int rc = ble_gatts_notify_custom(connHandle, _impl->handle, om);
  if (rc != 0) {
    log_w("Characteristic %s: notify failed for conn=%u rc=%d", _impl->uuid.toString().c_str(), connHandle, rc);
  }
  return (rc == 0) ? BTStatus::OK : BTStatus::Fail;
}

/**
 * @brief Sends an indication to every connection that has indications enabled in the CCCD.
 * @param data Optional payload; if null or empty, the current characteristic value is used.
 * @param length Number of bytes at @a data, or 0 to use the stored value.
 * @return @c OK when the fan-out completes, or a prior error state if the characteristic is not registered.
 * @note Indications use NimBLE @c ble_gatts_indicate_custom and require client confirmation at the ATT level.
 */
BTStatus BLECharacteristic::indicate(const uint8_t *data, size_t length) {
  if (!_impl || _impl->handle == 0) {
    log_w("Characteristic: indicate called but not registered (handle=0)");
    return BTStatus::InvalidState;
  }

  std::vector<std::pair<uint16_t, uint16_t>> subs;
  {
    SemaphoreHandle_t serverMtx = (_impl->service && _impl->service->server) ? _impl->service->server->mtx : nullptr;
    BLELockGuard lock(serverMtx);
    subs = _impl->subscribers;
  }
  for (auto &[connHandle, subVal] : subs) {
    // CCCD bit 1 = Indications Enabled (BT Core Spec v5.x, Vol 3, Part G, §3.3.3.3).
    if (subVal & 0x0002) {
      indicate(connHandle, data, length);
    }
  }
  return BTStatus::OK;
}

/**
 * @brief Sends an indication on one connection if indications are enabled for that connection.
 * @param connHandle Target connection.
 * @param data Optional payload; if null or length 0, the stored value is used.
 * @param length Byte length of @a data, or 0 to use the stored value.
 * @return @c OK on success, or a failure if the handle is invalid, allocation fails, or the stack returns an error.
 * @note Indications require an explicit ATT_HANDLE_VALUE_CONFIRM from the client before
 *       the server may send the next indication on the same connection.
 *       GATT §4.11; ATT_HANDLE_VALUE_IND/CONFIRM exchange.
 *       This path uses @c ble_gatts_indicate_custom.
 */
BTStatus BLECharacteristic::indicate(uint16_t connHandle, const uint8_t *data, size_t length) {
  if (!_impl || _impl->handle == 0) {
    log_w("Characteristic: indicate(conn) called but not registered (handle=0)");
    return BTStatus::InvalidState;
  }

  log_d("Characteristic %s: indicate conn=%u len=%u", _impl->uuid.toString().c_str(), connHandle, length);
  struct os_mbuf *om = nullptr;
  if (data && length > 0) {
    om = ble_hs_mbuf_from_flat(data, length);
  } else {
    BLELockGuard lock(_impl->valueMtx);
    om = ble_hs_mbuf_from_flat(_impl->value.data(), _impl->value.size());
  }
  if (!om) {
    log_e("Characteristic %s: indicate - no memory for mbuf", _impl->uuid.toString().c_str());
    return BTStatus::NoMemory;
  }

  int rc = ble_gatts_indicate_custom(connHandle, _impl->handle, om);
  if (rc != 0) {
    log_w("Characteristic %s: indicate failed for conn=%u rc=%d", _impl->uuid.toString().c_str(), connHandle, rc);
  }
  return (rc == 0) ? BTStatus::OK : BTStatus::Fail;
}

/**
 * @brief Allocates a new descriptor, maps permissions to ATT flags, and attaches it to this characteristic for the next GATT build.
 * @param uuid Descriptor UUID.
 * @param perms Access permissions and security modifiers.
 * @param maxLen Initial reserved length for the descriptor value buffer.
 * @return A @c BLEDescriptor handle, or an empty object if validation fails.
 */
BLEDescriptor BLECharacteristic::createDescriptor(const BLEUUID &uuid, BLEPermission perms, size_t maxLen) {
  BLE_CHECK_IMPL(BLEDescriptor());

  // Stage-1 validation before we spend memory allocating the descriptor;
  // a rejected spec gets logged and the caller receives an empty handle.
  if (!bleValidateDescProps(uuid, _impl->uuid, perms)) {
    return BLEDescriptor();
  }

  auto desc = std::make_shared<BLEDescriptor::Impl>();
  desc->uuid = uuid;
  desc->chr = _impl.get();
  desc->permissions = perms;
  uuidToNimble(uuid, desc->nimbleUUID);

  // Fail-closed mapping from BLEPermission to NimBLE att flags. Unlike the
  // previous revision this does NOT fall back to READ when no permission
  // bits are set — that path is now rejected by the validator above, so the
  // only way to reach here with `perms == None` is via direct Impl
  // construction, in which case leaving the descriptor inaccessible (and
  // logged as an error) is the safer failure mode.
  uint8_t flags = 0;
  uint16_t p = static_cast<uint16_t>(perms);
  if (p & static_cast<uint16_t>(BLEPermission::Read)) {
    flags |= BLE_ATT_F_READ;
  }
  if (p & static_cast<uint16_t>(BLEPermission::Write)) {
    flags |= BLE_ATT_F_WRITE;
  }
  if (p & static_cast<uint16_t>(BLEPermission::ReadEncrypted)) {
    flags |= BLE_ATT_F_READ | BLE_ATT_F_READ_ENC;
  }
  if (p & static_cast<uint16_t>(BLEPermission::ReadAuthenticated)) {
    flags |= BLE_ATT_F_READ | BLE_ATT_F_READ_AUTHEN;
  }
  if (p & static_cast<uint16_t>(BLEPermission::ReadAuthorized)) {
    flags |= BLE_ATT_F_READ | BLE_ATT_F_READ_AUTHOR;
  }
  if (p & static_cast<uint16_t>(BLEPermission::WriteEncrypted)) {
    flags |= BLE_ATT_F_WRITE | BLE_ATT_F_WRITE_ENC;
  }
  if (p & static_cast<uint16_t>(BLEPermission::WriteAuthenticated)) {
    flags |= BLE_ATT_F_WRITE | BLE_ATT_F_WRITE_AUTHEN;
  }
  if (p & static_cast<uint16_t>(BLEPermission::WriteAuthorized)) {
    flags |= BLE_ATT_F_WRITE | BLE_ATT_F_WRITE_AUTHOR;
  }
  desc->attFlags = flags;
  desc->value.reserve(maxLen);

  impl.descriptors.push_back(desc);

  return BLEDescriptor(desc);
}

/**
 * @brief Sets the user description by writing UUID 0x2901, creating the descriptor on demand if needed.
 * @param desc Human-readable description string.
 */
void BLECharacteristic::setDescription(const String &desc) {
  if (!_impl) {
    return;
  }
  auto existing = getDescriptor(BLEUUID(static_cast<uint16_t>(0x2901)));
  if (existing) {
    existing.setValue(desc);
  } else {
    auto d = createDescriptor(BLEUUID(static_cast<uint16_t>(0x2901)), BLEPermission::Read, desc.length() + 1);
    d.setValue(desc);
  }
}

// --------------------------------------------------------------------------
// NimBLE GATT table builder
// --------------------------------------------------------------------------

/**
 * @brief Maps @c BLEProperty and @c BLEPermission to NimBLE @c ble_gatt_chr_flags (fail-closed: read/write bits require matching permissions).
 * @param props Declared GATT property bits (read, write, notify, etc.).
 * @param perms Permission and security requirements for the characteristic.
 * @return Fused characteristic flags for @c ble_gatt_chr_def.
 * @note Notify, indicate, and broadcast are not gated on the read/write permission mask the same way as basic read and write.
 */
static ble_gatt_chr_flags mapPropertyFlags(BLEProperty props, BLEPermission perms) {
  ble_gatt_chr_flags f = 0;

  const bool anyReadPerm = (perms & BLEPermission::Read) || (perms & BLEPermission::ReadEncrypted) || (perms & BLEPermission::ReadAuthenticated)
                           || (perms & BLEPermission::ReadAuthorized);
  const bool anyWritePerm = (perms & BLEPermission::Write) || (perms & BLEPermission::WriteEncrypted) || (perms & BLEPermission::WriteAuthenticated)
                            || (perms & BLEPermission::WriteAuthorized);

  // Operation axis — gated on matching permission direction.
  if (props & BLEProperty::Broadcast) {
    f |= BLE_GATT_CHR_F_BROADCAST;
  }
  if ((props & BLEProperty::Read) && anyReadPerm) {
    f |= BLE_GATT_CHR_F_READ;
  }
  if ((props & BLEProperty::WriteNR) && anyWritePerm) {
    f |= BLE_GATT_CHR_F_WRITE_NO_RSP;
  }
  if ((props & BLEProperty::Write) && anyWritePerm) {
    f |= BLE_GATT_CHR_F_WRITE;
  }
  if (props & BLEProperty::Notify) {
    f |= BLE_GATT_CHR_F_NOTIFY;
  }
  if (props & BLEProperty::Indicate) {
    f |= BLE_GATT_CHR_F_INDICATE;
  }
  if ((props & BLEProperty::SignedWrite) && anyWritePerm) {
    f |= BLE_GATT_CHR_F_AUTH_SIGN_WRITE;
  }

  // Security axis — additive modifiers on the operation bits above.
  // The *_AUTHOR flags are forwarded so NimBLE's ATT layer dispatches a
  // BLE_GAP_EVENT_AUTHORIZE to the server's GAP handler on every ATT
  // read/write, which we in turn route into BLESecurity::onAuthorization.
  // This keeps authorization inside the standard NimBLE security pipeline
  // (same site that checks enc/authen), reports a semantically-correct
  // BLE_ATT_ERR_INSUFFICIENT_AUTHOR on deny, and avoids duplicating the
  // check inside the characteristic access callback.
  if (perms & BLEPermission::ReadEncrypted) {
    f |= BLE_GATT_CHR_F_READ_ENC;
  }
  if (perms & BLEPermission::ReadAuthenticated) {
    f |= BLE_GATT_CHR_F_READ_AUTHEN;
  }
  if (perms & BLEPermission::ReadAuthorized) {
    f |= BLE_GATT_CHR_F_READ_AUTHOR;
  }
  if (perms & BLEPermission::WriteEncrypted) {
    f |= BLE_GATT_CHR_F_WRITE_ENC;
  }
  if (perms & BLEPermission::WriteAuthenticated) {
    f |= BLE_GATT_CHR_F_WRITE_AUTHEN;
  }
  if (perms & BLEPermission::WriteAuthorized) {
    f |= BLE_GATT_CHR_F_WRITE_AUTHOR;
  }
  return f;
}

// Persistent storage for GATT table arrays (must outlive ble_gatts_start)
static std::vector<ble_gatt_svc_def> s_gattSvcs;
static std::vector<std::vector<ble_gatt_chr_def>> s_gattChrs;
static std::vector<std::vector<ble_gatt_dsc_def>> s_gattDscs;
static std::vector<std::vector<const ble_gatt_svc_def *>> s_gattIncludes;

/**
 * @brief Builds the NimBLE @c ble_gatt_svc_def table from the in-memory service tree, then calls @c ble_gatts_count_cfg and @c ble_gatts_add_svcs.
 * @param services All primary services to register, each with characteristics and descriptors.
 * @return 0 on success, or a negative NimBLE error code (e.g. @c BLE_HS_EINVAL) if validation or registration fails.
 */
int nimbleRegisterGattServices(const std::vector<std::shared_ptr<BLEService::Impl>> &services) {
  log_d("GATT: registering %u service(s)", (unsigned)services.size());
  s_gattSvcs.clear();
  s_gattChrs.clear();
  s_gattDscs.clear();
  s_gattIncludes.clear();

  for (auto &svc : services) {
    log_d("GATT: service %s with %u characteristic(s)", svc->uuid.toString().c_str(), (unsigned)svc->characteristics.size());
    uuidToNimble(svc->uuid, svc->nimbleUUID);
    std::vector<ble_gatt_chr_def> chrs;

    for (auto &chr : svc->characteristics) {
      if (!bleValidateCharFinal(*chr, /*stackIsNimble=*/true)) {
        log_e("GATT: characteristic %s rejected by validation", chr->uuid.toString().c_str());
        return BLE_HS_EINVAL;
      }

      uuidToNimble(chr->uuid, chr->nimbleUUID);
      std::vector<ble_gatt_dsc_def> dscs;
      for (auto &dscImpl : chr->descriptors) {
        uuidToNimble(dscImpl->uuid, dscImpl->nimbleUUID);
        ble_gatt_dsc_def d = {};
        d.uuid = &dscImpl->nimbleUUID.u;
        d.att_flags = dscImpl->attFlags;
        d.access_cb = BLECharacteristic::Impl::descAccessCallback;
        d.arg = dscImpl.get();
        dscs.push_back(d);
      }
      dscs.push_back({});
      s_gattDscs.push_back(std::move(dscs));

      ble_gatt_chr_def c = {};
      c.uuid = &chr->nimbleUUID.u;
      c.access_cb = BLECharacteristic::Impl::accessCallback;
      c.arg = chr.get();
      c.flags = mapPropertyFlags(chr->properties, chr->permissions);
      c.val_handle = &chr->handle;
      c.descriptors = s_gattDscs.back().data();
      chrs.push_back(c);
    }
    chrs.push_back({});
    s_gattChrs.push_back(std::move(chrs));

    ble_gatt_svc_def s = {};
    s.type = BLE_GATT_SVC_TYPE_PRIMARY;
    s.uuid = &svc->nimbleUUID.u;
    s.characteristics = s_gattChrs.back().data();
    s_gattSvcs.push_back(s);
  }
  s_gattSvcs.push_back({});  // sentinel

  // Fixup Included Service pointers. Done after the vector is finalized so
  // element addresses are stable. NimBLE's includes field is a
  // null-terminated array of const ble_gatt_svc_def* pointing into
  // s_gattSvcs.
  for (size_t i = 0; i < services.size(); ++i) {
    auto &svc = services[i];
    if (svc->includedServices.empty()) {
      continue;
    }
    std::vector<const ble_gatt_svc_def *> incPtrs;
    for (auto &inc : svc->includedServices) {
      for (size_t j = 0; j < services.size(); ++j) {
        if (services[j] == inc) {
          incPtrs.push_back(&s_gattSvcs[j]);
          break;
        }
      }
    }
    incPtrs.push_back(nullptr);  // sentinel
    s_gattIncludes.push_back(std::move(incPtrs));
    s_gattSvcs[i].includes = s_gattIncludes.back().data();
  }

  int rc = ble_gatts_count_cfg(s_gattSvcs.data());
  if (rc != 0) {
    log_e("ble_gatts_count_cfg: rc=%d", rc);
    return rc;
  }

  rc = ble_gatts_add_svcs(s_gattSvcs.data());
  if (rc != 0) {
    log_e("ble_gatts_add_svcs: rc=%d", rc);
  }
  return rc;
}

#else /* !BLE_GATT_SERVER_SUPPORTED -- stubs */

// Stubs for BLE_GATT_SERVER_SUPPORTED == 0: notify/indicate return NotSupported; no descriptors.

BTStatus BLECharacteristic::notify(const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLECharacteristic::notify(uint16_t, const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLECharacteristic::indicate(const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLECharacteristic::indicate(uint16_t, const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BLEDescriptor BLECharacteristic::createDescriptor(const BLEUUID &, BLEPermission, size_t) {
  log_w("GATT server not supported");
  return BLEDescriptor();
}

void BLECharacteristic::setDescription(const String &) {
  log_w("GATT server not supported");
}

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_NIMBLE */
