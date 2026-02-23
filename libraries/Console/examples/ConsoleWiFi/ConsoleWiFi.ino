/*
 * ConsoleWiFi — Wi-Fi management CLI commands.
 *
 * Implements a set of commands for scanning, connecting, and monitoring Wi-Fi:
 *
 *   wifi scan                     — scan and list nearby access points
 *   wifi connect <ssid> [<pass>]  — connect to a network
 *   wifi disconnect               — disconnect from the current network
 *   wifi status                   — show connection status and IP info
 *   ping <host> [-c <count>]      — ICMP ping a host by name or IP
 *
 * Demonstrates argtable3 with optional arguments (password is optional)
 * and optional flags (-c for ping count).
 *
 * Created by lucasssvaz
 */

#include <Arduino.h>
#include <Console.h>
#include <WiFi.h>
#include "ping/ping_sock.h"
#include "argtable3/argtable3.h"

// ---------------------------------------------------------------------------
// wifi scan
// ---------------------------------------------------------------------------

static int cmd_wifi_scan(int argc, char **argv) {
  printf("Scanning...\n");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    printf("No networks found.\n");
    return 0;
  }

  printf("%-4s  %-32s  %5s  %4s  %s\n", "#", "SSID", "RSSI", "Ch", "Encryption");
  printf("%-4s  %-32s  %5s  %4s  %s\n", "---", "--------------------------------", "-----", "----", "----------");

  for (int i = 0; i < n; i++) {
    const char *enc;
    switch (WiFi.encryptionType(i)) {
      case WIFI_AUTH_OPEN:          enc = "Open";     break;
      case WIFI_AUTH_WEP:           enc = "WEP";      break;
      case WIFI_AUTH_WPA_PSK:       enc = "WPA";      break;
      case WIFI_AUTH_WPA2_PSK:      enc = "WPA2";     break;
      case WIFI_AUTH_WPA_WPA2_PSK:  enc = "WPA/WPA2"; break;
      case WIFI_AUTH_WPA3_PSK:      enc = "WPA3";     break;
      default:                      enc = "Unknown";  break;
    }
    printf("%-4d  %-32s  %5d  %4d  %s\n", i + 1, WiFi.SSID(i).c_str(), (int)WiFi.RSSI(i), (int)WiFi.channel(i), enc);
  }

  WiFi.scanDelete();
  return 0;
}

// ---------------------------------------------------------------------------
// wifi connect <ssid> [<password>]
// ---------------------------------------------------------------------------

static struct {
  struct arg_str *ssid;      // required positional string: network name
  struct arg_str *password;  // optional positional string: network password
  struct arg_end *end;       // sentinel: tracks parse errors
} wifi_connect_args;

static int cmd_wifi_connect(int argc, char **argv) {
  int nerrors = arg_parse(argc, argv, (void **)&wifi_connect_args);
  if (nerrors != 0) {
    arg_print_errors(stdout, wifi_connect_args.end, argv[0]);
    return 1;
  }

  const char *ssid = wifi_connect_args.ssid->sval[0];       // access parsed string via ->sval[0]
  // For optional args, check ->count to see if it was provided
  const char *pass = (wifi_connect_args.password->count > 0) ? wifi_connect_args.password->sval[0] : NULL;

  printf("Connecting to '%s'...\n", ssid);
  WiFi.STA.connect(ssid, pass);

  uint32_t deadline = millis() + 15000;
  while (!WiFi.STA.connected() && millis() < deadline) {
    delay(500);
    printf(".");
    fflush(stdout);
  }
  printf("\n");

  if (WiFi.STA.connected()) {
    printf("Connected. IP: %s\n", WiFi.STA.localIP().toString().c_str());
    return 0;
  } else {
    printf("Connection failed (status %d)\n", (int)WiFi.STA.status());
    return 1;
  }
}

// ---------------------------------------------------------------------------
// wifi disconnect
// ---------------------------------------------------------------------------

static int cmd_wifi_disconnect(int argc, char **argv) {
  WiFi.STA.disconnect();
  printf("Disconnected.\n");
  return 0;
}

// ---------------------------------------------------------------------------
// wifi status
// ---------------------------------------------------------------------------

static int cmd_wifi_status(int argc, char **argv) {
  wl_status_t s = WiFi.STA.status();
  const char *statusStr;
  switch (s) {
    case WL_CONNECTED:        statusStr = "Connected";        break;
    case WL_NO_SSID_AVAIL:    statusStr = "SSID not found";   break;
    case WL_CONNECT_FAILED:   statusStr = "Connect failed";   break;
    case WL_DISCONNECTED:     statusStr = "Disconnected";     break;
    case WL_IDLE_STATUS:      statusStr = "Idle";             break;
    default:                  statusStr = "Unknown";          break;
  }

  printf("Status    : %s\n", statusStr);

  if (s == WL_CONNECTED) {
    printf("SSID      : %s\n", WiFi.STA.SSID().c_str());
    printf("BSSID     : %s\n", WiFi.STA.BSSIDstr().c_str());
    printf("RSSI      : %d dBm\n", (int)WiFi.STA.RSSI());
    printf("IP        : %s\n", WiFi.STA.localIP().toString().c_str());
    printf("Gateway   : %s\n", WiFi.STA.gatewayIP().toString().c_str());
    printf("DNS       : %s\n", WiFi.STA.dnsIP().toString().c_str());
    printf("Subnet    : %s\n", WiFi.STA.subnetMask().toString().c_str());
  }

  printf("MAC       : %s\n", WiFi.STA.macAddress().c_str());
  return 0;
}

// ---------------------------------------------------------------------------
// Top-level dispatcher: wifi <subcmd> ...
// ---------------------------------------------------------------------------

static int cmd_wifi(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: wifi <scan|connect|disconnect|status>\n");
    return 1;
  }

  if (strcmp(argv[1], "scan") == 0) {
    return cmd_wifi_scan(argc - 1, argv + 1);
  } else if (strcmp(argv[1], "connect") == 0) {
    return cmd_wifi_connect(argc - 1, argv + 1);
  } else if (strcmp(argv[1], "disconnect") == 0) {
    return cmd_wifi_disconnect(argc - 1, argv + 1);
  } else if (strcmp(argv[1], "status") == 0) {
    return cmd_wifi_status(argc - 1, argv + 1);
  } else {
    printf("wifi: unknown sub-command '%s'\n", argv[1]);
    return 1;
  }
}

// ---------------------------------------------------------------------------
// ping <host> [-c <count>]
// ---------------------------------------------------------------------------

static struct {
  struct arg_str *host;   // required positional: hostname or IP address
  struct arg_int *count;  // optional: -c <count> (default 5)
  struct arg_end *end;
} ping_args;

static SemaphoreHandle_t ping_done_sem = NULL;

static void ping_on_success(esp_ping_handle_t hdl, void *args) {
  uint32_t elapsed;
  uint32_t size;
  uint32_t seqno;
  uint8_t ttl;
  ip_addr_t addr;
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO,   &seqno,   sizeof(seqno));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TTL,      &ttl,     sizeof(ttl));
  esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE,     &size,    sizeof(size));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP,  &elapsed, sizeof(elapsed));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR,   &addr,    sizeof(addr));
  printf("%lu bytes from %s: icmp_seq=%lu ttl=%d time=%lu ms\n",
         (unsigned long)size, ipaddr_ntoa(&addr),
         (unsigned long)seqno, (int)ttl, (unsigned long)elapsed);
}

static void ping_on_timeout(esp_ping_handle_t hdl, void *args) {
  uint32_t seqno;
  ip_addr_t addr;
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &addr, sizeof(addr));
  printf("From %s icmp_seq=%lu timeout\n", ipaddr_ntoa(&addr), (unsigned long)seqno);
}

static void ping_on_end(esp_ping_handle_t hdl, void *args) {
  uint32_t transmitted;
  uint32_t received;
  uint32_t total_ms;
  esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST,  &transmitted, sizeof(transmitted));
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY,    &received,    sizeof(received));
  esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_ms,    sizeof(total_ms));
  uint32_t loss = transmitted ? ((transmitted - received) * 100 / transmitted) : 0;
  printf("\n--- ping statistics ---\n");
  printf("%lu packets transmitted, %lu received, %lu%% packet loss, time %lu ms\n",
         (unsigned long)transmitted, (unsigned long)received,
         (unsigned long)loss, (unsigned long)total_ms);
  xSemaphoreGive(ping_done_sem);
}

static int cmd_ping(int argc, char **argv) {
  int nerrors = arg_parse(argc, argv, (void **)&ping_args);
  if (nerrors != 0) {
    arg_print_errors(stdout, ping_args.end, argv[0]);
    return 1;
  }

  if (!WiFi.STA.connected()) {
    printf("Not connected to Wi-Fi.\n");
    return 1;
  }

  const char *host = ping_args.host->sval[0];
  int count = (ping_args.count->count > 0) ? ping_args.count->ival[0] : 5;
  if (count < 1) {
    count = 1;
  }

  IPAddress addr;
  if (!Network.hostByName(host, addr)) {
    printf("ping: unknown host '%s'\n", host);
    return 1;
  }

  ip_addr_t target_addr;
  target_addr.type = IPADDR_TYPE_V4;
  target_addr.u_addr.ip4.addr = (uint32_t)addr;

  printf("PING %s (%s): 64 bytes of data, count=%d\n",
         host, addr.toString().c_str(), count);
  fflush(stdout);

  esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
  ping_config.target_addr = target_addr;
  ping_config.count = count;

  esp_ping_callbacks_t cbs = {};
  cbs.on_ping_success = ping_on_success;
  cbs.on_ping_timeout = ping_on_timeout;
  cbs.on_ping_end     = ping_on_end;

  esp_ping_handle_t ping_handle = NULL;
  if (esp_ping_new_session(&ping_config, &cbs, &ping_handle) != ESP_OK) {
    printf("Failed to create ping session\n");
    return 1;
  }

  esp_ping_start(ping_handle);
  xSemaphoreTake(ping_done_sem, portMAX_DELAY);
  esp_ping_delete_session(ping_handle);

  return 0;
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Initialize WiFi
  WiFi.STA.begin();

  // Create semaphore for ping done
  ping_done_sem = xSemaphoreCreateBinary();

  // Initialise argtable.
  // arg_str1() = required string.  arg_str0() = optional string.
  // NULL, NULL for shortopts/longopts makes the argument positional.
  wifi_connect_args.ssid     = arg_str1(NULL, NULL, "<ssid>",       "Network SSID");
  wifi_connect_args.password = arg_str0(NULL, NULL, "[<password>]", "Network password (optional for open networks)");
  wifi_connect_args.end      = arg_end(2);  // sentinel — stores up to 2 parse errors

  // arg_int0() = optional integer.  "c" is the short option (-c), "count"
  // is the long option (--count).  Default ping count is 5 if not given.
  ping_args.host  = arg_str1(NULL, NULL, "<host>",  "Hostname or IP address to ping");
  ping_args.count = arg_int0("c", "count", "<n>",   "Number of pings (default 5)");
  ping_args.end   = arg_end(2);

  // Configure Console prompt
  Console.setPrompt("wifi> ");

  // Initialize Console
  if (!Console.begin()) {
    Serial.println("Console init failed");
    return;
  }

  // Add commands
  Console.addCmd("wifi", "Manage Wi-Fi connections", "<scan|connect|disconnect|status>", cmd_wifi);
  Console.addCmd("ping", "ICMP ping a host",        (void *)&ping_args,                cmd_ping);

  // Add built-in help command
  Console.addHelpCmd();

  // Begin Read-Eval-Print Loop
  Console.beginRepl();
}

void loop() {
  // Loop task is not used in this example, so we delete it to free up resources
  vTaskDelete(NULL);
}
