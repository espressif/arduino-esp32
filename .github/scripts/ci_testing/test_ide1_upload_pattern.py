#!/usr/bin/env python3
"""Simulate Arduino IDE 1.8.19 SerialUploader prefs + StringReplacer for upload.pattern."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PLATFORM_TXT = ROOT / "platform.txt"


def load_platform_txt(path: Path) -> dict[str, str]:
    props: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            continue
        key, _, value = line.partition("=")
        props[key.strip()] = value.strip()
    return props


def collapse_os(props: dict[str, str], os_name: str) -> dict[str, str]:
    """Strip .windows/.linux/.macosx suffixes like PreferencesMap.load()."""
    suffix = {
        "linux": ".linux",
        "windows": ".windows",
        "macosx": ".macosx",
    }.get(os_name)
    if not suffix:
        return dict(props)
    out: dict[str, str] = {}
    for key, value in props.items():
        if key.endswith(suffix):
            out[key[: -len(suffix)]] = value
        elif not any(key.endswith(s) for s in (".linux", ".windows", ".macosx")):
            out.setdefault(key, value)
    return out


def get_tool_prefs(props: dict[str, str], tool: str) -> dict[str, str]:
    prefix = f"tools.{tool}."
    out: dict[str, str] = {}
    for key, value in props.items():
        if key.startswith(prefix):
            out[key[len(prefix) :]] = value
    return out


def replace_from_mapping(src: str, mapping: dict[str, str], rounds: int = 10) -> str:
    for _ in range(rounds):
        prev = src
        for key, value in mapping.items():
            src = src.replace("{" + key + "}", value)
        if src == prev:
            break
    return src


def simulate_upload_pattern(
    props: dict[str, str],
    *,
    tool: str = "esptool_py",
    os_name: str = "linux",
    platform_path: str = "/plat",
    build_path: str = "/tmp/build",
    serial_port: str = "/dev/ttyUSB0",
) -> str:
    props = collapse_os(props, os_name)
    prefs: dict[str, str] = {
        "runtime.platform.path": platform_path,
        "build.path": build_path,
        "build.project_name": "Sk",
        "serial.port": serial_port,
        "upload.speed": "921600",
        "upload.flags": "",
        "upload.erase_cmd": "",
        "upload.extra_flags": "",
        "build.mcu": "esp32",
        "build.bootloader_addr": "0x1000",
    }
    prefs.update(get_tool_prefs(props, tool))
    pattern = prefs.get("upload.pattern", "")
    if not pattern:
        raise SystemExit(f"missing upload.pattern for tool {tool!r}")
    return replace_from_mapping(pattern, prefs)


def main() -> int:
    props = load_platform_txt(PLATFORM_TXT)
    for tool in ("esptool_py", "esptool_py_app_only"):
        for os_name in ("linux", "windows"):
            cmd = simulate_upload_pattern(props, tool=tool, os_name=os_name)
            if "{upload.flash_prefix}" in cmd:
                print(
                    f"FAIL ({tool}/{os_name}): unexpanded {{upload.flash_prefix}} in:\n  {cmd}",
                    file=sys.stderr,
                )
                return 1
            if "flasher" not in cmd:
                print(
                    f"FAIL ({tool}/{os_name}): flasher not in command:\n  {cmd}",
                    file=sys.stderr,
                )
                return 1
            print(f"OK ({tool}/{os_name}): {cmd[:120]}...")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
