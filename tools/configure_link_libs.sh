#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Arduino IDE only (Linux/macOS): write {build.path}/link_libs.rsp for the final link.
# When the sketch uses libraries/Matter and lib/libbt.matter.a exists (bt_matter_harvest
# on esp32c3, esp32c5, esp32h2), replaces standalone -lbt with -lbt.matter in flags/ld_libs.

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
LIBBT_MATTER_A="${SDK_PATH}/lib/libbt.matter.a"

if [ ! -f "$LD_LIBS" ]; then
	echo "configure_link_libs: missing ${LD_LIBS}" >&2
	exit 1
fi

if [ -d "${BUILD_PATH}/libraries/Matter" ] && [ -f "$LIBBT_MATTER_A" ]; then
	sed -E 's/(^| )-lbt( |$)/\1-lbt.matter\2/g' "$LD_LIBS" > "$OUT"
else
	cp -f "$LD_LIBS" "$OUT"
fi

exit 0
