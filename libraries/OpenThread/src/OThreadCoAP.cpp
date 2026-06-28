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

#include "OThreadCoAP.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

#include "OThread.h"
#include "esp_openthread_lock.h"

#include <algorithm>
#include <string.h>
#include <vector>

#include <openthread/coap.h>
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
#include <openthread/coap_secure.h>
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// The dispatch callbacks live in othread_coap_detail (so they can be friends of
// the request/response classes). Pull their names in for unqualified use below.
using othread_coap_detail::defaultRequestCb;
using othread_coap_detail::resourceRequestCb;

namespace {

struct OtLock {
  bool held;
  explicit OtLock(TickType_t ticks = portMAX_DELAY) : held(esp_openthread_lock_acquire(ticks)) {}
  ~OtLock() {
    if (held) {
      esp_openthread_lock_release();
    }
  }
  operator bool() const {
    return held;
  }
};

inline void ipToOt(const IPAddress &in, otIp6Address &out) {
  for (int i = 0; i < 16; ++i) {
    out.mFields.m8[i] = in[i];
  }
}

inline IPAddress otToIp(const otIp6Address &in) {
  return IPAddress(IPv6, in.mFields.m8);
}

inline bool parseIp(const char *host, IPAddress &ip) {
  if (!host) {
    return false;
  }
  return ip.fromString(host) && ip.type() == IPv6;
}

// True for an IPv6 multicast destination (first byte 0xFF). OpenThread invokes a
// request's response handler once per responder for multicast, so the blocking
// context must survive multiple callbacks (see ClientBlockingContext).
inline bool isMulticastIp(const IPAddress &ip) {
  return ip.type() == IPv6 && ip[0] == 0xFF;
}

inline int otCodeToArduino(otCoapCode code) {
  uint8_t raw = static_cast<uint8_t>(code);
  int klass = (raw >> 5) & 0x07;
  int detail = raw & 0x1f;
  return klass * 100 + detail;
}

inline otCoapCode arduinoCodeToOt(int code, otCoapCode fallback = static_cast<otCoapCode>(OT_COAP_CODE(2, 5))) {
  if (code < 0) {
    return fallback;
  }
  int klass = code / 100;
  int detail = code % 100;
  if (klass < 0 || klass > 7 || detail < 0 || detail > 31) {
    return fallback;
  }
  return static_cast<otCoapCode>(OT_COAP_CODE(klass, detail));
}

inline otCoapCode reqMethodToOt(int method) {
  return static_cast<otCoapCode>(method);
}

inline int methodFromOt(otCoapCode code) {
  switch (static_cast<uint8_t>(code)) {
    case OT_COAP_REQ_GET:    return OT_COAP_REQ_GET;
    case OT_COAP_REQ_POST:   return OT_COAP_REQ_POST;
    case OT_COAP_REQ_PUT:    return OT_COAP_REQ_PUT;
    case OT_COAP_REQ_DELETE: return OT_COAP_REQ_DELETE;
    default:                 return 0;
  }
}

inline uint8_t methodMaskFromMethod(int method) {
  switch (method) {
    case OT_COAP_REQ_GET:    return OT_COAP_METHOD_GET;
    case OT_COAP_REQ_POST:   return OT_COAP_METHOD_POST;
    case OT_COAP_REQ_PUT:    return OT_COAP_METHOD_PUT;
    case OT_COAP_REQ_DELETE: return OT_COAP_METHOD_DELETE;
    default:                 return 0;
  }
}

inline int errorFromOt(otError err, int invalidStateCode = OT_COAP_ERROR_INVALID_STATE) {
  switch (err) {
    case OT_ERROR_NONE:             return 0;
    case OT_ERROR_RESPONSE_TIMEOUT: return OT_COAP_ERROR_TIMEOUT;
    case OT_ERROR_NO_BUFS:          return OT_COAP_ERROR_NO_BUFS;
    case OT_ERROR_INVALID_ARGS:     return OT_COAP_ERROR_INVALID_ARGS;
    case OT_ERROR_INVALID_STATE:    return invalidStateCode;
    default:                        return OT_COAP_ERROR_NO_RESPONSE;
  }
}

struct RequestState {
  String path;
  int method = 0;
  IPAddress remoteIp = IPAddress(IPv6);
  uint16_t remotePort = 0;
  bool confirmable = false;
  // True when the request was addressed to an IPv6 multicast group (its
  // destination, not the sender, is multicast). Handlers should treat these as
  // fire-and-forget commands and generally avoid responding (RFC 7252 §8.2).
  bool multicast = false;
  uint8_t payload[OT_COAP_MAX_PAYLOAD] = {0};
  size_t payloadLen = 0;
};

struct ResponseState {
  otInstance *instance = nullptr;
  const otMessage *request = nullptr;
  otMessageInfo messageInfo = {};
  int code = OT_COAP_RESP_OK;
  uint16_t contentFormat = OT_COAP_FORMAT_TEXT;
  String locationPath;
  bool hasLocation = false;
  bool hasMaxAge = false;
  uint32_t maxAge = 0;
  uint8_t payload[OT_COAP_MAX_PAYLOAD] = {0};
  size_t payloadLen = 0;
  bool secure = false;
  bool sent = false;
};

// RFC 7252 §8.2 — group requests must not get unicast-style error replies.
// log_i: expected silence (handler chose not to reply, or onNotFound on a group miss).
// log_w: likely misconfiguration (wrong method, missing handler, wrong API for multicast).
static void logMulticastExpected(const char *context, const char *path, bool confirmable, const char *reason) {
  log_i("OThreadCoAP: %s — not replying to multicast %s on '%s' (%s; RFC 7252 §8.2)", context, confirmable ? "CON" : "NON", path ? path : "", reason);
}

static void logMulticastWarn(const char *context, const char *path, bool confirmable, const char *reason) {
  log_w("OThreadCoAP: %s — not replying to multicast %s on '%s' (%s; RFC 7252 §8.2)", context, confirmable ? "CON" : "NON", path ? path : "", reason);
}

struct ServerImpl;

struct ServerResourceSlot {
  bool inUse = false;
  ServerImpl *owner = nullptr;
  String path;
  uint8_t methodMask = 0;
  OThreadCoAPHandler handler = nullptr;
  void *handlerContext = nullptr;
  otCoapResource resource = {};
  bool registered = false;
  // serve() only: lock-protected snapshot read by serveSlotHandler on the OT task.
  String *serveValuePtr = nullptr;
  String serveSnapshot;
};

struct ServerImpl {
  uint16_t port = OT_COAP_DEFAULT_PORT;
  bool running = false;
  bool secure = false;
  OThreadCoAPHandler notFoundHandler = nullptr;
  void *notFoundContext = nullptr;
  OThreadCoAPResourceStore *resourceStore = nullptr;
  uint16_t maxConnectionAttempts = 0;
  std::vector<uint8_t> psk;
  String pskIdentity;
  String certPem;
  String keyPem;
  String caPem;
  bool verifyPeer = true;
  ServerResourceSlot slots[OT_COAP_MAX_RESOURCES];
  // IPv6 multicast groups this server joined, so stop() can leave them.
  std::vector<IPAddress> joinedGroups;
};

struct ResourceStoreImpl {
  struct Entry {
    String id;
    String body;
  };
  OThreadCoAPServerClass *server = nullptr;
  String basePath;
  size_t maxItems = 8;
  uint32_t nextId = 1;
  std::vector<Entry> items;
  OThreadCoAPResourceStore::ChangeCallback onChange = nullptr;
  void *changeContext = nullptr;
};

struct ClientBlockingContext {
  SemaphoreHandle_t done = nullptr;
  int code = OT_COAP_ERROR_NO_RESPONSE;
  IPAddress remoteIp = IPAddress(IPv6);
  uint8_t payload[OT_COAP_MAX_PAYLOAD] = {0};
  size_t payloadLen = 0;
  // True for requests to an IPv6 multicast group. OpenThread then invokes the
  // response handler once per responder (without finalizing the transaction) and
  // a final time on timeout, so this context must outlive several callbacks.
  bool multicast = false;
  // Heap-owned and shared between the blocking caller and the OpenThread response
  // handler. The three flags below are mutated only under the OT lock and form a
  // two-party hand-off: the context is freed exactly once, by whichever party is
  // last to release it. This makes both the wrapper timeout and multicast
  // multi-callbacks safe against use-after-free / double-free.
  bool signaled = false;        // caller has been woken with the first result
  bool callerReleased = false;  // blocking caller is finished with ctx
  bool otReleased = false;      // OpenThread made its final callback; no more are coming
};

// Registry of blocking contexts the caller has released while OpenThread still
// owns a pending transaction (see ClientBlockingContext's release flags). The
// context is normally freed by the handler's final callback. If the stack is torn
// down (OThread.end()) those handlers never fire; reapStalePending() reclaims them
// once the owning otInstance is gone or replaced. `sPendingInstance` records the
// instance the pending handlers belong to. All registry mutation during normal
// operation happens under the OT lock.
static otInstance *sPendingInstance = nullptr;
static std::vector<ClientBlockingContext *> sPendingClients;

// Tracks whether the single per-instance CoAP service (otCoapStart) is currently
// active. A CoAP client must have this service running before otCoapSendRequest()
// will accept a message; otherwise it returns OT_ERROR_INVALID_STATE, which the
// error mapper turns into OT_COAP_ERROR_INVALID_STATE (not OT_COAP_ERROR_NOT_ATTACHED).
// begin()/stop() and the client's lazy start share one UDP bind via a refcount so
// stopping a server does not tear down the socket while a plain client still needs it.
// Reset when the stack is torn down (OThread.end()) because the instance — and with it
// the CoAP service — is destroyed. Guarded by the OT lock at every touch.
static bool sCoapServiceActive = false;
static uint16_t sCoapServicePort = 0;
static uint16_t sCoapServiceRefCount = 0;
static bool sCoapClientServiceHeld = false;
static uint16_t sCoapClientInstanceCount = 0;
// True after plain server begin() acquires the shared service until successful stop().
// Stays true across failed stop() so begin() can re-bind without double incrementing refcount.
static bool sPlainServerServiceHeld = false;
// Plain server that owns otCoapSetDefaultHandler (ResourceStore, onNotFound, 404).
static ServerImpl *sPlainServerOwner = nullptr;
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
static ServerImpl *sSecureServerOwner = nullptr;
static OThreadCoAPSecureClient *sSecureClientOwner = nullptr;
// True after OThreadCoAPSecureServer::begin() until successful stop(); survives failed stop().
static bool sSecureServerServiceHeld = false;
#endif

static constexpr int kOtLockStopAttempts = 5;
static constexpr TickType_t kOtLockStopRetryMs = 50;

static void registerPendingClient(ClientBlockingContext *ctx) {
  sPendingInstance = OThread.getInstance();
  sPendingClients.push_back(ctx);
}

static void unregisterPendingClient(ClientBlockingContext *ctx) {
  for (size_t i = 0; i < sPendingClients.size(); ++i) {
    if (sPendingClients[i] == ctx) {
      sPendingClients.erase(sPendingClients.begin() + static_cast<long>(i));
      return;
    }
  }
}

// Releases a client context and its semaphore. Removes it from the pending
// registry first (no-op if it was never registered). MUST be called with the OT
// lock held, because it mutates the shared registry concurrently with the OT
// worker task. Contexts that were never registered (send-failure and stack-down
// paths) are freed inline instead, so they need no lock.
static void freeClientCtx(ClientBlockingContext *ctx) {
  unregisterPendingClient(ctx);
  if (ctx->done) {
    vSemaphoreDelete(ctx->done);
  }
  delete ctx;
}

// Defined after the secure registry; reclaims abandoned contexts of a dead stack.
static void reapStalePending(otInstance *current);

struct SecureClientState {
  OThreadCoAPSecureClient *owner = nullptr;
  std::vector<uint8_t> psk;
  String identity;
  String certPem;
  String keyPem;
  String caPem;
  bool verifyPeer = true;
  uint32_t timeoutMs = OT_COAP_DEFAULT_TIMEOUT_MS;
  uint32_t connectTimeoutMs = OT_COAP_DEFAULT_TIMEOUT_MS;
  bool confirmable = true;
  uint16_t contentFormat = OT_COAP_FORMAT_TEXT;
  bool connected = false;
  bool active = false;
  IPAddress peer = IPAddress(IPv6);
  uint16_t peerPort = OT_COAP_SECURE_DEFAULT_PORT;
  int lastCode = OT_COAP_ERROR_NOT_CONNECTED;
  uint8_t payload[OT_COAP_MAX_PAYLOAD] = {0};
  size_t payloadLen = 0;
  OThreadCoAPSecureClient::ConnectCallback connectCb = nullptr;
  void *connectCtx = nullptr;
  SemaphoreHandle_t connectSignal = nullptr;
};

static std::vector<SecureClientState> sSecureClients;
static std::vector<std::pair<OThreadCoAPResourceStore *, ResourceStoreImpl *>> sResourceStores;

static SemaphoreHandle_t sServeSlotMutex = nullptr;
static SemaphoreHandle_t sResourceStoreMutex = nullptr;

static void ensureServeSlotMutex() {
  if (!sServeSlotMutex) {
    sServeSlotMutex = xSemaphoreCreateMutex();
  }
}

static void ensureResourceStoreMutex() {
  if (!sResourceStoreMutex) {
    sResourceStoreMutex = xSemaphoreCreateRecursiveMutex();
  }
}

struct ServeSlotLock {
  bool held;
  ServeSlotLock() : held(sServeSlotMutex != nullptr && xSemaphoreTake(sServeSlotMutex, portMAX_DELAY) == pdTRUE) {}
  ~ServeSlotLock() {
    if (held) {
      xSemaphoreGive(sServeSlotMutex);
    }
  }
  explicit operator bool() const {
    return held;
  }
};

struct ResourceStoreLock {
  bool held;
  ResourceStoreLock() {
    ensureResourceStoreMutex();
    held = xSemaphoreTakeRecursive(sResourceStoreMutex, portMAX_DELAY) == pdTRUE;
  }
  ~ResourceStoreLock() {
    if (held) {
      xSemaphoreGiveRecursive(sResourceStoreMutex);
    }
  }
  explicit operator bool() const {
    return held;
  }
};

static String sanitizePath(const char *path) {
  String out = path ? path : "";
  while (out.length() && out[0] == '/') {
    out.remove(0, 1);
  }
  while (out.endsWith("/")) {
    out.remove(out.length() - 1);
  }
  return out;
}

static String extractUriPath(const otMessage *message) {
  if (!message) {
    return String();
  }
  otCoapOptionIterator it;
  if (otCoapOptionIteratorInit(&it, message) != OT_ERROR_NONE) {
    return String();
  }
  const otCoapOption *opt = otCoapOptionIteratorGetFirstOptionMatching(&it, OT_COAP_OPTION_URI_PATH);
  String path;
  bool first = true;
  while (opt) {
    std::vector<uint8_t> segment(opt->mLength + 1, 0);
    if (opt->mLength > 0 && otCoapOptionIteratorGetOptionValue(&it, segment.data()) != OT_ERROR_NONE) {
      break;
    }
    if (!first) {
      path += "/";
    }
    path += reinterpret_cast<const char *>(segment.data());
    first = false;
    opt = otCoapOptionIteratorGetNextOptionMatching(&it, OT_COAP_OPTION_URI_PATH);
  }
  return path;
}

// Append CoAP path options (URI-Path, Location-Path, …) as one option value per
// path segment (RFC 7252 §5.10.7).
static otError appendCoapPathOption(otMessage *message, uint16_t optionNumber, const char *path) {
  if (!message || !path) {
    return OT_ERROR_INVALID_ARGS;
  }
  otError err = OT_ERROR_NONE;
  const char *cur = path;
  while (*cur == '/') {
    cur++;
  }
  while (*cur && err == OT_ERROR_NONE) {
    const char *end = strchr(cur, '/');
    if (end) {
      err = otCoapMessageAppendOption(message, optionNumber, static_cast<uint16_t>(end - cur), cur);
      cur = end + 1;
      while (*cur == '/') {
        cur++;
      }
    } else {
      err = otCoapMessageAppendOption(message, optionNumber, static_cast<uint16_t>(strlen(cur)), cur);
      break;
    }
  }
  return err;
}

static size_t readPayloadFromMessage(const otMessage *message, uint8_t *buffer, size_t maxLen) {
  if (!message || !buffer || maxLen == 0) {
    return 0;
  }
  uint16_t offset = otMessageGetOffset(message);
  uint16_t length = otMessageGetLength(message);
  if (length <= offset) {
    return 0;
  }
  uint16_t payloadLen = length - offset;
  uint16_t toCopy = static_cast<uint16_t>(std::min<size_t>(payloadLen, maxLen));
  return otMessageRead(message, offset, buffer, toCopy);
}

static void fillRequestState(RequestState &state, otMessage *message, const otMessageInfo *info, const String &pathOverride = String()) {
  state.path = pathOverride.length() ? pathOverride : extractUriPath(message);
  state.method = methodFromOt(otCoapMessageGetCode(message));
  if (info) {
    state.remoteIp = otToIp(info->mPeerAddr);
    state.remotePort = info->mPeerPort;
    // mSockAddr is the address the datagram was sent to; multicast there means
    // this request reached us via a group subscription, not a unicast.
    state.multicast = isMulticastIp(otToIp(info->mSockAddr));
  }
  state.confirmable = (otCoapMessageGetType(message) == OT_COAP_TYPE_CONFIRMABLE);
  state.payloadLen = readPayloadFromMessage(message, state.payload, sizeof(state.payload));
}

static bool sendResponseMessage(ResponseState *state) {
  if (!state || !state->instance || !state->request || state->sent) {
    return false;
  }
  otMessage *response = otCoapNewMessage(state->instance, nullptr);
  if (!response) {
    return false;
  }

  otCoapType responseType = (otCoapMessageGetType(state->request) == OT_COAP_TYPE_CONFIRMABLE) ? OT_COAP_TYPE_ACKNOWLEDGMENT : OT_COAP_TYPE_NON_CONFIRMABLE;
  otError err = otCoapMessageInitResponse(response, state->request, responseType, arduinoCodeToOt(state->code));
  // CoAP options MUST be appended in ascending option-number order (RFC 7252
  // §3.1): Location-Path (8) < Content-Format (12) < Max-Age (14). Appending them
  // out of order makes OpenThread reject the message, so the response is silently
  // dropped — which previously broke POST replies (their Location-Path was added
  // after Content-Format) and triggered duplicate handling via CON retransmits.
  if (err == OT_ERROR_NONE && state->hasLocation) {
    err = appendCoapPathOption(response, OT_COAP_OPTION_LOCATION_PATH, state->locationPath.c_str());
  }
  if (err == OT_ERROR_NONE) {
    err = otCoapMessageAppendContentFormatOption(response, static_cast<otCoapOptionContentFormat>(state->contentFormat));
  }
  if (err == OT_ERROR_NONE && state->hasMaxAge) {
    err = otCoapMessageAppendMaxAgeOption(response, state->maxAge);
  }
  if (err == OT_ERROR_NONE && state->payloadLen > 0) {
    err = otCoapMessageSetPayloadMarker(response);
  }
  if (err == OT_ERROR_NONE && state->payloadLen > 0) {
    err = otMessageAppend(response, state->payload, static_cast<uint16_t>(state->payloadLen));
  }
  if (err == OT_ERROR_NONE) {
    if (state->secure) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
      err = otCoapSecureSendResponse(state->instance, response, &state->messageInfo);
#else
      err = OT_ERROR_INVALID_STATE;
#endif
    } else {
      err = otCoapSendResponse(state->instance, response, &state->messageInfo);
    }
  }
  if (err != OT_ERROR_NONE) {
    otMessageFree(response);
    log_w("OThreadCoAP: response send failed (otError=%d)", static_cast<int>(err));
    return false;
  }
  state->sent = true;
  return true;
}

static bool sendCodeOnly(otInstance *instance, bool secure, otMessage *request, const otMessageInfo *info, int code) {
  ResponseState response;
  response.instance = instance;
  response.request = request;
  if (info) {
    response.messageInfo = *info;
  }
  response.code = code;
  response.secure = secure;
  response.payloadLen = 0;
  return sendResponseMessage(&response);
}

static String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); ++i) {
    char c = in[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:   out += c; break;
    }
  }
  return out;
}

}  // namespace

// storeHandleRequest is defined later at file scope (internal linkage). Declare it
// here, OUTSIDE the anonymous namespace, so the dispatch callbacks below and the
// definition all refer to the same ::storeHandleRequest entity at link time.
static bool storeHandleRequest(OThreadCoAPResourceStore *store, RequestState &request, OThreadCoAPResponse &response);

// resourceRequestCb/defaultRequestCb run as OpenThread C callbacks and build the
// OThreadCoAPRequest/OThreadCoAPResponse views, so they live in a named namespace
// to be friends of those classes (declared in OThreadCoAP.h). They can still see
// the anonymous-namespace helpers above (same translation unit).
namespace othread_coap_detail {

void resourceRequestCb(void *context, otMessage *message, const otMessageInfo *messageInfo) {
  ServerResourceSlot *slot = static_cast<ServerResourceSlot *>(context);
  if (!slot || !message || !messageInfo) {
    log_w("OThreadCoAP: resource callback ignored (invalid context or message)");
    return;
  }

  // slot context is embedded in the owning server impl; identify owner by resource context lifetime.
  // mContext always points to slot, so we only need the active OpenThread instance + method check.
  otInstance *instance = OThread.getInstance();
  if (!instance) {
    log_w("OThreadCoAP: resource callback ignored (OpenThread instance unavailable)");
    return;
  }

  // Recover the owning impl by searching active server objects attached to this slot isn't available.
  // For phase 1 we rely on direct callback invocation through slot handler only.
  RequestState requestState;
  fillRequestState(requestState, message, messageInfo, slot->path);
  uint8_t methodBit = methodMaskFromMethod(requestState.method);

  bool secure = slot->owner ? slot->owner->secure : false;

  // Per RFC 7252 §8.2, never answer a multicast request with an error (every group
  // member would reply to the same datagram). OpenThread core suppresses these for
  // multicast too (see coap.cpp ProcessReceivedRequest); mirror that here.
  if (!methodBit || (slot->methodMask & methodBit) == 0) {
    if (!requestState.multicast) {
      (void)sendCodeOnly(instance, secure, message, messageInfo, OT_COAP_RESP_METHOD_NA);
    } else {
      logMulticastWarn("on()", requestState.path.c_str(), requestState.confirmable, "method not allowed on resource");
    }
    return;
  }
  if (!slot->handler) {
    if (!requestState.multicast) {
      (void)sendCodeOnly(instance, secure, message, messageInfo, OT_COAP_RESP_INTERNAL_ERROR);
    } else {
      logMulticastWarn("on()", requestState.path.c_str(), requestState.confirmable, "resource has no handler");
    }
    return;
  }

  ResponseState responseState;
  responseState.instance = instance;
  responseState.request = message;
  responseState.messageInfo = *messageInfo;
  responseState.secure = secure;
  responseState.code = OT_COAP_RESP_OK;

  OThreadCoAPRequest request(&requestState);
  OThreadCoAPResponse response(&responseState);
  slot->handler(request, response, slot->handlerContext);

  // Auto-respond only to unicast CON or GET requests. Never auto-answer a
  // multicast request: per RFC 7252 §8.2 many nodes sharing the group would
  // otherwise reply to the same datagram and cause a response storm. A handler
  // that explicitly calls response.send() still overrides this.
  if (!responseState.sent && requestState.multicast) {
    logMulticastExpected("on()", requestState.path.c_str(), requestState.confirmable, "handler did not reply");
  } else if (!responseState.sent && (requestState.confirmable || requestState.method == OT_COAP_REQ_GET)) {
    (void)sendResponseMessage(&responseState);
  }
}

void defaultRequestCb(void *context, otMessage *message, const otMessageInfo *messageInfo) {
  ServerImpl *impl = static_cast<ServerImpl *>(context);
  if (!impl || !message || !messageInfo) {
    log_w("OThreadCoAP: default handler ignored (invalid context or message)");
    return;
  }
  RequestState requestState;
  fillRequestState(requestState, message, messageInfo);

  ResponseState responseState;
  responseState.instance = OThread.getInstance();
  responseState.request = message;
  responseState.messageInfo = *messageInfo;
  responseState.secure = impl->secure;
  OThreadCoAPResponse response(&responseState);

  if (impl->resourceStore) {
    if (storeHandleRequest(impl->resourceStore, requestState, response)) {
      return;
    }
  }
  if (impl->notFoundHandler) {
    OThreadCoAPRequest request(&requestState);
    impl->notFoundHandler(request, response, impl->notFoundContext);
    // Auto-send 4.04 only for unicast: a multicast miss must stay silent (§8.2).
    if (!responseState.sent && !requestState.multicast) {
      responseState.code = OT_COAP_RESP_NOT_FOUND;
      response.send();
    } else if (!responseState.sent && requestState.multicast) {
      logMulticastExpected("onNotFound()", requestState.path.c_str(), requestState.confirmable, "path not handled (onNotFound did not reply)");
    }
    return;
  }
  // Unmatched path: reply 4.04 to unicast only. OpenThread core suppresses
  // SendNotFound for multicast (coap.cpp); replicate that since installing this
  // default handler bypasses the core's own multicast guard.
  if (!requestState.multicast) {
    (void)sendCodeOnly(responseState.instance, responseState.secure, message, messageInfo, OT_COAP_RESP_NOT_FOUND);
  } else {
    logMulticastWarn("defaultRequestCb", requestState.path.c_str(), requestState.confirmable, "unknown path");
  }
}

}  // namespace othread_coap_detail

namespace {

static ServerResourceSlot *findSlot(ServerImpl *impl, const String &path) {
  if (!impl) {
    return nullptr;
  }
  for (auto &slot : impl->slots) {
    if (slot.inUse && slot.path == path) {
      return &slot;
    }
  }
  return nullptr;
}

static ServerResourceSlot *allocSlot(ServerImpl *impl) {
  if (!impl) {
    return nullptr;
  }
  for (auto &slot : impl->slots) {
    if (!slot.inUse) {
      slot.inUse = true;
      return &slot;
    }
  }
  return nullptr;
}

static otError addSlotToOt(ServerResourceSlot *slot, ServerImpl *impl, otInstance *inst);

static void releasePlainCoapService(otInstance *instance);

static bool attachRegisteredResources(ServerImpl *impl, otInstance *inst, const char *logPrefix) {
  if (!impl || !inst) {
    return false;
  }
  for (auto &slot : impl->slots) {
    if (!slot.inUse || !slot.handler || slot.path.isEmpty()) {
      continue;
    }
    otError err = addSlotToOt(&slot, impl, inst);
    if (err != OT_ERROR_NONE) {
      log_w("%s: attach resource '%s' failed (otError=%d)", logPrefix, slot.path.c_str(), static_cast<int>(err));
      return false;
    }
  }
  return true;
}

static void detachRegisteredResources(ServerImpl *impl, otInstance *inst) {
  for (auto &slot : impl->slots) {
    if (!slot.inUse || !slot.registered) {
      continue;
    }
    if (impl->secure) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
      otCoapSecureRemoveResource(inst, &slot.resource);
#endif
    } else {
      otCoapRemoveResource(inst, &slot.resource);
    }
    slot.registered = false;
  }
}

// Remove every in-use resource from OpenThread (ignores slot.registered). Used on begin()
// to recover from a failed stop() that reset wrapper state without OT detach.
static void forceUnregisterAllResourcesFromOt(ServerImpl *impl, otInstance *inst) {
  if (!impl || !inst) {
    return;
  }
  for (auto &slot : impl->slots) {
    if (!slot.inUse || !slot.handler || slot.path.isEmpty()) {
      continue;
    }
    slot.resource.mUriPath = slot.path.c_str();
    slot.resource.mHandler = resourceRequestCb;
    slot.resource.mContext = &slot;
    slot.resource.mNext = nullptr;
    if (impl->secure) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
      otCoapSecureRemoveResource(inst, &slot.resource);
#endif
    } else {
      otCoapRemoveResource(inst, &slot.resource);
    }
    slot.registered = false;
  }
}

// Restart or validate the plain UDP service for the singleton server (resync / recovery).
static otError ensurePlainCoapServiceForServer(ServerImpl *impl, otInstance *inst) {
  if (!impl || !inst) {
    return OT_ERROR_INVALID_ARGS;
  }
  if (sCoapServiceActive) {
    return sCoapServicePort == impl->port ? OT_ERROR_NONE : OT_ERROR_INVALID_STATE;
  }
  otError err = otCoapStart(inst, impl->port);
  if (err != OT_ERROR_NONE) {
    return err;
  }
  sCoapServiceActive = true;
  sCoapServicePort = impl->port;
  if (sPlainServerServiceHeld) {
    if (sCoapServiceRefCount == 0) {
      sCoapServiceRefCount = 1;
    }
  } else {
    if (sCoapServiceRefCount >= UINT16_MAX) {
      log_w("OThreadCoAP: plain CoAP service refcount cap reached");
      otCoapStop(inst);
      sCoapServiceActive = false;
      sCoapServicePort = 0;
      return OT_ERROR_NO_BUFS;
    }
    sCoapServiceRefCount++;
    sPlainServerServiceHeld = true;
  }
  return OT_ERROR_NONE;
}

static bool resyncPlainServerOnOt(ServerImpl *impl, otInstance *inst) {
  otError err = ensurePlainCoapServiceForServer(impl, inst);
  if (err != OT_ERROR_NONE) {
    log_w("OThreadCoAPServer: resync failed (otError=%d)", static_cast<int>(err));
    return false;
  }
  forceUnregisterAllResourcesFromOt(impl, inst);
  if (!attachRegisteredResources(impl, inst, "OThreadCoAPServer")) {
    detachRegisteredResources(impl, inst);
    return false;
  }
  otCoapSetDefaultHandler(inst, defaultRequestCb, impl);
  sPlainServerOwner = impl;
  return true;
}

#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
static bool ensureSecureCoapServiceForServer(ServerImpl *impl, otInstance *inst) {
  if (!impl || !inst) {
    return false;
  }
  if (sSecureServerServiceHeld && sSecureServerOwner == nullptr) {
    otCoapSecureStop(inst);
    sSecureServerServiceHeld = false;
  }
  otError err = OT_ERROR_NONE;
  if (impl->maxConnectionAttempts > 0) {
    err = otCoapSecureStartWithMaxConnAttempts(inst, impl->port, impl->maxConnectionAttempts, nullptr, nullptr);
  } else {
    err = otCoapSecureStart(inst, impl->port);
  }
  if (err != OT_ERROR_NONE && err != OT_ERROR_ALREADY) {
    log_w("OThreadCoAPSecureServer: ensure service failed (otError=%d)", static_cast<int>(err));
    return false;
  }
  sSecureServerServiceHeld = true;
  return true;
}

static bool resyncSecureServerOnOt(ServerImpl *impl, otInstance *inst) {
  if (!ensureSecureCoapServiceForServer(impl, inst)) {
    return false;
  }
  forceUnregisterAllResourcesFromOt(impl, inst);
  if (!attachRegisteredResources(impl, inst, "OThreadCoAPSecureServer")) {
    detachRegisteredResources(impl, inst);
    return false;
  }
  otCoapSecureSetDefaultHandler(inst, defaultRequestCb, impl);
  sSecureServerOwner = impl;
  return true;
}
#endif

static void markPlainServerInactiveAfterResyncFailure(ServerImpl *impl, otInstance *inst) {
  if (!impl) {
    return;
  }
  impl->running = false;
  if (inst) {
    forceUnregisterAllResourcesFromOt(impl, inst);
    if (sPlainServerOwner == impl) {
      otCoapSetDefaultHandler(inst, nullptr, nullptr);
      sPlainServerOwner = nullptr;
    }
    if (sPlainServerServiceHeld) {
      releasePlainCoapService(inst);
      sPlainServerServiceHeld = false;
    }
  } else if (sPlainServerOwner == impl) {
    sPlainServerOwner = nullptr;
  }
}

#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
static void markSecureServerInactiveAfterResyncFailure(ServerImpl *impl, otInstance *inst) {
  if (!impl) {
    return;
  }
  impl->running = false;
  if (inst) {
    forceUnregisterAllResourcesFromOt(impl, inst);
    if (sSecureServerOwner == impl) {
      otCoapSecureSetDefaultHandler(inst, nullptr, nullptr);
      sSecureServerOwner = nullptr;
    }
    if (sSecureServerServiceHeld) {
      otCoapSecureStop(inst);
      sSecureServerServiceHeld = false;
    }
  } else if (sSecureServerOwner == impl) {
    sSecureServerOwner = nullptr;
  }
}
#endif

static void applyServerSlotRegistration(
  ServerResourceSlot *slot, ServerImpl *impl, const String &normalized, uint8_t methodMask, OThreadCoAPHandler handler, void *context
) {
  slot->path = normalized;
  slot->owner = impl;
  slot->methodMask = methodMask;
  slot->handler = handler;
  slot->handlerContext = context;
  slot->resource.mUriPath = slot->path.c_str();
  slot->resource.mHandler = resourceRequestCb;
  slot->resource.mContext = slot;
  slot->resource.mNext = nullptr;
}

static void clearServerSlot(ServerResourceSlot *slot) {
  if (!slot) {
    return;
  }
  slot->inUse = false;
  slot->registered = false;
  slot->owner = nullptr;
  slot->path = String();
  slot->methodMask = 0;
  slot->handler = nullptr;
  slot->handlerContext = nullptr;
  slot->serveValuePtr = nullptr;
  slot->serveSnapshot = String();
  slot->resource = {};
}

static otError removeSlotFromOt(ServerResourceSlot *slot, ServerImpl *impl, otInstance *inst) {
  if (!slot || !impl || !inst || !slot->registered) {
    return OT_ERROR_NONE;
  }
  if (impl->secure) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
    otCoapSecureRemoveResource(inst, &slot->resource);
#endif
  } else {
    otCoapRemoveResource(inst, &slot->resource);
  }
  slot->registered = false;
  return OT_ERROR_NONE;
}

// Best-effort OT detach (ignores slot.registered). Used when rollback re-add fails.
static void ensureSlotRemovedFromOt(ServerResourceSlot *slot, ServerImpl *impl, otInstance *inst) {
  if (!slot || !impl || !inst || slot->path.isEmpty()) {
    return;
  }
  slot->resource.mUriPath = slot->path.c_str();
  slot->resource.mHandler = resourceRequestCb;
  slot->resource.mContext = slot;
  slot->resource.mNext = nullptr;
  if (impl->secure) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
    otCoapSecureRemoveResource(inst, &slot->resource);
#endif
  } else {
    otCoapRemoveResource(inst, &slot->resource);
  }
  slot->registered = false;
}

static otError addSlotToOt(ServerResourceSlot *slot, ServerImpl *impl, otInstance *inst) {
  if (!slot || !impl || !inst || !slot->handler) {
    return OT_ERROR_INVALID_ARGS;
  }
  slot->resource.mUriPath = slot->path.c_str();
  slot->resource.mHandler = resourceRequestCb;
  slot->resource.mContext = slot;
  slot->resource.mNext = nullptr;
  if (impl->secure) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
    otCoapSecureAddResource(inst, &slot->resource);
#endif
  } else {
    otCoapAddResource(inst, &slot->resource);
  }
  slot->registered = true;
  return OT_ERROR_NONE;
}

#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
static bool isSecureServerStackActive() {
  return (sSecureServerOwner != nullptr && sSecureServerOwner->running) || sSecureServerServiceHeld;
}

static void resetSecureClientSessionFlags() {
  for (auto &entry : sSecureClients) {
    entry.connected = false;
    entry.active = false;
  }
}
#endif

// Shared by plain and secure singleton servers. Pass handler=nullptr to unregister a path
// (WebServer uses removeRoute(); CoAP uses the same on() entry point with a null handler).
static bool registerServerPath(ServerImpl *impl, const char *logPrefix, const char *path, uint8_t methodMask, OThreadCoAPHandler handler, void *context) {
  if (!impl) {
    log_w("%s: on() failed (invalid server object)", logPrefix);
    return false;
  }
  String normalized = sanitizePath(path);
  if (!normalized.length()) {
    log_w("%s: on() failed (empty or invalid path)", logPrefix);
    return false;
  }
  ServerResourceSlot *slot = findSlot(impl, normalized);
  if (handler == nullptr) {
    if (!slot) {
      return true;
    }
    otInstance *inst = OThread.getInstance();
    if (impl->running && inst) {
      for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
        OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
        if (!lock) {
          continue;
        }
        otError err = removeSlotFromOt(slot, impl, inst);
        if (err != OT_ERROR_NONE) {
          log_w("%s: on() failed to unregister '%s' (otError=%d)", logPrefix, normalized.c_str(), static_cast<int>(err));
          return false;
        }
        clearServerSlot(slot);
        return true;
      }
      log_w("%s: on() failed to unregister '%s' (could not acquire OpenThread lock; OT registration unchanged — retry)", logPrefix, normalized.c_str());
      return false;
    }
    clearServerSlot(slot);
    return true;
  }
  const bool isNewSlot = (slot == nullptr);
  if (!slot) {
    slot = allocSlot(impl);
  }
  if (!slot) {
    log_e("%s: on() failed (resource table full, max %d)", logPrefix, OT_COAP_MAX_RESOURCES);
    return false;
  }
  otInstance *inst = OThread.getInstance();
  if (impl->running && inst) {
    for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
      OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
      if (!lock) {
        continue;
      }
      const bool wasRegistered = slot->registered;
      const bool hadPriorHandler = slot->inUse && slot->handler != nullptr;
      String prevPath = slot->path;
      uint8_t prevMask = slot->methodMask;
      OThreadCoAPHandler prevHandler = slot->handler;
      void *prevCtx = slot->handlerContext;
      if (wasRegistered) {
        otError remErr = removeSlotFromOt(slot, impl, inst);
        if (remErr != OT_ERROR_NONE) {
          log_w("%s: on() failed for '%s' (otCoapRemoveResource otError=%d)", logPrefix, normalized.c_str(), static_cast<int>(remErr));
          return false;
        }
      }
      applyServerSlotRegistration(slot, impl, normalized, methodMask, handler, context);
      otError addErr = addSlotToOt(slot, impl, inst);
      if (addErr != OT_ERROR_NONE) {
        log_w("%s: on() failed for '%s' (otCoapAddResource otError=%d)", logPrefix, normalized.c_str(), static_cast<int>(addErr));
        if (wasRegistered && hadPriorHandler) {
          applyServerSlotRegistration(slot, impl, prevPath, prevMask, prevHandler, prevCtx);
          otError restoreErr = addSlotToOt(slot, impl, inst);
          if (restoreErr != OT_ERROR_NONE) {
            log_e(
              "%s: on() rollback re-add failed for '%s' (otError=%d); re-register with on() and call begin()", logPrefix, prevPath.c_str(),
              static_cast<int>(restoreErr)
            );
            ensureSlotRemovedFromOt(slot, impl, inst);
            clearServerSlot(slot);
          }
        } else if (hadPriorHandler) {
          applyServerSlotRegistration(slot, impl, prevPath, prevMask, prevHandler, prevCtx);
          slot->registered = false;
        } else {
          clearServerSlot(slot);
        }
        return false;
      }
      return true;
    }
    log_w(
      "%s: on() failed for '%s' (could not acquire OpenThread lock%s)", logPrefix, normalized.c_str(),
      isNewSlot ? "" : "; existing registration unchanged — retry"
    );
    if (isNewSlot) {
      clearServerSlot(slot);
    }
    return false;
  }
  applyServerSlotRegistration(slot, impl, normalized, methodMask, handler, context);
  return true;
}

// Ensures the per-instance plain CoAP UDP service is running on @p port and records
// one logical holder (server begin or first client lazy-start). MUST be called with
// the OT lock held. Returns OT_ERROR_INVALID_STATE on port mismatch.
static otError acquirePlainCoapService(otInstance *instance, uint16_t port) {
  if (sCoapServiceActive && sCoapServicePort != port) {
    return OT_ERROR_INVALID_STATE;
  }
  if (!sCoapServiceActive) {
    otError err = otCoapStart(instance, port);
    if (err != OT_ERROR_NONE) {
      return err;
    }
    sCoapServiceActive = true;
    sCoapServicePort = port;
  }
  if (sCoapServiceRefCount >= UINT16_MAX) {
    log_w("OThreadCoAP: plain CoAP service refcount cap reached");
    return OT_ERROR_NO_BUFS;
  }
  sCoapServiceRefCount++;
  return OT_ERROR_NONE;
}

// Drops one plain CoAP service holder; stops the UDP socket when the last holder is
// released. MUST be called with the OT lock held.
static void releasePlainCoapService(otInstance *instance) {
  if (sCoapServiceRefCount == 0) {
    return;
  }
  sCoapServiceRefCount--;
  if (sCoapServiceRefCount == 0 && sCoapServiceActive && instance) {
    otCoapStop(instance);
    sCoapServiceActive = false;
    sCoapServicePort = 0;
  }
}

static otError ensureCoapServiceStarted(otInstance *instance) {
  if (sCoapClientServiceHeld && sCoapServiceActive) {
    return OT_ERROR_NONE;
  }
  otError err = acquirePlainCoapService(instance, OT_COAP_DEFAULT_PORT);
  if (err == OT_ERROR_NONE) {
    sCoapClientServiceHeld = true;
  }
  return err;
}

// Derive otCoapTxParameters so CON retransmission ends within about timeoutMs.
// Uses the RFC 7252 span formula (see coap.h) with ACK_RANDOM_FACTOR 1.5.
static void fillCoapTxParameters(uint32_t timeoutMs, bool useRfcDefaults, otCoapTxParameters &tx) {
  tx.mAckRandomFactorNumerator = 3;
  tx.mAckRandomFactorDenominator = 2;

  if (useRfcDefaults || timeoutMs == 0) {
    tx.mAckTimeout = 2000;
    tx.mMaxRetransmit = 4;
    return;
  }

  // Full RFC default exchange span is ~93 s; keep stack defaults when allowed.
  if (timeoutMs >= 93000) {
    tx.mAckTimeout = 2000;
    tx.mMaxRetransmit = 4;
    return;
  }

  for (int n = 4; n >= 0; --n) {
    const uint32_t denom = (1u << (n + 1)) - 1u;
    const uint64_t ack64 = ((uint64_t)timeoutMs * 2u) / (3u * (uint64_t)denom);
    if (ack64 >= OT_COAP_MIN_ACK_TIMEOUT_MS && ack64 <= 0xFFFFFFFFu) {
      tx.mAckTimeout = static_cast<uint32_t>(ack64);
      tx.mMaxRetransmit = static_cast<uint8_t>(n);
      return;
    }
  }

  // Shorter than OpenThread can honor with valid params — single retry at min ACK.
  tx.mAckTimeout = OT_COAP_MIN_ACK_TIMEOUT_MS;
  tx.mMaxRetransmit = 0;
}

static otError sendClientRequest(
  otInstance *instance, bool secure, bool confirmable, const IPAddress &host, uint16_t port, const char *path, int methodCode, const uint8_t *payload,
  size_t payloadLen, uint16_t contentFormat, otCoapResponseHandler responseHandler, void *context, uint32_t timeoutMs, bool useRfcRetransmit
) {
  // Opportunistically reclaim abandoned contexts left over from a previous stack
  // lifecycle before issuing a new request (runs under the OT lock held by callers).
  reapStalePending(instance);
  // A plain (non-secure) client needs the CoAP service open before sending. The
  // secure path uses otCoapSecureSendRequest after its own connect()/start, so it
  // is left untouched here.
  if (!secure) {
    otError startErr = ensureCoapServiceStarted(instance);
    if (startErr != OT_ERROR_NONE) {
      return startErr;
    }
  }
  otMessage *message = otCoapNewMessage(instance, nullptr);
  if (!message) {
    return OT_ERROR_NO_BUFS;
  }

  otCoapType reqType = confirmable ? OT_COAP_TYPE_CONFIRMABLE : OT_COAP_TYPE_NON_CONFIRMABLE;
  otError err = OT_ERROR_NONE;
  otCoapMessageInit(message, reqType, reqMethodToOt(methodCode));
  otCoapMessageGenerateToken(message, 2);
  err = otCoapMessageAppendUriPathOptions(message, path ? path : "");
  if (err == OT_ERROR_NONE && payload && payloadLen > 0) {
    err = otCoapMessageAppendContentFormatOption(message, static_cast<otCoapOptionContentFormat>(contentFormat));
  }
  if (err == OT_ERROR_NONE && payload && payloadLen > 0) {
    err = otCoapMessageSetPayloadMarker(message);
  }
  if (err == OT_ERROR_NONE && payload && payloadLen > 0) {
    err = otMessageAppend(message, payload, static_cast<uint16_t>(payloadLen));
  }

  otMessageInfo msgInfo;
  memset(&msgInfo, 0, sizeof(msgInfo));
  ipToOt(host, msgInfo.mPeerAddr);
  msgInfo.mPeerPort = port;

  if (err == OT_ERROR_NONE) {
    if (secure) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
      err = otCoapSecureSendRequest(instance, message, responseHandler, context);
#else
      err = OT_ERROR_INVALID_STATE;
#endif
    } else {
      otCoapTxParameters tx = {};
      if (confirmable) {
        fillCoapTxParameters(timeoutMs, useRfcRetransmit, tx);
      } else {
        fillCoapTxParameters(timeoutMs, true, tx);
      }
      err = otCoapSendRequestWithParameters(instance, message, &msgInfo, responseHandler, context, &tx);
    }
  }
  if (err != OT_ERROR_NONE) {
    otMessageFree(message);
  }
  return err;
}

// Runs on the OpenThread task (under the OT lock). `context` is a heap
// ClientBlockingContext shared with the blocking caller. For a multicast request
// OpenThread calls this once per responder (with OT_ERROR_NONE, transaction NOT
// finalized) and a final time on timeout (with an error); unicast requests are
// always finalized on the single callback. The context is freed here only when
// this is the final callback AND the caller has already released its share.
static void clientResponseCb(void *context, otMessage *message, const otMessageInfo *messageInfo, otError result) {
  ClientBlockingContext *ctx = static_cast<ClientBlockingContext *>(context);
  if (!ctx) {
    return;
  }

  // Unicast transactions are finalized on this callback. A multicast transaction
  // is only finalized on its timeout callback, which carries a non-NONE error.
  const bool isFinal = !ctx->multicast || (result != OT_ERROR_NONE);

  // Capture the first response (or the terminal error) and wake the caller once.
  if (!ctx->signaled) {
    if (result == OT_ERROR_NONE && message) {
      ctx->code = otCodeToArduino(otCoapMessageGetCode(message));
      if (messageInfo) {
        ctx->remoteIp = otToIp(messageInfo->mPeerAddr);
      }
      ctx->payloadLen = readPayloadFromMessage(message, ctx->payload, sizeof(ctx->payload));
      ctx->signaled = true;
    } else if (isFinal) {
      ctx->code = errorFromOt(result, OT_COAP_ERROR_NO_RESPONSE);
      ctx->payloadLen = 0;
      ctx->signaled = true;
    }
    if (ctx->signaled && ctx->done) {
      xSemaphoreGive(ctx->done);
    }
  }

  if (isFinal) {
    ctx->otReleased = true;
    if (ctx->callerReleased) {
      // Caller already copied its result and left; we are last → free.
      freeClientCtx(ctx);
    }
  }
}

// Shares ownership of the heap `ctx` with the OpenThread response handler. The
// caller waits for the first response (or its own timeout), copies the result out,
// then releases its share. Whichever party is last — this function, or the
// handler's final callback — frees `ctx`. This makes both the wrapper timeout and
// multicast multi-callbacks safe: neither side ever frees memory the other uses.
static int waitBlockingClient(
  ClientBlockingContext *ctx, otError sendErr, uint32_t timeoutMs, bool useRfcRetransmit, uint8_t *outPayload, size_t *outLen, IPAddress *outIp
) {
  if (outLen) {
    *outLen = 0;
  }
  if (sendErr != OT_ERROR_NONE) {
    // OpenThread registered no transaction for this ctx, and it was never added to
    // the pending registry, so reclaim it directly. Do NOT call freeClientCtx()
    // here: that touches the shared registry vector and we hold no OT lock, which
    // would race the worker task freeing some other pending context.
    if (ctx->done) {
      vSemaphoreDelete(ctx->done);
    }
    delete ctx;
    return errorFromOt(sendErr);
  }

  const uint32_t waitMs = timeoutMs + (useRfcRetransmit ? 1000u : 500u);
  bool gotResponse = (xSemaphoreTake(ctx->done, pdMS_TO_TICKS(waitMs)) == pdTRUE);

  // Synchronize with the OT task: the handler runs under this same lock, so once
  // we hold it the handler is not running and ctx's flags are stable.
  OtLock lock;
  if (!gotResponse) {
    gotResponse = (xSemaphoreTake(ctx->done, 0) == pdTRUE);
  }

  int code = gotResponse ? ctx->code : OT_COAP_ERROR_TIMEOUT;
  if (gotResponse && code >= 0 && outPayload && outLen) {
    size_t n = std::min(ctx->payloadLen, static_cast<size_t>(OT_COAP_MAX_PAYLOAD));
    memcpy(outPayload, ctx->payload, n);
    *outLen = n;
    if (outIp) {
      *outIp = ctx->remoteIp;
    }
  }

  if (!lock) {
    // OT lock unavailable => the stack is down, the worker task is gone and no
    // further callback can fire, so the caller solely owns ctx. ctx is not in the
    // registry yet (only the branch below registers it), so free it directly.
    if (ctx->done) {
      vSemaphoreDelete(ctx->done);
    }
    delete ctx;
    return code;
  }

  ctx->callerReleased = true;
  if (ctx->otReleased) {
    // OpenThread already made its final callback; we are last → free.
    freeClientCtx(ctx);
  } else {
    // OpenThread still owns a pending transaction (unicast response not yet
    // delivered, or a multicast still collecting responses). It will free ctx on
    // its final callback; register it so teardown can reclaim it otherwise.
    registerPendingClient(ctx);
  }
  return code;
}

#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
static SecureClientState *getSecureState(const OThreadCoAPSecureClient *owner, bool createIfMissing) {
  for (auto &entry : sSecureClients) {
    if (entry.owner == owner) {
      return &entry;
    }
  }
  if (!createIfMissing) {
    return nullptr;
  }
  if (sSecureClients.empty()) {
    sSecureClients.reserve(4);
  }
  SecureClientState state;
  state.owner = const_cast<OThreadCoAPSecureClient *>(owner);
  state.connectSignal = xSemaphoreCreateBinary();
  sSecureClients.push_back(state);
  return &sSecureClients.back();
}

static void removeSecureState(const OThreadCoAPSecureClient *owner) {
  for (size_t i = 0; i < sSecureClients.size(); ++i) {
    if (sSecureClients[i].owner == owner) {
      if (sSecureClients[i].connectSignal) {
        vSemaphoreDelete(sSecureClients[i].connectSignal);
      }
      sSecureClients.erase(sSecureClients.begin() + static_cast<long>(i));
      return;
    }
  }
}

static bool anySecureClientSessionActive(const OThreadCoAPSecureClient *except) {
  for (const auto &entry : sSecureClients) {
    if (except != nullptr && entry.owner == except) {
      continue;
    }
    if (entry.connected || entry.active) {
      return true;
    }
  }
  return false;
}

static void cleanupFailedSecureConnect(OThreadCoAPSecureClient *owner, otInstance *inst, SecureClientState *state) {
  if (!state) {
    return;
  }
  state->active = false;
  state->connected = false;
  if (sSecureClientOwner == owner) {
    sSecureClientOwner = nullptr;
  }
  if (inst) {
    OtLock lock;
    if (lock) {
      otCoapSecureDisconnect(inst);
    }
  }
}

static void secureConnectEventCb(otCoapSecureConnectEvent event, void *context) {
  auto *owner = static_cast<OThreadCoAPSecureClient *>(context);
  SecureClientState *state = getSecureState(owner, false);
  if (!state) {
    return;
  }
  state->connected = (event == OT_COAP_SECURE_CONNECTED);
  if (event == OT_COAP_SECURE_CONNECTED) {
    state->active = true;
  } else if (event == OT_COAP_SECURE_DISCONNECTED_LOCAL_CLOSED || event == OT_COAP_SECURE_DISCONNECTED_PEER_CLOSED || event == OT_COAP_SECURE_DISCONNECTED_ERROR
             || event == OT_COAP_SECURE_DISCONNECTED_TIMEOUT || event == OT_COAP_SECURE_DISCONNECTED_MAX_ATTEMPTS) {
    state->active = false;
    state->connected = false;
    if (sSecureClientOwner == owner) {
      sSecureClientOwner = nullptr;
    }
  }
  if (state->connectCb) {
    int wrapped = OT_COAP_SECURE_DISCONNECTED_ERROR;
    switch (event) {
      case OT_COAP_SECURE_CONNECTED:                 wrapped = OT_COAP_SECURE_CONNECTED; break;
      case OT_COAP_SECURE_DISCONNECTED_PEER_CLOSED:  wrapped = OT_COAP_SECURE_DISCONNECTED_PEER; break;
      case OT_COAP_SECURE_DISCONNECTED_LOCAL_CLOSED: wrapped = OT_COAP_SECURE_DISCONNECTED_LOCAL; break;
      case OT_COAP_SECURE_DISCONNECTED_MAX_ATTEMPTS: wrapped = OT_COAP_SECURE_DISCONNECTED_MAX_ATTEMPTS; break;
      case OT_COAP_SECURE_DISCONNECTED_TIMEOUT:      wrapped = OT_COAP_SECURE_DISCONNECTED_TIMEOUT; break;
      default:                                       wrapped = OT_COAP_SECURE_DISCONNECTED_ERROR; break;
    }
    state->connectCb(wrapped, state->connectCtx);
  }
  if (state->connectSignal) {
    xSemaphoreGive(state->connectSignal);
  }
}

struct SecureBlockingContext {
  OThreadCoAPSecureClient *owner = nullptr;
  SemaphoreHandle_t done = nullptr;
  // Secure (DTLS) requests are always unicast point-to-point, so the OpenThread
  // response handler fires exactly once. On a wrapper timeout the caller sets
  // `abandoned` (under the OT lock) and hands ownership to that pending handler,
  // which frees the context when it finally fires.
  bool abandoned = false;
};

static std::vector<SecureBlockingContext *> sPendingSecures;

static void registerPendingSecure(SecureBlockingContext *ctx) {
  sPendingInstance = OThread.getInstance();
  sPendingSecures.push_back(ctx);
}

static void unregisterPendingSecure(SecureBlockingContext *ctx) {
  for (size_t i = 0; i < sPendingSecures.size(); ++i) {
    if (sPendingSecures[i] == ctx) {
      sPendingSecures.erase(sPendingSecures.begin() + static_cast<long>(i));
      return;
    }
  }
}

// Runs on the OpenThread task (under the OT lock). The response payload is stored
// in the persistent SecureClientState; `ctx` only carries the wakeup handshake.
static void secureResponseCb(void *context, otMessage *message, const otMessageInfo *messageInfo, otError result) {
  SecureBlockingContext *ctx = static_cast<SecureBlockingContext *>(context);
  if (!ctx) {
    return;
  }
  if (ctx->abandoned) {
    unregisterPendingSecure(ctx);
    if (ctx->done) {
      vSemaphoreDelete(ctx->done);
    }
    delete ctx;
    return;
  }
  SecureClientState *state = getSecureState(ctx->owner, false);
  if (state) {
    state->payloadLen = 0;
    state->lastCode = (result == OT_ERROR_INVALID_STATE) ? OT_COAP_ERROR_NOT_CONNECTED : errorFromOt(result, OT_COAP_ERROR_TLS_FAILED);
    if (result == OT_ERROR_NONE && message) {
      state->lastCode = otCodeToArduino(otCoapMessageGetCode(message));
      if (messageInfo) {
        state->peer = otToIp(messageInfo->mPeerAddr);
      }
      state->payloadLen = readPayloadFromMessage(message, state->payload, sizeof(state->payload));
    }
  }
  if (ctx->done) {
    xSemaphoreGive(ctx->done);
  }
}

// Takes ownership of the heap `ctx`. Same lifetime handshake as waitBlockingClient.
static int waitBlockingSecure(SecureBlockingContext *ctx, otError sendErr, uint32_t timeoutMs) {
  OThreadCoAPSecureClient *owner = ctx->owner;
  if (sendErr != OT_ERROR_NONE) {
    if (ctx->done) {
      vSemaphoreDelete(ctx->done);
    }
    delete ctx;
    return errorFromOt(sendErr, OT_COAP_ERROR_NOT_CONNECTED);
  }

  bool gotResponse = (xSemaphoreTake(ctx->done, pdMS_TO_TICKS(timeoutMs + 1000)) == pdTRUE);

  OtLock lock;
  if (!gotResponse) {
    gotResponse = (xSemaphoreTake(ctx->done, 0) == pdTRUE);
  }
  if (!gotResponse) {
    if (lock) {
      ctx->abandoned = true;
      registerPendingSecure(ctx);
      return OT_COAP_ERROR_TIMEOUT;
    }
    vSemaphoreDelete(ctx->done);
    delete ctx;
    return OT_COAP_ERROR_TIMEOUT;
  }

  vSemaphoreDelete(ctx->done);
  delete ctx;
  SecureClientState *state = getSecureState(owner, false);
  return state ? state->lastCode : OT_COAP_ERROR_NOT_CONNECTED;
}

int abortSecureBlockingClient(const char *method, SecureBlockingContext *ctx) {
  log_w("OThreadCoAPSecureClient: %s() failed (could not acquire OpenThread lock)", method);
  vSemaphoreDelete(ctx->done);
  delete ctx;
  return OT_COAP_ERROR_INVALID_STATE;
}
#endif

// Reclaims abandoned blocking contexts whose response handlers can no longer fire
// because the owning otInstance is gone (OThread.end()) or has been replaced.
// A no-op while the same live stack is running (those handlers will still fire and
// free their own context). Safe to call from the Arduino task: it only frees when
// the pending handlers belong to a different instance than @p current.
static void reapStalePending(otInstance *current) {
  if (sPendingInstance == nullptr || sPendingInstance == current) {
    return;
  }
  for (auto *ctx : sPendingClients) {
    if (ctx->done) {
      vSemaphoreDelete(ctx->done);
    }
    delete ctx;
  }
  sPendingClients.clear();
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  for (auto *ctx : sPendingSecures) {
    if (ctx->done) {
      vSemaphoreDelete(ctx->done);
    }
    delete ctx;
  }
  sPendingSecures.clear();
#endif
  sPendingInstance = nullptr;
}

// Frees every pending blocking context regardless of otInstance (stack teardown).
static void reapAllPendingContexts() {
  for (auto *ctx : sPendingClients) {
    if (ctx->done) {
      vSemaphoreDelete(ctx->done);
    }
    delete ctx;
  }
  sPendingClients.clear();
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  for (auto *ctx : sPendingSecures) {
    if (ctx->done) {
      vSemaphoreDelete(ctx->done);
    }
    delete ctx;
  }
  sPendingSecures.clear();
#endif
  sPendingInstance = nullptr;
}

}  // namespace

static int abortPlainBlockingClient(const char *method, ClientBlockingContext *ctx) {
  log_w("OThreadCoAPClient: %s() failed (could not acquire OpenThread lock)", method);
  vSemaphoreDelete(ctx->done);
  delete ctx;
  return OT_COAP_ERROR_INVALID_STATE;
}

void detachResourceStoresFromServer(OThreadCoAPServerClass *server);

OThreadCoAPRequest::OThreadCoAPRequest(const void *internal) : _internal(internal) {}

const char *OThreadCoAPRequest::path() const {
  const RequestState *s = static_cast<const RequestState *>(_internal);
  return s ? s->path.c_str() : "";
}

int OThreadCoAPRequest::method() const {
  const RequestState *s = static_cast<const RequestState *>(_internal);
  return s ? s->method : 0;
}

IPAddress OThreadCoAPRequest::remoteIP() const {
  const RequestState *s = static_cast<const RequestState *>(_internal);
  return s ? s->remoteIp : IPAddress(IPv6);
}

uint16_t OThreadCoAPRequest::remotePort() const {
  const RequestState *s = static_cast<const RequestState *>(_internal);
  return s ? s->remotePort : 0;
}

size_t OThreadCoAPRequest::payloadLength() const {
  const RequestState *s = static_cast<const RequestState *>(_internal);
  return s ? s->payloadLen : 0;
}

size_t OThreadCoAPRequest::readPayload(uint8_t *buffer, size_t length) const {
  const RequestState *s = static_cast<const RequestState *>(_internal);
  if (!s || !buffer || length == 0) {
    return 0;
  }
  size_t n = std::min(length, s->payloadLen);
  memcpy(buffer, s->payload, n);
  return n;
}

String OThreadCoAPRequest::payloadString() const {
  const RequestState *s = static_cast<const RequestState *>(_internal);
  if (!s || s->payloadLen == 0) {
    return String();
  }
  String out;
  out.reserve(s->payloadLen + 1);
  for (size_t i = 0; i < s->payloadLen; ++i) {
    out += static_cast<char>(s->payload[i]);
  }
  return out;
}

bool OThreadCoAPRequest::isConfirmable() const {
  const RequestState *s = static_cast<const RequestState *>(_internal);
  return s ? s->confirmable : false;
}

bool OThreadCoAPRequest::isMulticast() const {
  const RequestState *s = static_cast<const RequestState *>(_internal);
  return s ? s->multicast : false;
}

OThreadCoAPResponse::OThreadCoAPResponse(void *internal) : _internal(internal) {}

void OThreadCoAPResponse::setCode(int code) {
  ResponseState *s = static_cast<ResponseState *>(_internal);
  if (s) {
    s->code = code;
  }
}

void OThreadCoAPResponse::setContentFormat(uint16_t format) {
  ResponseState *s = static_cast<ResponseState *>(_internal);
  if (s) {
    s->contentFormat = format;
  }
}

void OThreadCoAPResponse::setPayload(const char *text) {
  ResponseState *s = static_cast<ResponseState *>(_internal);
  if (!s) {
    return;
  }
  if (!text) {
    s->payloadLen = 0;
    return;
  }
  size_t len = strnlen(text, OT_COAP_MAX_PAYLOAD);
  memcpy(s->payload, text, len);
  s->payloadLen = len;
}

void OThreadCoAPResponse::setPayload(const uint8_t *data, size_t length) {
  ResponseState *s = static_cast<ResponseState *>(_internal);
  if (!s || !data) {
    return;
  }
  size_t n = std::min(length, static_cast<size_t>(OT_COAP_MAX_PAYLOAD));
  memcpy(s->payload, data, n);
  s->payloadLen = n;
}

void OThreadCoAPResponse::setLocation(const char *path) {
  ResponseState *s = static_cast<ResponseState *>(_internal);
  if (!s) {
    return;
  }
  s->locationPath = sanitizePath(path);
  s->hasLocation = s->locationPath.length() > 0;
}

void OThreadCoAPResponse::setMaxAge(uint32_t seconds) {
  ResponseState *s = static_cast<ResponseState *>(_internal);
  if (!s) {
    return;
  }
  s->hasMaxAge = true;
  s->maxAge = seconds;
}

void OThreadCoAPResponse::send() {
  ResponseState *s = static_cast<ResponseState *>(_internal);
  (void)sendResponseMessage(s);
}

bool OThreadCoAP::isAttached() {
  if (!OpenThread::otStarted) {
    return false;
  }
  return OpenThread::otGetDeviceRole() >= OT_ROLE_CHILD;
}

bool OThreadCoAP::secureApiEnabled() {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  return true;
#else
  return false;
#endif
}

OThreadCoAPServerClass &OThreadCoAP::plainServer() {
  return OThreadCoAPServer;
}

OThreadCoAPSecureServerClass &OThreadCoAP::secureServer() {
  return OThreadCoAPSecureServer;
}

// Drop wrapper-side server state after OThread.end() or when stop() cannot reach
// the OpenThread APIs. Does not call otCoap* — the otInstance may already be gone.
static void detachPlainServerResourcesBestEffort(ServerImpl *impl, otInstance *inst) {
  if (!impl || !inst) {
    return;
  }
  for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
    OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
    if (!lock) {
      continue;
    }
    forceUnregisterAllResourcesFromOt(impl, inst);
    if (sPlainServerOwner == impl) {
      otCoapSetDefaultHandler(inst, nullptr, nullptr);
      sPlainServerOwner = nullptr;
    }
    return;
  }
}

#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
static void detachSecureServerResourcesBestEffort(ServerImpl *impl, otInstance *inst) {
  if (!impl || !inst) {
    return;
  }
  for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
    OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
    if (!lock) {
      continue;
    }
    forceUnregisterAllResourcesFromOt(impl, inst);
    if (sSecureServerOwner == impl) {
      otCoapSecureSetDefaultHandler(inst, nullptr, nullptr);
      sSecureServerOwner = nullptr;
    }
    return;
  }
}

static bool mayStopCoapSecureAfterClientDisconnect(const OThreadCoAPSecureClient *except) {
  if (anySecureClientSessionActive(except)) {
    return false;
  }
  return sSecureServerOwner == nullptr || !sSecureServerOwner->running;
}
#endif

void resetPlainServerWrapperState(OThreadCoAPServerClass &server) {
  ServerImpl *impl = static_cast<ServerImpl *>(server._impl);
  if (!impl) {
    return;
  }
  impl->running = false;
  impl->joinedGroups.clear();
  for (auto &slot : impl->slots) {
    if (slot.inUse) {
      slot.registered = false;
    }
  }
  impl->resourceStore = nullptr;
  for (auto &pair : sResourceStores) {
    ResourceStoreImpl *storeImpl = pair.second;
    if (storeImpl && storeImpl->server == &server) {
      storeImpl->server = nullptr;
    }
  }
  if (sPlainServerOwner == impl) {
    sPlainServerOwner = nullptr;
  }
}

void resetSecureServerWrapperState(OThreadCoAPSecureServerClass &server) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  ServerImpl *impl = static_cast<ServerImpl *>(server._impl);
  if (!impl) {
    return;
  }
  impl->running = false;
  for (auto &slot : impl->slots) {
    if (slot.inUse) {
      slot.registered = false;
    }
  }
  if (sSecureServerOwner == impl) {
    sSecureServerOwner = nullptr;
  }
#else
  (void)server;
#endif
}

void OThreadCoAP::releaseAllPending(otInstance *stackInstance) {
  // Called from OThread.end() while the otInstance is still valid (before worker
  // task deletion and esp_openthread_deinit). Pass the live instance so
  // otCoapStop / otCoapSecureStop run. After services stop, force-reap every
  // pending blocking context (reapStalePending alone is a no-op when the instance
  // matches sPendingInstance). If stackInstance is null and the stack is still
  // marked started, getInstance() is used as a fallback.
  otInstance *current = stackInstance;
  if (!current && OpenThread::otStarted) {
    current = OThread.getInstance();
  }
  if (current) {
    bool coapStopped = false;
    for (int attempt = 0; attempt < kOtLockStopAttempts + 2; ++attempt) {
      OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
      if (!lock) {
        continue;
      }
      if (sCoapClientServiceHeld) {
        releasePlainCoapService(current);
        sCoapClientServiceHeld = false;
      }
      while (sCoapServiceRefCount > 0) {
        releasePlainCoapService(current);
      }
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
      otCoapSecureStop(current);
      sSecureServerServiceHeld = false;
#endif
      coapStopped = true;
      break;
    }
    if (!coapStopped) {
      log_e(
        "OThreadCoAP: releaseAllPending could not acquire OpenThread lock after %d attempts; "
        "clearing wrapper state (esp_openthread_deinit follows)",
        kOtLockStopAttempts + 2
      );
      delay(100);
      OtLock finalLock(portMAX_DELAY);
      if (finalLock) {
        if (sCoapClientServiceHeld) {
          releasePlainCoapService(current);
          sCoapClientServiceHeld = false;
        }
        while (sCoapServiceRefCount > 0) {
          releasePlainCoapService(current);
        }
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
        otCoapSecureStop(current);
        sSecureServerServiceHeld = false;
#endif
        coapStopped = true;
      }
    }
  }
  reapAllPendingContexts();
  sCoapServiceActive = false;
  sCoapServicePort = 0;
  sCoapServiceRefCount = 0;
  sCoapClientServiceHeld = false;
  sCoapClientInstanceCount = 0;
  sPlainServerServiceHeld = false;
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  sSecureClientOwner = nullptr;
  sSecureServerServiceHeld = false;
  resetSecureClientSessionFlags();
#endif
  resetPlainServerWrapperState(OThreadCoAPServer);
  resetSecureServerWrapperState(OThreadCoAPSecureServer);
}

const char *OThreadCoAP::responseCodeToString(int code) {
  switch (code) {
    case OT_COAP_RESP_CREATED:        return "2.01 Created";
    case OT_COAP_RESP_DELETED:        return "2.02 Deleted";
    case OT_COAP_RESP_CHANGED:        return "2.04 Changed";
    case OT_COAP_RESP_OK:             return "2.05 Content";
    case OT_COAP_RESP_BAD_REQUEST:    return "4.00 Bad Request";
    case OT_COAP_RESP_NOT_FOUND:      return "4.04 Not Found";
    case OT_COAP_RESP_METHOD_NA:      return "4.05 Method Not Allowed";
    case OT_COAP_RESP_INTERNAL_ERROR: return "5.00 Internal Server Error";
    default:                          return "unknown";
  }
}

const char *OThreadCoAP::errorToString(int error) {
  switch (error) {
    case OT_COAP_ERROR_TIMEOUT:       return "timeout";
    case OT_COAP_ERROR_NO_RESPONSE:   return "no response";
    case OT_COAP_ERROR_NOT_ATTACHED:  return "not attached";
    case OT_COAP_ERROR_NO_BUFS:       return "no bufs";
    case OT_COAP_ERROR_INVALID_ARGS:  return "invalid args";
    case OT_COAP_ERROR_NOT_CONNECTED: return "not connected";
    case OT_COAP_ERROR_TLS_FAILED:    return "tls failed";
    case OT_COAP_ERROR_INVALID_STATE: return "invalid state";
    default:                          return "unknown";
  }
}

OThreadCoAPClient::OThreadCoAPClient()
  : _timeoutMs(OT_COAP_DEFAULT_TIMEOUT_MS), _port(OT_COAP_DEFAULT_PORT), _confirmable(true), _useRfcRetransmit(false), _contentFormat(OT_COAP_FORMAT_TEXT),
    _lastCode(OT_COAP_ERROR_NO_RESPONSE), _remoteIp(IPv6), _payloadLen(0) {
  memset(_payload, 0, sizeof(_payload));
  if (sCoapClientInstanceCount < UINT16_MAX) {
    ++sCoapClientInstanceCount;
  }
}

OThreadCoAPClient::~OThreadCoAPClient() {
  if (sCoapClientInstanceCount > 0) {
    --sCoapClientInstanceCount;
  }
  if (sCoapClientInstanceCount != 0 || !sCoapClientServiceHeld) {
    return;
  }
  otInstance *inst = OThread.getInstance();
  if (!inst) {
    sCoapClientServiceHeld = false;
    return;
  }
  bool released = false;
  for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
    OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
    if (!lock) {
      continue;
    }
    releasePlainCoapService(inst);
    sCoapClientServiceHeld = false;
    released = true;
    break;
  }
  if (!released) {
    log_w("OThreadCoAPClient: destructor could not release lazy-start CoAP service (call OThread.end() or retry after stack idle)");
  }
}

void OThreadCoAPClient::setTimeout(uint32_t timeoutMs) {
  if (!timeoutMs) {
    timeoutMs = OT_COAP_DEFAULT_TIMEOUT_MS;
  } else if (!_useRfcRetransmit && timeoutMs < OT_COAP_MIN_ALIGNED_TIMEOUT_MS) {
    timeoutMs = OT_COAP_MIN_ALIGNED_TIMEOUT_MS;
  }
  _timeoutMs = timeoutMs;
}

void OThreadCoAPClient::useDefaultCoapRetransmit() {
  _useRfcRetransmit = true;
}

void OThreadCoAPClient::setConfirmable(bool confirmable) {
  _confirmable = confirmable;
}

void OThreadCoAPClient::setPort(uint16_t port) {
  _port = port;
}

void OThreadCoAPClient::setContentFormat(uint16_t format) {
  _contentFormat = format;
}

int OThreadCoAPClient::GET(const IPAddress &host, const char *path) {
  otInstance *instance = OThread.getInstance();
  if (!instance || !OThreadCoAP::isAttached()) {
    return OT_COAP_ERROR_NOT_ATTACHED;
  }
  ClientBlockingContext *ctx = new ClientBlockingContext();
  ctx->done = xSemaphoreCreateBinary();
  if (!ctx->done) {
    delete ctx;
    return OT_COAP_ERROR_NO_BUFS;
  }
  ctx->multicast = isMulticastIp(host);
  _payloadLen = 0;
  _lastCode = OT_COAP_ERROR_NO_RESPONSE;
  otError err = OT_ERROR_INVALID_STATE;
  {
    OtLock lock;
    if (!lock) {
      return abortPlainBlockingClient("GET", ctx);
    }
    err = sendClientRequest(
      instance, false, (_confirmable && !ctx->multicast), host, _port, sanitizePath(path).c_str(), OT_COAP_REQ_GET, nullptr, 0, _contentFormat,
      clientResponseCb, ctx, _timeoutMs, _useRfcRetransmit
    );
  }
  _lastCode = waitBlockingClient(ctx, err, _timeoutMs, _useRfcRetransmit, _payload, &_payloadLen, &_remoteIp);
  return _lastCode;
}

int OThreadCoAPClient::GET(const char *host, const char *path) {
  IPAddress ip;
  if (!parseIp(host, ip)) {
    return OT_COAP_ERROR_INVALID_ARGS;
  }
  return GET(ip, path);
}

int OThreadCoAPClient::PUT(const IPAddress &host, const char *path, const char *payload) {
  return PUT(host, path, reinterpret_cast<const uint8_t *>(payload), payload ? strlen(payload) : 0);
}

int OThreadCoAPClient::PUT(const IPAddress &host, const char *path, const uint8_t *payload, size_t length) {
  otInstance *instance = OThread.getInstance();
  if (!instance || !OThreadCoAP::isAttached()) {
    return OT_COAP_ERROR_NOT_ATTACHED;
  }
  ClientBlockingContext *ctx = new ClientBlockingContext();
  ctx->done = xSemaphoreCreateBinary();
  if (!ctx->done) {
    delete ctx;
    return OT_COAP_ERROR_NO_BUFS;
  }
  ctx->multicast = isMulticastIp(host);
  _payloadLen = 0;
  _lastCode = OT_COAP_ERROR_NO_RESPONSE;
  otError err = OT_ERROR_INVALID_STATE;
  {
    OtLock lock;
    if (!lock) {
      return abortPlainBlockingClient("PUT", ctx);
    }
    err = sendClientRequest(
      instance, false, (_confirmable && !ctx->multicast), host, _port, sanitizePath(path).c_str(), OT_COAP_REQ_PUT, payload, length, _contentFormat,
      clientResponseCb, ctx, _timeoutMs, _useRfcRetransmit
    );
  }
  _lastCode = waitBlockingClient(ctx, err, _timeoutMs, _useRfcRetransmit, _payload, &_payloadLen, &_remoteIp);
  return _lastCode;
}

int OThreadCoAPClient::POST(const IPAddress &host, const char *path, const char *payload) {
  return POST(host, path, reinterpret_cast<const uint8_t *>(payload), payload ? strlen(payload) : 0);
}

int OThreadCoAPClient::POST(const IPAddress &host, const char *path, const uint8_t *payload, size_t length) {
  otInstance *instance = OThread.getInstance();
  if (!instance || !OThreadCoAP::isAttached()) {
    return OT_COAP_ERROR_NOT_ATTACHED;
  }
  ClientBlockingContext *ctx = new ClientBlockingContext();
  ctx->done = xSemaphoreCreateBinary();
  if (!ctx->done) {
    delete ctx;
    return OT_COAP_ERROR_NO_BUFS;
  }
  ctx->multicast = isMulticastIp(host);
  _payloadLen = 0;
  _lastCode = OT_COAP_ERROR_NO_RESPONSE;
  otError err = OT_ERROR_INVALID_STATE;
  {
    OtLock lock;
    if (!lock) {
      return abortPlainBlockingClient("POST", ctx);
    }
    err = sendClientRequest(
      instance, false, (_confirmable && !ctx->multicast), host, _port, sanitizePath(path).c_str(), OT_COAP_REQ_POST, payload, length, _contentFormat,
      clientResponseCb, ctx, _timeoutMs, _useRfcRetransmit
    );
  }
  _lastCode = waitBlockingClient(ctx, err, _timeoutMs, _useRfcRetransmit, _payload, &_payloadLen, &_remoteIp);
  return _lastCode;
}

int OThreadCoAPClient::DELETE(const IPAddress &host, const char *path) {
  otInstance *instance = OThread.getInstance();
  if (!instance || !OThreadCoAP::isAttached()) {
    return OT_COAP_ERROR_NOT_ATTACHED;
  }
  ClientBlockingContext *ctx = new ClientBlockingContext();
  ctx->done = xSemaphoreCreateBinary();
  if (!ctx->done) {
    delete ctx;
    return OT_COAP_ERROR_NO_BUFS;
  }
  ctx->multicast = isMulticastIp(host);
  _payloadLen = 0;
  _lastCode = OT_COAP_ERROR_NO_RESPONSE;
  otError err = OT_ERROR_INVALID_STATE;
  {
    OtLock lock;
    if (!lock) {
      return abortPlainBlockingClient("DELETE", ctx);
    }
    err = sendClientRequest(
      instance, false, (_confirmable && !ctx->multicast), host, _port, sanitizePath(path).c_str(), OT_COAP_REQ_DELETE, nullptr, 0, _contentFormat,
      clientResponseCb, ctx, _timeoutMs, _useRfcRetransmit
    );
  }
  _lastCode = waitBlockingClient(ctx, err, _timeoutMs, _useRfcRetransmit, _payload, &_payloadLen, &_remoteIp);
  return _lastCode;
}

bool OThreadCoAPClient::sendNonBlocking(const IPAddress &host, uint8_t method, const char *path, const uint8_t *payload, size_t length) {
  otInstance *instance = OThread.getInstance();
  if (!instance || !OThreadCoAP::isAttached()) {
    log_w("OThreadCoAPClient: sendNonBlocking() failed (OpenThread not attached)");
    return false;
  }
  OtLock lock;
  if (!lock) {
    log_w("OThreadCoAPClient: sendNonBlocking() failed (could not acquire OpenThread lock)");
    return false;
  }
  // NON message with no response handler => OpenThread tracks no transaction, so
  // this truly returns once the datagram is queued (correct for multicast).
  otError err = sendClientRequest(
    instance, false, false, host, _port, sanitizePath(path).c_str(), method, payload, length, _contentFormat, nullptr, nullptr, _timeoutMs, _useRfcRetransmit
  );
  if (err != OT_ERROR_NONE) {
    log_w("OThreadCoAPClient: sendNonBlocking() failed (otError=%d)", static_cast<int>(err));
    return false;
  }
  return true;
}

bool OThreadCoAPClient::sendNonBlocking(const IPAddress &host, uint8_t method, const char *path, const char *payload) {
  return sendNonBlocking(host, method, path, reinterpret_cast<const uint8_t *>(payload), payload ? strlen(payload) : 0);
}

int OThreadCoAPClient::responseCode() const {
  return _lastCode;
}

size_t OThreadCoAPClient::payloadLength() const {
  return _payloadLen;
}

size_t OThreadCoAPClient::readPayload(uint8_t *buffer, size_t length) const {
  if (!buffer || length == 0) {
    return 0;
  }
  size_t n = std::min(length, _payloadLen);
  memcpy(buffer, _payload, n);
  return n;
}

String OThreadCoAPClient::getString() const {
  String out;
  out.reserve(_payloadLen + 1);
  for (size_t i = 0; i < _payloadLen; ++i) {
    out += static_cast<char>(_payload[i]);
  }
  return out;
}

IPAddress OThreadCoAPClient::remoteIP() const {
  return _remoteIp;
}

OThreadCoAPServerClass::OThreadCoAPServerClass(uint16_t port) : _impl(new ServerImpl()) {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  impl->port = port;
}

OThreadCoAPServerClass::~OThreadCoAPServerClass() {
  stop();
  detachResourceStoresFromServer(this);
  delete static_cast<ServerImpl *>(_impl);
  _impl = nullptr;
}

bool OThreadCoAPServerClass::begin() {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (!impl) {
    log_e("OThreadCoAPServer: begin() failed (invalid server object)");
    return false;
  }
  otInstance *inst = OThread.getInstance();
  if (!inst || !OThreadCoAP::isAttached()) {
    log_w("OThreadCoAPServer: begin() failed (OpenThread not attached)");
    return false;
  }
  for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
    OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
    if (!lock) {
      continue;
    }
    if (impl->running) {
      if (!resyncPlainServerOnOt(impl, inst)) {
        markPlainServerInactiveAfterResyncFailure(impl, inst);
        return false;
      }
      return true;
    }
    forceUnregisterAllResourcesFromOt(impl, inst);
    const bool acquiredService = !sPlainServerServiceHeld || !sCoapServiceActive;
    if (acquiredService) {
      otError err = acquirePlainCoapService(inst, impl->port);
      if (err != OT_ERROR_NONE) {
        log_e("OThreadCoAPServer: begin() failed (otCoapStart otError=%d)", static_cast<int>(err));
        return false;
      }
      sPlainServerServiceHeld = true;
    } else if (sCoapServicePort != impl->port) {
      log_e("OThreadCoAPServer: begin() failed (plain CoAP service already active on port %u)", (unsigned)sCoapServicePort);
      return false;
    }
    if (!attachRegisteredResources(impl, inst, "OThreadCoAPServer")) {
      detachRegisteredResources(impl, inst);
      otCoapSetDefaultHandler(inst, nullptr, nullptr);
      if (acquiredService) {
        releasePlainCoapService(inst);
        sPlainServerServiceHeld = false;
      }
      return false;
    }
    otCoapSetDefaultHandler(inst, defaultRequestCb, impl);
    sPlainServerOwner = impl;
    impl->running = true;
    return true;
  }
  log_w("OThreadCoAPServer: begin() failed (could not acquire OpenThread lock)");
  return false;
}

void OThreadCoAPServerClass::stop() {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (!impl) {
    return;
  }
  // Always leave multicast groups we joined, even if running was already cleared
  // (e.g. failed stop() or resync failure).
  for (const IPAddress &g : impl->joinedGroups) {
    OThread.unsubscribeMulticast(g);
  }
  impl->joinedGroups.clear();

  if (!impl->running) {
    return;
  }

  otInstance *inst = OThread.getInstance();
  if (!inst) {
    resetPlainServerWrapperState(*this);
    return;
  }

  bool stopped = false;
  for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
    OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
    if (!lock) {
      continue;
    }
    detachRegisteredResources(impl, inst);
    if (sPlainServerOwner == impl) {
      otCoapSetDefaultHandler(inst, nullptr, nullptr);
      sPlainServerOwner = nullptr;
    }
    if (sPlainServerServiceHeld) {
      releasePlainCoapService(inst);
      sPlainServerServiceHeld = false;
    }
    impl->running = false;
    stopped = true;
    break;
  }
  if (!stopped) {
    log_w("OThreadCoAPServer: stop() could not acquire OpenThread lock; wrapper state reset "
          "(call begin() to re-synchronize or OThread.end() if the CoAP service remains active)");
    detachPlainServerResourcesBestEffort(impl, inst);
    resetPlainServerWrapperState(*this);
  }
}

OThreadCoAPServerClass::operator bool() const {
  const ServerImpl *impl = static_cast<const ServerImpl *>(_impl);
  return impl ? impl->running : false;
}

bool OThreadCoAPServerClass::joinMulticastGroup(const IPAddress &group) {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (!impl) {
    log_w("OThreadCoAPServer: joinMulticastGroup() failed (invalid server object)");
    return false;
  }
  if (!impl->running) {
    log_w("OThreadCoAPServer: joinMulticastGroup() failed (call begin() first)");
    return false;
  }
  if (!OThread.subscribeMulticast(group)) {
    log_w("OThreadCoAPServer: joinMulticastGroup() failed for %s", group.toString().c_str());
    return false;
  }
  // Remember the group so stop() leaves it. Avoid duplicate bookkeeping.
  for (const IPAddress &g : impl->joinedGroups) {
    if (g == group) {
      return true;
    }
  }
  impl->joinedGroups.push_back(group);
  return true;
}

bool OThreadCoAPServerClass::leaveMulticastGroup(const IPAddress &group) {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (!impl) {
    return false;
  }
  for (size_t i = 0; i < impl->joinedGroups.size(); ++i) {
    if (impl->joinedGroups[i] == group) {
      impl->joinedGroups.erase(impl->joinedGroups.begin() + static_cast<long>(i));
      break;
    }
  }
  return OThread.unsubscribeMulticast(group);
}

bool OThreadCoAPServerClass::on(const char *path, uint8_t methodMask, OThreadCoAPHandler handler, void *context) {
  return registerServerPath(static_cast<ServerImpl *>(_impl), "OThreadCoAPServer", path, methodMask, handler, context);
}

bool OThreadCoAPServerClass::onNotFound(OThreadCoAPHandler handler, void *context) {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (!impl) {
    return false;
  }
  otInstance *inst = OThread.getInstance();
  if (impl->running && inst) {
    for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
      OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
      if (!lock) {
        continue;
      }
      impl->notFoundHandler = handler;
      impl->notFoundContext = context;
      return true;
    }
    log_w("OThreadCoAPServer: onNotFound() failed (could not acquire OpenThread lock)");
    return false;
  }
  impl->notFoundHandler = handler;
  impl->notFoundContext = context;
  return true;
}

static void serveSlotHandler(OThreadCoAPRequest &request, OThreadCoAPResponse &response, void *context) {
  ServerResourceSlot *slot = static_cast<ServerResourceSlot *>(context);
  if (!slot || !slot->serveValuePtr) {
    response.setCode(OT_COAP_RESP_INTERNAL_ERROR);
    response.send();
    return;
  }
  // RFC 7252 §8.2 — serve() is unicast-oriented; do not reply to group requests.
  if (request.isMulticast()) {
    logMulticastWarn("serve()", request.path(), request.isConfirmable(), "serve() is unicast-only");
    return;
  }
  if (request.method() == OT_COAP_REQ_GET) {
    String snapshot;
    {
      ServeSlotLock guard;
      if (!guard) {
        response.setCode(OT_COAP_RESP_INTERNAL_ERROR);
        response.send();
        return;
      }
      snapshot = slot->serveSnapshot;
    }
    response.setCode(OT_COAP_RESP_OK);
    response.setContentFormat(OT_COAP_FORMAT_TEXT);
    response.setPayload(snapshot.c_str());
    response.send();
    return;
  }
  if (request.method() == OT_COAP_REQ_PUT) {
    String body = request.payloadString();
    ServeSlotLock guard;
    if (!guard) {
      response.setCode(OT_COAP_RESP_INTERNAL_ERROR);
      response.send();
      return;
    }
    slot->serveSnapshot = body;
    *slot->serveValuePtr = slot->serveSnapshot;
    response.setCode(OT_COAP_RESP_CHANGED);
    response.setPayload(slot->serveSnapshot.c_str());
    response.send();
    return;
  }
  response.setCode(OT_COAP_RESP_METHOD_NA);
  response.send();
}

bool OThreadCoAPServerClass::serve(const char *path, String *valuePointer) {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (!impl || !valuePointer) {
    log_w("OThreadCoAPServer: serve() failed (invalid server object or value pointer)");
    return false;
  }
  String normalized = sanitizePath(path);
  if (!normalized.length()) {
    log_w("OThreadCoAPServer: serve() failed (empty or invalid path)");
    return false;
  }
  ServerResourceSlot *slot = findSlot(impl, normalized);
  const bool isNewSlot = (slot == nullptr);
  if (!slot) {
    slot = allocSlot(impl);
  }
  if (!slot) {
    log_e("OThreadCoAPServer: serve() failed (resource table full, max %d)", OT_COAP_MAX_RESOURCES);
    return false;
  }
  ensureServeSlotMutex();
  slot->path = normalized;
  slot->serveValuePtr = valuePointer;
  {
    ServeSlotLock guard;
    if (!guard) {
      log_w("OThreadCoAPServer: serve() failed (could not acquire serve lock)");
      if (isNewSlot) {
        clearServerSlot(slot);
      }
      return false;
    }
    slot->serveSnapshot = *valuePointer;
  }
  const bool ok = registerServerPath(impl, "OThreadCoAPServer", path, OT_COAP_METHOD_GET | OT_COAP_METHOD_PUT, serveSlotHandler, slot);
  if (!ok && isNewSlot) {
    clearServerSlot(slot);
  }
  return ok;
}

bool OThreadCoAPServerClass::setResourceValue(const char *path, const String &value) {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (!impl) {
    log_w("OThreadCoAPServer: setResourceValue() failed (invalid server object)");
    return false;
  }
  String normalized = sanitizePath(path);
  for (auto &slot : impl->slots) {
    if (!slot.inUse || slot.path != normalized) {
      continue;
    }
    // Only serve()-created slots keep a String* in serveValuePtr. A path registered
    // via on(path, ..., handler, &userCtx) stores the user's own pointer in
    // handlerContext; casting that to String* and writing it would corrupt the
    // caller's object. Act only on serve()-backed slots.
    if (slot.handler != serveSlotHandler) {
      log_w("OThreadCoAPServer: setResourceValue() failed for '%s' (path not registered via serve())", normalized.c_str());
      return false;
    }
    String *bound = slot.serveValuePtr;
    if (!bound) {
      log_w("OThreadCoAPServer: setResourceValue() failed for '%s' (missing value pointer)", normalized.c_str());
      return false;
    }
    // serveSlotHandler reads serveSnapshot on the OpenThread task. setResourceValue()
    // and serveSlotHandler synchronize via sServeSlotMutex (not OtLock — callbacks
    // run on the OT task while it already holds the stack lock).
    ensureServeSlotMutex();
    ServeSlotLock guard;
    if (!guard) {
      log_w("OThreadCoAPServer: setResourceValue() failed for '%s' (could not acquire serve lock)", normalized.c_str());
      return false;
    }
    *bound = value;
    slot.serveSnapshot = value;
    return true;
  }
  log_w("OThreadCoAPServer: setResourceValue() failed for '%s' (path not registered)", normalized.c_str());
  return false;
}

OThreadCoAPSecureServerClass::OThreadCoAPSecureServerClass(uint16_t port) : _impl(new ServerImpl()) {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  impl->secure = true;
  impl->port = port;
}

OThreadCoAPSecureServerClass::~OThreadCoAPSecureServerClass() {
  stop();
  delete static_cast<ServerImpl *>(_impl);
  _impl = nullptr;
}

void OThreadCoAPSecureServerClass::setPSK(const uint8_t *psk, size_t pskLength, const char *identity) {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (!impl) {
    return;
  }
  impl->psk.clear();
  if (psk && pskLength) {
    impl->psk.insert(impl->psk.end(), psk, psk + pskLength);
  }
  impl->pskIdentity = identity ? identity : "";
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  otInstance *inst = OThread.getInstance();
  if (impl->running && inst && !impl->psk.empty() && impl->pskIdentity.length()) {
    for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
      OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
      if (!lock) {
        continue;
      }
      otCoapSecureSetPsk(
        inst, impl->psk.data(), static_cast<uint16_t>(impl->psk.size()), reinterpret_cast<const uint8_t *>(impl->pskIdentity.c_str()),
        static_cast<uint16_t>(impl->pskIdentity.length())
      );
      return;
    }
    log_w("OThreadCoAPSecureServer: setPSK() stored locally but could not push to stack (OpenThread lock busy)");
  } else if (impl->running && inst && (impl->psk.empty() || !impl->pskIdentity.length())) {
    log_w("OThreadCoAPSecureServer: setPSK() cleared credentials while running; call stop() then begin() to apply");
  }
#endif
}

void OThreadCoAPSecureServerClass::setCertificate(const char *certPem, size_t certLen, const char *keyPem, size_t keyLen, const char *caPem, size_t caLen) {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (!impl) {
    return;
  }
  impl->certPem = certPem ? String(certPem).substring(0, certLen) : "";
  impl->keyPem = keyPem ? String(keyPem).substring(0, keyLen) : "";
  impl->caPem = caPem ? String(caPem).substring(0, caLen) : "";
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  otInstance *inst = OThread.getInstance();
  if (impl->running && inst && impl->certPem.length() && impl->keyPem.length()) {
    for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
      OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
      if (!lock) {
        continue;
      }
      otCoapSecureSetCertificate(
        inst, reinterpret_cast<const uint8_t *>(impl->certPem.c_str()), impl->certPem.length(), reinterpret_cast<const uint8_t *>(impl->keyPem.c_str()),
        impl->keyPem.length()
      );
      if (impl->caPem.length()) {
        otCoapSecureSetCaCertificateChain(inst, reinterpret_cast<const uint8_t *>(impl->caPem.c_str()), impl->caPem.length());
      }
      return;
    }
    log_w("OThreadCoAPSecureServer: setCertificate() stored locally but could not push to stack (OpenThread lock busy)");
  } else if (impl->running && inst && (!impl->certPem.length() || !impl->keyPem.length())) {
    log_w("OThreadCoAPSecureServer: setCertificate() cleared credentials while running; call stop() then begin() to apply");
  }
#endif
}

void OThreadCoAPSecureServerClass::setMaxConnectionAttempts(uint16_t maxAttempts) {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (impl) {
    impl->maxConnectionAttempts = maxAttempts;
  }
}

bool OThreadCoAPSecureServerClass::begin() {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (!impl) {
    log_e("OThreadCoAPSecureServer: begin() failed (invalid server object)");
    return false;
  }
  otInstance *inst = OThread.getInstance();
  if (!inst || !OThreadCoAP::isAttached()) {
    log_w("OThreadCoAPSecureServer: begin() failed (OpenThread not attached)");
    return false;
  }
  if (anySecureClientSessionActive(nullptr)) {
    log_w("OThreadCoAPSecureServer: begin() failed (OThreadCoAPSecureClient session is active on this device; disconnect the client first)");
    return false;
  }
  for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
    OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
    if (!lock) {
      continue;
    }
    if (impl->running) {
      if (!resyncSecureServerOnOt(impl, inst)) {
        markSecureServerInactiveAfterResyncFailure(impl, inst);
        return false;
      }
      return true;
    }
    forceUnregisterAllResourcesFromOt(impl, inst);
    if (!impl->psk.empty() && impl->pskIdentity.length()) {
      otCoapSecureSetPsk(
        inst, impl->psk.data(), static_cast<uint16_t>(impl->psk.size()), reinterpret_cast<const uint8_t *>(impl->pskIdentity.c_str()),
        static_cast<uint16_t>(impl->pskIdentity.length())
      );
    }
    if (impl->certPem.length() && impl->keyPem.length()) {
      otCoapSecureSetCertificate(
        inst, reinterpret_cast<const uint8_t *>(impl->certPem.c_str()), impl->certPem.length(), reinterpret_cast<const uint8_t *>(impl->keyPem.c_str()),
        impl->keyPem.length()
      );
      if (impl->caPem.length()) {
        otCoapSecureSetCaCertificateChain(inst, reinterpret_cast<const uint8_t *>(impl->caPem.c_str()), impl->caPem.length());
      }
    }
    otError err = OT_ERROR_NONE;
    if (impl->maxConnectionAttempts > 0) {
      err = otCoapSecureStartWithMaxConnAttempts(inst, impl->port, impl->maxConnectionAttempts, nullptr, nullptr);
    } else {
      err = otCoapSecureStart(inst, impl->port);
    }
    if (err != OT_ERROR_NONE && err != OT_ERROR_ALREADY) {
      log_e("OThreadCoAPSecureServer: begin() failed (otCoapSecureStart otError=%d)", static_cast<int>(err));
      return false;
    }
    sSecureServerServiceHeld = true;
    if (!attachRegisteredResources(impl, inst, "OThreadCoAPSecureServer")) {
      detachRegisteredResources(impl, inst);
      otCoapSecureSetDefaultHandler(inst, nullptr, nullptr);
      otCoapSecureStop(inst);
      sSecureServerServiceHeld = false;
      return false;
    }
    otCoapSecureSetDefaultHandler(inst, defaultRequestCb, impl);
    sSecureServerOwner = impl;
    impl->running = true;
    return true;
  }
  log_w("OThreadCoAPSecureServer: begin() failed (could not acquire OpenThread lock)");
  return false;
#else
  log_w("OThreadCoAPSecureServer: begin() failed (CoAP secure API not enabled in build)");
  return false;
#endif
}

void OThreadCoAPSecureServerClass::stop() {
  ServerImpl *impl = static_cast<ServerImpl *>(_impl);
  if (!impl || !impl->running) {
    return;
  }
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  otInstance *inst = OThread.getInstance();
  if (!inst) {
    resetSecureServerWrapperState(*this);
    return;
  }

  bool stopped = false;
  for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
    OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
    if (!lock) {
      continue;
    }
    detachRegisteredResources(impl, inst);
    if (sSecureServerOwner == impl) {
      otCoapSecureSetDefaultHandler(inst, nullptr, nullptr);
      otCoapSecureStop(inst);
      sSecureServerOwner = nullptr;
    }
    sSecureServerServiceHeld = false;
    impl->running = false;
    stopped = true;
    break;
  }
  if (!stopped) {
    log_w("OThreadCoAPSecureServer: stop() could not acquire OpenThread lock; wrapper state reset "
          "(call begin() to re-synchronize or OThread.end() if CoAPS remains active)");
    detachSecureServerResourcesBestEffort(impl, inst);
    resetSecureServerWrapperState(*this);
  }
#else
  impl->running = false;
#endif
}

OThreadCoAPSecureServerClass::operator bool() const {
  const ServerImpl *impl = static_cast<const ServerImpl *>(_impl);
  return impl ? impl->running : false;
}

bool OThreadCoAPSecureServerClass::on(const char *path, uint8_t methodMask, OThreadCoAPHandler handler, void *context) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  return registerServerPath(static_cast<ServerImpl *>(_impl), "OThreadCoAPSecureServer", path, methodMask, handler, context);
#else
  (void)path;
  (void)methodMask;
  (void)handler;
  (void)context;
  log_w("OThreadCoAPSecureServer: on() failed (CoAP secure API not enabled in build)");
  return false;
#endif
}

OThreadCoAPResourceStore::OThreadCoAPResourceStore() : _impl(new ResourceStoreImpl()) {}

void clearResourceStoreLink(OThreadCoAPServerClass *server, OThreadCoAPResourceStore *store) {
  if (!server) {
    return;
  }
  ServerImpl *serverImpl = static_cast<ServerImpl *>(server->_impl);
  if (serverImpl && serverImpl->resourceStore == store) {
    serverImpl->resourceStore = nullptr;
  }
}

void detachResourceStoresFromServer(OThreadCoAPServerClass *server) {
  if (!server) {
    return;
  }
  ServerImpl *serverImpl = static_cast<ServerImpl *>(server->_impl);
  if (serverImpl) {
    serverImpl->resourceStore = nullptr;
  }
  for (auto &pair : sResourceStores) {
    ResourceStoreImpl *impl = pair.second;
    if (impl && impl->server == server) {
      impl->server = nullptr;
    }
  }
}

OThreadCoAPResourceStore::~OThreadCoAPResourceStore() {
  ResourceStoreImpl *impl = static_cast<ResourceStoreImpl *>(_impl);
  ensureResourceStoreMutex();
  ResourceStoreLock guard;
  if (impl) {
    clearResourceStoreLink(impl->server, this);
  }
  for (size_t i = 0; i < sResourceStores.size(); ++i) {
    if (sResourceStores[i].first == this) {
      sResourceStores.erase(sResourceStores.begin() + static_cast<long>(i));
      break;
    }
  }
  delete impl;
  _impl = nullptr;
}

bool OThreadCoAPResourceStore::attach(OThreadCoAPServerClass &server, const char *basePath, size_t maxItems) {
  ResourceStoreImpl *impl = static_cast<ResourceStoreImpl *>(_impl);
  ServerImpl *serverImpl = static_cast<ServerImpl *>(server._impl);
  if (!impl || !serverImpl) {
    log_w("OThreadCoAPResourceStore: attach() failed (invalid store or server object)");
    return false;
  }
  if (&server != &OThreadCoAPServer) {
    log_w("OThreadCoAPResourceStore: attach() failed (use the library global OThreadCoAPServer)");
    return false;
  }
  ensureResourceStoreMutex();
  ResourceStoreLock guard;
  if (!guard) {
    log_w("OThreadCoAPResourceStore: attach() failed (could not acquire store lock)");
    return false;
  }
  clearResourceStoreLink(impl->server, this);
  if (serverImpl->resourceStore != nullptr && serverImpl->resourceStore != this) {
    log_e("OThreadCoAPResourceStore: server already has a store attached (one store per server)");
    return false;
  }
  impl->server = &server;
  impl->basePath = sanitizePath(basePath);
  impl->maxItems = maxItems;
  serverImpl->resourceStore = this;
  bool found = false;
  for (auto &pair : sResourceStores) {
    if (pair.first == this) {
      pair.second = impl;
      found = true;
      break;
    }
  }
  if (!found) {
    sResourceStores.push_back({this, impl});
  }
  if (impl->basePath.length() == 0) {
    impl->server = nullptr;
    clearResourceStoreLink(&server, this);
    log_w("OThreadCoAPResourceStore: attach() failed (empty or invalid basePath)");
    return false;
  }
  return true;
}

void OThreadCoAPResourceStore::onChange(ChangeCallback callback, void *context) {
  ResourceStoreImpl *impl = static_cast<ResourceStoreImpl *>(_impl);
  if (!impl) {
    return;
  }
  impl->onChange = callback;
  impl->changeContext = context;
}

size_t OThreadCoAPResourceStore::count() const {
  const ResourceStoreImpl *impl = static_cast<const ResourceStoreImpl *>(_impl);
  if (!impl) {
    return 0;
  }
  ResourceStoreLock guard;
  if (!guard) {
    return 0;
  }
  return impl->items.size();
}

bool OThreadCoAPResourceStore::getByIndex(size_t index, Item &out) const {
  const ResourceStoreImpl *impl = static_cast<const ResourceStoreImpl *>(_impl);
  if (!impl) {
    return false;
  }
  ResourceStoreLock guard;
  if (!guard) {
    return false;
  }
  if (index >= impl->items.size()) {
    return false;
  }
  out.id = impl->items[index].id;
  out.body = impl->items[index].body;
  return true;
}

bool OThreadCoAPResourceStore::getById(const char *id, Item &out) const {
  const ResourceStoreImpl *impl = static_cast<const ResourceStoreImpl *>(_impl);
  if (!impl || !id) {
    return false;
  }
  ResourceStoreLock guard;
  if (!guard) {
    return false;
  }
  for (const auto &entry : impl->items) {
    if (entry.id == id) {
      out.id = entry.id;
      out.body = entry.body;
      return true;
    }
  }
  return false;
}

bool OThreadCoAPResourceStore::create(const char *body, String &assignedId) {
  ResourceStoreImpl *impl = static_cast<ResourceStoreImpl *>(_impl);
  if (!impl) {
    return false;
  }
  ResourceStoreLock guard;
  if (!guard) {
    return false;
  }
  if (impl->items.size() >= impl->maxItems) {
    log_w("OThreadCoAPResourceStore: create() failed (store full, max %u items)", static_cast<unsigned>(impl->maxItems));
    return false;
  }
  ResourceStoreImpl::Entry entry;
  entry.id = String(impl->nextId++);
  entry.body = body ? body : "";
  impl->items.push_back(entry);
  assignedId = entry.id;
  if (impl->onChange) {
    impl->onChange(impl->basePath.c_str(), impl->changeContext);
  }
  return true;
}

bool OThreadCoAPResourceStore::update(const char *id, const char *body) {
  ResourceStoreImpl *impl = static_cast<ResourceStoreImpl *>(_impl);
  if (!impl || !id) {
    return false;
  }
  ResourceStoreLock guard;
  if (!guard) {
    return false;
  }
  for (auto &entry : impl->items) {
    if (entry.id == id) {
      entry.body = body ? body : "";
      if (impl->onChange) {
        impl->onChange(impl->basePath.c_str(), impl->changeContext);
      }
      return true;
    }
  }
  return false;
}

bool OThreadCoAPResourceStore::remove(const char *id) {
  ResourceStoreImpl *impl = static_cast<ResourceStoreImpl *>(_impl);
  if (!impl || !id) {
    return false;
  }
  ResourceStoreLock guard;
  if (!guard) {
    return false;
  }
  for (size_t i = 0; i < impl->items.size(); ++i) {
    if (impl->items[i].id == id) {
      impl->items.erase(impl->items.begin() + static_cast<long>(i));
      if (impl->onChange) {
        impl->onChange(impl->basePath.c_str(), impl->changeContext);
      }
      return true;
    }
  }
  return false;
}

static bool storeHandleRequest(OThreadCoAPResourceStore *store, RequestState &request, OThreadCoAPResponse &response) {
  if (!store) {
    return false;
  }
  // RFC 7252 §8.2 — do not answer group requests (same as on() / defaultRequestCb).
  if (request.multicast) {
    logMulticastWarn("ResourceStore", request.path.c_str(), request.confirmable, "CRUD store is unicast-only");
    return false;
  }
  ResourceStoreImpl *impl = nullptr;
  for (auto &pair : sResourceStores) {
    if (pair.first == store) {
      impl = pair.second;
      break;
    }
  }
  if (!impl || impl->basePath.isEmpty()) {
    return false;
  }
  ResourceStoreLock guard;
  if (!guard) {
    return false;
  }
  String path = request.path;
  if (path != impl->basePath && !path.startsWith(impl->basePath + "/")) {
    return false;
  }
  String id;
  if (path.length() > impl->basePath.length()) {
    id = path.substring(impl->basePath.length() + 1);
  }

  auto sendJson = [&](int code, const String &json) {
    if (json.length() > OT_COAP_MAX_PAYLOAD) {
      response.setCode(OT_COAP_RESP_INTERNAL_ERROR);
      response.setContentFormat(OT_COAP_FORMAT_JSON);
      response.setPayload("{\"error\":\"response too large\"}");
      response.send();
      return;
    }
    response.setCode(code);
    response.setContentFormat(OT_COAP_FORMAT_JSON);
    response.setPayload(json.c_str());
    response.send();
  };

  switch (request.method) {
    case OT_COAP_REQ_GET:
    {
      if (id.isEmpty()) {
        String json = "[";
        for (size_t i = 0; i < impl->items.size(); ++i) {
          if (i) {
            json += ",";
          }
          json += "{\"id\":\"";
          json += jsonEscape(impl->items[i].id);
          json += "\",\"body\":\"";
          json += jsonEscape(impl->items[i].body);
          json += "\"}";
        }
        json += "]";
        sendJson(OT_COAP_RESP_OK, json);
      } else {
        OThreadCoAPResourceStore::Item item;
        if (!store->getById(id.c_str(), item)) {
          sendJson(OT_COAP_RESP_NOT_FOUND, "{\"error\":\"not found\"}");
          return true;
        }
        String json = "{\"id\":\"";
        json += jsonEscape(item.id);
        json += "\",\"body\":\"";
        json += jsonEscape(item.body);
        json += "\"}";
        sendJson(OT_COAP_RESP_OK, json);
      }
      return true;
    }
    case OT_COAP_REQ_POST:
    {
      if (!id.isEmpty()) {
        sendJson(OT_COAP_RESP_BAD_REQUEST, "{\"error\":\"post on collection only\"}");
        return true;
      }
      String assigned;
      String body;
      body.reserve(request.payloadLen + 1);
      for (size_t i = 0; i < request.payloadLen; ++i) {
        body += static_cast<char>(request.payload[i]);
      }
      if (!store->create(body.c_str(), assigned)) {
        sendJson(OT_COAP_RESP_INTERNAL_ERROR, "{\"error\":\"create failed\"}");
        return true;
      }
      response.setCode(OT_COAP_RESP_CREATED);
      response.setContentFormat(OT_COAP_FORMAT_JSON);
      String location = impl->basePath + "/" + assigned;
      response.setLocation(location.c_str());
      String json = "{\"id\":\"";
      json += jsonEscape(assigned);
      json += "\"}";
      response.setPayload(json.c_str());
      response.send();
      return true;
    }
    case OT_COAP_REQ_PUT:
    {
      if (id.isEmpty()) {
        sendJson(OT_COAP_RESP_BAD_REQUEST, "{\"error\":\"missing id\"}");
        return true;
      }
      String body;
      body.reserve(request.payloadLen + 1);
      for (size_t i = 0; i < request.payloadLen; ++i) {
        body += static_cast<char>(request.payload[i]);
      }
      if (!store->update(id.c_str(), body.c_str())) {
        sendJson(OT_COAP_RESP_NOT_FOUND, "{\"error\":\"not found\"}");
        return true;
      }
      sendJson(OT_COAP_RESP_CHANGED, "{\"status\":\"updated\"}");
      return true;
    }
    case OT_COAP_REQ_DELETE:
    {
      if (id.isEmpty()) {
        sendJson(OT_COAP_RESP_BAD_REQUEST, "{\"error\":\"missing id\"}");
        return true;
      }
      if (!store->remove(id.c_str())) {
        sendJson(OT_COAP_RESP_NOT_FOUND, "{\"error\":\"not found\"}");
        return true;
      }
      sendJson(OT_COAP_RESP_DELETED, "{\"status\":\"deleted\"}");
      return true;
    }
    default: sendJson(OT_COAP_RESP_METHOD_NA, "{\"error\":\"method not allowed\"}"); return true;
  }
}

OThreadCoAPSecureClient::OThreadCoAPSecureClient() {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  (void)getSecureState(this, true);
#endif
}

OThreadCoAPSecureClient::~OThreadCoAPSecureClient() {
  disconnect();
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  removeSecureState(this);
#endif
}

void OThreadCoAPSecureClient::setPSK(const uint8_t *psk, size_t pskLength, const char *identity) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  if (!state) {
    return;
  }
  state->psk.clear();
  if (psk && pskLength) {
    state->psk.insert(state->psk.end(), psk, psk + pskLength);
  }
  state->identity = identity ? identity : "";
#else
  (void)psk;
  (void)pskLength;
  (void)identity;
#endif
}

void OThreadCoAPSecureClient::setCertificate(const char *certPem, size_t certLen, const char *keyPem, size_t keyLen, const char *caPem, size_t caLen) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  if (!state) {
    return;
  }
  state->certPem = certPem ? String(certPem).substring(0, certLen) : "";
  state->keyPem = keyPem ? String(keyPem).substring(0, keyLen) : "";
  state->caPem = caPem ? String(caPem).substring(0, caLen) : "";
#else
  (void)certPem;
  (void)certLen;
  (void)keyPem;
  (void)keyLen;
  (void)caPem;
  (void)caLen;
#endif
}

void OThreadCoAPSecureClient::setVerifyPeer(bool verify) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  if (state) {
    state->verifyPeer = verify;
  }
#else
  (void)verify;
#endif
}

void OThreadCoAPSecureClient::setTimeout(uint32_t timeoutMs) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  if (state) {
    state->timeoutMs = timeoutMs ? timeoutMs : OT_COAP_DEFAULT_TIMEOUT_MS;
  }
#else
  (void)timeoutMs;
#endif
}

void OThreadCoAPSecureClient::setConnectTimeout(uint32_t timeoutMs) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  if (state) {
    state->connectTimeoutMs = timeoutMs ? timeoutMs : OT_COAP_DEFAULT_TIMEOUT_MS;
  }
#else
  (void)timeoutMs;
#endif
}

void OThreadCoAPSecureClient::setConfirmable(bool confirmable) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  if (state) {
    state->confirmable = confirmable;
  }
#else
  (void)confirmable;
#endif
}

void OThreadCoAPSecureClient::setContentFormat(uint16_t format) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  if (state) {
    state->contentFormat = format;
  }
#else
  (void)format;
#endif
}

bool OThreadCoAPSecureClient::connect(const IPAddress &host, uint16_t port) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  otInstance *inst = OThread.getInstance();
  if (!state || !inst || !OThreadCoAP::isAttached()) {
    log_w("OThreadCoAPSecureClient: connect() failed (OpenThread not attached)");
    return false;
  }
  if (!state->connectSignal) {
    log_e("OThreadCoAPSecureClient: connect() failed (connect semaphore unavailable)");
    return false;
  }
  if (isSecureServerStackActive()) {
    log_w("OThreadCoAPSecureClient: connect() failed (OThreadCoAPSecureServer is active on this device; run the client on another node)");
    return false;
  }
  if (state->connected) {
    if (state->peer == host && state->peerPort == port) {
      return true;
    }
    log_w("OThreadCoAPSecureClient: connect() failed (already connected to %s:%u; disconnect() first)", state->peer.toString().c_str(), state->peerPort);
    return false;
  }
  if (state->active && !state->connected) {
    cleanupFailedSecureConnect(this, inst, state);
  }
  if (anySecureClientSessionActive(this)) {
    log_w("OThreadCoAPSecureClient: connect() failed (another secure client already has a CoAPS session)");
    return false;
  }
  xSemaphoreTake(state->connectSignal, 0);
  otError err = OT_ERROR_INVALID_STATE;
  for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
    OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
    if (!lock) {
      continue;
    }
    otCoapSecureSetSslAuthMode(inst, state->verifyPeer);
    if (!state->psk.empty() && state->identity.length()) {
      otCoapSecureSetPsk(
        inst, state->psk.data(), static_cast<uint16_t>(state->psk.size()), reinterpret_cast<const uint8_t *>(state->identity.c_str()),
        static_cast<uint16_t>(state->identity.length())
      );
    }
    if (state->certPem.length() && state->keyPem.length()) {
      otCoapSecureSetCertificate(
        inst, reinterpret_cast<const uint8_t *>(state->certPem.c_str()), state->certPem.length(), reinterpret_cast<const uint8_t *>(state->keyPem.c_str()),
        state->keyPem.length()
      );
      if (state->caPem.length()) {
        otCoapSecureSetCaCertificateChain(inst, reinterpret_cast<const uint8_t *>(state->caPem.c_str()), state->caPem.length());
      }
    }
    err = otCoapSecureStart(inst, OT_COAP_SECURE_DEFAULT_PORT);
    if (err != OT_ERROR_NONE && err != OT_ERROR_ALREADY) {
      log_e("OThreadCoAPSecureClient: connect() failed (otCoapSecureStart otError=%d)", static_cast<int>(err));
      return false;
    }
    otSockAddr peer = {};
    ipToOt(host, peer.mAddress);
    peer.mPort = port;
    state->peer = host;
    state->peerPort = port;
    state->connected = false;
    state->active = true;
    err = otCoapSecureConnect(inst, &peer, secureConnectEventCb, this);
    break;
  }
  if (err == OT_ERROR_INVALID_STATE) {
    log_w("OThreadCoAPSecureClient: connect() failed (could not acquire OpenThread lock)");
    return false;
  }
  if (err != OT_ERROR_NONE) {
    log_w("OThreadCoAPSecureClient: connect() failed (otCoapSecureConnect otError=%d)", static_cast<int>(err));
    cleanupFailedSecureConnect(this, inst, state);
    return false;
  }
  if (xSemaphoreTake(state->connectSignal, pdMS_TO_TICKS(state->connectTimeoutMs)) != pdTRUE) {
    log_w("OThreadCoAPSecureClient: connect() timed out waiting for %s:%u", host.toString().c_str(), port);
    cleanupFailedSecureConnect(this, inst, state);
    return false;
  }
  if (!state->connected) {
    log_w("OThreadCoAPSecureClient: connect() failed (DTLS handshake did not complete for %s:%u)", host.toString().c_str(), port);
    cleanupFailedSecureConnect(this, inst, state);
    return false;
  }
  sSecureClientOwner = this;
  return true;
#else
  (void)host;
  (void)port;
  log_w("OThreadCoAPSecureClient: connect() failed (CoAP secure API not enabled in build)");
  return false;
#endif
}

bool OThreadCoAPSecureClient::connect(const char *host, uint16_t port) {
  IPAddress ip;
  if (!parseIp(host, ip)) {
    log_w("OThreadCoAPSecureClient: connect() failed (invalid IPv6 host '%s')", host ? host : "(null)");
    return false;
  }
  return connect(ip, port);
}

void OThreadCoAPSecureClient::disconnect() {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, false);
  otInstance *inst = OThread.getInstance();
  if (!state) {
    return;
  }
  bool otUpdated = false;
  if (inst) {
    for (int attempt = 0; attempt < kOtLockStopAttempts; ++attempt) {
      OtLock lock(attempt == 0 ? portMAX_DELAY : pdMS_TO_TICKS(kOtLockStopRetryMs));
      if (!lock) {
        continue;
      }
      otCoapSecureDisconnect(inst);
      state->connected = false;
      state->active = false;
      if (sSecureClientOwner == this) {
        sSecureClientOwner = nullptr;
      }
      if (mayStopCoapSecureAfterClientDisconnect(this)) {
        otCoapSecureStop(inst);
        sSecureServerServiceHeld = false;
      }
      otUpdated = true;
      break;
    }
    if (!otUpdated) {
      log_w("OThreadCoAPSecureClient: disconnect() could not acquire OpenThread lock; session flags cleared locally");
    }
  }
  if (!otUpdated) {
    state->connected = false;
    state->active = false;
    if (sSecureClientOwner == this) {
      sSecureClientOwner = nullptr;
    }
  }
#endif
}

bool OThreadCoAPSecureClient::connected() const {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  const SecureClientState *state = getSecureState(this, false);
  return state ? state->connected : false;
#else
  return false;
#endif
}

bool OThreadCoAPSecureClient::isConnectionActive() const {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  const SecureClientState *state = getSecureState(this, false);
  return state ? state->active : false;
#else
  return false;
#endif
}

void OThreadCoAPSecureClient::onConnectEvent(ConnectCallback callback, void *context) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  if (state) {
    state->connectCb = callback;
    state->connectCtx = context;
  }
#else
  (void)callback;
  (void)context;
#endif
}

int OThreadCoAPSecureClient::GET(const char *path) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  otInstance *inst = OThread.getInstance();
  if (!state || !inst || !state->connected) {
    return OT_COAP_ERROR_NOT_CONNECTED;
  }
  SecureBlockingContext *ctx = new SecureBlockingContext();
  ctx->owner = this;
  ctx->done = xSemaphoreCreateBinary();
  if (!ctx->done) {
    delete ctx;
    return OT_COAP_ERROR_NO_BUFS;
  }
  otError err = OT_ERROR_INVALID_STATE;
  {
    OtLock lock;
    if (!lock) {
      return abortSecureBlockingClient("GET", ctx);
    }
    err = sendClientRequest(
      inst, true, state->confirmable, state->peer, state->peerPort, sanitizePath(path).c_str(), OT_COAP_REQ_GET, nullptr, 0, state->contentFormat,
      secureResponseCb, ctx, state->timeoutMs, true
    );
  }
  return waitBlockingSecure(ctx, err, state->timeoutMs);
#else
  (void)path;
  return OT_COAP_ERROR_NOT_CONNECTED;
#endif
}

int OThreadCoAPSecureClient::PUT(const char *path, const char *payload) {
  return PUT(path, reinterpret_cast<const uint8_t *>(payload), payload ? strlen(payload) : 0);
}

int OThreadCoAPSecureClient::PUT(const char *path, const uint8_t *payload, size_t length) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  otInstance *inst = OThread.getInstance();
  if (!state || !inst || !state->connected) {
    return OT_COAP_ERROR_NOT_CONNECTED;
  }
  SecureBlockingContext *ctx = new SecureBlockingContext();
  ctx->owner = this;
  ctx->done = xSemaphoreCreateBinary();
  if (!ctx->done) {
    delete ctx;
    return OT_COAP_ERROR_NO_BUFS;
  }
  otError err = OT_ERROR_INVALID_STATE;
  {
    OtLock lock;
    if (!lock) {
      return abortSecureBlockingClient("PUT", ctx);
    }
    err = sendClientRequest(
      inst, true, state->confirmable, state->peer, state->peerPort, sanitizePath(path).c_str(), OT_COAP_REQ_PUT, payload, length, state->contentFormat,
      secureResponseCb, ctx, state->timeoutMs, true
    );
  }
  return waitBlockingSecure(ctx, err, state->timeoutMs);
#else
  (void)path;
  (void)payload;
  (void)length;
  return OT_COAP_ERROR_NOT_CONNECTED;
#endif
}

int OThreadCoAPSecureClient::POST(const char *path, const char *payload) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  otInstance *inst = OThread.getInstance();
  if (!state || !inst || !state->connected) {
    return OT_COAP_ERROR_NOT_CONNECTED;
  }
  SecureBlockingContext *ctx = new SecureBlockingContext();
  ctx->owner = this;
  ctx->done = xSemaphoreCreateBinary();
  if (!ctx->done) {
    delete ctx;
    return OT_COAP_ERROR_NO_BUFS;
  }
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(payload);
  size_t len = payload ? strlen(payload) : 0;
  otError err = OT_ERROR_INVALID_STATE;
  {
    OtLock lock;
    if (!lock) {
      return abortSecureBlockingClient("POST", ctx);
    }
    err = sendClientRequest(
      inst, true, state->confirmable, state->peer, state->peerPort, sanitizePath(path).c_str(), OT_COAP_REQ_POST, bytes, len, state->contentFormat,
      secureResponseCb, ctx, state->timeoutMs, true
    );
  }
  return waitBlockingSecure(ctx, err, state->timeoutMs);
#else
  (void)path;
  (void)payload;
  return OT_COAP_ERROR_NOT_CONNECTED;
#endif
}

int OThreadCoAPSecureClient::DELETE(const char *path) {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  SecureClientState *state = getSecureState(this, true);
  otInstance *inst = OThread.getInstance();
  if (!state || !inst || !state->connected) {
    return OT_COAP_ERROR_NOT_CONNECTED;
  }
  SecureBlockingContext *ctx = new SecureBlockingContext();
  ctx->owner = this;
  ctx->done = xSemaphoreCreateBinary();
  if (!ctx->done) {
    delete ctx;
    return OT_COAP_ERROR_NO_BUFS;
  }
  otError err = OT_ERROR_INVALID_STATE;
  {
    OtLock lock;
    if (!lock) {
      return abortSecureBlockingClient("DELETE", ctx);
    }
    err = sendClientRequest(
      inst, true, state->confirmable, state->peer, state->peerPort, sanitizePath(path).c_str(), OT_COAP_REQ_DELETE, nullptr, 0, state->contentFormat,
      secureResponseCb, ctx, state->timeoutMs, true
    );
  }
  return waitBlockingSecure(ctx, err, state->timeoutMs);
#else
  (void)path;
  return OT_COAP_ERROR_NOT_CONNECTED;
#endif
}

int OThreadCoAPSecureClient::responseCode() const {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  const SecureClientState *state = getSecureState(this, false);
  return state ? state->lastCode : OT_COAP_ERROR_NOT_CONNECTED;
#else
  return OT_COAP_ERROR_NOT_CONNECTED;
#endif
}

String OThreadCoAPSecureClient::getString() const {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  const SecureClientState *state = getSecureState(this, false);
  if (!state) {
    return String();
  }
  String out;
  out.reserve(state->payloadLen + 1);
  for (size_t i = 0; i < state->payloadLen; ++i) {
    out += static_cast<char>(state->payload[i]);
  }
  return out;
#else
  return String();
#endif
}

size_t OThreadCoAPSecureClient::readPayload(uint8_t *buffer, size_t length) const {
#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
  const SecureClientState *state = getSecureState(this, false);
  if (!state || !buffer || length == 0) {
    return 0;
  }
  size_t n = std::min(length, state->payloadLen);
  memcpy(buffer, state->payload, n);
  return n;
#else
  (void)buffer;
  (void)length;
  return 0;
#endif
}

namespace othread_coap_detail {

struct PlainCoAPServerHolder {
  OThreadCoAPServerClass instance;
  PlainCoAPServerHolder() : instance(OT_COAP_DEFAULT_PORT) {}
};

struct SecureCoAPServerHolder {
  OThreadCoAPSecureServerClass instance;
  SecureCoAPServerHolder() : instance(OT_COAP_SECURE_DEFAULT_PORT) {}
};

PlainCoAPServerHolder gPlainCoAPServer;
SecureCoAPServerHolder gSecureCoAPServer;

}  // namespace othread_coap_detail

OThreadCoAPServerClass &OThreadCoAPServer = othread_coap_detail::gPlainCoAPServer.instance;
OThreadCoAPSecureServerClass &OThreadCoAPSecureServer = othread_coap_detail::gSecureCoAPServer.instance;

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
