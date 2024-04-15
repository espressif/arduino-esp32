/* STARTSSL example

   Inline upgrading from a clear-text connection to an SSL/TLS connection.

   Some protocols such as SMTP, XMPP, Mysql, Postgresql and others allow, or require,
   that you start the connection without encryption; and then send a command to switch
   over to encryption.

   E.g. a typical SMTP submission would entail a dialog such as this:

   1. client connects to server in the clear
   2. server says hello
   3. client sents a EHLO
   4. server tells the client that it supports SSL/TLS
   5. client sends a 'STARTTLS' to make use of this faciltiy
   6. client/server negiotiate a SSL or TLS connection.
   7. client sends another EHLO
   8. server now tells the client what (else) is supported; such as additional authentication options.
   ... conversation continues encrypted.

   This can be enabled in NetworkClientSecure by telling it to start in plaintext:

          client.setPlainStart();

   and client is than a plain, TCP, connection (just as NetworkClient would be); until the client calls
   the method:

          client.startTLS(); // returns zero on error; non zero on success.

    After which things switch to TLS/SSL.
*/

#include <WiFi.h>
#include <NetworkClientSecure.h>

#ifndef WIFI_NETWORK
#define WIFI_NETWORK "YOUR Wifi SSID"
#endif

#ifndef WIFI_PASSWD
#define WIFI_PASSWD "your-secret-password"
#endif

#ifndef SMTP_HOST
#define SMTP_HOST "smtp.gmail.com"
#endif

#ifndef SMTP_PORT
#define SMTP_PORT (587)  // Standard (plaintext) submission port
#endif

const char* ssid = WIFI_NETWORK;        // your network SSID (name of wifi network)
const char* password = WIFI_PASSWD;     // your network password
const char* server = SMTP_HOST;         // Server URL
const int submission_port = SMTP_PORT;  // submission port.

NetworkClientSecure client;

static bool readAllSMTPLines();

void setup() {
  int ret;
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  delay(100);

  Serial.print("Attempting to connect to SSID: ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
  }

  Serial.print("Connected to ");
  Serial.println(ssid);

  Serial.printf("\nStarting connection to server: %s:%d\n", server, submission_port);


  // skip verification for this demo. In production one should at the very least
  // enable TOFU; or ideally hardcode a (CA) certificate that is trusted.
  client.setInsecure();

  // Enable a plain-test start.
  client.setPlainStart();

  if (!client.connect(server, SMTP_PORT)) {
    Serial.println("Connection failed!");
    return;
  };

  Serial.println("Connected to server (in the clear, in plaintest)");

  if (!readAllSMTPLines()) goto err;

  Serial.println("Sending  : EHLO\t\tin the clear");
  client.print("EHLO there\r\n");

  if (!readAllSMTPLines()) goto err;

  Serial.println("Sending  : STARTTLS\t\tin the clear");
  client.print("STARTTLS\r\n");

  if (!readAllSMTPLines()) goto err;

  Serial.println("Upgrading connection to TLS");
  if ((ret = client.startTLS()) <= 0) {
    Serial.printf("Upgrade connection failed: err %d\n", ret);
    goto err;
  }

  Serial.println("Sending  : EHLO again\t\tover the now encrypted connection");
  client.print("EHLO again\r\n");

  if (!readAllSMTPLines()) goto err;

  // normally, as this point - we'd be authenticating and then be submitting
  // an email. This has been left out of this example.

  Serial.println("Sending  : QUIT\t\t\tover the now encrypted connection");
  client.print("QUIT\r\n");

  if (!readAllSMTPLines()) goto err;

  Serial.println("Completed OK\n");
err:
  Serial.println("Closing connection");
  client.stop();
}

// SMTP command repsponse start with three digits and a space;
// or, for continuation, with three digits and a '-'.
static bool readAllSMTPLines() {
  String s = "";
  int i;

  // blocking read; we cannot rely on a timeout
  // of a NetworkClientSecure read; as it is non
  // blocking.
  const unsigned long timeout = 15 * 1000;
  unsigned long start = millis();  // the timeout is for the entire CMD block response; not per character/line.
  while (1) {
    while ((i = client.available()) == 0 && millis() - start < timeout) {
      /* .. wait */
    };
    if (i == 0) {
      Serial.println("Timeout reading SMTP response");
      return false;
    };
    if (i < 0)
      break;

    i = client.read();
    if (i < 0)
      break;

    if (i > 31 && i < 128) s += (char)i;
    if (i == 0x0A) {
      Serial.print("Receiving: ");
      Serial.println(s);
      if (s.charAt(3) == ' ')
        return true;
      s = "";
    }
  }
  Serial.printf("Error reading SMTP command response line: %d\n", i);
  return false;
}

void loop() {
  // do nothing
}
