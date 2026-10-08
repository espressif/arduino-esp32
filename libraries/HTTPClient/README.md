# HTTPClient

Arduino HTTP and HTTPS client for ESP32. It can use a caller-owned `NetworkClient` / `NetworkClientSecure`, or create the transport itself through the compatibility `begin(url)` API.

## HTTPS certificate validation

`begin("https://...")` needs a trust anchor: a CA certificate, the builtin CA certificate bundle (`useBuiltinCACertBundle()`), or an explicit `setInsecure()`. Without one, the request fails with `HTTPC_ERROR_CONNECTION_REFUSED`.

That is a breaking change from 3.x, where the same call silently skipped validation.

The builtin CA bundle adds about 65 KB of flash, so it is only linked into sketches that call `useBuiltinCACertBundle()`.

Certificate date checks need a valid system clock. Call `configTime()` (NTP) before HTTPS requests, or verification can fail while the clock is still at the epoch.

```cpp
HTTPClient http;

// Public HTTPS: verify the server against the builtin CA bundle
http.useBuiltinCACertBundle();
http.begin("https://example.com/");
int code = http.GET();
http.end();
```

```cpp
HTTPClient http;

// Self-signed or private CA: pass the trust anchor
http.begin("https://device.local/", rootCACertificate);
```

```cpp
HTTPClient http;

// Insecure: skip certificate validation. Opt-in only, MITM-vulnerable.
http.setInsecure();
http.begin("https://192.168.1.1/");
```

`HTTPClient::useBuiltinCACertBundle()` and `HTTPClient::setInsecure()` do not change a caller-owned client. When you pass your own `NetworkClientSecure`, configure trust on that client.

```cpp
NetworkClientSecure client;
client.useBuiltinCACertBundle();  // or client.setCACert(rootCA); or client.setInsecure();

HTTPClient http;
http.begin(client, "https://example.com/");
```
