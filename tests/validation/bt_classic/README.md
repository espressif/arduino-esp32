# BT Classic (BluetoothSerial) Validation Test

Two-DUT hardware validation for the `BluetoothSerial` SPP library.

## Coverage gaps (HITL-deferred)

Multi-client SPP scenarios (two simultaneous centrals to one acceptor, per-peer
`writeTo()` / `onPeerData()` routing, broadcast to N peers) require a **third
DUT** or two external phones. This suite uses one server + one client only.

What *is* covered with two DUTs:

- `peerCount()`, `peers()`, `onConnect` / `onDisconnect`, `onPeerData`, `writeTo()`,
  and `disconnect(addr)` (phase 2)
- Shared Bluedroid host ownership: an externally enabled host is adopted and
  left up by BLE and BluetoothSerial (phase 1); BLE-first and Classic-first
  coexistence, `end()` of the non-owner, and `end(true)` of a non-owner while
  the other library keeps the host up (phase 2)
- Existing bidirectional data, bond management, and lifecycle phases

See `libraries/BluetoothSerial/examples/MultiClientSerial` for manual multi-client
testing.
