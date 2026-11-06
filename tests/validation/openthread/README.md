# OpenThread Validation Test

Validates the OpenThread stack and Arduino `OThread` / `OThreadCLI` APIs on chips with an 802.15.4 radio. This is a **multi-DUT** test: device0 forms a Thread network as leader and device1 joins as child using the leader's active dataset relayed by pytest.

## Architecture

```
 ┌──────────────┐         802.15.4          ┌──────────────┐
 │    Leader    │◄─────── wireless ────────►│    Child     │
 │  (device0)   │                           │  (device1)   │
 └──────┬───────┘                           └──────┬───────┘
        │ serial                                   │ serial
        ▼                                          ▼
 ┌─────────────────────────────────────────────────────────┐
 │                 pytest (test_openthread.py)             │
 └─────────────────────────────────────────────────────────┘
```

## Test Cases

### Leader (`leader/leader.ino`)

| Test Function | Description |
|---|---|
| `test_leader_stack_running` | Verify `OThread` and `OThreadCLI` are started |
| `test_leader_cli_version` | CLI `version` contains `OPENTHREAD` |
| `test_leader_cli_detached_state` | CLI `state` is `detached` before joining |
| `test_leader_cli_util_exec` | `otGetRespCmd("state")` returns detached |
| `test_leader_form_network` | Init dataset, start Thread, become leader, export dataset hex |
| `test_leader_ot_role` | `otGetDeviceRole()` and CLI `state` report leader |
| `test_leader_ot_network_getters` | Network name, channel, PAN ID, ext PAN ID, network key |
| `test_leader_ot_leader_addresses` | RLOC, mesh-local EID, unicast/multicast address lists |
| `test_leader_ot_current_dataset` | Active `DataSet` matches formed network |
| `test_leader_dataset_class` | `DataSet` setters/getters on a local instance |
| `test_leader_cli_util_network_fields` | CLI `networkname`, `channel`, `panid`, `extpanid` |
| `test_leader_cli_print_network_info` | `otPrintRespCLI` and `otPrintNetworkInformation` |

### Child (`child/child.ino`)

| Test Function | Description |
|---|---|
| `test_child_stack_running` | Verify `OThread` and `OThreadCLI` are started |
| `test_child_cli_version` | CLI `version` contains `OPENTHREAD` |
| `test_child_cli_detached_state` | CLI `state` is `detached` before joining |
| `test_child_cli_util_exec` | `otGetRespCmd("state")` returns detached |
| `test_child_join_network` | Import dataset from pytest, start Thread, attach to network |
| `test_child_network_matches_leader` | Network getters match `NETINFO` from pytest |
| `test_child_ot_role_attached` | Role is child or router after attach |
| `test_child_ot_network_getters` | Network getters match expected netinfo |
| `test_child_ot_attached_addresses` | RLOC16, mesh-local EID, address caches |
| `test_child_ot_current_dataset` | Active `DataSet` matches expected netinfo |
| `test_child_cli_active_dataset_hex` | CLI `dataset active -x` matches imported dataset |
| `test_child_cli_util_network_fields` | CLI network fields match `OThread` getters |
| `test_child_cli_print_network_info` | `otPrintRespCLI` and `otPrintNetworkInformation` |

## Requirements

- **Hardware**: Two ESP32-C6 or ESP32-H2 boards (802.15.4 radio required)
- **Wokwi/QEMU**: Not supported (multi-device test)
- **CI Runner**: `two_duts`
- **SoC Config**: `CONFIG_OPENTHREAD_ENABLED=y`, `CONFIG_SOC_IEEE802154_SUPPORTED=y`
- **FQBN**: `PartitionScheme=huge_app,OThreadMode=ot_rcp` (both devices)

## Serial Protocol

1. Both devices print `[LEADER] Ready` / `[CHILD] Ready`
2. pytest sends `GO` to the leader
3. Leader runs Unity tests, forms the network, then prints `[LEADER] DATASET=<hex>`, `[LEADER] NETINFO name=... channel=... panid=... extpanid=...`, `[LEADER] NETWORK_READY`, and `[LEADER] DONE`
4. pytest sends `DATASET <hex>` and `NETINFO name=... channel=... panid=... extpanid=...` to the child
5. Child runs Unity tests and prints `[CHILD] JOINED`, then `[CHILD] DONE`

## Notes

- Shared helpers are in `ot_test_helpers.h`; sketches include it as `#include "../ot_test_helpers.h"`.
- CLI RX/TX buffer sizes must be set before `OThreadCLI.begin()`; resizing after the CLI task starts breaks command processing.
- Network attachment timeout is 30 seconds per device.
- If leader Unity tests fail, the leader prints `[LEADER] EXPORT_FAILED` instead of a dataset.
