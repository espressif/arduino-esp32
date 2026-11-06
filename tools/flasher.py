#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""esptool wrapper that enables fast reflashing via --diff-with."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

# Hex-address pattern used by Arduino platform.txt to specify flash offsets.
_HEX_ADDR_RE = re.compile(r"^0x[0-9a-fA-F]+$")


def _parse_flash_binaries(args: list[str]) -> list[Path] | None:
    """Return binary file paths from a write-flash command, in address order.

    Scans for hex-offset file-path pairs that appear after the write-flash
    verb, following the same token pattern used by esptool and ESP-IDF's
    run_serial_tool.cmake.  Returns None when write-flash is not present.
    """
    try:
        wf_idx = args.index("write-flash")
    except ValueError:
        return None  # Not a write-flash command; nothing to parse.

    binaries: list[Path] = []
    tokens = args[wf_idx + 1 :]
    i = 0
    while i < len(tokens):
        if _HEX_ADDR_RE.match(tokens[i]) and i + 1 < len(tokens):
            binaries.append(Path(tokens[i + 1]))
            i += 2
        else:
            i += 1
    return binaries


def _flashed_path(binary: Path, build_dir: Path | None) -> Path:
    """Return the *_flashed.bin companion path for a binary.

    All reference files are placed in build_dir so that platform-level
    binaries (e.g. boot_app0.bin from the platform tools tree) do not
    create files outside the per-sketch build directory.

    Example with build_dir=/tmp/build:
        sketch.bin            -> /tmp/build/sketch_flashed.bin
        sketch.bootloader.bin -> /tmp/build/sketch.bootloader_flashed.bin
        boot_app0.bin         -> /tmp/build/boot_app0_flashed.bin
    """
    dest_dir = build_dir if build_dir is not None else binary.parent
    return dest_dir / (binary.stem + "_flashed" + binary.suffix)


_ALWAYS_FLASH = {"boot_app0.bin"}
"""Binaries that are always fully flashed regardless of reference availability.

boot_app0.bin selects the active OTA partition slot.  If the user has
performed an OTA update since the last flash, the on-chip copy will differ
from the reference even though our local copy has not changed.  Passing
'skip' forces esptool to write it unconditionally, avoiding any dependency
on the MD5 fallback path.
"""


def _build_diff_with(binaries: list[Path], build_dir: Path | None) -> tuple[list[str], bool]:
    """Build the --diff-with argument list from available _flashed.bin files.

    Returns (diff_list, have_any) where diff_list contains absolute paths or
    the literal 'skip' for files without a reference, and have_any is True
    when at least one reference file was found (excluding always-flash files).

    Binaries whose name appears in ``_ALWAYS_FLASH`` always receive 'skip' so
    that they are unconditionally written, regardless of whether a reference
    exists.
    """
    diff_list: list[str] = []
    have_any = False
    for binary in binaries:
        if binary.name in _ALWAYS_FLASH:
            diff_list.append("skip")
            continue
        ref = _flashed_path(binary, build_dir)
        if ref.exists():
            diff_list.append(str(ref))
            have_any = True
        else:
            diff_list.append("skip")
    return diff_list, have_any


def _save_flashed_copies(binaries: list[Path], build_dir: Path | None) -> None:
    """Copy each flashed binary to its _flashed.bin companion for next upload."""
    for binary in binaries:
        try:
            if binary.exists():
                shutil.copy2(binary, _flashed_path(binary, build_dir))
        except OSError as e:
            print(
                f"flasher: warning: could not save reference file for {binary.name}"
                f" ({e}); next upload will perform a full flash for this binary.",
                file=sys.stderr,
            )


def _parse_flasher_args() -> tuple[str, Path | None, bool, list[str]]:
    """Parse flasher's own options, returning (esptool, build_dir, no_fast_flash, esptool_args).

    parse_known_args() is used so that unrecognized tokens (all esptool flags
    and arguments) are collected into esptool_args without triggering errors.
    """
    parser = argparse.ArgumentParser(
        prog="flasher",
        description="esptool wrapper that enables fast reflashing via --diff-with.",
        epilog=(
            "All unrecognized arguments are forwarded to esptool unchanged.\n"
            "See https://docs.espressif.com/projects/esptool for esptool usage."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--esptool",
        default="esptool",
        metavar="<binary>",
        help="path to the esptool binary (default: 'esptool' from PATH)",
    )
    parser.add_argument(
        "--build-dir",
        metavar="<path>",
        help=(
            "directory where *_flashed.bin reference files are stored; "
            "all references land here regardless of where the original binary lives, "
            "so platform files such as boot_app0.bin do not pollute the platform "
            "installation tree (default: each binary's own parent directory)"
        ),
    )
    parser.add_argument(
        "--no-fast-flash",
        action="store_true",
        default=False,
        help=(
            "disable fast reflashing; esptool is invoked directly without any "
            "--diff-with argument and no reference files are saved or read "
            "(useful when the flash state is unknown, e.g. after OTA updates "
            "or erase-before-flash)"
        ),
    )

    args, esptool_args = parser.parse_known_args()
    build_dir = Path(args.build_dir) if args.build_dir else None
    return args.esptool, build_dir, args.no_fast_flash, esptool_args


def main() -> None:
    esptool, build_dir, no_fast_flash, esptool_args = _parse_flasher_args()
    binaries: list[Path] = []

    # Only attempt fast reflash when not explicitly disabled, for write-flash
    # commands that do not already carry a --diff-with argument.
    if not no_fast_flash and "--diff-with" not in esptool_args:
        try:
            detected = _parse_flash_binaries(esptool_args)
            if detected:
                diff_list, have_any = _build_diff_with(detected, build_dir)
                if have_any:
                    esptool_args = esptool_args + ["--diff-with"] + diff_list
                binaries = detected
        except Exception as e:
            print(
                f"flasher: warning: fast-reflash preparation failed ({e});" " falling back to a full flash.",
                file=sys.stderr,
            )

    result = subprocess.run([esptool] + esptool_args)

    if result.returncode == 0 and binaries:
        _save_flashed_copies(binaries, build_dir)

    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
