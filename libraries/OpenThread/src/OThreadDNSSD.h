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

#include "soc/soc_caps.h"
#include "sdkconfig.h"

#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED
#if CONFIG_OPENTHREAD_SRP_CLIENT

#include <Arduino.h>
#include "OThread.h"
#include <openthread/srp_client.h>
#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
#include <openthread/dns.h>
#include <openthread/dns_client.h>
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @file OThreadDNSSD.h
 * @brief Arduino-style Thread DNS-SD API (SRP advertise + DNS discover).
 *
 * This is DNS-Based Service Discovery for Thread — not mDNS on the mesh.
 * Sketches register services with an SRP server (typical Border Router) and/or
 * browse/resolve via the DNS client (same server as SRP when auto-set).
 * Wi-Fi LAN mDNS remains ESPmDNS / OpenThread CLI mdns on BR infra.
 *
 * Advertise (ESPmDNS-like):
 *   OThreadDNSSD.begin("sensor-1");
 *   OThreadDNSSD.addService("ot", "udp", 12345);  // "_ot" / "_udp" also OK
 *   OThreadDNSSD.waitForAnnounce(30000);
 *
 * Discover (ESPmDNS-like; needs `CONFIG_OPENTHREAD_DNS_CLIENT`):
 *   OThreadDNSSD.begin("browser");
 *   int n = OThreadDNSSD.queryService("ot", "udp");
 *   IPAddress a = OThreadDNSSD.queryHost("sensor-1");
 *   // Async: onQueryEvent + startQueryService / startQueryHost
 *
 * After a discover timeout, end(), or before the next query, do not treat
 * result getters as stable — copy results when the call returns (or on
 * OT_DNSSD_QUERY_DONE). Each DNS request tags its OpenThread callback with a
 * generation so a late response cannot apply to a newer query.
 *
 * Name conflicts (`OT_ERROR_DUPLICATED`) are reported to the sketch; the library
 * does not rename. Prefer unique hostnames and keep NVS across reflash.
 *
 * Fixed-size pools (like OThreadScan) avoid heap growth from sketch misuse.
 * Override caps with `#define` before including this header.
 *
 * Requires `CONFIG_OPENTHREAD_SRP_CLIENT`. Discover APIs additionally require
 * `CONFIG_OPENTHREAD_DNS_CLIENT`. The global instance is @ref OThreadDNSSD.
 */

/** @brief Max services that can be advertised concurrently (fail closed when full). */
#ifndef OT_DNSSD_MAX_SERVICES
#define OT_DNSSD_MAX_SERVICES 5
#endif

/** @brief Max TXT key/value pairs stored per advertised service. */
#ifndef OT_DNSSD_MAX_TXT_ENTRIES
#define OT_DNSSD_MAX_TXT_ENTRIES 4
#endif

/** @brief Max DNS-SD subtype labels per advertised service. */
#ifndef OT_DNSSD_MAX_SUBTYPES
#define OT_DNSSD_MAX_SUBTYPES 4
#endif

/** @brief Max instances retained from one @ref OThreadDNSSDClass::queryService. */
#ifndef OT_DNSSD_MAX_QUERY_RESULTS
#define OT_DNSSD_MAX_QUERY_RESULTS 16
#endif

/**
 * @brief In-flight OpenThread DNS callback contexts.
 *
 * A slot stays reserved until OpenThread invokes the DNS callback — including
 * after a local query timeout or @ref OThreadDNSSDClass::end. Short
 * `queryHost` timeouts therefore accumulate slots until the stack's DNS wait
 * (typically several seconds) completes. Must be large enough for overlapping
 * abandoned requests; 4 is too small for a tight timeout loop.
 */
#ifndef OT_DNSSD_MAX_DNS_CB_CTX
#define OT_DNSSD_MAX_DNS_CB_CTX 16
#endif

/* Pool sizes are indexed/counted with uint8_t throughout OThreadDNSSD. */
#if OT_DNSSD_MAX_SERVICES < 1 || OT_DNSSD_MAX_SERVICES > 255
#error "OT_DNSSD_MAX_SERVICES must be in the range 1..255"
#endif
#if OT_DNSSD_MAX_TXT_ENTRIES < 1 || OT_DNSSD_MAX_TXT_ENTRIES > 255
#error "OT_DNSSD_MAX_TXT_ENTRIES must be in the range 1..255"
#endif
#if OT_DNSSD_MAX_SUBTYPES < 1 || OT_DNSSD_MAX_SUBTYPES > 255
#error "OT_DNSSD_MAX_SUBTYPES must be in the range 1..255"
#endif
#if OT_DNSSD_MAX_QUERY_RESULTS < 1 || OT_DNSSD_MAX_QUERY_RESULTS > 255
#error "OT_DNSSD_MAX_QUERY_RESULTS must be in the range 1..255 (stored in uint8_t)"
#endif
#if OT_DNSSD_MAX_DNS_CB_CTX < 1 || OT_DNSSD_MAX_DNS_CB_CTX > 255
#error "OT_DNSSD_MAX_DNS_CB_CTX must be in the range 1..255"
#endif

/** @brief Max host label length (excluding domain), bytes. */
#ifndef OT_DNSSD_HOST_NAME_MAX
#define OT_DNSSD_HOST_NAME_MAX 63
#endif

/** @brief Max service instance label length, bytes. */
#ifndef OT_DNSSD_INSTANCE_NAME_MAX
#define OT_DNSSD_INSTANCE_NAME_MAX 63
#endif

/** @brief Max single DNS label for service/proto inputs, bytes. */
#ifndef OT_DNSSD_LABEL_MAX
#define OT_DNSSD_LABEL_MAX 15
#endif

/** @brief Max encoded service name (`_type._proto`), bytes. */
#ifndef OT_DNSSD_SERVICE_NAME_MAX
#define OT_DNSSD_SERVICE_NAME_MAX (2 * OT_DNSSD_LABEL_MAX + 3)
#endif

#if OT_DNSSD_LABEL_MAX < 1 || OT_DNSSD_LABEL_MAX > 63
#error "OT_DNSSD_LABEL_MAX must be in the range 1..63"
#endif
#if OT_DNSSD_SERVICE_NAME_MAX < (2 * OT_DNSSD_LABEL_MAX + 3)
#error "OT_DNSSD_SERVICE_NAME_MAX must be >= 2*OT_DNSSD_LABEL_MAX+3 (\"_type._proto\")"
#endif

/** @brief Max TXT key length stored in a slot, bytes. */
#ifndef OT_DNSSD_TXT_KEY_MAX
#define OT_DNSSD_TXT_KEY_MAX 16
#endif

/** @brief Max TXT value length stored in a slot, bytes. */
#ifndef OT_DNSSD_TXT_VALUE_MAX
#define OT_DNSSD_TXT_VALUE_MAX 32
#endif

/** @brief Max subtype label length, bytes. */
#ifndef OT_DNSSD_SUBTYPE_MAX
#define OT_DNSSD_SUBTYPE_MAX 16
#endif

/**
 * @brief Default timeout (ms) for @ref OThreadDNSSDClass::queryService browse.
 *
 * Must be longer than OpenThread's DNS client response wait (typically 7000 ms)
 * so an empty Discovery Proxy answer (~6 s per RFC 8766) is received as
 * success with 0 instances, not aborted as a local semaphore timeout.
 */
#ifndef OT_DNSSD_QUERY_TIMEOUT_MS
#define OT_DNSSD_QUERY_TIMEOUT_MS 10000
#endif

/** @brief Events delivered to @ref OThreadDNSSDClass::onServiceEvent. */
typedef enum {
  OT_DNSSD_EVENT_ANNOUNCED = 0,  ///< Host + services registered with SRP server.
  OT_DNSSD_EVENT_REMOVED = 1,    ///< Host gone, last local service gone, or @ref OThreadDNSSDClass::end.
  OT_DNSSD_EVENT_ERROR = 2,      ///< Registration/update failed (see error code).
} ot_dnssd_event_t;

/**
 * @brief Service registration event callback.
 *
 * Context depends on the event source:
 * - @ref OT_DNSSD_EVENT_ANNOUNCED, @ref OT_DNSSD_EVENT_ERROR, and
 *   @ref OT_DNSSD_EVENT_REMOVED from the SRP client callback run on the
 *   OpenThread task. One SRP callback delivers at most one of ERROR, REMOVED,
 *   or ANNOUNCED (REMOVED is host-gone or last local service gone — not a
 *   partial service delete that still leaves others registered).
 * - @ref OT_DNSSD_EVENT_REMOVED from @ref OThreadDNSSDClass::end runs on the
 *   caller task (e.g. Arduino `loop()` / `setup()`).
 *
 * In all cases, do not call other OThreadDNSSD methods from inside the
 * callback (set flags / copy state only). Do not assume OT-task-only APIs
 * are safe for every delivery.
 *
 * @param event   Event type.
 * @param error   OpenThread error (`OT_ERROR_NONE` on success).
 * @param context User context from @ref OThreadDNSSDClass::onServiceEvent.
 */
typedef void (*OThreadDNSSDEventCallback)(ot_dnssd_event_t event, otError error, void *context);

#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
/** @brief Which async discover operation finished. */
typedef enum {
  OT_DNSSD_QUERY_SERVICE = 0,  ///< @ref OThreadDNSSDClass::startQueryService
  OT_DNSSD_QUERY_HOST = 1,     ///< @ref OThreadDNSSDClass::startQueryHost
} ot_dnssd_query_kind_t;

/** @brief Events delivered to @ref OThreadDNSSDClass::onQueryEvent. */
typedef enum {
  OT_DNSSD_QUERY_DONE = 0,   ///< Finished (may be 0 results); see error / count.
  OT_DNSSD_QUERY_ERROR = 1,  ///< DNS failure / timeout (count usually 0).
} ot_dnssd_query_event_t;

/**
 * @brief Async discover event callback.
 *
 * Invoked on the OpenThread task for DNS completion. @ref OThreadDNSSDClass::end
 * may invoke it on the **caller** task with @ref OT_DNSSD_QUERY_ERROR /
 * `OT_ERROR_ABORT` if a discover was in flight. Do not call other OThreadDNSSD
 * methods from inside the callback (set flags / copy state only). Read results
 * from `loop()` via the indexed getters or @ref OThreadDNSSDClass::resolvedAddress.
 *
 * @param kind    Service browse vs host resolve.
 * @param event   Done or error.
 * @param error   OpenThread / DNS error.
 * @param count   For service browse: number of instances in the result pool.
 *                For host resolve: `1` if an address was stored, else `0`.
 * @param context User context from @ref OThreadDNSSDClass::onQueryEvent.
 */
typedef void (*OThreadDNSSDQueryCallback)(
  ot_dnssd_query_kind_t kind, ot_dnssd_query_event_t event, otError error, int count, void *context
);
#endif /* CONFIG_OPENTHREAD_DNS_CLIENT */

/**
 * @brief Thread DNS-SD facade (SRP advertise + DNS discover).
 *
 * Global instance: @ref OThreadDNSSD. Policy (rename, re-advertise, give-up)
 * stays in the sketch; this class reports status and errors.
 */
class OThreadDNSSDClass {
public:
  OThreadDNSSDClass();
  ~OThreadDNSSDClass();

  /**
   * @brief Configure local host name and enable SRP auto-start.
   *
   * Also enables DNS default-server auto-follow when the DNS client is built
   * in. Requires `OThread.begin()` and a live OpenThread instance. Prefer an
   * attached role (child/router/leader) before expecting announce or queries
   * to succeed.
   *
   * @param hostName Host label (no domain), e.g. `"sensor-1"`. Prefer a
   *                 per-device unique label when multiple nodes share an OTBR.
   * @return true if configured successfully; false on missing stack, bad name,
   *         or OpenThread error. False is local/config, not "SRP server not
   *         ready". Do not call addService / query APIs until a later successful
   *         begin().
   */
  bool begin(const char *hostName);
  bool begin(const String &hostName) {
    return begin(hostName.c_str());
  }

  /**
   * @brief Unregister host/services, stop SRP client, clear query results.
   *
   * Also called from `OThread.end()`. Delivers @ref OT_DNSSD_EVENT_REMOVED via
   * @ref onServiceEvent on the **caller** task (not the OpenThread task).
   * If an async discover is in flight, also delivers @ref OT_DNSSD_QUERY_ERROR
   * with `OT_ERROR_ABORT` via @ref onQueryEvent (caller task) before REMOVED.
   * Invalidates any in-flight discover; do not use query result getters until
   * a new @ref begin and query.
   */
  void end();

  /** @return true after a successful @ref begin and before @ref end. */
  bool started() const {
    return _started;
  }

  /** @brief Local host label from a successful @ref begin (advertise side). Empty if not started. */
  const char *hostname() const {
    return _hostName;
  }

  /**
   * @brief Override instance label used for subsequent @ref addService calls.
   *
   * Default instance name is the host name from @ref begin.
   */
  void setInstanceName(const char *name);
  void setInstanceName(const String &name) {
    setInstanceName(name.c_str());
  }

  /**
   * @brief Advertise a service (ESPmDNS-style).
   *
   * @param service Service type, with or without a leading underscore, e.g. `"http"` or `"_ot"`.
   *                All leading underscores are stripped; the result must be a non-empty label.
   * @param proto   Protocol, with or without a leading underscore, e.g. `"tcp"` or `"_udp"`.
   * @param port    Service port.
   * @return true if queued for registration; false if full, not started, invalid, or the
   *         matching slot is still being removed (@ref removeService).
   *
   * Calling again with the same service+proto updates the existing slot
   * (idempotent) unless a remove of that slot is still in flight.
   */
  bool addService(const char *service, const char *proto, uint16_t port);
  bool addService(const String &service, const String &proto, uint16_t port) {
    return addService(service.c_str(), proto.c_str(), port);
  }

  /**
   * @brief Add a TXT key/value to an existing service.
   *
   * Service must already exist via @ref addService. Fixed TXT slots per service.
   * Rejected if the service is still being removed.
   */
  bool addServiceTxt(const char *service, const char *proto, const char *key, const char *value);
  bool addServiceTxt(const String &service, const String &proto, const String &key, const String &value) {
    return addServiceTxt(service.c_str(), proto.c_str(), key.c_str(), value.c_str());
  }

  /**
   * @brief Add a DNS-SD subtype label (Advanced; fixed per-service cap).
   *
   * @param service Service type (leading underscores optional; stripped).
   * @param proto   Protocol (leading underscores optional; stripped).
   * @param subtype Subtype label (leading underscores optional; stripped). Empty after
   *                strip is rejected. Adding an existing subtype is idempotent.
   * @return true if stored and queued for update (or already present); false if full or invalid.
   */
  bool addServiceSubtype(const char *service, const char *proto, const char *subtype);
  bool addServiceSubtype(const String &service, const String &proto, const String &subtype) {
    return addServiceSubtype(service.c_str(), proto.c_str(), subtype.c_str());
  }

  /**
   * @brief Request removal of one service from the SRP server.
   * @return true if remove was queued (or already in flight / cleared locally).
   *
   * Slot string buffers stay valid until OpenThread reports the service
   * removed (callback) or @ref end clears the client. Do not assume the
   * slot is free for reuse immediately after this returns; @ref addService
   * of the same type returns false until the slot is reclaimed.
   */
  bool removeService(const char *service, const char *proto);
  bool removeService(const String &service, const String &proto) {
    return removeService(service.c_str(), proto.c_str());
  }

  /**
   * @brief Register event callback (optional).
   *
   * Do not call other OThreadDNSSD methods from the callback.
   */
  void onServiceEvent(OThreadDNSSDEventCallback callback, void *context = nullptr);

  /**
   * @brief true when the local SRP client reports host + services Registered.
   *
   * Reads live OpenThread state (`otSrpClientGetHostInfo` /
   * `otSrpClientGetServices`) on each call so the sketch sees updates as soon
   * as the client reaches Registered (typically right after the SRP server
   * accepts the update). This is not a probe of the Border Router itself.
   */
  bool isAnnounceComplete();

  /**
   * @brief Block until announced, a terminal SRP error, or timeout.
   *
   * Retryable errors (e.g. response timeout) do not abort the wait. Name
   * conflicts (`OT_ERROR_DUPLICATED`) and security rejects end the wait with
   * false; check @ref lastError and decide in the sketch (new name, etc.).
   * After a terminal failure, later OpenThread retries may still reach
   * Registered — poll @ref isAnnounceComplete from `loop()` if needed.
   *
   * @param timeoutMs Max wait (pass UINT32_MAX for forever).
   * @return true if announced successfully.
   */
  bool waitForAnnounce(uint32_t timeoutMs);

  /** @brief Last SRP or DNS callback error (`OT_ERROR_NONE` if none). */
  otError lastError() const {
    return _lastError;
  }

#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
  /**
   * @brief Browse for service instances (ESPmDNS-style).
   *
   * Blocking DNS PTR browse of `_service._proto.default.service.arpa`.
   * Fills a fixed result pool (max @ref OT_DNSSD_MAX_QUERY_RESULTS).
   * Requires @ref begin so SRP auto-start can select the DNS/SRP server.
   *
   * On success or empty browse, read getters before starting another discover.
   * After a timeout (`lastError` == `OT_ERROR_RESPONSE_TIMEOUT`) or @ref end,
   * do not rely on getters until a new query; copy what you need on return.
   *
   * @return Number of results stored (0 on failure / none found).
   */
  int queryService(const char *service, const char *proto);
  int queryService(const String &service, const String &proto) {
    return queryService(service.c_str(), proto.c_str());
  }

  /**
   * @brief Resolve a host label to an IPv6 address (ESPmDNS-style).
   *
   * Queries `host.default.service.arpa` (or @p host as-is if it already
   * contains a domain). Returns an empty IPv6 `IPAddress` on failure.
   * Requires @ref begin.
   *
   * Use the returned `IPAddress` (or @ref resolvedAddress right after a
   * successful call). After a timeout or @ref end, do not rely on
   * @ref resolvedAddress until a new resolve.
   *
   * @param host      Host label or FQDN.
   * @param timeoutMs Max wait for the DNS response (default @ref OT_DNSSD_QUERY_TIMEOUT_MS).
   *                 The default is also raised to the OpenThread DNS client wait if that
   *                 is longer. An explicit value is honored as-is (including short fail-fast
   *                 and UINT32_MAX for forever).
   */
  IPAddress queryHost(const char *host, uint32_t timeoutMs = OT_DNSSD_QUERY_TIMEOUT_MS);
  IPAddress queryHost(const String &host, uint32_t timeoutMs = OT_DNSSD_QUERY_TIMEOUT_MS) {
    return queryHost(host.c_str(), timeoutMs);
  }

  /**
   * @brief Register async discover callback (optional).
   *
   * Used with @ref startQueryService / @ref startQueryHost. Do not call other
   * OThreadDNSSD methods from the callback. On @ref OT_DNSSD_QUERY_DONE, copy
   * results in `loop()`; after timeout / @ref end, do not rely on getters until
   * a new query.
   */
  void onQueryEvent(OThreadDNSSDQueryCallback callback, void *context = nullptr);

  /**
   * @brief Start a non-blocking service browse (same results as @ref queryService).
   *
   * Returns immediately. Completion is reported via @ref onQueryEvent. Only one
   * discover operation (blocking or async) may be in flight.
   *
   * @return true if the browse was started; false if busy, not started, or invalid.
   */
  bool startQueryService(const char *service, const char *proto);
  bool startQueryService(const String &service, const String &proto) {
    return startQueryService(service.c_str(), proto.c_str());
  }

  /**
   * @brief Start a non-blocking host resolve (same result as @ref queryHost).
   *
   * Returns immediately. On @ref OT_DNSSD_QUERY_DONE, read @ref resolvedAddress.
   *
   * @return true if the resolve was started; false if busy, not started, or invalid.
   */
  bool startQueryHost(const char *host);
  bool startQueryHost(const String &host) {
    return startQueryHost(host.c_str());
  }

  /** @return true while a blocking or async discover is in flight. */
  bool isQueryInProgress() const {
    return _queryInProgress;
  }

  /**
   * @brief Number of instances from the last service browse
   *        (@ref queryService or @ref startQueryService).
   */
  int queryResultCount() const {
    return (int)_queryResultCount;
  }

  /**
   * @brief IPv6 address from the last host resolve
   *        (@ref queryHost or @ref startQueryHost).
   */
  IPAddress resolvedAddress() const {
    return _dnsResolvedAddr;
  }

  /**
   * @brief Host label from the last @ref queryService result at @p idx.
   *
   * Distinct from @ref hostname() with no arguments (local SRP host from @ref begin).
   * @return Empty string if @p idx is out of range.
   */
  const char *hostname(int idx) const;
  /**
   * @brief Service instance label from the last @ref queryService result at @p idx.
   * @return Empty string if @p idx is out of range.
   */
  const char *instanceName(int idx) const;
  /**
   * @brief IPv6 address from the last query result at @p idx.
   * @return Empty IPv6 address if unknown or @p idx is out of range.
   */
  IPAddress address(int idx) const;
  /** @brief Service port from the last @ref queryService result at @p idx (0 if unknown). */
  uint16_t port(int idx) const;
  /** @brief Number of TXT entries stored for result @p idx. */
  int numTxt(int idx) const;
  /** @return true if result @p idx has a TXT entry with key @p key. */
  bool hasTxt(int idx, const char *key) const;
  /** @brief TXT value for @p key on result @p idx (empty String if missing). */
  String txt(int idx, const char *key) const;
  /** @brief TXT value at index @p txtIdx on result @p idx. */
  String txt(int idx, int txtIdx) const;
  /** @brief TXT key at index @p txtIdx on result @p idx. */
  String txtKey(int idx, int txtIdx) const;
#endif /* CONFIG_OPENTHREAD_DNS_CLIENT */

private:
  struct TxtSlot {
    char key[OT_DNSSD_TXT_KEY_MAX + 1];
    uint8_t value[OT_DNSSD_TXT_VALUE_MAX];
    uint16_t valueLen;
    bool used;
  };

  struct ServiceSlot {
    bool used;
    bool registeredWithOt;
    bool pendingRemove;  ///< true after RemoveService until OT reclaims via callback
    char serviceName[OT_DNSSD_SERVICE_NAME_MAX + 1];  ///< e.g. "_http._tcp"
    char instanceName[OT_DNSSD_INSTANCE_NAME_MAX + 1];
    char subtypes[OT_DNSSD_MAX_SUBTYPES][OT_DNSSD_SUBTYPE_MAX + 1];
    const char *subtypePtrs[OT_DNSSD_MAX_SUBTYPES + 1];
    uint8_t numSubtypes;
    TxtSlot txt[OT_DNSSD_MAX_TXT_ENTRIES];
    uint8_t numTxt;
    otDnsTxtEntry txtEntries[OT_DNSSD_MAX_TXT_ENTRIES];
    otSrpClientService otService;
    uint16_t port;
  };

#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
  struct QueryTxtSlot {
    char key[OT_DNSSD_TXT_KEY_MAX + 1];
    char value[OT_DNSSD_TXT_VALUE_MAX + 1];
    bool used;
  };

  struct QueryResultSlot {
    bool used;
    char instanceName[OT_DNSSD_INSTANCE_NAME_MAX + 1];
    char hostName[OT_DNSSD_HOST_NAME_MAX + 1];
    IPAddress address;
    uint16_t port;
    QueryTxtSlot txt[OT_DNSSD_MAX_TXT_ENTRIES];
    uint8_t numTxt;
  };
#endif

  static void handleSrpCallback(
    otError aError, const otSrpClientHostInfo *aHostInfo, const otSrpClientService *aServices,
    const otSrpClientService *aRemovedServices, void *aContext
  );
  void onSrpCallback(
    otError aError, const otSrpClientHostInfo *aHostInfo, const otSrpClientService *aServices,
    const otSrpClientService *aRemovedServices
  );

  bool ensureAnnounceSem();
  void resetSlots();
  void clearAnnounceState();
  bool copyCString(char *dst, size_t dstSize, const char *src) const;
  bool buildServiceName(char *dst, size_t dstSize, const char *service, const char *proto);
  int findServiceIndex(const char *serviceName) const;
  int allocServiceIndex();
  void wireOtService(ServiceSlot &slot);
  bool pushServiceToOt(ServiceSlot &slot);
  bool updateServiceOnOt(ServiceSlot &slot);
  void notifyEvent(ot_dnssd_event_t event, otError error);
  void reclaimRemovedServices(const otSrpClientService *removedServices);
  bool evaluateAnnounceComplete(const otSrpClientHostInfo *hostInfo, const otSrpClientService *services) const;
  void refreshAnnounceFlag(const otSrpClientHostInfo *hostInfo, const otSrpClientService *services);
  bool syncAnnounceCompleteFromOt();
  static bool isTerminalSrpError(otError error);

#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
  bool ensureDnsSem();
  void resetQueryResults();
  void copyHostLabel(char *dst, size_t dstSize, const char *fqdnOrLabel);
  void fillTxtFromDnsData(QueryResultSlot &slot, const uint8_t *txtData, uint16_t txtLen);
  bool resolveMissingServiceDetails(const char *serviceFqdn);
  IPAddress resolveAddressFqdn(const char *fqdn, uint32_t timeoutMs);
  /** Wait long enough for OT DNS (+ Discovery Proxy empty) before local abort. */
  uint32_t dnsResponseWaitMs(otInstance *inst) const;
  /** Blocking host wait: default uses @ref dnsResponseWaitMs; explicit timeoutMs is honored. */
  uint32_t hostResolveWaitMs(otInstance *inst, uint32_t timeoutMs) const;
  bool buildHostFqdn(char *dst, size_t dstSize, const char *host) const;
  bool buildServiceFqdn(char *dst, size_t dstSize, const char *shortName) const;
  bool slotNeedsDetailResolve(const QueryResultSlot &slot) const;
  bool anySlotNeedsDetailResolve() const;
  bool startDetailResolveAt(uint8_t startIdx);
  void finishAsyncQuery(otError error);
  void notifyQueryEvent(ot_dnssd_query_event_t event, otError error, int count);
  /** Begin a discover op; invalidates any prior in-flight DNS callback. */
  void armQueryOp(bool async, ot_dnssd_query_kind_t kind);
  /** End the current discover op; late DNS callbacks become no-ops. */
  void clearQueryOp();
  bool isQueryCallbackCurrent(uint32_t gen) const;
  void wakeDnsWaiter();
  /** After a semaphore timeout: true if the op did not complete (caller must fail). */
  bool abandonIfBlockingWaitTimedOut();

  struct DnsCbCtx {
    OThreadDNSSDClass *self;
    uint32_t gen;
    bool used;
  };
  static constexpr uint8_t kDnsCbCtxCount = OT_DNSSD_MAX_DNS_CB_CTX;
  DnsCbCtx *allocDnsCbCtx();
  void freeDnsCbCtx(DnsCbCtx *ctx);

  static void handleDnsBrowseCallback(otError aError, const otDnsBrowseResponse *aResponse, void *aContext);
  void onDnsBrowseCallback(otError aError, const otDnsBrowseResponse *aResponse, uint32_t gen);

  static void handleDnsAddressCallback(otError aError, const otDnsAddressResponse *aResponse, void *aContext);
  void onDnsAddressCallback(otError aError, const otDnsAddressResponse *aResponse, uint32_t gen);

  static void handleDnsServiceCallback(otError aError, const otDnsServiceResponse *aResponse, void *aContext);
  void onDnsServiceCallback(otError aError, const otDnsServiceResponse *aResponse, uint32_t gen);
#endif

  bool _started;
  bool _announceComplete;
  otError _lastError;
  char _hostName[OT_DNSSD_HOST_NAME_MAX + 1];
  char _instanceName[OT_DNSSD_INSTANCE_NAME_MAX + 1];
  bool _instanceNameSet;
  ServiceSlot _services[OT_DNSSD_MAX_SERVICES];
  OThreadDNSSDEventCallback _eventCb;
  void *_eventCtx;
  SemaphoreHandle_t _announceSem;

#if defined(CONFIG_OPENTHREAD_DNS_CLIENT) && CONFIG_OPENTHREAD_DNS_CLIENT
  QueryResultSlot _queryResults[OT_DNSSD_MAX_QUERY_RESULTS];
  uint8_t _queryResultCount;
  SemaphoreHandle_t _dnsSem;
  volatile bool _dnsDone;
  IPAddress _dnsResolvedAddr;
  int _dnsServiceResolveIdx;
  char _dnsServiceFqdn[OT_DNS_MAX_NAME_SIZE];
  bool _queryInProgress;
  bool _queryAsync;
  ot_dnssd_query_kind_t _queryKind;
  /** Monotonic counter; bumped on arm/clear so stale callbacks cannot match. */
  volatile uint32_t _queryGen;
  /** Generation of the armed op (0 = none). OT callbacks carry the gen from dispatch. */
  volatile uint32_t _queryActiveGen;
  DnsCbCtx _dnsCbCtx[kDnsCbCtxCount];
  OThreadDNSSDQueryCallback _queryCb;
  void *_queryCtx;
#endif
};

extern OThreadDNSSDClass OThreadDNSSD;  ///< Global Thread DNS-SD instance.

#endif /* CONFIG_OPENTHREAD_SRP_CLIENT */
#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
