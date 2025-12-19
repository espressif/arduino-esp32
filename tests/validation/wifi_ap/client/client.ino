#include <WiFi.h>

String ssid = "";
String password = "";

void readWiFiCredentials() {
  Serial.println("[CLIENT] Waiting for WiFi credentials...");
  Serial.println("[CLIENT] Send SSID:");

  // Wait for SSID
  while (ssid.length() == 0) {
    if (Serial.available()) {
      ssid = Serial.readStringUntil('\n');
      ssid.trim();
    }
    delay(100);
  }

  Serial.println("[CLIENT] Send Password:");

  // Wait for password (allow empty password)
  bool password_received = false;
  while (!password_received) {
    if (Serial.available()) {
      password = Serial.readStringUntil('\n');
      password.trim();
      password_received = true; // Accept even empty password
    }
    delay(100);
  }

  Serial.print("[CLIENT] SSID: ");
  Serial.println(ssid);
  Serial.print("[CLIENT] Password: ");
  Serial.println(password);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // delete old config
  WiFi.disconnect(true);
  delay(1000);

  // Wait for test to be ready
  Serial.println("[CLIENT] Device ready for WiFi credentials");

  // Read WiFi credentials from serial
  readWiFiCredentials();

  WiFi.mode(WIFI_STA);

  bool found = false;
  do {
    Serial.println("[CLIENT] Scan start");
    // WiFi.scanNetworks will return the number of networks found.
    int n = WiFi.scanNetworks();
    Serial.println("[CLIENT] Scan done");
    if (n == 0) {
      Serial.println("[CLIENT] no networks found");
    } else {
      Serial.print("[CLIENT] ");
      Serial.print(n);
      Serial.println(" networks found:");
      for (int i = 0; i < n; ++i) {
        // Print SSID for each network found
        Serial.printf("%s\n", WiFi.SSID(i).c_str());
        Serial.println();
        delay(10);
        if (WiFi.SSID(i) == ssid) {
          found = true;
          break;
        }
      }
    }
    Serial.println("");
    delay(5000);
  } while (!found);

  // Delete the scan result to free memory for code below.
  WiFi.scanDelete();

  WiFi.begin(ssid, password);

  Serial.printf("[CLIENT] Connecting to SSID=%s Password=%s ...\n", ssid.c_str(), password.c_str());

  int retries = 50;
  while (WiFi.status() != WL_CONNECTED && retries--) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[CLIENT] Connected! IP=%s\n",
                   WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[CLIENT] Failed to connect");
  }
}

void loop() {
  delay(2000);
  wl_status_t st = WiFi.status();
  Serial.printf("[CLIENT] Status=%d\n", st);
}
