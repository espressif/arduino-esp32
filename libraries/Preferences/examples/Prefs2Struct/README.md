# Preferences struct as bytes

Stores a small schedule as raw bytes with `putBytes` and reads it back with `getBytes`. Preferences has no struct type. The sketch copies the bytes into a `schedule_t` array.

## Supported Targets

Any ESP32 board.

## How to Use Example

Open the Serial Monitor at 115200 baud, then flash the sketch. It uses the namespace `schedule` and the key `schedule`.

The first write stores two entries:

| hour | minute | setting1 | setting2 |
| ---- | ------ | -------- | -------- |
| 9    | 30     | 235      | 255      |
| 20   | 15     | 0        | 1        |

The sketch prints the second entry, copies both entries into a 3-element array, writes `{8, 30, 20, 21}` as the third entry, and prints those bytes in hex.

Expected log:

```
20:15 0/1
09 1E EB FF 14 0F 00 01 08 1E 14 15
```

NVS is meant for small values. The bytes are stored as a blob, so the struct layout is not saved with them. A struct that changes often is a poor fit for this partition.
