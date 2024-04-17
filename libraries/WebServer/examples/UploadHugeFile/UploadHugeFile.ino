#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <uri/UriRegex.h>
#include <SD.h>

const char* ssid = "**********";
const char* password = "**********";

WebServer server(80);

File rawFile;
void handleCreate() {
  server.send(200, "text/plain", "");
}
void handleCreateProcess() {
  String path = "/" + server.pathArg(0);
  HTTPRaw& raw = server.raw();
  if (raw.status == RAW_START) {
    if (SD.exists((char*)path.c_str())) {
      SD.remove((char*)path.c_str());
    }
    rawFile = SD.open(path.c_str(), FILE_WRITE);
    Serial.print("Upload: START, filename: ");
    Serial.println(path);
  } else if (raw.status == RAW_WRITE) {
    if (rawFile) {
      rawFile.write(raw.buf, raw.currentSize);
    }
    Serial.print("Upload: WRITE, Bytes: ");
    Serial.println(raw.currentSize);
  } else if (raw.status == RAW_END) {
    if (rawFile) {
      rawFile.close();
    }
    Serial.print("Upload: END, Size: ");
    Serial.println(raw.totalSize);
  }
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void) {
  Serial.begin(115200);

  while (!SD.begin()) delay(1);
  Serial.println("SD Card initialized.");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on(UriRegex("/upload/(.*)"), HTTP_PUT, handleCreate, handleCreateProcess);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  delay(2);  //allow the cpu to switch to other tasks
}
