
# Arduino Nano Nora Recovery Sketch

This sketch implements the DFU recovery mode logic, called by all sketches
when a double tap on the RESET button is detected. It should not be uploaded
as any other sketch; instead, this should be compiled and then flashed in
the module's `factory` partition.

## Compilation

The binary can be compiled with the Arduino 2.x IDE or CLI using the
`nano_nora` variant. In particular, using the CLI the resulting binary
can be exported to the `build` directory with the `-e` switch to
`arduino-cli compile`.

## Automatic installation

By replacing the binary in the current folder, automatic installation
can be performed by running the "Upload with Programmer" action on any
sketch in the Arduino 2.x IDE or CLI. In particular, using the CLI the
binary can be installed via the command:

```
arduino-cli compile -u --programmer esptool
```

## Manual installation

Once compiled, the binary can also be installed on a board using `esptool.py`
with the following command:

```
esptool.py --chip esp32s3 --port "/dev/ttyACM0" --baud 921600  --before default_reset --after hard_reset write_flash  -z --flash_mode dio --flash_freq 80m --flash_size 16MB 0xF70000 "nora_recovery.ino.bin"
```

where:
- `esptool.py` is located in your core's install path under `tools/esptool_py`;
- `/dev/ttyACM0` is the serial port exposed by the board to be used;
- `0xF70000` is the factory partition address (make sure it matches the
  offset in the variant's `{build.partitions}` file);
- `nora_recovery.ino.bin` is the compiled sketch image.

Due to a BSP issue, the first call to `esptool.py` will enter the hardware
bootloader for programming, but fail with an "Input/output error". This is
a known issue; calling the program again with the same arguments will now
work correctly.

Once flashing is complete, a power cycle (or RESET button tap) is required
to leave the `esptool.py` flashing mode and load user sketches.
