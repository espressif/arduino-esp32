# IPv6 Multicast on Thread (OpenThread Arduino)

This guide explains how to **join and leave IPv6 multicast groups** on the Thread
interface, how the library layers relate, and which shipped examples use each
pattern.

Multicast lets one sender reach many receivers on the mesh without knowing each
node’s unicast address. On Thread, application traffic commonly uses
**realm-local** groups (`ff03::/16`) so every node in the partition can
participate.

---

## Send vs receive

| Role | Subscribe required? | Typical API |
|------|---------------------|-------------|
| **Receiver** — node must accept packets sent to a group address | **Yes** | `subscribeMulticast()`, `beginMulticast()`, or `joinMulticastGroup()` |
| **Sender** — node only transmits to a group address | **No** | `beginPacket(group, port)` / `endPacket()` (UDP) or CoAP client send |

Sending to `ff03::abcd` does not require joining that group. Only nodes that
**listen** for group traffic must subscribe.

---

## Stack-level API (`OThread`)

These methods call OpenThread `otIp6SubscribeMulticastAddress()` /
`otIp6UnsubscribeMulticastAddress()` on the Thread interface:

```cpp
bool subscribeMulticast(const IPAddress &group);
bool unsubscribeMulticast(const IPAddress &group);
```

- **`group`** must be an **IPv6 multicast** address (first byte `0xFF`).
- Call on the global **`OThread`** singleton after `OThread.begin()`.
- **Best practice:** subscribe after the device has **attached** (role Child,
  Router, or Leader) so the mesh-local address is ready; binding before attach
  can succeed but traffic may not flow until attach completes.
- Pair every subscribe with an unsubscribe before shutdown (or use a wrapper
  that does it in `stop()`).

### Reference counting

Membership is **reference-counted** inside the library:

| Event | Effect |
|-------|--------|
| First `subscribeMulticast(g)` for group `g` | OpenThread joins `g`; refcount = 1 |
| Another subscribe to the same `g` | Refcount only (no second OT join) |
| `unsubscribeMulticast(g)` while refcount > 1 | Refcount only |
| Last unsubscribe for `g` | OpenThread leaves `g` |

Multiple subsystems can share one group safely — for example
`OThreadUDP.beginMulticast()` and `OThreadCoAPServer.joinMulticastGroup()` on
the same address. The refcount table is mutex-protected.

### Querying membership

Use the cached multicast address helpers on `OThread`:

- `getMulticastAddressCount()` / `getMulticastAddress(index)`
- `getAllMulticastAddresses()`
- `clearMulticastAddressCache()` after role or membership changes

---

## UDP: `OThreadUDP`

### `beginMulticast(group, port)` (recommended for UDP receivers)

```cpp
OThreadUDP Udp;
const uint8_t groupBytes[16] = {0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xab, 0xcd};
IPAddress group(IPv6, groupBytes);  // ff03::abcd

Udp.beginMulticast(group, 5051);
// ... parsePacket() / read() in loop() ...
Udp.stop();  // unsubscribes and closes the socket
```

Internally: `begin(port)` then `OThread.subscribeMulticast(group)`.
`stop()` calls `unsubscribeMulticast()` for that group.

The socket also receives **unicast** datagrams on the same port (useful when
the receiver replies with a unicast ACK).

### `begin(port)` only (UDP senders and unicast receivers)

```cpp
Udp.begin(5051);  // bind; no group join
Udp.beginPacket(multicastGroup, 5051);
Udp.write(...);
Udp.endPacket();
```

Use this on **clients** that send to a multicast group but do not need to
receive group traffic. They still bind a local port if they expect **unicast**
replies (for example an ACK from the lamp).

### Manual subscribe + bind

When you need explicit control:

```cpp
Udp.begin(5050);
OThread.subscribeMulticast(group);
// ...
OThread.unsubscribeMulticast(group);
Udp.stop();
```

Prefer `beginMulticast()` unless you are sharing one subscription across several
sockets or toggling membership without closing the socket.

---

## CoAP: `OThreadCoAPServer`

CoAP servers that must **receive** requests sent to a multicast group need a
group join **after** `begin()`:

```cpp
OThreadCoAPServer.on("Lamp", OT_COAP_METHOD_GET | OT_COAP_METHOD_PUT, onLamp);
OThreadCoAPServer.begin();
OThreadCoAPServer.joinMulticastGroup(LAMP_GROUP);  // e.g. ff03::abcd
```

- `joinMulticastGroup()` delegates to `OThread.subscribeMulticast()`.
- `leaveMulticastGroup(group)` removes one reference at runtime.
- `stop()` leaves all groups recorded by the server.

### CoAP multicast behavior (RFC 7252)

- **Clients** sending to a multicast address should use **non-confirmable** (NON)
  requests (§8.1). Use `sendNonBlocking()` for fire-and-forget.
- **Servers** should **not reply** to multicast requests (§8.2). Check
  `req.isMulticast()` before `resp.send()`.
- For a **unicast** command to one server, address it by unicast and use
  blocking `client.PUT(...)` / `GET(...)`.

### Multicast and CON

**CON and multicast cannot be used together.** If the destination is an IPv6
multicast address (`0xFF…`), `OThreadCoAPClient` always transmits **NON**, even
when `setConfirmable(true)` — RFC 7252 §8.1. CON retransmission and
`setTimeout()`-aligned retries **never apply** to those sends.

| Client API | Multicast destination |
|------------|------------------------|
| `setConfirmable(true)` + blocking method | Forced **NON** on the wire; may still block for first reply |
| `sendNonBlocking()` | **NON**, no wait — recommended for group commands |
| Unicast CON to one server | Use the device's IPv6 address, not the group |

On the server, `joinMulticastGroup()` is required to **receive** group CoAP;
incoming group requests are NON. Skip `resp.send()` for `req.isMulticast()`.
Full CON/NON reference: library [README.md](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/README.md) and
[openthread_coap.rst](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_coap.html) (`coap_con_non`).

### `OThreadCoAPSecureServer`

The secure server does **not** expose `joinMulticastGroup()`. For multicast
receive on a dual-transport device, use plain `OThreadCoAPServer` on port **5683**
for group traffic and `OThreadCoAPSecureServer` on **5684** for secured
unicast, or call `OThread.subscribeMulticast(group)` at the stack level.

---

## CLI equivalent

Interactive and programmatic CLI sketches join groups with:

```text
ipmaddr add ff03::abcd
```

That is equivalent to `OThread.subscribeMulticast()` for application purposes.
CLI UDP examples pair `ipmaddr add` with `udp bind`.

---

## Multicast scopes

| Prefix | Scope | Typical use in this library |
|--------|--------|-----------------------------|
| `ff02::` | Link-local (one hop) | Rare in examples |
| `ff03::` | Realm-local (whole Thread partition) | Native UDP/CoAP light switch (`ff03::abcd`) |
| `ff05::` | Site-local | CLI CoAP lamp/switch (`ff05::abcd`) |

Choose the scope that matches how far commands must propagate. When mixing CLI
and Native boards, align **network key, channel, group address, and scope**.

Built-in groups such as `ff03::1` (realm-local all nodes) appear in address
listings; application demos usually define a custom suffix (for example
`ff03::abcd`).

---

## Shutdown order

Before `OThread.end()`:

1. `OThreadCoAPServer.stop()` / destroy CoAP clients (releases group refs held
   by the server)
2. `OThreadUDP.stop()` (unsubscribes if `beginMulticast()` was used)
3. Any direct `unsubscribeMulticast()` calls for groups not owned by wrappers
4. `OThread.end()`

See [README.md](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/README.md) (Shutdown order) and
[`examples/Native/StackShutdown/`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/StackShutdown).

---

## Examples that use multicast

### Native API (C++ wrappers)

| Example | Sketch | Multicast role | Mechanism |
|---------|--------|----------------|-----------|
| [UDP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch) | `light/light.ino` | **Receiver** | `OThreadUDP.beginMulticast(ff03::abcd, 5051)` |
| [UDP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch) | `switch/switch.ino` | **Sender** | `Udp.begin(5051)` + send to `ff03::abcd` (no subscribe) |
| [CoAP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch) | `light/light.ino` | **Receiver** | `OThreadCoAPServer.joinMulticastGroup(ff03::abcd)` |
| [CoAP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch) | `switch/switch.ino` | **Sender** | NON multicast `PUT` to `ff03::abcd` via `sendNonBlocking()` |

Native UDP sensor network (`UDP_SensorNetwork/`) uses **unicast** to the Leader
RLOC, not application multicast.

### CLI API

| Example | Sketch | Multicast role | Mechanism |
|---------|--------|----------------|-----------|
| [UDP sensor collector](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/UDP/udp_sensor_collector) | `udp_sensor_collector.ino` | **Receiver** | `ipmaddr add ff03::abcd` + `udp bind` |
| [UDP sensor node](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/UDP/udp_sensor_node) | `udp_sensor_node.ino` | **Sender** | `udp send ff03::abcd 5050 ...` |
| [CoAP lamp](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_lamp) | `coap_lamp.ino` | **Receiver** | CLI multicast setup (`ff05::abcd`) |
| [CoAP switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_switch) | `coap_switch.ino` | **Sender** | CON CoAP PUT to `ff05::abcd` |

### Documentation sample

The Sphinx UDP page shows a minimal **receiver** with `beginMulticast(ff03::1, 7)`
— see [openthread_udp.rst](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_udp.html).

---

## Quick reference

```
Receiver (UDP)     →  Udp.beginMulticast(group, port)     →  subscribeMulticast inside
Receiver (CoAP)    →  CoAPServer.joinMulticastGroup(g)    →  subscribeMulticast inside
Sender             →  beginPacket(group, port)            →  no subscribe
Stack / advanced   →  OThread.subscribeMulticast(g)       →  pair with unsubscribe
Runtime leave      →  leaveMulticastGroup(g) or unsubscribeMulticast(g)
Teardown           →  Udp.stop() / CoAPServer.stop()      →  auto unsubscribe
```

## See also

- [README.md](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/README.md) — library overview, shutdown order, CoAP multicast notes
- [docs/en/openthread/Multicasting.rst](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/Multicasting.html) — Sphinx version of this guide
- [openthread_core.rst](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html) — `subscribeMulticast` API reference
- [openthread_udp.rst](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_udp.html) — `OThreadUDP` API
- [openthread_coap.rst](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_coap.html) — CoAP server multicast and RFC notes
