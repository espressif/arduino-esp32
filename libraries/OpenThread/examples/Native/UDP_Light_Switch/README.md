# UDP Light + Switch over Thread - Native API

This folder contains a two-board "smart light" example built with the Arduino
OpenThread Native API (`OThread` + `OThreadUDP`). The pair uses Thread
commissioning for network admission and raw IPv6 UDP packets for application
traffic.

| Sketch | Role |
| ------ | ---- |
| [`light/light.ino`](light/) | Server: Thread **Leader + Commissioner** and UDP-controlled RGB light. |
| [`switch/switch.ino`](switch/) | Client: Thread **Joiner** and UDP switch that toggles the light. |

One `light` can serve many `switch` nodes. The light forms the Thread network,
opens the Commissioner join window for PSKd `J01NME`, subscribes to
`ff03::abcd` on UDP port `5051`, and sends a unicast `ACK ON` / `ACK OFF` back
to the switch that sent the command.

The example uses UDP port `5051` intentionally. Avoid `5683` / `5684`
(CoAP/CoAPs) and `61631` (Thread TMF CoAP), because those ports are used by
OpenThread internals.

## How to Run

1. Flash [`light/light.ino`](light/light.ino) on one ESP32-H2 / ESP32-C6 /
   ESP32-C5 board and open Serial Monitor at `115200`.
2. Wait for the light to become Leader and print that the Commissioner is
   active.
3. Flash [`switch/switch.ino`](switch/switch.ino) on one or more additional
   boards.
4. Press the BOOT button on a switch to send `TOGGLE` to the light.

## See Also

- [`../ThreadCommissioning/`](../ThreadCommissioning/) - Same Joiner /
  Commissioner flow without UDP traffic.
- [`../UDP_SensorNetwork/`](../UDP_SensorNetwork/) - Many-to-one UDP telemetry
  example using a collector and multiple sensor nodes.

## License

Apache License 2.0.
