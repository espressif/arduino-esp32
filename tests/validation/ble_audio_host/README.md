# LE Audio Host-Interop Validation Test

Validates the LE Audio (Generic Audio Framework) stack the library implements by
driving it from a **real Bluetooth host** — a Linux machine with an LE-Audio-capable
USB dongle — instead of a second ESP. The ESP is the peripheral/acceptor and
Auracast source **and** Auracast sink; the Linux host is the central/peer.
When the host controller cannot BIG-sync (typical of current Intel BE200
firmware), a Galaxy **S23 / S24** fills in as the other Auracast endpoint.

Where the ESP-to-ESP [`ble_audio`](../ble_audio) suite proves the library
interoperates *with itself*, this suite proves it interoperates with an
independent stack: **[Bumble](https://github.com/google/bumble)** owns HCI and
drives ASCS/CIS/BIS + LC3 in-process (control-plane GATT, Auracast announcement,
and the real LC3 data plane).

This is a **bench / HITL** suite. It carries the `le_audio_host` runner tag (no
normal-CI runner has an LE Audio dongle) and self-skips, per phase, when the
required host tooling is not present — so running it on a machine without the
bench simply skips rather than fails.

## Architecture

```
┌───────────────────────────┐        BLE / ISO         ┌──────────────────────────┐
│         ESP (DUT)          │◄───── advertising ───────│   Linux host + dongle    │
│  ble_audio_host.ino        │   connect + CIS / BIS    │  Bumble (exclusive HCI)  │
│  peripheral / acceptor /   │                          │  central / peer          │
│  Auracast source + sink    │                          │  (+ phone Auracast)      │
└─────────────┬──────────────┘                          └────────────┬─────────────┘
              │ serial (phase handshake + result lines)               │ Bumble GATT / CIS / BIG
              ▼                                                        ▼
        ┌──────────────────────────────────────────────────────────────────┐
        │                pytest (test_ble_audio_host.py)                     │
        │        runs on the SAME Linux host and drives the dongle           │
        └──────────────────────────────────────────────────────────────────┘
```

**Constraint:** one dongle → BlueZ and Bumble cannot share `hci0`. Stop and
runtime-mask `bluetooth.service` before the suite (see bring-up below).

This suite covers the DUT's **server/acceptor** side against an independent
stack. The library's **client/initiator** APIs are covered by the dual-DUT suite
in `../ble_audio`.

## Test Cases

Phases are driven over serial with a `START_PHASE_<n>` handshake; each is
recorded individually via `record_property`.

| Phase | Label | What it covers |
|---|---|---|
| 1 | control_gatt | Bumble GATT client validates the control surface: topology (PACS/ASCS/VCS/MICS/CSIS/CAS/HAS/TMAS + media & call bearer), coexistence (`0xA10C…`), VCP, MICP, MCP, CCP, CSIP, CAP (CAS DUT-side), HAS, TMAP, GMAP (when compiled in). Cross-checks `[DUT] …` serial lines. |
| 2 | broadcast_announcement | Host scans extended advertising for Broadcast Audio Announcement (`0x1852`) and Broadcast ID `0x123456`. |
| 3 | unicast_audio | Bumble Unicast Client: PACS/ASCS → CIS → LC3_16_2_1 tone to the DUT sink (DUT reports samples/meanamp); host optionally Goertzel-analyzes the DUT source. |
| 4 | broadcast_audio | **ESP = Auracast source.** Prefer Bumble PA/BIG sync + LC3 decode of the DUT 1 kHz tone. If the controller lacks Synchronized Receiver, a Galaxy S23/S24 **listens** and the operator confirms they hear the tone. |
| 5 | scan_delegator | **ESP = Auracast sink.** Bumble still checks BASS GATT. Then the phone **broadcasts** (no password); the DUT self-syncs and the host asserts BIS SDUs (`received >= 20`). |
| 6 | memory_release | Heap reclaimed on `BLE.end(true)`; reinit blocked afterward. |

## Requirements

### Hardware

- **1× ESP** with LE Audio support (`esp32s31`; only `esp32s31` is enabled in `ci.yml`).
- **1× Galaxy S23 or S24** (One UI 6.1+) for phone-assisted Auracast when the
  host HCI cannot BIG-sync. Both TX (Broadcast sound) and RX (Listen) are used.
- **1× LE-Audio-capable Bluetooth controller on the host.** Reference bench:
  **AICSemi AIC8800D80** USB dongle (HCI v13 / BT 5.4). After Bumble powers on,
  the suite probes CIS Central / Synchronized Receiver via HCI LE features.
  **ISO HCI caveat:** this dongle advertises CIS/BIG roles but
  `LE_Read_Buffer_Size_V2` often reports `iso_len=0` / `iso_packets=0`. Link
  setup (Create CIS / BIG sync) can still succeed; the ISO **data** path does
  not (`HCI_HARDWARE_ERROR` while pumping → DUT PLC silence). Phases
  `unicast_audio` / `broadcast_audio` **skip** in that case. For a green data
  plane, use a controller that reports non-zero ISO HCI buffers (nRF5340 Audio
  DK, Intel AX209 LE Audio, …).

### Host software (Linux bench)

1. **Free the HCI for Bumble** (BlueZ must not own the dongle; keep btusb bound
   so AIC8800 firmware stays loaded):

   ```bash
   sudo ./setup_bumble_hci.sh --once
   export BUMBLE_TRANSPORT=hci-socket:0
   ```

   The script stops/masks `bluetooth.service`, **stops user WirePlumber/PipeWire**
   (otherwise WP re-powers `hci0` after `HCIDEVDOWN`), and brings `hci0` DOWN via
   ioctl. Bumble needs that DOWN state for `HCI_CHANNEL_USER`. Do **not** use raw
   `usb:368b:8d81` on this dongle — HCI Reset hangs without kernel firmware.
   When finished: `sudo ./setup_bumble_hci.sh --undo`.
   AIC8800 note: if `HCIDEVUP` times out (`errno 110`) and BDADDR stays
   `00:00:…`, BT firmware is missing — the script re-enumerates USB so
   `aic_load_fw` can reload it. If that still fails, **physically unplug/replug**
   the dongle (known AIC quirk after Wi‑Fi/BT resets).
   Open the transport as root (`sudo -E …pytest…`).

2. **Python deps** (venv under this directory):

   ```bash
   python3 -m venv .venv
   .venv/bin/pip install -r ../../requirements.txt -r requirements.txt
   ```

   Installs `bumble[auracast]` (pulls in the `lc3` module). Bleak/BlueZ/PipeWire
   are not used.

3. **CI Runner tag**: `le_audio_host`.

Do **not** run BlueZ against the same dongle while the suite runs. Leave
`/etc/bluetooth/main.conf` alone unless you want to restore LE-only defaults for
other work outside this suite.

## Running the suite

```bash
sudo ./setup_bumble_hci.sh --once
export ESPPORT=/dev/ttyUSB0 BUMBLE_TRANSPORT=hci-socket:0

# fish: set -x ESPPORT /dev/ttyUSB0; set -x BUMBLE_TRANSPORT hci-socket:0
sudo -E env ESPPORT=$ESPPORT BUMBLE_TRANSPORT=$BUMBLE_TRANSPORT \
  .venv/bin/pytest -s test_ble_audio_host.py \
  --build-dir "$HOME/.arduino/tests/esp32s31/ble_audio_host/build.tmp" \
  --embedded-services esp,arduino --tb=short

sudo ./setup_bumble_hci.sh --undo
```

(`tests_run.sh` alone will not find the venv’s `pytest` under `sudo`.)

Phone assist is **on by default** (`BLE_AUDIO_PHONE=1`). Pass
`BLE_AUDIO_PHONE=0` in the `env` line to skip phone fallbacks. Under `pkexec`
there is no TTY: confirm phase 4 from another terminal with
`touch /tmp/bleah_phone_ok`.

## Phone-assisted Auracast (Galaxy S23 / S24)

The Intel BE200 on this bench has CIS but typically **no** Synchronized Receiver
or Isochronous Broadcaster in HCI LE features, so Bumble cannot join or emit a
BIG. The suite then uses the phone as the other Auracast endpoint:

| Phase | ESP role | Phone role | Pass criterion |
|---|---|---|---|
| 4 `broadcast_audio` | Source (1 kHz tone, name `BLEAH_*`, no password, PBA `0x1856`) | **Listen** | Operator hears the tone |
| 5 `scan_delegator` | Sink (scans any source, 16/24/48 kHz) | **Broadcast** | DUT `received >= 20` BIS SDUs |

**One UI 6.1+ (S23/S24).** Paths vary slightly by One UI version:

1. **Listen (phase 4)** — Quick Settings → *Audio broadcast* → Listen
   (or Settings → Connections → Bluetooth → ⋮ → *Listen to Auracast broadcast*).
   Join the DUT name printed in the pytest prompt (`BLEAH_xxxx`). Leave the
   password empty. You should hear a 1 kHz tone. Compatible Galaxy Buds are
   the reliable playback path; some One UI builds also play on the phone speaker.
   Then press Enter in the pytest terminal, or `touch /tmp/bleah_phone_ok`.

2. **Broadcast (phase 5)** — First **connect the phone to the DUT** (`BLEAH_*`)
   in Bluetooth settings (LE Audio / audio sink). Then Settings → Connections →
   Bluetooth → ⋮ → *Broadcast sound using Auracast*. Leave the password
   **empty** (no lock icon), start broadcast, and play media. Samsung drives
   BASS on that connection; broadcasting without connecting to the ESP does
   not sync. Pytest polls `[DUT] BcastSink result received=…` for up to 180 s.

Rebuild and flash `ble_audio_host` after library/sketch changes (PBA injection,
48 kHz sink PAC, sink SDU counter) before this HITL run.

## Pairing

Every phase pairs with Just Works (no I/O, no MITM, bonding on): the defaults in
most phases, set explicitly in `unicast_audio`, which also clears old bonds so a
previous Bumble identity cannot break SMP. Bumble's pairing config matches
(`mitm=False`, `NO_OUTPUT_NO_INPUT`). Each control-plane connect pairs once so
encryption-gated LE Audio characteristics are readable.

## Serial Protocol

1. The DUT prints `Device ready for name`; pytest sends a unique name.
2. Each phase is gated by a `START_PHASE_<n>` line; the DUT configures the
   roles that phase needs and prints a `[DUT] … ready` line.
3. The DUT echoes every observed control write as a `[DUT] …` event line.
   For the audio phases the host requests metrics with a `REPORT` line.

4. Every phase ends with a `[DUT] Phase<n> <label> done` line after its
   teardown; pytest waits for it when advancing and fails the previous phase's
   teardown if it never arrives (a crash or hang).

**TEMPORARY soft-reboot** (`BLE_AUDIO_HOST_PHASE_SOFT_REBOOT` /
`PHASE_SOFT_REBOOT` — keep matched): `audio.end()` releases every profile
through `common_deinit`, so `audio.begin()` may run again in the same boot, but
the suite still runs each audio phase on its own boot. Once same-boot re-init
passes on this bench, set both flags to `0`/`False` and delete the soft-reboot
branches.

## Self-skip behavior

- Whole suite: no HCI under `/sys/class/bluetooth`, or `bumble`/`lc3` missing.
- `control_gatt` / `broadcast_announcement` / `scan_delegator`: engine not compiled in.
- `unicast_audio` / `broadcast_audio`: LC3 not compiled in; controller lacks the
  needed ISO role (CIS Central / Synchronized Receiver); or stream setup fails
  in a way the phase records as SKIP (broadcast capture).
- With `BLE_AUDIO_PHONE=1` (default), phase 4 falls back to a phone Listen
  confirm instead of skipping; phase 5 additionally requires BIS SDUs from a
  phone source. Set `BLE_AUDIO_PHONE=0` to restore the old skip.

## Notes

- With `PHASE_SOFT_REBOOT` (the default, see above), each phase is a fresh boot.
- CAS (`0x1853`) is verified **DUT-side** (`[DUT] localsvc …`); it has no
  characteristics of its own. That lookup reads the NimBLE GATT table, so other
  builds print `[DUT] localsvc unsupported` and only the host view is checked.
- GMAS (`0x1858`) is checked only when the DUT registered it (GMAP compiled
  in); the GMAP Role must then include UGT.
- `broadcast_announcement` asserts the Public Broadcast Announcement (`0x1856`)
  when the DUT reports `pbp=1` (PBP compiled in).
- `unicast_audio` also configures the source ASE and asserts the DUT's Recorder
  tone arrives (RMS), besides the DUT decoding the host's stream.
- Control points that notify their result (HAS / GTBS / MCS) need a CCCD
  subscription before writes.
- CCP originate is expected to fail with Invalid Outgoing URI (`0x06`) on a
  GTBS-only build; the suite then accepts/terminates the DUT's incoming call.
- Unicast LC3 preset is **LC3_16_2_1** (16 kHz / 10 ms / 40 octets), matching
  the `BLEAudioUnicastServer` Mono defaults — not a BlueZ workaround.
