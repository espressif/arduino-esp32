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

#if SOC_IEEE802154_SUPPORTED && CONFIG_OPENTHREAD_ENABLED

#include <Arduino.h>
#include <IPAddress.h>
#include "OThread.h"

#ifndef OT_COAP_DEFAULT_PORT
#define OT_COAP_DEFAULT_PORT 5683
#endif

#ifndef OT_COAP_SECURE_DEFAULT_PORT
#define OT_COAP_SECURE_DEFAULT_PORT 5684
#endif

#ifndef OT_COAP_DEFAULT_TIMEOUT_MS
#define OT_COAP_DEFAULT_TIMEOUT_MS 5000
#endif

// OpenThread OT_COAP_MIN_ACK_TIMEOUT (coap.h) — shortest CON ACK spacing on the wire.
#ifndef OT_COAP_MIN_ACK_TIMEOUT_MS
#define OT_COAP_MIN_ACK_TIMEOUT_MS 1000
#endif

// Shortest setTimeout() honored in aligned CON mode (min ACK × RFC ACK_RANDOM_FACTOR 1.5).
#ifndef OT_COAP_MIN_ALIGNED_TIMEOUT_MS
#define OT_COAP_MIN_ALIGNED_TIMEOUT_MS ((OT_COAP_MIN_ACK_TIMEOUT_MS * 3) / 2)
#endif

#ifndef OT_COAP_MAX_PAYLOAD
#define OT_COAP_MAX_PAYLOAD 512
#endif

#ifndef OT_COAP_MAX_RESOURCES
#define OT_COAP_MAX_RESOURCES 4
#endif

// Response codes (class*100+detail). Do not use OT_COAP_CODE_* enum names here —
// they are defined in OpenThread coap.h and break compilation if redefined as macros.
#define OT_COAP_RESP_OK             205
#define OT_COAP_RESP_CREATED        201
#define OT_COAP_RESP_CHANGED        204
#define OT_COAP_RESP_DELETED        202
#define OT_COAP_RESP_BAD_REQUEST    400
#define OT_COAP_RESP_NOT_FOUND      404
#define OT_COAP_RESP_METHOD_NA      405
#define OT_COAP_RESP_INTERNAL_ERROR 500

#define OT_COAP_ERROR_TIMEOUT       (-11)
#define OT_COAP_ERROR_NO_RESPONSE   (-12)
#define OT_COAP_ERROR_NOT_ATTACHED  (-13)
#define OT_COAP_ERROR_NO_BUFS       (-14)
#define OT_COAP_ERROR_INVALID_ARGS  (-15)
#define OT_COAP_ERROR_NOT_CONNECTED (-16)
#define OT_COAP_ERROR_TLS_FAILED    (-17)
#define OT_COAP_ERROR_INVALID_STATE (-18)  // CoAP service or OT lock not ready (not role attachment)

#define OT_COAP_SECURE_CONNECTED                 0
#define OT_COAP_SECURE_DISCONNECTED_PEER         1
#define OT_COAP_SECURE_DISCONNECTED_LOCAL        2
#define OT_COAP_SECURE_DISCONNECTED_MAX_ATTEMPTS 3
#define OT_COAP_SECURE_DISCONNECTED_ERROR        4
#define OT_COAP_SECURE_DISCONNECTED_TIMEOUT      5

#define OT_COAP_METHOD_GET    (1 << 0)
#define OT_COAP_METHOD_POST   (1 << 1)
#define OT_COAP_METHOD_PUT    (1 << 2)
#define OT_COAP_METHOD_DELETE (1 << 3)

// Request method values returned by OThreadCoAPRequest::method().
#define OT_COAP_REQ_GET    1
#define OT_COAP_REQ_POST   2
#define OT_COAP_REQ_PUT    3
#define OT_COAP_REQ_DELETE 4

#define OT_COAP_CONFIRMABLE     1
#define OT_COAP_NON_CONFIRMABLE 0

// IANA CoAP Content-Format codes (mirrors otCoapOptionContentFormat in ESP-IDF coap.h).
// Full registry: https://www.iana.org/assignments/core-parameters/core-parameters.xhtml#content-formats
#define OT_COAP_FORMAT_TEXT             0    // text/plain; charset=utf-8
#define OT_COAP_FORMAT_COSE_ENCRYPT0    16   // application/cose; cose-type="cose-encrypt0"
#define OT_COAP_FORMAT_COSE_MAC0        17   // application/cose; cose-type="cose-mac0"
#define OT_COAP_FORMAT_COSE_SIGN1       18   // application/cose; cose-type="cose-sign1"
#define OT_COAP_FORMAT_LINK             40   // application/link-format
#define OT_COAP_FORMAT_XML              41   // application/xml
#define OT_COAP_FORMAT_OCTET_STREAM     42   // application/octet-stream
#define OT_COAP_FORMAT_EXI              47   // application/exi
#define OT_COAP_FORMAT_JSON             50   // application/json
#define OT_COAP_FORMAT_JSON_PATCH       51   // application/json-patch+json
#define OT_COAP_FORMAT_MERGE_PATCH_JSON 52   // application/merge-patch+json
#define OT_COAP_FORMAT_CBOR             60   // application/cbor
#define OT_COAP_FORMAT_CWT              61   // application/cwt
#define OT_COAP_FORMAT_COSE_ENCRYPT     96   // application/cose; cose-type="cose-encrypt"
#define OT_COAP_FORMAT_COSE_MAC         97   // application/cose; cose-type="cose-mac"
#define OT_COAP_FORMAT_COSE_SIGN        98   // application/cose; cose-type="cose-sign"
#define OT_COAP_FORMAT_COSE_KEY         101  // application/cose-key
#define OT_COAP_FORMAT_COSE_KEY_SET     102  // application/cose-key-set
#define OT_COAP_FORMAT_SENML_JSON       110  // application/senml+json
#define OT_COAP_FORMAT_SENSML_JSON      111  // application/sensml+json
#define OT_COAP_FORMAT_SENML_CBOR       112  // application/senml+cbor
#define OT_COAP_FORMAT_SENSML_CBOR      113  // application/sensml+cbor
#define OT_COAP_FORMAT_SENML_EXI        114  // application/senml-exi
#define OT_COAP_FORMAT_SENSML_EXI       115  // application/sensml-exi
#define OT_COAP_FORMAT_COAP_GROUP_JSON  256  // application/coap-group+json
#define OT_COAP_FORMAT_SENML_XML        310  // application/senml+xml
#define OT_COAP_FORMAT_SENSML_XML       311  // application/sensml+xml

class OThreadCoAPRequest;
class OThreadCoAPResponse;
class OThreadCoAPClient;
class OThreadCoAPServerClass;
class OThreadCoAPSecureClient;
class OThreadCoAPSecureServerClass;
class OThreadCoAPResourceStore;

/**
 * @brief Callback invoked when a registered server resource receives a request.
 * @param request   Incoming request (path, method, payload, peer address).
 * @param response  Response to populate; call response.send() before returning.
 * @param context   User pointer supplied to on()/onNotFound().
 *
 * @note Auto-response: if the handler returns without calling response.send() and
 * the request was a unicast CON or GET, the server automatically sends a 2.05
 * Content with no payload (so a confirmable client is never left waiting). This
 * means a handler that intends an error (4.xx/5.xx) MUST call response.send()
 * itself — otherwise the failure is silently reported as success. Multicast
 * requests are never auto-answered (RFC 7252 §8.2); see isMulticast().
 */
typedef void (*OThreadCoAPHandler)(OThreadCoAPRequest &request, OThreadCoAPResponse &response, void *context);

/**
 * @brief Static helpers shared by the OThreadCoAP client/server classes.
 * @note Implementation lives in `OThreadCoAP.cpp`.
 */
class OThreadCoAP {
public:
  /**
   * @brief Whether the Thread device is attached and able to send CoAP traffic.
   * @return true when the device role is Child or higher.
   */
  static bool isAttached();

  /**
   * @brief Whether CoAPS (DTLS) was compiled into the OpenThread stack and wrapper.
   * @return true when OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE is set at build time.
   */
  static bool secureApiEnabled();

  /**
   * @brief Convert a CoAP response code to a human-readable string.
   * @param code Response code such as OT_COAP_RESP_OK (205).
   * @return Static string, e.g. "2.05 Content"; "unknown" for unmapped codes.
   */
  static const char *responseCodeToString(int code);

  /**
   * @brief Convert a negative OT_COAP_ERROR_* value to a human-readable string.
   * @param error Negative error code returned by client calls.
   * @return Static string, e.g. "timeout"; "unknown" for unmapped values.
   */
  static const char *errorToString(int error);

  /**
   * @brief The singleton plain CoAP resource server (port 5683).
   * @return Same object as the global OThreadCoAPServer.
   */
  static OThreadCoAPServerClass &plainServer();

  /**
   * @brief The singleton CoAPS resource server (port 5684).
   * @return Same object as the global OThreadCoAPSecureServer.
   */
  static OThreadCoAPSecureServerClass &secureServer();

private:
  // Internal: reclaim blocking-request contexts that were left in flight when the
  // OpenThread worker task was torn down (their response handlers can no longer
  // fire). Called by OpenThread::end() as part of stack teardown; a no-op if CoAP
  // was never used. Not part of the public API. @p stackInstance is the live
  // otInstance when OpenThread::end() tears the stack down (before deinit).
  static void releaseAllPending(otInstance *stackInstance = nullptr);
  friend class OpenThread;
};

// Internal CoAP request-dispatch callbacks, implemented in OThreadCoAP.cpp and
// registered with OpenThread (otCoapRequestHandler). They build the request and
// response views below, so they are declared here to be friends of those classes.
// Not part of the public API; do not call from sketches.
namespace othread_coap_detail {
void resourceRequestCb(void *context, otMessage *message, const otMessageInfo *messageInfo);
void defaultRequestCb(void *context, otMessage *message, const otMessageInfo *messageInfo);
struct PlainCoAPServerHolder;
struct SecureCoAPServerHolder;
}  // namespace othread_coap_detail

/**
 * @brief Read-only view of an incoming CoAP request passed to a server handler.
 *
 * Instances are created by the server and are only valid for the duration of the
 * handler call. Do not store the reference.
 */
class OThreadCoAPRequest {
public:
  /** @brief URI path of the request, without leading slash (e.g. "notes/1"). */
  const char *path() const;

  /** @brief Request method as one of OT_COAP_REQ_GET/POST/PUT/DELETE. */
  int method() const;

  /** @brief IPv6 address of the requesting peer. */
  IPAddress remoteIP() const;

  /** @brief UDP source port of the requesting peer. */
  uint16_t remotePort() const;

  /** @brief Length in bytes of the request payload. */
  size_t payloadLength() const;

  /**
   * @brief Copy the request payload into a caller buffer.
   * @param buffer Destination buffer.
   * @param length Capacity of @p buffer in bytes.
   * @return Number of bytes copied (at most @p length).
   */
  size_t readPayload(uint8_t *buffer, size_t length) const;

  /** @brief Request payload returned as an Arduino String. */
  String payloadString() const;

  /** @brief true if the request was confirmable (CON), false for non-confirmable (NON). */
  bool isConfirmable() const;

  /**
   * @brief true if the request was addressed to an IPv6 multicast group.
   *
   * Multicast requests are fire-and-forget commands that may reach many servers
   * at once. Per RFC 7252 §8.2 a handler should usually NOT send a response to a
   * multicast request, to avoid a response storm when several nodes answer the
   * same datagram. Use this to skip response.send() for group commands.
   */
  bool isMulticast() const;

private:
  const void *_internal;
  explicit OThreadCoAPRequest(const void *internal);
  friend class OThreadCoAPServerClass;
  friend class OThreadCoAPSecureServerClass;
  friend class OThreadCoAPResourceStore;
  friend void othread_coap_detail::resourceRequestCb(void *, otMessage *, const otMessageInfo *);
  friend void othread_coap_detail::defaultRequestCb(void *, otMessage *, const otMessageInfo *);
};

/**
 * @brief Response builder passed to a server handler.
 *
 * Configure the code/payload/options, then call send() exactly once before the
 * handler returns. The instance is only valid for the duration of the handler.
 */
class OThreadCoAPResponse {
public:
  /**
   * @brief Set the CoAP response code.
   * @param code One of the OT_COAP_RESP_* constants (e.g. OT_COAP_RESP_OK).
   */
  void setCode(int code);

  /**
   * @brief Set the Content-Format option of the response payload.
   * @param format IANA Content-Format code — use OT_COAP_FORMAT_* constants.
   *        See openthread_coap.rst for the full ESP-IDF list. Any uint16_t IANA
   *        code is accepted; codes not in the enum still work at the wire level.
   */
  void setContentFormat(uint16_t format);

  /**
   * @brief Set a null-terminated text payload.
   * @param text Null-terminated string copied into the response.
   */
  void setPayload(const char *text);

  /**
   * @brief Set a binary payload.
   * @param data   Source bytes copied into the response.
   * @param length Number of bytes from @p data.
   */
  void setPayload(const uint8_t *data, size_t length);

  /**
   * @brief Set the Location-Path option (e.g. the URI of a created resource).
   * @param path Location path without leading slash.
   */
  void setLocation(const char *path);

  /**
   * @brief Set the Max-Age option (response cache lifetime).
   * @param seconds Lifetime in seconds.
   */
  void setMaxAge(uint32_t seconds);

  /** @brief Send the response to the requesting peer. Call once per handler. */
  void send();

private:
  void *_internal;
  explicit OThreadCoAPResponse(void *internal);
  friend class OThreadCoAPServerClass;
  friend class OThreadCoAPSecureServerClass;
  friend class OThreadCoAPResourceStore;
  friend void othread_coap_detail::resourceRequestCb(void *, otMessage *, const otMessageInfo *);
  friend void othread_coap_detail::defaultRequestCb(void *, otMessage *, const otMessageInfo *);
};

/**
 * @brief Blocking, HTTPClient-style client over plain CoAP (UDP, default 5683).
 *
 * Each request method blocks until a response arrives or setTimeout() elapses,
 * then returns the CoAP response code (>= 0) or a negative OT_COAP_ERROR_*.
 * For confirmable requests, wire retransmission is tuned to match setTimeout()
 * by default (see useDefaultCoapRetransmit()).
 * Reuse a single instance for many requests. Requires the device to be attached.
 */
class OThreadCoAPClient {
public:
  /** @brief Construct a client with default port, timeout, and CON mode. */
  OThreadCoAPClient();
  ~OThreadCoAPClient();

  /**
   * @brief Set how long subsequent blocking requests wait for a response.
   * @param timeoutMs Maximum wait in milliseconds (default OT_COAP_DEFAULT_TIMEOUT_MS).
   *
   * Values are in **milliseconds** (same as Arduino `HTTPClient::setTimeout()` and
   * OpenThread `otCoapTxParameters::mAckTimeout`). Use e.g. `5000` for five seconds.
   *
   * For confirmable (CON) plain CoAP requests, CON retransmission is derived from
   * this limit so stack and sketch give up together. Values below
   * OT_COAP_MIN_ALIGNED_TIMEOUT_MS (1500 ms by default — one CON attempt at
   * OT_COAP_MIN_ACK_TIMEOUT_MS × 1.5) are raised automatically unless
   * @ref useDefaultCoapRetransmit() was called first.
   *
   * Call @ref useDefaultCoapRetransmit() before a short timeout if you need a
   * sketch wait below that floor while keeping RFC wire retries.
   *
   * CoAPS (OThreadCoAPSecureClient): @p timeoutMs is the sketch wait cap only.
   */
  void setTimeout(uint32_t timeoutMs);

  /**
   * @brief Use RFC 7252 default CON retransmit timing instead of aligning with setTimeout().
   *
   * After this call, setTimeout() limits only how long blocking methods wait in
   * the sketch. OpenThread may keep retransmitting a confirmable request in the
   * background after OT_COAP_ERROR_TIMEOUT is returned.
   */
  void useDefaultCoapRetransmit();

  /**
   * @brief Choose confirmable (CON) or non-confirmable (NON) for next request(s).
   * @param confirmable true for CON (reliable, default), false for NON.
   */
  void setConfirmable(bool confirmable);

  /**
   * @brief Set the destination UDP port for subsequent requests.
   * @param port Destination port (default OT_COAP_DEFAULT_PORT, 5683).
   */
  void setPort(uint16_t port);

  /**
   * @brief Set the Content-Format option for the next request payload(s).
   * @param format IANA Content-Format code — use OT_COAP_FORMAT_* constants
   *        (default OT_COAP_FORMAT_TEXT). Ignored when the request has no payload.
   */
  void setContentFormat(uint16_t format);

  /**
   * @brief Perform a blocking GET.
   * @param host IPv6 address (unicast or multicast) of the server.
   * @param path Resource path without leading slash.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int GET(const IPAddress &host, const char *path);

  /**
   * @brief Perform a blocking GET, resolving @p host as an IPv6 string.
   * @param host IPv6 address string.
   * @param path Resource path without leading slash.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int GET(const char *host, const char *path);

  /**
   * @brief Perform a blocking PUT with a text payload.
   * @param host    Server IPv6 address.
   * @param path    Resource path without leading slash.
   * @param payload Null-terminated text payload.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int PUT(const IPAddress &host, const char *path, const char *payload);

  /**
   * @brief Perform a blocking PUT with a binary payload.
   * @param host    Server IPv6 address.
   * @param path    Resource path without leading slash.
   * @param payload Source bytes.
   * @param length  Number of bytes from @p payload.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int PUT(const IPAddress &host, const char *path, const uint8_t *payload, size_t length);

  /**
   * @brief Perform a blocking POST with a text payload.
   * @param host    Server IPv6 address.
   * @param path    Resource path without leading slash.
   * @param payload Null-terminated text payload.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int POST(const IPAddress &host, const char *path, const char *payload);

  /**
   * @brief Perform a blocking POST with a binary payload.
   * @param host    Server IPv6 address.
   * @param path    Resource path without leading slash.
   * @param payload Source bytes.
   * @param length  Number of bytes from @p payload.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int POST(const IPAddress &host, const char *path, const uint8_t *payload, size_t length);

  /**
   * @brief Perform a blocking DELETE.
   * @param host Server IPv6 address.
   * @param path Resource path without leading slash.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int DELETE(const IPAddress &host, const char *path);

  /**
   * @brief Fire-and-forget NON request: send and return without awaiting a reply.
   *
   * Always uses a non-confirmable (NON) message and never blocks. Ideal for
   * multicast commands (which must be NON per RFC 7252 §8.1 and may have 0..N
   * responders) and for unicast telemetry where the reply is not needed. Any
   * response that arrives is ignored.
   *
   * The blocking GET/PUT/POST/DELETE already auto-downgrade multicast requests to
   * NON and wait for the first responder; use this instead when you do not want
   * to block at all.
   * @param host    Destination IPv6 address (unicast or multicast).
   * @param method  One of OT_COAP_REQ_GET/POST/PUT/DELETE.
   * @param path    Resource path without leading slash.
   * @param payload Source bytes (optional).
   * @param length  Number of bytes from @p payload.
   * @return true if the request was queued for transmission.
   */
  bool sendNonBlocking(const IPAddress &host, uint8_t method, const char *path, const uint8_t *payload = nullptr, size_t length = 0);

  /**
   * @brief Fire-and-forget NON request with a text payload. See sendNonBlocking().
   * @param host    Destination IPv6 address (unicast or multicast).
   * @param method  One of OT_COAP_REQ_GET/POST/PUT/DELETE.
   * @param path    Resource path without leading slash.
   * @param payload Null-terminated text payload.
   * @return true if the request was queued for transmission.
   */
  bool sendNonBlocking(const IPAddress &host, uint8_t method, const char *path, const char *payload);

  /** @brief Response code from the most recent request. */
  int responseCode() const;

  /** @brief Length in bytes of the most recent response payload. */
  size_t payloadLength() const;

  /**
   * @brief Copy the most recent response payload into a caller buffer.
   * @param buffer Destination buffer.
   * @param length Capacity of @p buffer in bytes.
   * @return Number of bytes copied (at most @p length).
   */
  size_t readPayload(uint8_t *buffer, size_t length) const;

  /** @brief Most recent response payload as an Arduino String. */
  String getString() const;

  /** @brief IPv6 address that produced the most recent response. */
  IPAddress remoteIP() const;

private:
  uint32_t _timeoutMs;
  uint16_t _port;
  bool _confirmable;
  bool _useRfcRetransmit;
  uint16_t _contentFormat;
  int _lastCode;
  IPAddress _remoteIp;
  uint8_t _payload[OT_COAP_MAX_PAYLOAD];
  size_t _payloadLen;
};

/**
 * @brief WebServer-style resource server over plain CoAP.
 *
 * Register handlers with on(); they run on the OpenThread worker task when a
 * matching request arrives (no loop() polling). Keep handlers short.
 *
 * @note OpenThread allows **one plain CoAP server per device** (one UDP bind on
 * port 5683 by default). Use the library singleton global ``OThreadCoAPServer``
 * (or ``OThreadCoAP::plainServer()``) — do not construct this class. Register
 * many URI paths on that server with on(), serve(), and OThreadCoAPResourceStore.
 * Multiple OThreadCoAPClient instances are fine. Plain ``OThreadCoAPServer`` and
 * ``OThreadCoAPSecureServer`` (5684) may run together (see CoAP_Greenhouse).
 */
class OThreadCoAPServerClass {
public:
  /**
   * @brief Start the CoAP server and begin dispatching to registered handlers.
   *
   * Idempotent: if already running, re-registers resources and the default handler
   * on the current OpenThread instance (safe after stack recovery).
   * @return true on success.
   */
  bool begin();

  /**
   * @brief Stop this server and unregister its resources.
   *
   * Stops the shared plain CoAP UDP socket only when no plain client lazy-start
   * and no other holder still needs it.
   */
  void stop();

  /** @brief true if the server is running. */
  operator bool() const;

  /**
   * @brief Join an IPv6 multicast group so the server receives requests sent to it.
   *
   * Required to serve a multicast resource (e.g. a group like ff03::abcd): the
   * Thread interface does not receive datagrams for custom groups until it
   * subscribes. Requires begin() to have succeeded first. Groups joined here are
   * automatically left by stop(). Delegates to OThread.subscribeMulticast().
   * @param group IPv6 multicast address (first byte 0xFF).
   * @return true on success or if already joined; false if begin() was not called.
   */
  bool joinMulticastGroup(const IPAddress &group);

  /**
   * @brief Leave a multicast group previously joined with joinMulticastGroup().
   * @param group IPv6 multicast address.
   * @return true on success.
   */
  bool leaveMulticastGroup(const IPAddress &group);

  /**
   * @brief Register or unregister a handler for a URI path.
   * @param path       Resource path without leading slash (e.g. "hello").
   * @param methodMask Bitmask of OT_COAP_METHOD_* this handler accepts (ignored when unregistering).
   * @param handler    Callback invoked on a matching request, or @c nullptr to unregister @p path
   *                   (similar to WebServer::removeRoute(); there is no separate off() API).
   * @param context    Optional user pointer passed back to @p handler.
   * @return true on success (false if the resource table is full or OpenThread rejected the change).
   */
  bool on(const char *path, uint8_t methodMask, OThreadCoAPHandler handler, void *context = nullptr);

  /**
   * @brief Register a default handler for requests that match no path.
   * @param handler Callback invoked for unknown paths, or @c nullptr to disable.
   * @param context Optional user pointer passed back to @p handler.
   * @return true on success.
   */
  bool onNotFound(OThreadCoAPHandler handler, void *context = nullptr);

  /**
   * @brief Expose a String variable as a GET/PUT resource (convenience sugar).
   * @param path         Resource path without leading slash.
   * @param valuePointer Pointer to the backing String (must outlive the server).
   * @return true on success.
   */
  bool serve(const char *path, String *valuePointer);

  /**
   * @brief Update the value of a serve()-backed resource.
   *
   * Thread-safe: the write is serialized with the server handler, which reads the
   * value on the OpenThread task. Always update a serve()-backed String through
   * this method — assigning the backing String directly from loop() races the
   * handler and can corrupt the response.
   * @param path  Path previously registered with serve().
   * @param value New value.
   * @return true if the path was found and updated.
   */
  bool setResourceValue(const char *path, const String &value);

private:
  explicit OThreadCoAPServerClass(uint16_t port = OT_COAP_DEFAULT_PORT);
  ~OThreadCoAPServerClass();
  OThreadCoAPServerClass(const OThreadCoAPServerClass &) = delete;
  OThreadCoAPServerClass &operator=(const OThreadCoAPServerClass &) = delete;

  void *_impl;
  friend struct othread_coap_detail::PlainCoAPServerHolder;
  friend class OThreadCoAPResourceStore;
  friend void resetPlainServerWrapperState(OThreadCoAPServerClass &server);
  friend void clearResourceStoreLink(OThreadCoAPServerClass *server, OThreadCoAPResourceStore *store);
  friend void detachResourceStoresFromServer(OThreadCoAPServerClass *server);
};

/**
 * @brief Blocking CoAP-over-DTLS (CoAPS) client (UDP, default 5684).
 *
 * Set credentials (PSK or certificate), connect() once, then issue path-only
 * requests; the connected peer is the request target. Requires a build with
 * OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE.
 *
 * @note OpenThread supports **one active CoAPS client session** per device.
 * Only one OThreadCoAPSecureClient may be connected at a time; create multiple
 * client objects if needed, but connect() one before using the next.
 *
 * @note Do **not** call connect() on the same device that is running
 * ``OThreadCoAPSecureServer`` — OpenThread shares one CoAPS stack and credentials
 * per instance. Use the secure server on one node and the secure client on another
 * (see CoAP_Secure / CoAP_Greenhouse).
 */
class OThreadCoAPSecureClient {
public:
  /**
   * @brief DTLS connection state callback.
   * @param event   One of the OT_COAP_SECURE_* event constants.
   * @param context User pointer supplied to onConnectEvent().
   */
  typedef void (*ConnectCallback)(int event, void *context);

  OThreadCoAPSecureClient();
  ~OThreadCoAPSecureClient();

  /**
   * @brief Use a pre-shared key for the DTLS handshake.
   * @param psk       PSK bytes.
   * @param pskLength Length of @p psk in bytes.
   * @param identity  PSK identity string.
   */
  void setPSK(const uint8_t *psk, size_t pskLength, const char *identity);

  /**
   * @brief Use an X.509 certificate for the DTLS handshake.
   * @param certPem Client certificate (PEM).
   * @param certLen Length of @p certPem.
   * @param keyPem  Private key (PEM).
   * @param keyLen  Length of @p keyPem.
   * @param caPem   Optional CA certificate (PEM) to verify the peer.
   * @param caLen   Length of @p caPem.
   */
  void setCertificate(const char *certPem, size_t certLen, const char *keyPem, size_t keyLen, const char *caPem = nullptr, size_t caLen = 0);

  /**
   * @brief Enable or disable verification of the peer certificate.
   * @param verify true to require a valid peer certificate.
   */
  void setVerifyPeer(bool verify);

  /**
   * @brief Set how long subsequent blocking requests wait for a response.
   * @param timeoutMs Maximum wait in milliseconds.
   *
   * Sketch wait cap only. CoAPS wire retries use OpenThread stack defaults and
   * are not aligned with @p timeoutMs (unlike plain ``OThreadCoAPClient``).
   */
  void setTimeout(uint32_t timeoutMs);

  /**
   * @brief Set the timeout for the DTLS handshake in connect().
   * @param timeoutMs Timeout in milliseconds.
   */
  void setConnectTimeout(uint32_t timeoutMs);

  /**
   * @brief Choose confirmable (CON) or non-confirmable (NON) for next request(s).
   * @param confirmable true for CON (default), false for NON.
   */
  void setConfirmable(bool confirmable);

  /**
   * @brief Set the Content-Format option for the next request payload(s).
   * @param format IANA Content-Format code — use OT_COAP_FORMAT_* constants.
   */
  void setContentFormat(uint16_t format);

  /**
   * @brief Open a DTLS connection to a server.
   * @param host Server IPv6 address.
   * @param port Server port (default OT_COAP_SECURE_DEFAULT_PORT, 5684).
   * @return true once connected.
   */
  bool connect(const IPAddress &host, uint16_t port = OT_COAP_SECURE_DEFAULT_PORT);

  /**
   * @brief Open a DTLS connection, resolving @p host as an IPv6 string.
   * @param host Server IPv6 address string.
   * @param port Server port (default 5684).
   * @return true once connected.
   */
  bool connect(const char *host, uint16_t port = OT_COAP_SECURE_DEFAULT_PORT);

  /** @brief Close the DTLS connection. */
  void disconnect();

  /** @brief true if the DTLS session is established. */
  bool connected() const;

  /** @brief true if a connection is active (established or in progress). */
  bool isConnectionActive() const;

  /**
   * @brief Register a connection-state callback.
   * @param callback Invoked on connect/disconnect events.
   * @param context  Optional user pointer passed back to @p callback.
   */
  void onConnectEvent(ConnectCallback callback, void *context = nullptr);

  /**
   * @brief Blocking GET to the connected peer.
   * @param path Resource path without leading slash.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int GET(const char *path);

  /**
   * @brief Blocking PUT with a text payload to the connected peer.
   * @param path    Resource path without leading slash.
   * @param payload Null-terminated text payload.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int PUT(const char *path, const char *payload);

  /**
   * @brief Blocking PUT with a binary payload to the connected peer.
   * @param path    Resource path without leading slash.
   * @param payload Source bytes.
   * @param length  Number of bytes from @p payload.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int PUT(const char *path, const uint8_t *payload, size_t length);

  /**
   * @brief Blocking POST with a text payload to the connected peer.
   * @param path    Resource path without leading slash.
   * @param payload Null-terminated text payload.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int POST(const char *path, const char *payload);

  /**
   * @brief Blocking DELETE to the connected peer.
   * @param path Resource path without leading slash.
   * @return CoAP response code (>= 0) or negative OT_COAP_ERROR_*.
   */
  int DELETE(const char *path);

  /** @brief Response code from the most recent request. */
  int responseCode() const;

  /** @brief Most recent response payload as an Arduino String. */
  String getString() const;

  /**
   * @brief Copy the most recent response payload into a caller buffer.
   * @param buffer Destination buffer.
   * @param length Capacity of @p buffer in bytes.
   * @return Number of bytes copied (at most @p length).
   */
  size_t readPayload(uint8_t *buffer, size_t length) const;
};

/**
 * @brief CoAP-over-DTLS (CoAPS) resource server (UDP, default 5684).
 *
 * Set credentials, register handlers with on(), then begin(). Can run alongside
 * the plain ``OThreadCoAPServer`` on the same device. Requires a build with
 * OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE.
 *
 * Plain-only server helpers (onNotFound, serve, joinMulticastGroup) are not
 * exposed here — use ``OThreadCoAPServer`` for those patterns or register handlers
 * manually on both transports.
 *
 * @note Use the library singleton global ``OThreadCoAPSecureServer``
 * (or ``OThreadCoAP::secureServer()``) — do not construct this class. Register
 * many paths with on() on that server.
 *
 * @note ``begin()`` fails while an ``OThreadCoAPSecureClient`` session is active
 * on the same device (mirror of the client ``connect()`` guard).
 */
class OThreadCoAPSecureServerClass {
public:
  /**
   * @brief Accept clients presenting this pre-shared key.
   * @param psk       PSK bytes.
   * @param pskLength Length of @p psk in bytes.
   * @param identity  PSK identity string.
   */
  void setPSK(const uint8_t *psk, size_t pskLength, const char *identity);

  /**
   * @brief Use an X.509 certificate to authenticate the server (and optionally peers).
   * @param certPem Server certificate (PEM).
   * @param certLen Length of @p certPem.
   * @param keyPem  Private key (PEM).
   * @param keyLen  Length of @p keyPem.
   * @param caPem   Optional CA certificate (PEM) to verify clients.
   * @param caLen   Length of @p caPem.
   */
  void setCertificate(const char *certPem, size_t certLen, const char *keyPem, size_t keyLen, const char *caPem = nullptr, size_t caLen = 0);

  /**
   * @brief Limit the number of DTLS handshake attempts.
   * @param maxAttempts Maximum attempts (0 for unlimited).
   */
  void setMaxConnectionAttempts(uint16_t maxAttempts);

  /**
   * @brief Start the secure server.
   * @return true on success (false if a CoAPS client session is active on this device).
   */
  bool begin();

  /** @brief Stop the secure server and release its resources. */
  void stop();

  /** @brief true if the secure server is running. */
  operator bool() const;

  /**
   * @brief Register or unregister a handler for a URI path.
   * @param path       Resource path without leading slash.
   * @param methodMask Bitmask of OT_COAP_METHOD_* this handler accepts (ignored when unregistering).
   * @param handler    Callback invoked on a matching request, or @c nullptr to unregister @p path.
   * @param context    Optional user pointer passed back to @p handler.
   * @return true on success.
   */
  bool on(const char *path, uint8_t methodMask, OThreadCoAPHandler handler, void *context = nullptr);

private:
  explicit OThreadCoAPSecureServerClass(uint16_t port = OT_COAP_SECURE_DEFAULT_PORT);
  ~OThreadCoAPSecureServerClass();
  OThreadCoAPSecureServerClass(const OThreadCoAPSecureServerClass &) = delete;
  OThreadCoAPSecureServerClass &operator=(const OThreadCoAPSecureServerClass &) = delete;

  void *_impl;
  friend struct othread_coap_detail::SecureCoAPServerHolder;
  friend void resetSecureServerWrapperState(OThreadCoAPSecureServerClass &server);
};

/**
 * @brief In-memory REST collection helper that wires CRUD onto a server path.
 *
 * After attach(server, "notes"), it serves GET (list), GET/<id>, POST, PUT/<id>,
 * and DELETE/<id> automatically and keeps the items in memory.
 */
class OThreadCoAPResourceStore {
public:
  /** @brief A single stored item: its assigned id and body. */
  struct Item {
    String id;
    String body;
  };

  /**
   * @brief Callback invoked when the collection changes.
   * @param basePath Base path the store is attached to.
   * @param context  User pointer supplied to onChange().
   */
  typedef void (*ChangeCallback)(const char *basePath, void *context);

  OThreadCoAPResourceStore();
  ~OThreadCoAPResourceStore();

  /**
   * @brief Attach the store to a running plain CoAP server under a base path.
   * @param server   Library global OThreadCoAPServer (not OThreadCoAPSecureServer).
   * @param basePath Collection base path without leading slash (e.g. "notes").
   * @param maxItems Maximum number of items to retain.
   * @return true on success.
   */
  bool attach(OThreadCoAPServerClass &server, const char *basePath, size_t maxItems = 8);

  /**
   * @brief Register a callback fired whenever the collection changes.
   * @param callback Change callback.
   * @param context  Optional user pointer passed back to @p callback.
   * @note The callback runs on the OpenThread task (same context as CoAP handlers).
   */
  void onChange(ChangeCallback callback, void *context = nullptr);

  /** @brief Number of items currently stored. */
  size_t count() const;

  /**
   * @brief Fetch an item by position.
   * @param index Zero-based index (< count()).
   * @param out   Receives the item on success.
   * @return true if @p index was valid.
   */
  bool getByIndex(size_t index, Item &out) const;

  /**
   * @brief Fetch an item by id.
   * @param id  Item id.
   * @param out Receives the item on success.
   * @return true if an item with @p id exists.
   */
  bool getById(const char *id, Item &out) const;

  /**
   * @brief Create a new item.
   * @param body       Item body.
   * @param assignedId Receives the id assigned to the new item.
   * @return true on success (false if the store is full).
   */
  bool create(const char *body, String &assignedId);

  /**
   * @brief Replace the body of an existing item.
   * @param id   Item id.
   * @param body New body.
   * @return true if an item with @p id existed and was updated.
   */
  bool update(const char *id, const char *body);

  /**
   * @brief Delete an item by id.
   * @param id Item id.
   * @return true if an item with @p id existed and was removed.
   */
  bool remove(const char *id);

private:
  void *_impl;
};

/**
 * @brief Singleton plain CoAP resource server for this device (port 5683).
 *
 * Like ``OThread`` for Thread or ``Wi-Fi`` for Wi-Fi: include ``OThreadCoAP.h`` and
 * use this global directly. Do not construct ``OThreadCoAPServerClass`` — register
 * many URI paths with ``on()`` on this one object. Same instance as
 * ``OThreadCoAP::plainServer()``.
 */
extern OThreadCoAPServerClass &OThreadCoAPServer;

/**
 * @brief Singleton CoAPS resource server for this device (port 5684).
 *
 * Same singleton pattern as ``OThreadCoAPServer``. Plain and secure servers may
 * run together on one device. Same instance as ``OThreadCoAP::secureServer()``.
 */
extern OThreadCoAPSecureServerClass &OThreadCoAPSecureServer;

#endif /* SOC_IEEE802154_SUPPORTED && CONFIG_OPENTHREAD_ENABLED */
