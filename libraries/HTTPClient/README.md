# HTTPClient

Arduino HTTP and HTTPS client for ESP32. It can use a caller-owned `NetworkClient` / `NetworkClientSecure`, or create the transport itself through the compatibility `begin(url)` API.

## HTTPS certificate validation

`begin("https://...")` needs a trust anchor: a CA certificate, the builtin CA certificate bundle (`useBuiltinCACertBundle()`), or an explicit `setInsecure()`. Without one, the request fails with `HTTPC_ERROR_CONNECTION_REFUSED`.

That is a breaking change from 3.x, where the same call silently skipped validation.

The builtin Mozilla CA bundle is linked only into sketches that call `useBuiltinCACertBundle()`. On ESP32 that is about **70 KB of flash** (the 65 KB bundle plus attach code). A single PEM passed to `begin(url, CAcert)` or `NetworkClientSecure::setCACert()` is about **1.3 KB**. Other SoCs are in the same range (about 71–75 KB in CI).

| Trust method | Typical FLASH vs plain HTTP |
|---|---:|
| `setInsecure()` | +70 B |
| Own CA (`begin(url, CAcert)`) | +1.3 KB |
| `useBuiltinCACertBundle()` | +70 KB |

Prefer a pinned CA when the sketch talks to one host and flash is tight. Use the builtin bundle when the client must trust public CAs in general.

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
