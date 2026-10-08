# BT Classic (BluetoothSerial) Validation Test

Two-DUT hardware validation for the `BluetoothSerial` SPP library.

## Coverage gaps (HITL-deferred)

Multi-client SPP scenarios (two simultaneous centrals to one acceptor, per-peer
`writeTo()` / `onPeerData()` routing, broadcast to N peers) require a **third
DUT** or two external phones. This suite uses one server + one client only.

What *is* covered with two DUTs:

- `peerCount()`, `peers()`, `onConnect` / `onDisconnect`, `onPeerData`, `writeTo()`,
  and `disconnect(addr)` (phase 2)
- Existing bidirectional data, bond management, and lifecycle phases

See `libraries/BluetoothSerial/examples/MultiClientSerial` for manual multi-client
testing.
