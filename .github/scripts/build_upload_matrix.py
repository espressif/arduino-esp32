#!/usr/bin/env python3
"""Decide whether Upload Tests should run and build the sharded GitHub Actions matrix."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_SOCS_CONFIG = SCRIPT_DIR / "socs_config.sh"
_ARRAY_LINE_RE = re.compile(r'^"([^"]+)"\s*,?\s*$')


def _load_bash_array(config_text: str, array_name: str) -> list[str]:
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
        raise ValueError(f"Could not parse {array_name} from socs_config.sh")
    return values


def load_core_socs(core_socs_csv: str | None = None, socs_config: Path = DEFAULT_SOCS_CONFIG) -> list[str]:
    if core_socs_csv and core_socs_csv.strip():
        return [s.strip() for s in core_socs_csv.split(",") if s.strip()]
    config_text = socs_config.read_text(encoding="utf-8")
    all_socs = _load_bash_array(config_text, "ALL_SOCS")
    skip_socs = set(_load_bash_array(config_text, "SKIP_LIB_BUILD_SOCS"))
    return [soc for soc in all_socs if soc not in skip_socs]


def resolve_target_socs(
    *,
    event: str,
    upload_changed: bool,
    fqbns_raw: str,
    input_socs: str,
    core_socs: list[str],
) -> tuple[list[str], str]:
    if event == "workflow_dispatch" and input_socs.strip():
        target_socs = [s.strip() for s in input_socs.replace(" ", ",").split(",") if s.strip()]
        unknown = [s for s in target_socs if s not in core_socs]
        if unknown:
            raise ValueError(f"Unknown SoC(s) in inputs.socs: {unknown} (valid CORE_SOCS: {core_socs})")
        return target_socs, f"workflow_dispatch custom SoCs: {target_socs}"

    if event != "pull_request":
        return list(core_socs), f"Non-PR ({event}); testing all {len(core_socs)} CORE_SOCS"

    if upload_changed:
        return list(core_socs), f"Upload-related path changed; testing all {len(core_socs)} CORE_SOCS"

    if fqbns_raw.strip():
        parsed = json.loads(fqbns_raw)
        changed_socs = {item.split(":")[2] for item in parsed.get("fqbn", []) if item.count(":") >= 2}
        target_socs = [s for s in core_socs if s in changed_socs]
        return target_socs, f"boards.txt-only PR; testing changed CORE_SOCS: {target_socs}"

    return [], "No official CORE_SOCS boards or upload-related paths changed; skipping mock upload"


def split_shards(items: list[str], max_shards: int) -> list[list[str]]:
    n = min(len(items), max_shards)
    base, extra = divmod(len(items), n)
    out: list[list[str]] = []
    idx = 0
    # Put smaller chunks first so heavier early SoCs (esp32s31/esp32) run in 2-SoC shards.
    for i in range(n):
        size = base + (1 if i >= (n - extra) else 0)
        out.append(items[idx : idx + size])
        idx += size
    return out


def add_shards(include: list[dict[str, str]], target_socs: list[str], os_name: str, tool: str, max_shards: int) -> None:
    chunks = split_shards(target_socs, max_shards)
    total = len(chunks)
    for idx, chunk in enumerate(chunks, 1):
        suffix = f" [{idx}/{total}]" if total > 1 else ""
        include.append(
            {
                "os": os_name,
                "tool": tool,
                "socs": ",".join(chunk),
                "label": f"{os_name} ({tool}{suffix})",
            }
        )


def build_matrix(target_socs: list[str], input_tool: str = "all") -> dict[str, list[dict[str, str]]]:
    if not target_socs:
        return {"include": []}

    # Keep concurrent macOS runners <= 4 (under GitHub's 5-runner macOS cap) and use
    # macos-26-intel (required for Arduino IDE v1; also avoids macos-latest arm64 queue starvation).
    include: list[dict[str, str]] = []
    if input_tool == "cli":
        add_shards(include, target_socs, "ubuntu-latest", "cli", 1)
        add_shards(include, target_socs, "windows-latest", "cli", 2)
        add_shards(include, target_socs, "macos-26-intel", "cli", 2)
    elif input_tool == "builder":
        add_shards(include, target_socs, "ubuntu-latest", "builder", 2)
        add_shards(include, target_socs, "windows-latest", "builder", 3)
        add_shards(include, target_socs, "macos-26-intel", "builder", 4)
    elif input_tool == "all":
        add_shards(include, target_socs, "ubuntu-latest", "all", 2)
        add_shards(include, target_socs, "windows-latest", "all", 3)
        add_shards(include, target_socs, "macos-26-intel", "all", 4)
    else:
        raise ValueError(f"Unsupported input_tool: {input_tool!r}")

    return {"include": include}


def write_github_output(gh_output: Path | None, *, should_run: bool, matrix_json: str) -> None:
    if gh_output is None:
        return
    with gh_output.open("a", encoding="utf-8") as fh:
        fh.write(f"should_run={'true' if should_run else 'false'}\n")
        fh.write(f"matrix={matrix_json}\n")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build sharded matrix for upload-tests.yml")
    parser.add_argument("--event", default=os.environ.get("EVENT_NAME", ""), help="GitHub event name")
    parser.add_argument(
        "--upload-changed",
        action="store_true",
        default=os.environ.get("UPLOAD_CHANGED", "").strip().lower() == "true",
        help="Whether non-boards.txt upload paths changed",
    )
    parser.add_argument("--fqbns", default=os.environ.get("FQBNS", ""), help="JSON FQBNs from find_boards.sh")
    parser.add_argument("--tool", default=os.environ.get("INPUT_TOOL") or "all", help="Upload tool (all/cli/builder)")
    parser.add_argument("--socs", default=os.environ.get("INPUT_SOCS", ""), help="Custom SoC list (workflow_dispatch)")
    parser.add_argument(
        "--core-socs",
        default=os.environ.get("CORE_SOCS_CSV", ""),
        help="Optional comma-separated CORE_SOCS override",
    )
    parser.add_argument(
        "--github-output",
        default=os.environ.get("GITHUB_OUTPUT", ""),
        help="Path to GITHUB_OUTPUT file",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        core_socs = load_core_socs(args.core_socs)
        target_socs, reason = resolve_target_socs(
            event=args.event,
            upload_changed=args.upload_changed,
            fqbns_raw=args.fqbns,
            input_socs=args.socs,
            core_socs=core_socs,
        )
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print(reason)
    matrix = build_matrix(target_socs, input_tool=args.tool.strip() or "all")
    matrix_json = json.dumps(matrix, separators=(",", ":"))
    should_run = bool(target_socs)
    if should_run:
        print(f"Generated {len(matrix['include'])} matrix jobs: {matrix_json}")

    gh_output = Path(args.github_output) if args.github_output else None
    write_github_output(gh_output, should_run=should_run, matrix_json=matrix_json)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
