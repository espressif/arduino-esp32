# ClientHTTP
The ClientHTTP class implements support for REST API client calls to a HTTP server. It inherits from Client and thus implements a superset of that class' interface.

The ClientHTTP class decorates the underlying stream with HTTP request headers and reads the HTTP response headers from the stream before the stream is made available to the program. Also chunked responses are handled 'under the hood' offering a clean stream to which a request payload (if any) can be written using the standard Print::write() functions and from which response payloads (if any) can be read using the standard Stream::read() functions.

The ClientHTTP class has a smaller memory footprint than the alternative HTTPClient class. Moreover: because of the Stream oriented nature of ClientHTTP no memory buffers are needed, neither for sending payload, nor for receiving payload. It still remains possible to use such memory buffers though. Chunked response from the stream is correctly implemented.
No Strings are used to prevent heap fragmentation. Only small stack buffers are used. Two exceptions are: the host name is copied into a heap char array, and the requestHeaders and responseHeaders are dynamic standard containers (map) with `std::string` key and `std::string` value.

ClientHTTP is able to handle redirections. Support for this may be omitted by clearing the `#define SUPPORT_REDIRECTS` in the header file, thus saving some program memory bytes.

Request headers can easily be set by ClientHTTP. Response headers can be easily collected by
ClientHTTP.

Any client may be used, e.g. `WiFiClient`, `WiFiClientSecure` and `EthernetClient`.

A HTTP `GET`, `HEAD`, `POST`, `PUT`, `PATCH` or `DELETE` always follows the same skeleton.

Integration with ArduinoJson, version 5 and 6, both for sending and receiving Json payloads is very intuitive.

The same library is defined for both the ESP8266 and the ESP32.

The following examples are available. All but one are HTTPS / TLS 1.2 examples:
- Authorization: how to use basic authorization
- BasicClientHTTP: how to `GET` a payload from a HTTP server and print it to `Serial`
- BasicClientHTTPS: how to `GET` a paylaod from a HTTPS server and print it to `Serial`
- DataClientHTTPS: how to `GET` a payload from a HTTPS server and handle the data using `StreamString` or a buffer
- POSTClientHTTPS: how to `POST` a payload to a HTTPS server and print the response to `Serial`
- POSTJsonClientHTTPS: how to `POST` an ArduinoJson to a HTTPS server and deserialize the response into another ArduinJson without buffers. All data is buffered into the ArduinoJson only
- POSTSizerClientHTTPS: how to determine the size of a payload by printing to a utility `class Sizer` first. Next the resulting length is used to set the Content-Length request header by calling `POST`
- ReuseConnectionHTTPS: how to reuse a connection, including correct handling of the Connection: close response header. This example DOES NOT WORK on the ESP8266 YET!

Still to be done:
- Support for redirection to a different host

## Request and response headers
Request headers can be set by just defining them before calling the REST method (`GET`, `POST`, etc):
```cpp
  http.requestHeaders["Content-Type"] = "application/json";
  http.requestHeaders["Connection"] = "close";
```
Response headers can be collected by just defining them before `::status()` is called:
```cpp
  http.responseHeaders["Content-Type"];
```
After the `::status()` command `responseHeaders['Content-Type']` is either an empty string if the server did not send the response header, or it is populated with the value sent.

## Skeleton program
A HTTP REST API call always follows the following skeleton
- Prepare a transport client, like `WiFiClient client` or `WiFiClientSecure client`
- Prepare a `ClientHTTP http(client)`, passing the transport client into the constructor
- Set request headers
- Define response headers to be collected
- `htpp.connect(host, port)`
- Call the REST API method, e.g. `http.POST(payload, payloadLength)`
- Send the payload, if any, to the server using standard `Print::write()` commands
- Call `http.status()` which reads the response headers from the stream and returns the HTTP response code (greater than 0) or an error code (less than 0)
- Check any response headers, if needed
- Read the payload sent by the server, if any, using standard `Stream::read()` commands
- Close the connection calling `http.stop()`, or reuse the connection if the server did not sent a `Connection: close` response header, nor closed it's side of the connection

## ArduinoJson integration
ArduinoJson documents, both version 5 and version 6, can directly be POSTed to a REST server:

```cpp
  DynamicJsonDocument requestDocument(requestCapacity);
  ...
  http.requestHeaders["Content-Type"] = "application/json";
  http.POST("/post", measureJson(requestDocument));
  serializeJson(requestDocument, http);
  ...
```
Also ArduinoJson documents, both version 5 and 6, can directly be deserialized from the response:
```cpp
  ...
  http.responseHeaders["Content-Type"];
  http.status();
  DynamicJsonDocument responseDocument(responseCapacity);
  if(http.responseHeaders["Content-Type"] != NULL && strcasecmp(http.responseHeaders["Content-Type"], "application/json") == 0) {
    deserializeJson(responseDocument, http);
  }
```
See the examples for how to use ArduinoJson 5.


