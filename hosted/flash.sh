#!/bin/bash

if [ "$#" -eq 2 ]; then
	esptool.py --port $1 --chip esp32c6 -b 2000000 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 4MB --flash_freq 80m 0x10000 $2
else
    echo "Error: You must pass the port and the file as arguments."
    exit 1
fi
