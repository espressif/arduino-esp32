#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# Build a Zigbee OTA (.zigbee) image from an Arduino-ESP32 sketch firmware binary.
# Intended for Sketch -> Export compiled Binary (see platform.txt postsavehex hook).
# Windows uses tools/zb_ota_image.exe from build_py_tools.yml (same as espota / gen_esp32part).
#
# Soft-skip (exit 0) when:
#   - Zigbee Mode is Disabled (empty --zigbee-mode)
#   - Sketch has no OTA client intent
#   - Required OTA #defines are missing or not literal integers
#

from __future__ import annotations

import argparse
import logging
import os
import re
import struct
import sys
from typing import Dict, Iterable, List, Optional, Tuple

logger = logging.getLogger("zb_ota_image")

# Sketch macros used for Export (document these in Zigbee OTA docs / example).
MACRO_MANUFACTURER = "OTA_UPGRADE_MANUFACTURER"
MACRO_IMAGE_TYPE = "OTA_UPGRADE_IMAGE_TYPE"
MACRO_FILE_VERSION = "OTA_UPGRADE_FILE_VERSION"
MACRO_RUNNING_VERSION = "OTA_UPGRADE_RUNNING_FILE_VERSION"
MACRO_HW_VERSION = "OTA_UPGRADE_HW_VERSION"
MACRO_MIN_HW = "OTA_UPGRADE_MIN_HW_VERSION"
MACRO_MAX_HW = "OTA_UPGRADE_MAX_HW_VERSION"
MACRO_HEADER_STRING = "OTA_UPGRADE_HEADER_STRING"

DEFAULT_MANUFACTURER = 0x1001
DEFAULT_IMAGE_TYPE = 0x1011

DEFINE_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s+(.+?)(?:\s*/\*.*?\*/\s*)?(?://.*)?$")
STRING_DEFINE_RE = re.compile(r'^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s+"([^"]*)"')
ADD_OTA_CLIENT_RE = re.compile(r"\baddOTAClient\s*\(")


def log(msg: str) -> None:
    print(f"zb_ota_image: {msg}")


def parse_int_literal(token: str) -> Optional[int]:
    """Accept only simple C integer literals (decimal / 0x hex / 0b binary)."""
    value = token.strip()
    if not value:
        return None
    # Strip trailing u/U/l/L suffixes common in sketches.
    while value and value[-1] in "uUlL":
        value = value[:-1]
    if not value:
        return None
    try:
        return int(value, 0)
    except ValueError:
        return None


def collect_sketch_sources(sketch_dir: str) -> List[str]:
    sources: List[str] = []
    for name in sorted(os.listdir(sketch_dir)):
        lower = name.lower()
        if lower.endswith((".ino", ".h", ".hpp", ".hh", ".cpp", ".c", ".cxx")):
            sources.append(os.path.join(sketch_dir, name))
    return sources


def scan_sketch(sketch_dir: str) -> Tuple[Dict[str, int], Dict[str, str], bool]:
    """Return (int_macros, string_macros, has_add_ota_client)."""
    ints: Dict[str, int] = {}
    strings: Dict[str, str] = {}
    has_add_ota = False

    for path in collect_sketch_sources(sketch_dir):
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                text = fh.read()
        except OSError as err:
            # logging.warning satisfies CodeQL insufficient-logging; log() is for the IDE console.
            logger.warning("cannot read %s: %s", path, err)
            log(f"warning: cannot read {path}: {err}")
            continue

        if ADD_OTA_CLIENT_RE.search(text):
            has_add_ota = True

        for line in text.splitlines():
            str_match = STRING_DEFINE_RE.match(line)
            if str_match:
                strings[str_match.group(1)] = str_match.group(2)
                continue
            match = DEFINE_RE.match(line)
            if not match:
                continue
            name, raw = match.group(1), match.group(2).strip()
            # Ignore function-like macros.
            if "(" in name:
                continue
            # Whole RHS (comments already stripped by DEFINE_RE) must be one literal.
            # Reject expressions (e.g. "0x01010100 + 1") — do not take only the first token.
            if not raw or len(raw.split()) != 1:
                continue
            rhs = raw.split()[0]
            if any(op in rhs for op in ("+", "*", "/", "(", ")")):
                continue
            # Allow a leading minus for signed literals only.
            if "-" in rhs[1:]:
                continue
            number = parse_int_literal(rhs)
            if number is not None:
                ints[name] = number
    return ints, strings, has_add_ota


def resolve_ota_params(ints: Dict[str, int], strings: Dict[str, str], has_add_ota: bool) -> Optional[Dict[str, object]]:
    has_version = MACRO_FILE_VERSION in ints or MACRO_RUNNING_VERSION in ints
    if not has_add_ota and not has_version:
        log("skip: no addOTAClient() and no OTA version #define in sketch")
        return None

    manuf = ints.get(MACRO_MANUFACTURER, DEFAULT_MANUFACTURER if has_add_ota or has_version else None)
    image_type = ints.get(MACRO_IMAGE_TYPE, DEFAULT_IMAGE_TYPE if has_add_ota or has_version else None)
    file_version = ints.get(MACRO_FILE_VERSION, ints.get(MACRO_RUNNING_VERSION))

    missing = []
    if manuf is None:
        missing.append(MACRO_MANUFACTURER)
    if image_type is None:
        missing.append(MACRO_IMAGE_TYPE)
    if file_version is None:
        missing.append(f"{MACRO_FILE_VERSION} or {MACRO_RUNNING_VERSION}")
    if missing:
        log("skip: missing OTA #define(s): " + ", ".join(missing))
        return None

    params: Dict[str, object] = {
        "manuf_id": manuf,
        "image_type": image_type,
        "file_version": file_version,
        "header_string": strings.get(MACRO_HEADER_STRING, ""),
    }
    # Only embed HW filters when both min/max are set. Do not use OTA_UPGRADE_HW_VERSION
    # here: zigpy rejects images with HW bounds when the coordinator query omits HW version.
    if MACRO_MIN_HW in ints and MACRO_MAX_HW in ints:
        params["min_hw_ver"] = ints[MACRO_MIN_HW]
        params["max_hw_ver"] = ints[MACRO_MAX_HW]
    return params


def generate_ota_file(firmware_path: str, **kwargs) -> bytes:
    file_id = int(kwargs.get("file_id") or 0x0BEEF11E)
    header_version = int(kwargs.get("header_version") or 0x0100)
    manuf_id = int(kwargs["manuf_id"])
    image_type = int(kwargs["image_type"])
    file_version = int(kwargs["file_version"])
    stack_version = int(kwargs.get("stack_version") or 0x0002)
    header_string = str(kwargs.get("header_string") or "")
    min_hw_ver = kwargs.get("min_hw_ver")
    max_hw_ver = kwargs.get("max_hw_ver")

    with open(firmware_path, "rb") as fh:
        firmware = fh.read()
    if not firmware:
        raise ValueError(f"firmware binary is empty: {firmware_path}")

    # Sub-element: Upgrade Image (tag 0x0000)
    tag_block = struct.pack("<HI", 0x0000, len(firmware)) + firmware

    header_str = header_string.encode("ascii")
    if len(header_str) > 32:
        raise ValueError("OTA_UPGRADE_HEADER_STRING requires max 32 bytes")
    header_str = header_str.ljust(32, b"\x00")

    field_ctrl = 0
    opt = b""
    if min_hw_ver is not None and max_hw_ver is not None:
        field_ctrl |= 0x0004
        opt += struct.pack("<HH", int(min_hw_ver), int(max_hw_ver))

    header_len = 4 + 2 + 2 + 2 + 2 + 2 + 4 + 2 + 32 + 4 + len(opt)
    total_len = header_len + len(tag_block)
    header = struct.pack(
        "<I H H H H H I H 32s I",
        file_id,
        header_version,
        header_len,
        field_ctrl,
        manuf_id,
        image_type,
        file_version,
        stack_version,
        header_str,
        total_len,
    )
    return header + opt + tag_block


def export_dir(sketch_path: str, fqbn: str) -> str:
    parts = fqbn.split(":")
    board_dir = ".".join(parts[:3]) if len(parts) >= 3 else fqbn.replace(":", ".")
    path = os.path.join(sketch_path, "build", board_dir)
    os.makedirs(path, exist_ok=True)
    return path


def write_outputs(data: bytes, out_name: str, destinations: Iterable[str]) -> None:
    for directory in destinations:
        if not directory:
            continue
        os.makedirs(directory, exist_ok=True)
        path = os.path.join(directory, out_name)
        with open(path, "wb") as fh:
            fh.write(data)
        log(f"wrote {path}")


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Create Zigbee OTA image from Arduino sketch export")
    parser.add_argument("--zigbee-mode", default="", help="Value of build.zigbee_mode (empty = Disabled)")
    parser.add_argument("--sketch", required=True, help="Sketch folder path")
    parser.add_argument("--bin", required=True, help="Compiled application .bin path")
    parser.add_argument("--fqbn", default="", help="Board FQBN for export folder naming")
    parser.add_argument("--build-path", default="", help="Arduino build path (also write .zigbee here)")
    args = parser.parse_args(argv)

    zigbee_mode = (args.zigbee_mode or "").strip()
    if not zigbee_mode:
        log("skip: Zigbee Mode is Disabled")
        return 0

    sketch = os.path.abspath(args.sketch)
    firmware = os.path.abspath(args.bin)
    if not os.path.isdir(sketch):
        log(f"skip: sketch path not found: {sketch}")
        return 0
    if not os.path.isfile(firmware):
        log(f"skip: firmware binary not found: {firmware}")
        return 0

    ints, strings, has_add_ota = scan_sketch(sketch)
    params = resolve_ota_params(ints, strings, has_add_ota)
    if params is None:
        return 0

    try:
        data = generate_ota_file(firmware, **params)
    except Exception as err:  # noqa: BLE001 - surface as soft skip with message
        log(f"skip: failed to build OTA image: {err}")
        return 0

    manuf = int(params["manuf_id"])
    image_type = int(params["image_type"])
    file_version = int(params["file_version"])
    out_name = f"{manuf:04X}-{image_type:04X}-{file_version:08X}-ota-file.zigbee"

    destinations = []
    if args.build_path:
        destinations.append(os.path.abspath(args.build_path))
    if args.fqbn:
        destinations.append(export_dir(sketch, args.fqbn))
    if not destinations:
        destinations.append(sketch)

    write_outputs(data, out_name, destinations)
    log(f"OTA image OK (manuf=0x{manuf:04X} type=0x{image_type:04X} ver=0x{file_version:08X}, " f"{len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
