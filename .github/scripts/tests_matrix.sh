#!/bin/bash

# QEMU is disabled for now
qemu_enabled="false"

build_types='"validation"'
hw_types='"validation"'
wokwi_types='"validation"'
qemu_types='"validation"'

if [[ $IS_PR != 'true' ]] || [[ $PERFORMANCE_ENABLED == 'true' ]]; then
    build_types+=',"performance"'
    hw_types+=',"performance"'
    #wokwi_types+=',"performance"'
    #qemu_types+=',"performance"'
fi

hw_targets='"esp32","esp32s2","esp32s3","esp32c3","esp32c5","esp32c6","esp32h2","esp32p4"'
wokwi_targets='"esp32","esp32s2","esp32s3","esp32c3","esp32c6","esp32h2","esp32p4"'
qemu_targets='"esp32","esp32c3"'

# The build targets should be the sum of the hw, wokwi and qemu targets without duplicates
build_targets=$(echo "$hw_targets,$wokwi_targets,$qemu_targets" | tr ',' '\n' | sort -u | tr '\n' ',' | sed 's/,$//')

mkdir -p info

# Create a single JSON file with all test matrix information
cat > info/test_matrix.json <<EOF
{
    "performance_enabled": $PERFORMANCE_ENABLED,
    "qemu_enabled": $qemu_enabled,
    "build_targets": [$build_targets],
    "build_types": [$build_types],
    "hw_targets": [$hw_targets],
    "hw_types": [$hw_types],
    "wokwi_targets": [$wokwi_targets],
    "wokwi_types": [$wokwi_types],
    "qemu_targets": [$qemu_targets],
    "qemu_types": [$qemu_types]
}
EOF

{
    echo "build-targets=[$build_targets]"
    echo "build-types=[$build_types]"
    echo "qemu-enabled=$qemu_enabled"
    echo "qemu-targets=[$qemu_targets]"
    echo "qemu-types=[$qemu_types]"
} >> "$GITHUB_OUTPUT"
