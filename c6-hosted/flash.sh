#!/bin/bash

if [ "$#" -eq 1 ]; then
	esptool --port $1 --chip esp32c6 -b 2000000 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 4MB --flash-freq 80m 0x0 ./bootloader.bin 0x8000 ./partition-table.bin 0xd000 ./ota_data_initial.bin 0x10000 ./network_adapter.bin
else
    echo "Error: You must pass the port as argument."
    exit 1
fi
