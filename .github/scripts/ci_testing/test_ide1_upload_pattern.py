#!/usr/bin/env python3
"""Simulate Arduino IDE 1.8.19 SerialUploader prefs + StringReplacer for upload.pattern."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PLATFORM_TXT = ROOT / "platform.txt"
BOARDS_TXT = ROOT / "boards.txt"
SOCS_CONFIG = ROOT / ".github" / "scripts" / "socs_config.sh"

_PLACEHOLDER_RE = re.compile(r"\{[^}]+\}")
_ARRAY_LINE_RE = re.compile(r'^"([^"]+)"\s*,?\s*$')


def load_bash_array(config_text: str, array_name: str) -> list[str]:
    """Parse a simple NAME=( \"a\" \"b\" ) array from socs_config.sh."""
    values: list[str] = []
    in_array = False
    for line in config_text.splitlines():
        stripped = line.strip()
        if stripped.startswith(f"{array_name}=("):
            in_array = True
            continue
        if in_array:
            if stripped == ")":
                break
            match = _ARRAY_LINE_RE.match(stripped)
            if match:
                values.append(match.group(1))
    if not values:
        raise SystemExit(f"could not parse {array_name} from {SOCS_CONFIG}")
    return values


def load_core_soc_boards() -> list[str]:
    """CORE_SOCS from socs_config.sh (ALL_SOCS minus SKIP_LIB_BUILD_SOCS)."""
    config_text = SOCS_CONFIG.read_text(encoding="utf-8")
    all_socs = load_bash_array(config_text, "ALL_SOCS")
    skip_socs = set(load_bash_array(config_text, "SKIP_LIB_BUILD_SOCS"))
    return [soc for soc in all_socs if soc not in skip_socs]


def load_properties(path: Path) -> dict[str, str]:
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


def board_upload_prefs(boards: dict[str, str], board_id: str) -> dict[str, str]:
    upload_prefix = f"{board_id}.upload."
    build_prefix = f"{board_id}.build."
    out: dict[str, str] = {}
    for key, value in boards.items():
        if key.startswith(upload_prefix):
            out["upload." + key[len(upload_prefix) :]] = value
        elif key.startswith(build_prefix):
            out["build." + key[len(build_prefix) :]] = value
    return out


def merge_menu_prefs(
    boards: dict[str, str],
    board_id: str,
    menu_id: str,
    option_id: str,
    prefs: dict[str, str],
) -> None:
    prefix = f"{board_id}.menu.{menu_id}.{option_id}."
    for key, value in boards.items():
        if key.startswith(prefix):
            prefs[key[len(f"{board_id}.menu.{menu_id}.{option_id}.") :]] = value


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
    board_prefs: dict[str, str] | None = None,
) -> str:
    props = collapse_os(props, os_name)
    prefs: dict[str, str] = {
        "runtime.platform.path": platform_path,
        "build.path": build_path,
        "build.project_name": "Sk",
        "serial.port": serial_port,
        "path": f"{platform_path}/tools/esptool",
        "cmd": "esptool",
        "upload.speed": "921600",
        "upload.flags": "",
        "upload.extra_flags": "",
        "build.mcu": "esp32",
        "build.flash_mode": "dio",
        "build.flash_freq": "80m",
        "build.flash_size": "4MB",
        "build.flash_offset": "0x10000",
    }
    if board_prefs:
        prefs.update(board_prefs)
    prefs.update(get_tool_prefs(props, tool))
    pattern = prefs.get("upload.pattern", "")
    if not pattern:
        raise SystemExit(f"missing upload.pattern for tool {tool!r}")
    return replace_from_mapping(pattern, prefs)


def flash_pairs(cmd: str) -> list[tuple[str, str]]:
    import shlex

    parts = shlex.split(cmd)
    wf = parts.index("write-flash")
    tokens = parts[wf + 1 :]
    i = 0
    while i < len(tokens) and not tokens[i].startswith("0x"):
        i += 1
    pairs: list[tuple[str, str]] = []
    while i < len(tokens):
        if tokens[i].startswith("0x") and i + 1 < len(tokens) and not tokens[i + 1].startswith("0x"):
            pairs.append((tokens[i], tokens[i + 1]))
            i += 2
        else:
            i += 1
    return pairs


def unexpanded_placeholders(cmd: str) -> list[str]:
    return _PLACEHOLDER_RE.findall(cmd)


def main() -> int:
    platform = load_properties(PLATFORM_TXT)
    boards = load_properties(BOARDS_TXT)
    core_soc_boards = load_core_soc_boards()

    for board_id in core_soc_boards:
        board_prefs = board_upload_prefs(boards, board_id)
        if board_prefs.get("upload.tool") not in (None, "esptool_py"):
            continue
        if "build.bootloader_addr" not in board_prefs:
            print(f"FAIL ({board_id}): missing build.bootloader_addr", file=sys.stderr)
            return 1
        if "upload.erase_cmd" not in board_prefs:
            print(
                f"FAIL ({board_id}): missing board-level upload.erase_cmd= "
                "(required for IDE 1.x SerialUploader prefs)",
                file=sys.stderr,
            )
            return 1

        for tool in ("esptool_py",):
            for os_name in ("linux", "windows"):
                cmd = simulate_upload_pattern(
                    platform,
                    tool=tool,
                    os_name=os_name,
                    board_prefs=board_prefs,
                )
                leftovers = unexpanded_placeholders(cmd)
                if leftovers:
                    print(
                        f"FAIL ({board_id}/{tool}/{os_name}): unexpanded placeholders {leftovers}:\n  {cmd}",
                        file=sys.stderr,
                    )
                    return 1
                if "flasher" not in cmd:
                    print(
                        f"FAIL ({board_id}/{tool}/{os_name}): flasher not in command:\n  {cmd}",
                        file=sys.stderr,
                    )
                    return 1
                pairs = flash_pairs(cmd)
                if len(pairs) != 4:
                    print(
                        f"FAIL ({board_id}/{tool}/{os_name}): expected 4 flash pairs, got {len(pairs)}:\n  {pairs}",
                        file=sys.stderr,
                    )
                    return 1
                print(f"OK ({board_id}/{os_name}): {cmd[:100]}...")

    erase_board = next(
        (
            board_id
            for board_id in core_soc_boards
            if f"{board_id}.menu.EraseFlash.all.upload.erase_cmd" in boards
        ),
        None,
    )
    if erase_board is None:
        print("FAIL: no CORE_SOC board defines EraseFlash.all.upload.erase_cmd", file=sys.stderr)
        return 1

    erase_all = board_upload_prefs(boards, erase_board)
    merge_menu_prefs(boards, erase_board, "EraseFlash", "all", erase_all)
    erase_cmd = simulate_upload_pattern(platform, board_prefs=erase_all)
    if " write-flash -e -z " not in f" {erase_cmd} ":
        print(f"FAIL: EraseFlash=all should pass -e after write-flash:\n  {erase_cmd}", file=sys.stderr)
        return 1

    for tool in ("esptool_py_app_only",):
        app_only_runtime = {"runtime.tools.esptool_py.path": "/plat/tools/esptool"}
        for os_name in ("linux", "windows"):
            cmd = simulate_upload_pattern(
                platform,
                tool=tool,
                os_name=os_name,
                board_prefs=app_only_runtime,
            )
            leftovers = unexpanded_placeholders(cmd)
            if leftovers:
                print(
                    f"FAIL ({tool}/{os_name}): unexpanded placeholders {leftovers}:\n  {cmd}",
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
