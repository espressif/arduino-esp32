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

hw_targets="'esp32','esp32s2','esp32s3','esp32c3','esp32c6','esp32h2','esp32p4'"
wokwi_targets="'esp32','esp32s2','esp32s3','esp32c3','esp32c6','esp32h2','esp32p4'"
qemu_targets="'esp32','esp32c3'"

mkdir -p info

echo "[$hw_targets]" > info/hw_targets.txt
echo "[$hw_types]" > info/hw_types.txt
echo "[$wokwi_targets]" > info/wokwi_targets.txt
echo "[$wokwi_types]" > info/wokwi_types.txt
echo "[$qemu_targets]" > info/qemu_targets.txt
echo "[$qemu_types]" > info/qemu_types.txt

{
    echo "build-types=[$build_types]"
    echo "hw_targets=[$hw_targets]"
    echo "hw-types=[$hw_types]"
    echo "wokwi_targets=[$wokwi_targets]"
    echo "wokwi-types=[$wokwi_types]"
    echo "qemu_targets=[$qemu_targets]"
    echo "qemu-types=[$qemu_types]"
} >> "$GITHUB_OUTPUT"
