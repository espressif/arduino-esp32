/* For any secure connection - it is (at least) essential for the
   the client to verify that it is talking with the server it
   thinks it is talking to. And not some (invisible) man in the middle.

   See https://en.wikipedia.org/wiki/Man-in-the-middle_attack,
   https://www.ai.rug.nl/mas/finishedprojects/2011/TLS/hermsencomputerservices.nl/mas/mitm.html or
   https://medium.com/@munteanu210/ssl-certificates-vs-man-in-the-middle-attacks-3fb7846fa5db
   for some background on this.

   Unfortunately this means that one needs to hardcode a server
   public key, certificate or some cryptographically strong hash
   thereoff into the code, to verify that you are indeed talking to
   the right server. This is sometimes somewhat impractical. Especially
   if you do not know the server in advance; or if your code needs to be
   stable ovr very long times - during which the server may change.

   However completely dispensing with any checks (See the WifiClientInSecure
   example) is also not a good idea either.

   This example gives you some middle ground; "Trust on First Use" --
   TOFU - see https://developer.mozilla.org/en-US/docs/Glossary/TOFU or
   https://en.wikipedia.org/wiki/Trust_on_first_use).

   In this scheme; we start the very first time without any security checks
   but once we have our first connection; we store the public crytpographic
   details (or a proxy, such as a sha256 of this). And then we use this for
   any subsequent connections.

   The assumption here is that we do our very first connection in a somewhat
   trusted network environment; where the chance of a man in the middle is
   very low; or one where the person doing the first run can check the
   details manually.

   So this is not quite as good as building a CA certificate into your
   code (as per the WifiClientSecure example). But not as bad as something
   with no trust management at all.

   To make it possible for the enduser to 'reset' this trust; the
   startup sequence checks if a certain GPIO is low (assumed to be wired
   to some physical button or jumper on the PCB). And we only allow
   the TOFU to be configured when this pin is LOW.
*/
#ifndef WIFI_NETWORK
#define WIFI_NETWORK "Your Wifi SSID"
#endif

#ifndef WIFI_PASSWD
#define WIFI_PASSWD "your-secret-wifi-password"
#endif

const char *ssid = WIFI_NETWORK;           // your network SSID (name of wifi network)
const char *password = WIFI_PASSWD;        // your network password
const char *server = "www.howsmyssl.com";  // Server to test with.

const int TOFU_RESET_BUTTON = 35; /* Trust reset button wired between GPIO 35 and GND (pulldown) */

#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <EEPROM.h>

/* Set aside some persistent memory (i.e. memory that is preserved on reboots and
  power cycling; and will generally survive software updates as well.
*/
EEPROMClass TOFU("tofu0");

// Utility function; checks if a given buffer is entirely
// with with 0 bytes over its full length. Returns 0 on
// success; a non zero value on fail.
//
static int memcmpzero(unsigned char *ptr, size_t len) {
  while (len--) {
    if (0xff != *ptr++) {
      return -1;
    }
  }
  return 0;
};

static void printSHA256(unsigned char *ptr) {
  for (int i = 0; i < 32; i++) {
    Serial.printf("%s%02x", i ? ":" : "", ptr[i]);
  }
  Serial.println("");
};

NetworkClientSecure client;

bool get_tofu();
bool doTOFU_Protected_Connection(uint8_t *fingerprint_tofu);

void setup() {
  bool tofu_reset = false;
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  delay(100);

  if (!TOFU.begin(32)) {
    Serial.println("Could not initialsize the EEPROM");
    return;
  }
  uint8_t fingerprint_tofu[32];

  // reset the trust if the tofu reset button is pressed.
  //
  pinMode(TOFU_RESET_BUTTON, INPUT_PULLUP);
  if (digitalRead(TOFU_RESET_BUTTON) == LOW) {
    Serial.println("The TOFU reset button is pressed.");
    tofu_reset = true;
  }
  /* if the button is not pressed; see if we can get the TOFU
      fingerprint from the EEPROM.
  */
  else if (32 != TOFU.readBytes(0, fingerprint_tofu, 32)) {
    Serial.println("Failed to get the fingerprint from memory.");
    tofu_reset = true;
  }
  /* And check that the EEPROM value is not all 0's; in which
      case we also need to do a TOFU.
  */
  else if (!memcmpzero(fingerprint_tofu, 32)) {
    Serial.println("TOFU fingerprint in memory all zero.");
    tofu_reset = true;
  };
  if (!tofu_reset) {
    Serial.print("TOFU pegged to fingerprint: SHA256=");
    printSHA256(fingerprint_tofu);
    Serial.print("Note: You can check this fingerprint by going to the URL\n"
                 "<https://");
    Serial.print(server);
    Serial.println("> and then click on the lock icon.\n");
  };

  // attempt to connect to Wifi network:
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
  }

  Serial.print("Connected to ");
  Serial.println(ssid);

  if (tofu_reset) {
    Serial.println("Resetting trust fingerprint.");
    if (!get_tofu()) {
      Serial.println("Trust reset failed. Giving up");
      return;
    }
    Serial.println("(New) Trust of First used configured. Rebooting in 3 seconds");
    delay(3 * 1000);
    ESP.restart();
  };

  Serial.println("Trying to connect to a server; using TOFU details from the eeprom");

  if (doTOFU_Protected_Connection(fingerprint_tofu)) {
    Serial.println("ALL OK");
  }
}

bool get_tofu() {
  Serial.println("\nStarting our insecure connection to server...");
  client.setInsecure();  //skip verification

  if (!client.connect(server, 443)) {
    Serial.println("Connection failed!");
    client.stop();
    return false;
  };

  Serial.println("Connected to server. Extracting trust data.");

  // Now extract the data of the certificate and show it to
  // the user over the serial connection for optional
  // verification.
  const mbedtls_x509_crt *peer = client.getPeerCertificate();
  char buf[1024];
  int l = mbedtls_x509_crt_info(buf, sizeof(buf), "", peer);
  if (l <= 0) {
    Serial.println("Peer conversion to printable buffer failed");
    client.stop();
    return false;
  };
  Serial.println();
  Serial.println(buf);

  // Extract the fingerprint - and store this in our EEPROM
  // to be used for future validation.

  uint8_t fingerprint_remote[32];
  if (!client.getFingerprintSHA256(fingerprint_remote)) {
    Serial.println("Failed to get the fingerprint");
    client.stop();
    return false;
  }
  if ((32 != TOFU.writeBytes(0, fingerprint_remote, 32)) || (!TOFU.commit())) {
    Serial.println("Could not write the fingerprint to the EEPROM");
    client.stop();
    return false;
  };
  TOFU.end();
  client.stop();

  Serial.print("TOFU pegged to fingerprint: SHA256=");
  printSHA256(fingerprint_remote);

  return true;
};

bool doTOFU_Protected_Connection(uint8_t *fingerprint_tofu) {

  // As we're not using a (CA) certificate to check the
  // connection; but the hash of the peer - we need to initially
  // allow the connection to be set up without the CA check.
  client.setInsecure();  //skip verification

  if (!client.connect(server, 443)) {
    Serial.println("Connection failed!");
    client.stop();
    return false;
  };

  // Now that we're connected - we can check that we have
  // end to end trust - by comparing the fingerprint we (now)
  // see (of the server certificate) to the one we have stored
  // in our EEPROM as part of an earlier trust-on-first use.
  uint8_t fingerprint_remote[32];
  if (!client.getFingerprintSHA256(fingerprint_remote)) {
    Serial.println("Failed to get the fingerprint of the server");
    client.stop();
    return false;
  }
  if (memcmp(fingerprint_remote, fingerprint_tofu, 32)) {
    Serial.println("TOFU fingerprint not the same as the one from the server.");
    Serial.print("TOFU  : SHA256=");
    printSHA256(fingerprint_tofu);
    Serial.print("Remote: SHA256=");
    printSHA256(fingerprint_remote);
    Serial.println("       : NOT identical -- Aborting!");
    client.stop();
    return false;
  };

  Serial.println("All well - you are talking to the same server as\n"
                 "when you set up TOFU. So we can now do a GET.\n\n");

  client.println("GET /a/check HTTP/1.0");
  client.print("Host: ");
  client.println(server);
  client.println("Connection: close");
  client.println();

  bool inhdr = true;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (inhdr && line == "\r") {
      inhdr = false;
      Serial.println("-- headers received. Payload follows\n\n");
    }
  }
  Serial.println("\n\n-- Payload ended.");
  client.stop();
  return true;
}

void loop() {}
