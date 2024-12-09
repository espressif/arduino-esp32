#!/bin/bash

build_types="'validation'"
hw_types="'validation'"
wokwi_types="'validation'"
qemu_types="'validation'"

if [[ $IS_PR != 'true' ]] || [[ $PERFORMANCE_ENABLED == 'true' ]]; then
    build_types+=",'performance'"
    hw_types+=",'performance'"
    #wokwi_types+=",'performance'"
    #qemu_types+=",'performance'"
fi

targets="'esp32','esp32s2','esp32s3','esp32c3','esp32c6','esp32h2','esp32p4'"

mkdir -p info

echo "[$wokwi_types]" > info/wokwi_types.txt
echo "[$targets]" > info/targets.txt

{
    echo "build-types=[$build_types]"
    echo "hw-types=[$hw_types]"
    echo "wokwi-types=[$wokwi_types]"
    echo "qemu-types=[$qemu_types]"
    echo "targets=[$targets]"
} >> "$GITHUB_OUTPUT"
