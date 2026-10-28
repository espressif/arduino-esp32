#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Arduino IDE only (Linux/macOS): write {build.path}/link_libs.rsp for the final link.
# Swaps -lbt for -lbt.matter when the sketch uses libraries/Matter and the SDK was built
# with bt_matter_harvest (flags/chipoble_bt_matter on esp32c3, esp32c5, esp32h2).
# PlatformIO uses flags/ld_libs via pioarduino-build.py and is not changed here.

set -euo pipefail

if [ "$#" -ne 2 ]; then
	echo "usage: configure_link_libs.sh <build.path> <compiler.sdk.path>" >&2
	exit 2
fi

BUILD_PATH="$1"
SDK_PATH="$2"
FLAGS_DIR="${SDK_PATH}/flags"
OUT="${BUILD_PATH}/link_libs.rsp"
LD_LIBS="${FLAGS_DIR}/ld_libs"

if [ ! -f "$LD_LIBS" ]; then
	echo "configure_link_libs: missing ${LD_LIBS}" >&2
	exit 1
fi

MATTER_LIB_DIR="${BUILD_PATH}/libraries/Matter"
CHIPOBLE_MARKER="${FLAGS_DIR}/chipoble_bt_matter"
LD_LIBS_NO_BT="${FLAGS_DIR}/ld_libs_no_bt"
MATTER_BT_LIB="${FLAGS_DIR}/matter_bt_lib"
LIBBT_MATTER_A="${SDK_PATH}/lib/libbt.matter.a"

mkdir -p "$BUILD_PATH"

if [ -d "$MATTER_LIB_DIR" ] && [ -f "$CHIPOBLE_MARKER" ] && [ -f "$LD_LIBS_NO_BT" ] && [ -f "$MATTER_BT_LIB" ]; then
	if [ ! -f "$LIBBT_MATTER_A" ]; then
		echo "configure_link_libs: Matter BLE optimization requires ${LIBBT_MATTER_A}" >&2
		echo "configure_link_libs: use esp32-arduino-libs for esp32c3/c5/h2 with bt_matter_harvest, or update the core SDK." >&2
		exit 1
	fi
	{
		tr -d '\n' < "$LD_LIBS_NO_BT"
		echo -n " "
		tr -d '\n' < "$MATTER_BT_LIB"
		echo
	} > "$OUT"
else
	cp -f "$LD_LIBS" "$OUT"
fi

exit 0
