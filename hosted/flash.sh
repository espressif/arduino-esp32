#!/bin/bash

if [ "$#" -eq 2 ]; then
	esptool --port $1 --chip esp32c6 -b 2000000 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 4MB --flash-freq 80m 0x10000 $2
else
    echo "Error: You must pass the port and the file as arguments."
    exit 1
fi
