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

/*
 * Thread DNSSD UDP Light — WiFi web UI
 *
 * Joins WiFi (same LAN as OTBR). Advertises http://otlight-ui.local via ESPmDNS.
 * Discovers the Thread light via LAN mDNS (_otlight._udp from OTBR Advertising
 * Proxy) and proxies HTTP controls to UDP ON/OFF/TOGGLE/STATUS.
 *
 * This board does not run OpenThread. See ../README.md.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <errno.h>
#include "mdns.h"

// Same WiFi LAN as the OTBR infrastructure interface.
static const char *WIFI_SSID = "your-ssid";
static const char *WIFI_PASS = "your-password";

// Optional: paste YOUR light OMR (unique per lab) if mDNS AAAA never appears.
// How to find it: switch Serial, OTBR `srp server host`, or light `ipaddr`
// (see web/README.md). Leave empty to use mDNS only.
static const char *LIGHT_IPV6_FALLBACK = "";

static const uint16_t LIGHT_PORT_FALLBACK = 5051;
static const uint32_t UDP_TIMEOUT_MS = 2500;
static const uint32_t REDISCOVER_MS = 20000;
static const uint32_t MDNS_QUERY_MS = 5000;

WebServer server(80);
WiFiUDP Udp;

static IPAddress s_lightAddr;
static uint16_t s_lightPort = LIGHT_PORT_FALLBACK;
static bool s_haveLight = false;
static String s_lastState = "unknown";
static uint32_t s_lastDiscoverMs = 0;
static volatile bool s_gotIp4 = false;
static volatile bool s_gotIp6 = false;

#if CONFIG_LWIP_IPV6
// Matches libraries/WiFi/examples/WiFiIPv6 — IPv6 events run on another task.
static void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      s_gotIp4 = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
      s_gotIp6 = true;
      Serial.print("STA IPv6: ");
      Serial.println(WiFi.linkLocalIPv6());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      s_gotIp4 = false;
      s_gotIp6 = false;
      break;
    default: break;
  }
}
#endif

static bool isEmptyAddr(const IPAddress &addr) {
  if (addr.type() == IPv6) {
    for (int i = 0; i < 16; ++i) {
      if (addr[i] != 0) {
        return false;
      }
    }
    return true;
  }
  return addr == IPAddress((uint32_t)0);
}

static bool addrFromResult(const mdns_result_t *r, IPAddress &out) {
  if (!r) {
    return false;
  }
  for (mdns_ip_addr_t *a = r->addr; a; a = a->next) {
    if (a->addr.type == ESP_IPADDR_TYPE_V6 || a->addr.type == MDNS_IP_PROTOCOL_V6) {
      out = IPAddress(IPv6, (const uint8_t *)a->addr.u_addr.ip6.addr);
      if (!isEmptyAddr(out)) {
        return true;
      }
    }
  }
  for (mdns_ip_addr_t *a = r->addr; a; a = a->next) {
    if (a->addr.type == ESP_IPADDR_TYPE_V4 || a->addr.type == MDNS_IP_PROTOCOL_V4) {
      out = IPAddress(a->addr.u_addr.ip4.addr);
      if (!isEmptyAddr(out)) {
        return true;
      }
    }
  }
  return false;
}

// OTBR Adv Proxy often returns PTR/SRV without A/AAAA. Resolve host (needs STA IPv6).
static bool resolveHostAddress(const String &host, const String &instance, IPAddress &out) {
  if (LIGHT_IPV6_FALLBACK[0] != '\0') {
    if (out.fromString(LIGHT_IPV6_FALLBACK)) {
      Serial.printf("Using LIGHT_IPV6_FALLBACK %s\r\n", out.toString().c_str());
      return true;
    }
  }

  if (host.length() > 0) {
#if CONFIG_LWIP_IPV6
    // Try bare hostname and hostname.local (ESP-IDF mDNS / Adv Proxy vary).
    const char *candidates[2] = {host.c_str(), nullptr};
    String hostLocal;
    if (!host.endsWith(".local")) {
      hostLocal = host + ".local";
      candidates[1] = hostLocal.c_str();
    }
    for (int c = 0; c < 2; ++c) {
      if (!candidates[c]) {
        break;
      }
      esp_ip6_addr_t a6;
      memset(&a6, 0, sizeof(a6));
      esp_err_t err = mdns_query_aaaa(candidates[c], MDNS_QUERY_MS, &a6);
      Serial.printf("mdns_query_aaaa(%s) err=%d%s\r\n", candidates[c], (int)err, (err == ESP_ERR_NOT_FOUND) ? " (NOT_FOUND/timeout)" : "");
      if (err == ESP_OK) {
        out = IPAddress(IPv6, (const uint8_t *)a6.addr);
        if (!isEmptyAddr(out)) {
          Serial.printf("  -> %s\r\n", out.toString().c_str());
          return true;
        }
      }
    }
#endif
    IPAddress a4 = MDNS.queryHost(host.c_str(), MDNS_QUERY_MS);
    if (!isEmptyAddr(a4)) {
      out = a4;
      Serial.printf("MDNS.queryHost(%s) -> %s\r\n", host.c_str(), out.toString().c_str());
      return true;
    }
  }

  // Full SRV query sometimes carries address records that PTR browse omitted.
  if (instance.length() > 0) {
    mdns_result_t *srv = nullptr;
    if (mdns_query_srv(instance.c_str(), "_otlight", "_udp", MDNS_QUERY_MS, &srv) == ESP_OK && srv) {
      Serial.printf("mdns_query_srv host=%s port=%u\r\n", srv->hostname ? srv->hostname : "?", srv->port);
      if (addrFromResult(srv, out)) {
        Serial.printf("  addr from SRV -> %s\r\n", out.toString().c_str());
        mdns_query_results_free(srv);
        return true;
      }
      if (srv->hostname && srv->hostname[0] && host != srv->hostname) {
        String h = srv->hostname;
        mdns_query_results_free(srv);
        return resolveHostAddress(h, String(), out);
      }
      mdns_query_results_free(srv);
    }
  }

  Serial.printf(
    "FAIL: no A/AAAA for host='%s' instance='%s'\r\n"
    "  Set LIGHT_IPV6_FALLBACK to OTBR `srp server host` OMR (fd…) to skip mDNS AAAA.\r\n",
    host.c_str(), instance.c_str()
  );
  return false;
}

static bool discoverLight() {
  Serial.println("MDNS.queryService(\"otlight\", \"udp\")...");
  int n = MDNS.queryService("otlight", "udp");
  s_lastDiscoverMs = millis();
  Serial.printf("Found %d mDNS instance(s)\r\n", n);
  if (n <= 0) {
    s_haveLight = false;
    return false;
  }

  for (int i = 0; i < n; ++i) {
    String host = MDNS.hostname(i);
    String instance = MDNS.instanceName(i);
    IPAddress v6 = MDNS.addressV6(i);
    IPAddress v4 = MDNS.address(i);
    uint16_t port = MDNS.port(i);
    Serial.printf(
      "  [%d] %s host=%s v4=%s v6=%s port=%u\r\n", i, instance.c_str(), host.c_str(), v4.toString().c_str(),
      v6.toString().c_str(), port
    );

    IPAddress addr;
    if (!isEmptyAddr(v6)) {
      addr = v6;
    } else if (!isEmptyAddr(v4)) {
      addr = v4;
    } else if (!resolveHostAddress(host, instance, addr)) {
      continue;
    }

    s_lightAddr = addr;
    s_lightPort = port ? port : LIGHT_PORT_FALLBACK;
    s_haveLight = true;
    Serial.printf("Using light [%s]:%u\r\n", s_lightAddr.toString().c_str(), s_lightPort);
    return true;
  }
  s_haveLight = false;
  return false;
}

static void drainUdp() {
  while (Udp.parsePacket() > 0) {
    while (Udp.available()) {
      Udp.read();
    }
  }
}

static bool sendLightCmd(const char *cmd, String &reply) {
  reply = "";
  if (!s_haveLight && !discoverLight()) {
    return false;
  }
  drainUdp();
  if (!Udp.beginPacket(s_lightAddr, s_lightPort)) {
    Serial.printf("UDP beginPacket failed errno=%d (need AF_INET6 socket for OMR)\r\n", errno);
    return false;
  }
  Udp.write((const uint8_t *)cmd, strlen(cmd));
  if (!Udp.endPacket()) {
    // Common causes: IPv4-only UDP socket, or no route to OMR via OTBR.
    Serial.printf(
      "UDP endPacket failed errno=%d dest=[%s]:%u\r\n"
      "  Tip: from a PC on this WiFi, try: ping %s\r\n"
      "  OTBR must route OMR (br / netdata); NAT64 is NOT required for this IPv6 path.\r\n",
      errno, s_lightAddr.toString().c_str(), s_lightPort, s_lightAddr.toString().c_str()
    );
    return false;
  }

  uint32_t start = millis();
  while (millis() - start < UDP_TIMEOUT_MS) {
    int n = Udp.parsePacket();
    if (n > 0) {
      char buf[48];
      int got = Udp.read(buf, (n < (int)sizeof(buf) - 1) ? n : (int)sizeof(buf) - 1);
      buf[got] = '\0';
      reply = buf;
      Serial.printf("UDP RX '%s'\r\n", buf);
      if (reply.startsWith("ACK ") || reply.startsWith("STATE ")) {
        s_lastState = reply.substring(reply.indexOf(' ') + 1);
      }
      return true;
    }
    delay(10);
  }
  Serial.println("UDP timeout (packet left WiFi; no reply — check OTBR IPv6 route / light up)");
  // Keep s_haveLight: AAAA was good; avoid rediscover spam on one lost reply.
  return false;
}

static void handleRoot() {
  String html;
  html.reserve(1200);
  html += F(
    "<!DOCTYPE html><html><head><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>OT Light</title>"
    "<style>"
    "body{font-family:system-ui,sans-serif;max-width:28rem;margin:2rem auto;padding:0 1rem}"
    "h1{font-size:1.4rem}button{font-size:1rem;margin:.25rem;padding:.5rem 1rem}"
    "#st{font-weight:600;margin:1rem 0}"
    "</style></head><body>"
    "<h1>Thread light (via OTBR)</h1>"
    "<p id=st>Status: "
  );
  html += s_lastState;
  html += F(
    "</p>"
    "<p>"
    "<button onclick=\"cmd('on')\">On</button>"
    "<button onclick=\"cmd('off')\">Off</button>"
    "<button onclick=\"cmd('toggle')\">Toggle</button>"
    "<button onclick=\"cmd('status')\">Refresh</button>"
    "</p>"
    "<p id=msg></p>"
    "<script>"
    "async function cmd(a){"
    " document.getElementById('msg').textContent='...';"
    " try{"
    "  const r=await fetch('/api/'+a);"
    "  const t=await r.text();"
    "  document.getElementById('msg').textContent=t;"
    "  const s=await fetch('/api/status');"
    "  document.getElementById('st').textContent='Status: '+await s.text();"
    " }catch(e){document.getElementById('msg').textContent=String(e);}"
    "}"
    "cmd('status');"
    "</script></body></html>"
  );
  server.send(200, "text/html", html);
}

static void handleApi(const char *udpCmd) {
  String reply;
  if (!sendLightCmd(udpCmd, reply)) {
    server.send(503, "text/plain", "light unreachable (mDNS/UDP). Check OTBR Advertising Proxy + IPv6.");
    return;
  }
  server.send(200, "text/plain", reply);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ThreadDNSSD_UDP_Light / web");

  // Same order as WiFiIPv6.ino: disconnect → events → mode → enableIPv6 → begin.
  // enableIPv6() must be called before WiFi.begin() (see WiFiSTA.h).
  WiFi.disconnect(true);
#if CONFIG_LWIP_IPV6
  WiFi.onEvent(onWiFiEvent);
#endif
  WiFi.mode(WIFI_STA);
#if CONFIG_LWIP_IPV6
  if (!WiFi.enableIPv6()) {
    Serial.println("WARN: WiFi.enableIPv6() failed");
  }
#else
  Serial.println("FAIL: CONFIG_LWIP_IPV6 is off — cannot reach Thread OMR addresses");
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("WiFi connecting to %s", WIFI_SSID);
  uint32_t start = millis();
  while (!s_gotIp4 && WiFi.status() != WL_CONNECTED && millis() - start < 60000) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("FAIL: WiFi connect");
    return;
  }
  Serial.print("WiFi IPv4: ");
  Serial.println(WiFi.localIP());

#if CONFIG_LWIP_IPV6
  // Wait for link-local (GOT_IP6), then a bit for OTBR RA / global if any.
  start = millis();
  while (!s_gotIp6 && isEmptyAddr(WiFi.linkLocalIPv6()) && millis() - start < 15000) {
    delay(200);
  }
  Serial.print("WiFi IPv6 LL: ");
  Serial.println(WiFi.linkLocalIPv6());
  start = millis();
  while (isEmptyAddr(WiFi.globalIPv6()) && millis() - start < 10000) {
    delay(200);
  }
  Serial.print("WiFi IPv6 global: ");
  Serial.println(WiFi.globalIPv6());
  if (isEmptyAddr(WiFi.linkLocalIPv6())) {
    Serial.println("WARN: no STA IPv6 yet — mDNS AAAA / UDP to OMR may fail");
  }
#ifndef CONFIG_LWIP_IPV6_ND6_ROUTE_INFO_OPTION_SUPPORT
  Serial.println(
    "WARN: lwIP RIO off (CONFIG_LWIP_IPV6_ND6_ROUTE_INFO_OPTION_SUPPORT).\r\n"
    "  PC may ping Thread OMR while this board UDP-times-out.\r\n"
    "  Enable RIO in Arduino libs, or use Thread+WiFi dual-homed UI."
  );
#endif
#endif

  if (!MDNS.begin("otlight-ui")) {
    Serial.println("FAIL: MDNS.begin(otlight-ui)");
  } else {
    Serial.println("Open http://otlight-ui.local");
  }
  MDNS.addService("http", "tcp", 80);

  // Must be AF_INET6: light AAAA is Thread OMR (fdxx:…). Udp.begin(0) opens IPv4-only
  // and endPacket(sendto IPv6) then fails even when mDNS resolves correctly.
  if (!Udp.begin(IPAddress(IPv6), 0)) {
    Serial.println("FAIL: WiFiUDP begin(IPv6)");
    return;
  }
  Serial.println("UDP socket: AF_INET6 (required for Thread OMR)");

  (void)discoverLight();

  server.on("/", handleRoot);
  server.on("/api/on", HTTP_GET, []() {
    handleApi("ON");
  });
  server.on("/api/off", HTTP_GET, []() {
    handleApi("OFF");
  });
  server.on("/api/toggle", HTTP_GET, []() {
    handleApi("TOGGLE");
  });
  server.on("/api/status", HTTP_GET, []() {
    String reply;
    if (!sendLightCmd("STATUS", reply)) {
      server.send(503, "text/plain", "unreachable");
      return;
    }
    // Prefer STATE token for the page; fall back to raw reply.
    if (reply.startsWith("STATE ")) {
      server.send(200, "text/plain", reply.substring(6));
    } else {
      server.send(200, "text/plain", reply);
    }
  });
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
  Serial.println("HTTP server on port 80");
}

void loop() {
  server.handleClient();
  if (!s_haveLight && (millis() - s_lastDiscoverMs >= REDISCOVER_MS)) {
    (void)discoverLight();
  }
  delay(2);
}
