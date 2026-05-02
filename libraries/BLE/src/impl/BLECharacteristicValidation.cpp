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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "impl/BLECharacteristicValidation.h"
#include "impl/BLECharacteristicBackend.h"  // full BLECharacteristic::Impl / BLEDescriptor::Impl
#include "esp32-hal-log.h"

/**
 * @file BLECharacteristicValidation.cpp
 * @brief GATT property and permission validation for characteristics and
 *        registered descriptors (construction-time and pre-registration
 *        checks, including SIG-reserved descriptor rules).
 *
 * Spec references:
 *  - BT Core Spec v5.x, Vol 3, Part G (GATT) — the primary reference for all
 *    characteristic and descriptor semantics validated here.
 *  - §3.3.1.1 — Characteristic Properties bit-field definitions (Read, Write, Notify…).
 *  - §3.3.3   — Characteristic Descriptor definitions (CCCD, SCCD, Presentation Format…).
 *  - §3.3.3.1 — Characteristic Extended Properties descriptor (0x2900).
 *  - §3.3.3.2 — Characteristic User Description descriptor (0x2901).
 *  - §3.3.3.3 — Client Characteristic Configuration Descriptor / CCCD (0x2902).
 *  - §3.3.3.4 — Server Characteristic Configuration Descriptor / SCCD (0x2903).
 *  - §3.3.3.5 — Characteristic Presentation Format descriptor (0x2904); 7-byte fixed size.
 *  - BT Core Spec v5.x, Vol 3, Part F (ATT), §3.2.5 — Attribute permission hierarchy
 *    (None < Open < Encrypted < Authenticated < Authorized).
 */

// --------------------------------------------------------------------------
// Predicates over BLEProperty / BLEPermission
// --------------------------------------------------------------------------

namespace {

// SIG-assigned 16-bit UUIDs for GATT-defined descriptors.
// BT Core Spec v5.x, Vol 3, Part G, §3.3.3.
constexpr uint16_t DSC_UUID_EXT_PROPS = 0x2900;  // Characteristic Extended Properties (§3.3.3.1)
constexpr uint16_t DSC_UUID_USER_DESC = 0x2901;  // Characteristic User Description (§3.3.3.2)
constexpr uint16_t DSC_UUID_CCCD = 0x2902;       // Client Characteristic Configuration (§3.3.3.3)
constexpr uint16_t DSC_UUID_SCCD = 0x2903;       // Server Characteristic Configuration (§3.3.3.4)
constexpr uint16_t DSC_UUID_PRES_FMT = 0x2904;   // Characteristic Presentation Format (§3.3.3.5)

/**
 * @brief Check whether any write-class property is set.
 * @param p Property bitmask to test.
 * @return true if Write, WriteNR, or SignedWrite is present.
 */
bool hasWriteProp(BLEProperty p) {
  return (p & BLEProperty::Write) || (p & BLEProperty::WriteNR) || (p & BLEProperty::SignedWrite);
}

/**
 * @brief Check whether any read-direction permission is set.
 * @param p Permission bitmask to test.
 * @return true if any of Read, ReadEncrypted, ReadAuthenticated, or ReadAuthorized is present.
 */
bool hasAnyReadPerm(BLEPermission p) {
  return (p & BLEPermission::Read) || (p & BLEPermission::ReadEncrypted) || (p & BLEPermission::ReadAuthenticated) || (p & BLEPermission::ReadAuthorized);
}

/**
 * @brief Check whether any write-direction permission is set.
 * @param p Permission bitmask to test.
 * @return true if any of Write, WriteEncrypted, WriteAuthenticated, or WriteAuthorized is present.
 */
bool hasAnyWritePerm(BLEPermission p) {
  return (p & BLEPermission::Write) || (p & BLEPermission::WriteEncrypted) || (p & BLEPermission::WriteAuthenticated) || (p & BLEPermission::WriteAuthorized);
}

/**
 * @brief Look up a descriptor Impl by 16-bit UUID on the characteristic.
 * @param chr The characteristic Impl whose descriptor list is searched.
 * @param uuid16 16-bit Bluetooth SIG UUID to match.
 * @return Pointer to the matching descriptor Impl, or nullptr when absent.
 */
const BLEDescriptor::Impl *findDsc(const BLECharacteristic::Impl &chr, uint16_t uuid16) {
  BLEUUID want(uuid16);
  for (const auto &d : chr.descriptors) {
    if (d && d->uuid == want) {
      return d.get();
    }
  }
  return nullptr;
}

/**
 * @brief Extract a 16-bit Bluetooth-assigned UUID for reserved-descriptor lookup.
 * @param u The UUID to inspect.
 * @return The 16-bit value if the UUID is a 16-bit SIG UUID, or 0 for
 *         non-16-bit UUIDs so reserved-UUID rules only fire unambiguously.
 */
uint16_t uuid16Of(const BLEUUID &u) {
  return (u.bitSize() == 16) ? u.toUint16() : 0;
}

}  // namespace

// --------------------------------------------------------------------------
// Stage 1: property / permission validation (no descriptors required)
// --------------------------------------------------------------------------

/**
 * @brief Validate a characteristic's properties/permissions combination.
 * @param uuid  Characteristic UUID (used only for log messages).
 * @param props Characteristic properties bitmask.
 * @param perms Characteristic permissions bitmask.
 * @return true if no hard errors were found, false otherwise.
 * @note Hard errors (log_e) return false and should prevent registration.
 *       Warnings (log_w) are emitted for likely-unintentional configurations
 *       (e.g. property declared without backing permission, security-level
 *       mixing) but do not cause failure.
 *
 * Spec reference: BT Core Spec v5.x, Vol 3, Part G, §3.3.1.1 (Characteristic
 * Properties), Vol 3, Part F, §3.2.5 (Attribute Permissions).
 */
bool bleValidateCharProps(const BLEUUID &uuid, BLEProperty props, BLEPermission perms) {
  String ustr = uuid.toString();
  [[maybe_unused]]
  const char *u = ustr.c_str();
  bool ok = true;

  // 2.1 Empty properties
  if (static_cast<uint8_t>(props) == 0) {
    log_e("Characteristic %s: no properties set (must have at least one)", u);
    ok = false;
  }

  // 2.2 Read permission requires READ property
  if (hasAnyReadPerm(perms) && !(props & BLEProperty::Read)) {
    log_e("Characteristic %s: read permission set but READ property is not", u);
    ok = false;
  }

  // 2.2 Write permission requires a write property
  if (hasAnyWritePerm(perms) && !hasWriteProp(props)) {
    log_e("Characteristic %s: write permission set but no write property (WRITE / WRITE_NR / SIGNED_WRITE)", u);
    ok = false;
  }

  // 2.4 Authorization requires base direction
  if ((perms & BLEPermission::ReadAuthorized) && !(props & BLEProperty::Read)) {
    log_e("Characteristic %s: ReadAuthorized permission requires READ property", u);
    ok = false;
  }
  if ((perms & BLEPermission::WriteAuthorized) && !hasWriteProp(props)) {
    log_e("Characteristic %s: WriteAuthorized permission requires a write property", u);
    ok = false;
  }

  // Fail-closed notice: property advertised but no permission to back it
  // means the backend suppresses the property (NimBLE) or rejects reads
  // (Bluedroid). Likely a mistake, hence a warning.
  if ((props & BLEProperty::Read) && !hasAnyReadPerm(perms)) {
    log_w(
      "Characteristic %s: READ property declared but no read permission — "
      "reads will be rejected; use BLEPermissions::OpenRead or EncryptedRead",
      u
    );
  }
  if (hasWriteProp(props) && !hasAnyWritePerm(perms)) {
    log_w(
      "Characteristic %s: write property declared but no write permission — "
      "writes will be rejected; use BLEPermissions::OpenWrite or EncryptedWrite",
      u
    );
  }

  // 3.3 Security level mixing
  if ((perms & BLEPermission::Read) && ((perms & BLEPermission::ReadEncrypted) || (perms & BLEPermission::ReadAuthenticated))) {
    log_w("Characteristic %s: plain READ mixed with encrypted READ (encryption made redundant)", u);
  }
  if ((perms & BLEPermission::Write) && ((perms & BLEPermission::WriteEncrypted) || (perms & BLEPermission::WriteAuthenticated))) {
    log_w("Characteristic %s: plain WRITE mixed with encrypted WRITE (encryption made redundant)", u);
  }

  // 3.4 AUTHEN without ENC — per ATT security hierarchy, authenticated access
  // implies the link is also encrypted (BT Core Spec v5.x, Vol 3, Part F,
  // §3.2.5 and Vol 3, Part H, §3.5.1 — authentication requires an encrypted
  // link as a prerequisite).
  if ((perms & BLEPermission::ReadAuthenticated) && !(perms & BLEPermission::ReadEncrypted)) {
    log_w("Characteristic %s: ReadAuthenticated implies ReadEncrypted — consider adding it", u);
  }
  if ((perms & BLEPermission::WriteAuthenticated) && !(perms & BLEPermission::WriteEncrypted)) {
    log_w("Characteristic %s: WriteAuthenticated implies WriteEncrypted — consider adding it", u);
  }

  // 3.5 WRITE and WRITE_NR combined
  if ((props & BLEProperty::Write) && (props & BLEProperty::WriteNR)) {
    log_w("Characteristic %s: WRITE and WRITE_NR both set — valid but uncommon, verify intent", u);
  }

  return ok;
}

// --------------------------------------------------------------------------
// Stage 2: full validation (run after any backend auto-creation)
// --------------------------------------------------------------------------

/**
 * @brief Validate the full characteristic state before native stack registration.
 * @param chr Characteristic Impl containing properties, permissions, and descriptors.
 * @param stackIsNimble true when using the NimBLE backend. NimBLE auto-creates the
 *                      CCCD internally, so a user-added CCCD is a hard error; on
 *                      Bluedroid the CCCD must be present in the descriptor list.
 * @return true if no hard errors were found, false otherwise.
 * @note Internally calls bleValidateCharProps() first, then performs descriptor-level
 *       checks: CCCD/SCCD presence vs. NOTIFY/INDICATE/BROADCAST properties,
 *       Extended Properties descriptor requirement, Presentation Format length,
 *       and per-descriptor permission re-validation via bleValidateDescProps().
 *       Also validates SIG-reserved descriptor value lengths (0x2900 must be 2 bytes,
 *       0x2902/0x2903 must be 0 or 2 bytes when pre-populated).
 */
bool bleValidateCharFinal(const BLECharacteristic::Impl &chr, bool stackIsNimble) {
  BLEProperty props = chr.properties;
  BLEPermission perms = chr.permissions;
  String ustr = chr.uuid.toString();
  [[maybe_unused]]
  const char *u = ustr.c_str();

  bool ok = bleValidateCharProps(chr.uuid, props, perms);

  const BLEDescriptor::Impl *cccd = findDsc(chr, DSC_UUID_CCCD);
  const BLEDescriptor::Impl *sccd = findDsc(chr, DSC_UUID_SCCD);
  const BLEDescriptor::Impl *extProp = findDsc(chr, DSC_UUID_EXT_PROPS);
  const BLEDescriptor::Impl *presFmt = findDsc(chr, DSC_UUID_PRES_FMT);

  const bool needsCccd = (props & BLEProperty::Notify) || (props & BLEProperty::Indicate);

  // 2.5 CCCD — must exist / must not exist
  // The CCCD (0x2902) is mandatory for NOTIFY and INDICATE properties.
  // Clients write to the CCCD to enable/disable notifications and indications.
  // BT Core Spec v5.x, Vol 3, Part G, §3.3.3.3 (CCCD).
  // Bit 0 = Notification Enable, Bit 1 = Indication Enable.
  // NimBLE's host stack creates the CCCD for NOTIFY/INDICATE internally, so
  // the descriptor is not present in our `descriptors` vector — skip the
  // "must exist" check for NimBLE. A user-added CCCD on NimBLE would be a
  // duplicate and is always a hard error.
  if (!stackIsNimble) {
    if (needsCccd && !cccd) {
      log_e("Characteristic %s: NOTIFY/INDICATE requires a CCCD (0x2902) descriptor", u);
      ok = false;
    }
  } else {
    if (cccd) {
      log_e(
        "Characteristic %s: CCCD (0x2902) must not be added manually on NimBLE "
        "— the stack creates it automatically for NOTIFY/INDICATE",
        u
      );
      ok = false;
    }
  }
  if (cccd && !needsCccd) {
    log_e("Characteristic %s: CCCD (0x2902) present but characteristic has no NOTIFY/INDICATE", u);
    ok = false;
  }

  // 2.7 SCCD — must exist / must not exist
  // The SCCD (0x2903) is required when the BROADCAST property is set.
  // Server writes to the SCCD to enable/disable broadcasts via advertising.
  // BT Core Spec v5.x, Vol 3, Part G, §3.3.3.4 (SCCD).
  if ((props & BLEProperty::Broadcast) && !sccd) {
    log_e("Characteristic %s: BROADCAST property requires a SCCD (0x2903) descriptor", u);
    ok = false;
  }
  if (sccd && !(props & BLEProperty::Broadcast)) {
    log_e("Characteristic %s: SCCD (0x2903) present but BROADCAST property not set", u);
    ok = false;
  }

  // 2.8 ExtendedProperties — requires descriptor 0x2900
  // When the ExtendedProps property bit is set, the characteristic MUST
  // contain an Extended Properties descriptor (0x2900) whose 2-byte value
  // encodes the Reliable Write (bit 0) and Writable Auxiliaries (bit 1) flags.
  // BT Core Spec v5.x, Vol 3, Part G, §3.3.3.1.
  if ((props & BLEProperty::ExtendedProps) && !extProp) {
    log_e("Characteristic %s: EXTENDED_PROPERTIES property requires descriptor 0x2900", u);
    ok = false;
  }

  // 2.9 Presentation format length
  // The Characteristic Presentation Format descriptor (0x2904) has a fixed
  // 7-byte structure: [Format(1)] [Exponent(1)] [Unit(2)] [Namespace(1)] [Description(2)].
  // BT Core Spec v5.x, Vol 3, Part G, §3.3.3.5.
  if (presFmt) {
    const size_t len = presFmt->value.size();
    if (len != 7) {
      log_e("Characteristic %s: Presentation Format (0x2904) must be exactly 7 bytes (got %u)", u, static_cast<unsigned>(len));
      ok = false;
    }
  }

  // 3.2 NOTIFY/INDICATE without READ
  if (needsCccd && !(props & BLEProperty::Read)) {
    log_w("Characteristic %s: NOTIFY/INDICATE without READ — some clients may not function correctly", u);
  }

  // 2.x Per-descriptor access profile recheck. `bleValidateDescProps` already
  // runs at construction time, but re-running here catches post-creation
  // `setPermissions()` mutations and keeps the pre-registration gate
  // authoritative for the final descriptor state.
  for (const auto &d : chr.descriptors) {
    if (!d) {
      continue;
    }
    if (!bleValidateDescProps(d->uuid, chr.uuid, d->permissions)) {
      ok = false;
    }

    // Value-length invariants for SIG-reserved descriptors where the value is
    // user-provided at the library layer (Presentation Format is already
    // checked above). CCCD/SCCD are stack-managed; length at registration
    // time is typically 0 and the stack writes 2 bytes on subscription, so
    // we only flag non-empty + non-2-byte values as user errors.
    const uint16_t id = uuid16Of(d->uuid);
    const size_t len = d->value.size();
    switch (id) {
      case DSC_UUID_EXT_PROPS:  // 0x2900
        if (len != 0 && len != 2) {
          String dstr = d->uuid.toString();
          log_e("Characteristic %s: Extended Properties (0x2900) must be exactly 2 bytes (got %u)", u, static_cast<unsigned>(len));
          ok = false;
        }
        break;
      case DSC_UUID_CCCD:  // 0x2902
      case DSC_UUID_SCCD:  // 0x2903
        if (len != 0 && len != 2) {
          log_e(
            "Characteristic %s: descriptor %s must be exactly 2 bytes when pre-populated (got %u)", u, d->uuid.toString().c_str(), static_cast<unsigned>(len)
          );
          ok = false;
        }
        break;
      default: break;
    }
  }

  return ok;
}

// --------------------------------------------------------------------------
// Stage 1: descriptor permission validation (construction time)
// --------------------------------------------------------------------------

/**
 * @brief Validate a descriptor's permission combination at construction time.
 * @param descUuid Descriptor UUID being created (used for log context and
 *                 SIG-reserved access-profile enforcement).
 * @param chrUuid  Parent characteristic UUID (used for log context only).
 * @param perms    Requested permission bitmask for the descriptor.
 * @return true if no hard errors were found, false otherwise.
 * @note Enforces two rule sets:
 *       1. Generic: at least one read or write direction must be declared
 *          (fail-closed), authorization requires a matching base direction.
 *       2. SIG-reserved UUIDs (Core Spec Vol 3, Part G §3.3.3): each has a
 *          required access profile (e.g. 0x2904 read-only, 0x2902 read+write).
 *       Security-level mixing and AUTHEN-without-ENC are flagged as warnings.
 */
bool bleValidateDescProps(const BLEUUID &descUuid, const BLEUUID &chrUuid, BLEPermission perms) {
  String dstr = descUuid.toString();
  String cstr = chrUuid.toString();
  [[maybe_unused]]
  const char *d = dstr.c_str();
  [[maybe_unused]]
  const char *c = cstr.c_str();

  const bool anyRead = hasAnyReadPerm(perms);
  const bool anyWrite = hasAnyWritePerm(perms);
  const uint16_t id16 = uuid16Of(descUuid);

  bool ok = true;

  // Fail-closed: a descriptor with no permissions is inaccessible by design.
  // The CCCD auto-creation path on Bluedroid bypasses this helper (it sets
  // OpenReadWrite directly), so user-created descriptors reaching this point
  // have genuinely been declared as "no access" — almost certainly a bug.
  if (!anyRead && !anyWrite) {
    log_e(
      "Descriptor %s on characteristic %s: no permissions set (use at least one "
      "BLEPermission::Read* or Write* direction)",
      d, c
    );
    ok = false;
  }

  // Authorization is a modifier, not a direction: it sits on top of a base
  // read/write access perm. Declaring `*Authorized` without any of the
  // corresponding base perms (Read/ReadEncrypted/ReadAuthenticated or their
  // write equivalents) leaves the descriptor effectively unreachable. The
  // `anyRead`/`anyWrite` predicates above count `*Authorized` itself as a
  // direction — that's fine for the "has any access" check but useless
  // here, so this branch tests the base perms explicitly.
  const bool hasBaseRead = (perms & BLEPermission::Read) || (perms & BLEPermission::ReadEncrypted) || (perms & BLEPermission::ReadAuthenticated);
  const bool hasBaseWrite = (perms & BLEPermission::Write) || (perms & BLEPermission::WriteEncrypted) || (perms & BLEPermission::WriteAuthenticated);
  if ((perms & BLEPermission::ReadAuthorized) && !hasBaseRead) {
    log_e(
      "Descriptor %s on %s: ReadAuthorized requires a base read permission "
      "(Read / ReadEncrypted / ReadAuthenticated)",
      d, c
    );
    ok = false;
  }
  if ((perms & BLEPermission::WriteAuthorized) && !hasBaseWrite) {
    log_e(
      "Descriptor %s on %s: WriteAuthorized requires a base write permission "
      "(Write / WriteEncrypted / WriteAuthenticated)",
      d, c
    );
    ok = false;
  }

  // Reserved descriptor access profiles (Bluetooth Core Spec Vol 3,
  // Part G §3.3.3). For each SIG-assigned descriptor UUID the spec dictates
  // which directions the attribute must expose; violating that tends to
  // make the descriptor unusable or silently misbehaving with standard
  // clients, so these are hard errors rather than warnings.
  switch (id16) {
    case 0x2900:  // Extended Properties — read-only, library populates value.
      if (anyWrite) {
        log_e("Descriptor 0x2900 on %s: Extended Properties must be read-only", c);
        ok = false;
      }
      if (!anyRead) {
        log_e("Descriptor 0x2900 on %s: Extended Properties must be readable", c);
        ok = false;
      }
      break;
    case 0x2901:  // User Description — readable; writable only when the
                  // char's 0x2900 Writable Auxiliaries bit is set. We can't
                  // easily cross-check that here, so enforce only the read
                  // requirement and let the full validator catch the rest.
      if (!anyRead) {
        log_e("Descriptor 0x2901 on %s: User Description must be readable", c);
        ok = false;
      }
      break;
    case 0x2902:  // CCCD — read + write both required so clients can
                  // subscribe/unsubscribe and inspect state.
      if (!anyRead || !anyWrite) {
        log_e(
          "Descriptor 0x2902 (CCCD) on %s: must have both read and write permissions "
          "(subscription writes must be allowed)",
          c
        );
        ok = false;
      }
      break;
    case 0x2903:  // SCCD — read + write required for broadcast enable/disable.
      if (!anyRead || !anyWrite) {
        log_e("Descriptor 0x2903 (SCCD) on %s: must have both read and write permissions", c);
        ok = false;
      }
      break;
    case 0x2904:  // Presentation Format — read-only per spec.
    case 0x2905:  // Aggregate Format — read-only per spec.
    case 0x2906:  // Valid Range — read-only per spec.
      if (anyWrite) {
        log_e("Descriptor %s on %s: this SIG-reserved descriptor must be read-only", d, c);
        ok = false;
      }
      if (!anyRead) {
        log_e("Descriptor %s on %s: this SIG-reserved descriptor must be readable", d, c);
        ok = false;
      }
      break;
    default: break;
  }

  // 3.1 Open access — fire only when a plain (no-security) perm is declared
  // alongside another direction with security, or for non-CCCD/SCCD where
  // open access is often unintentional. CCCD/SCCD are conventionally open,
  // so skip the noise there.
  const bool isStackManaged = (id16 == 0x2902 || id16 == 0x2903);
  if (!isStackManaged) {
    if ((perms & BLEPermission::Read) && !(perms & BLEPermission::ReadEncrypted) && !(perms & BLEPermission::ReadAuthenticated)
        && ((perms & BLEPermission::WriteEncrypted) || (perms & BLEPermission::WriteAuthenticated))) {
      log_w("Descriptor %s on %s: plain READ mixed with secure WRITE — readable with no security", d, c);
    }
    if ((perms & BLEPermission::Write) && !(perms & BLEPermission::WriteEncrypted) && !(perms & BLEPermission::WriteAuthenticated)
        && ((perms & BLEPermission::ReadEncrypted) || (perms & BLEPermission::ReadAuthenticated))) {
      log_w("Descriptor %s on %s: plain WRITE mixed with secure READ — writable with no security", d, c);
    }
  }

  // 3.3 Security-level mixing on the same direction.
  if ((perms & BLEPermission::Read) && ((perms & BLEPermission::ReadEncrypted) || (perms & BLEPermission::ReadAuthenticated))) {
    log_w("Descriptor %s on %s: plain READ mixed with encrypted READ (encryption made redundant)", d, c);
  }
  if ((perms & BLEPermission::Write) && ((perms & BLEPermission::WriteEncrypted) || (perms & BLEPermission::WriteAuthenticated))) {
    log_w("Descriptor %s on %s: plain WRITE mixed with encrypted WRITE (encryption made redundant)", d, c);
  }

  // 3.4 AUTHEN without ENC — per GATT security hierarchy, authenticated
  // implies encrypted; declaring AUTHEN alone works with most stacks but is
  // ambiguous at the API level.
  if ((perms & BLEPermission::ReadAuthenticated) && !(perms & BLEPermission::ReadEncrypted)) {
    log_w("Descriptor %s on %s: ReadAuthenticated implies ReadEncrypted — consider adding it", d, c);
  }
  if ((perms & BLEPermission::WriteAuthenticated) && !(perms & BLEPermission::WriteEncrypted)) {
    log_w("Descriptor %s on %s: WriteAuthenticated implies WriteEncrypted — consider adding it", d, c);
  }

  return ok;
}

#endif /* BLE_ENABLED */
