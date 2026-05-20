#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
Parse test serial logs for gcov data dumped by esp32-hal-gcov.c,
reconstruct .gcda files alongside the .gcno files in the build tree,
run lcov to produce a tracefile, and emit a JSON coverage summary.

Usage:
    python coverage_collector.py \
        --log tests_output.log \
        --build-dir ~/.arduino/tests/esp32 \
        --gcov-tool xtensa-esp-elf-gcov \
        --output coverage.json
"""

import argparse
import base64
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
GCOV_FILE_RE = re.compile(r"<<<GCOV_FILE:(.+?)>>>")
B64_CHAR_RE = re.compile(r"[A-Za-z0-9+/=]")
LCOV_LINE_RE = re.compile(r"lines\.*:\s*([\d.]+)%\s*\((\d+)\s*of\s*(\d+)")


def strip_ansi(line):
    return ANSI_RE.sub("", line)


def extract_gcov_files(log_text):
    """Return dict {gcda_path: bytes} extracted from the serial dump markers.

    Each base64 line in the serial dump is independently encoded by
    ``base64_encode_chars`` on the ESP32, so lines must be decoded
    individually and the resulting bytes concatenated.
    """
    files = {}
    current_file = None
    decoded_parts = []

    for raw_line in log_text.splitlines():
        line = strip_ansi(raw_line).strip()

        m = GCOV_FILE_RE.search(line)
        if m:
            current_file = m.group(1)
            decoded_parts = []
            continue

        if "<<<GCOV_END>>>" in line and current_file is not None:
            if decoded_parts:
                chunk = b"".join(decoded_parts)
                if current_file in files:
                    files[current_file] += chunk
                else:
                    files[current_file] = chunk
            current_file = None
            decoded_parts = []
            continue

        if current_file is not None:
            clean = "".join(B64_CHAR_RE.findall(line))
            if clean:
                try:
                    decoded_parts.append(base64.b64decode(clean))
                except Exception:
                    pass  # skip noisy lines that aren't valid base64

    return files


def find_gcno(basename, build_dirs):
    """Walk *build_dirs* looking for *basename* with .gcno extension."""
    gcno = basename.replace(".gcda", ".gcno")
    for bd in build_dirs:
        for root, _dirs, fnames in os.walk(bd):
            if gcno in fnames:
                return os.path.join(root, gcno)
    return None


def _match_build_dir(gcda_path, build_dirs):
    """Return the build dir whose path appears in *gcda_path*, or ``None``.

    When the firmware records the .gcda path it embeds the compile-time
    build directory (e.g. ``build0.tmp/objects/sketch.cpp.gcda``).
    We resolve both paths so that symlinks and ``/private/var`` vs
    ``/var`` mismatches do not prevent a match.
    """
    try:
        gcda_resolved = os.path.realpath(gcda_path)
    except (OSError, ValueError):
        gcda_resolved = gcda_path
    for bd in build_dirs:
        try:
            bd_resolved = os.path.realpath(bd)
        except (OSError, ValueError):
            bd_resolved = bd
        if bd_resolved in gcda_resolved or bd in gcda_path:
            return bd
    return None


def place_gcda_files(gcov_files, build_dirs):
    """Write each .gcda next to its matching .gcno.  Return list of written paths.

    When multiple build directories contain a ``.gcno`` with the same
    basename (common for multi-FQBN builds), the original ``.gcda``
    path from the firmware log is used to pick the correct build dir.
    If that heuristic fails, the first build dir with an unoccupied
    slot is chosen so that data from different configs is not
    overwritten.
    """
    written = set()
    for gcda_path, data in gcov_files.items():
        basename = os.path.basename(gcda_path)

        preferred_bd = _match_build_dir(gcda_path, build_dirs)
        if preferred_bd is not None:
            gcno = find_gcno(basename, [preferred_bd])
        else:
            gcno = None

        if gcno is None:
            gcno = find_gcno(basename, build_dirs)

        if gcno is None:
            print(f"WARNING: no .gcno match for {gcda_path} (basename {basename})", file=sys.stderr)
            continue

        dest = gcno.replace(".gcno", ".gcda")

        if dest in written:
            for bd in build_dirs:
                alt = find_gcno(basename, [bd])
                if alt is not None:
                    alt_dest = alt.replace(".gcno", ".gcda")
                    if alt_dest not in written:
                        dest = alt_dest
                        break

        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "wb") as f:
            f.write(data)
        written.add(dest)
    return list(written)


def run_lcov(build_dirs, gcov_tool, tracefile):
    """Run lcov --capture over every build dir and merge into *tracefile*."""
    partial_files = []
    for idx, bd in enumerate(build_dirs):
        part = f"{tracefile}.part{idx}"
        cmd = [
            "lcov",
            "--capture",
            "--directory", str(bd),
            "--output-file", part,
            "--ignore-errors", "gcov,source,empty",
            "--rc", "no_exception_branch=1",
            "--rc", "geninfo_unexecuted_blocks=1",
        ]
        if gcov_tool:
            cmd += ["--gcov-tool", gcov_tool]
        print(f"$ {' '.join(cmd)}", file=sys.stderr)
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0 and os.path.exists(part):
            partial_files.append(part)
        else:
            print(f"WARNING: lcov capture failed for {bd}:\n{result.stderr}", file=sys.stderr)

    if not partial_files:
        return False

    if len(partial_files) == 1:
        os.rename(partial_files[0], tracefile)
    else:
        merge_cmd = ["lcov"]
        for pf in partial_files:
            merge_cmd += ["--add-tracefile", pf]
        merge_cmd += ["--output-file", tracefile, "--ignore-errors", "empty"]
        print(f"$ {' '.join(merge_cmd)}", file=sys.stderr)
        subprocess.run(merge_cmd, check=True)
        for pf in partial_files:
            os.remove(pf)

    return os.path.exists(tracefile)


def parse_lcov_summary(tracefile):
    """Run lcov --summary and return (lines_covered, lines_total)."""
    result = subprocess.run(
        ["lcov", "--summary", tracefile, "--ignore-errors", "empty"],
        capture_output=True, text=True,
    )
    for line in (result.stdout + result.stderr).splitlines():
        m = LCOV_LINE_RE.search(line)
        if m:
            return int(m.group(2)), int(m.group(3))
    return 0, 0


def _demangle_names(mangled, gcov_tool=None):
    """Demangle a list of C++ symbol names via ``c++filt``.

    Derives the ``c++filt`` path from *gcov_tool* (e.g.
    ``xtensa-esp-elf-gcov`` -> ``xtensa-esp-elf-c++filt``).
    Falls back to the system ``c++filt``, then to returning the
    original mangled names if neither is available.
    """
    if not mangled:
        return {}

    cxxfilt = None
    if gcov_tool:
        candidate = gcov_tool.replace("gcov", "c++filt")
        if os.path.isfile(candidate) or os.sep not in candidate:
            cxxfilt = candidate
    if cxxfilt is None:
        cxxfilt = "c++filt"

    try:
        result = subprocess.run(
            [cxxfilt], input="\n".join(mangled),
            capture_output=True, text=True, timeout=30,
        )
        if result.returncode == 0:
            demangled = result.stdout.strip().splitlines()
            if len(demangled) == len(mangled):
                return dict(zip(mangled, demangled))
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    return {n: n for n in mangled}


def parse_tracefile_functions(tracefile):
    """Parse an lcov tracefile and return per-file function coverage data.

    Returns ``{source_file: [{"name": str, "hits": int, "line": int}]}``.
    """
    files = {}
    current_sf = None
    fn_lines = {}

    for raw in Path(tracefile).read_text().splitlines():
        if raw.startswith("SF:"):
            current_sf = raw[3:]
            fn_lines = {}
            continue
        if raw.startswith("FNL:"):
            parts = raw[4:].split(",")
            if len(parts) >= 2:
                fn_lines[parts[0]] = int(parts[1])
            continue
        if raw.startswith("FNA:"):
            parts = raw[4:].split(",", 2)
            if len(parts) >= 3 and current_sf is not None:
                idx, hits, name = parts[0], int(parts[1]), parts[2]
                line = fn_lines.get(idx, 0)
                files.setdefault(current_sf, []).append(
                    {"name": name, "hits": hits, "line": line}
                )
            continue
        if raw == "end_of_record":
            current_sf = None
            fn_lines = {}

    return files


def generate_uncovered_report(tracefile, gcov_tool=None, output_path=None, core_root=None):
    """Write a markdown report of functions with zero hits.

    Returns ``(total_functions, untested_count)`` for summary purposes.
    """
    all_functions = parse_tracefile_functions(tracefile)
    if not all_functions:
        if output_path:
            Path(output_path).write_text("# Uncovered Functions Report\n\nNo function data found.\n")
        return 0, 0

    all_names = set()
    for funcs in all_functions.values():
        for f in funcs:
            all_names.add(f["name"])

    demangled = _demangle_names(sorted(all_names), gcov_tool)

    total = 0
    untested = 0
    per_file = {}

    for sf, funcs in all_functions.items():
        display_path = sf
        if core_root:
            try:
                display_path = os.path.relpath(sf, core_root)
            except ValueError:
                pass

        file_total = len(funcs)
        file_untested = [f for f in funcs if f["hits"] == 0]
        total += file_total
        untested += len(file_untested)

        if file_untested:
            file_untested.sort(key=lambda f: f["line"])
            per_file[display_path] = {
                "total": file_total,
                "untested": file_untested,
            }

    if output_path:
        pct = (untested / total * 100.0) if total > 0 else 0.0
        lines = [
            "# Uncovered Functions Report\n",
            f"**{untested} of {total} functions untested ({pct:.1f}%)**\n",
        ]

        for sf in sorted(per_file, key=lambda s: s.lower()):
            info = per_file[sf]
            n_untested = len(info["untested"])
            lines.append(f"\n## {sf} ({n_untested}/{info['total']} untested)\n")
            lines.append("| Line | Function |")
            lines.append("|------|----------|")
            for f in info["untested"]:
                name = demangled.get(f["name"], f["name"])
                lines.append(f"| {f['line']} | {name} |")
            lines.append("")

        Path(output_path).write_text("\n".join(lines) + "\n")
        print(f"Uncovered functions report written to {output_path}")

    return total, untested


def main():
    parser = argparse.ArgumentParser(description="Collect gcov coverage from serial logs")
    parser.add_argument("--log", required=True, action="append", help="Log file or directory to scan (repeatable)")
    parser.add_argument("--build-dir", required=True, action="append", help="Build directory with .gcno files (repeatable)")
    parser.add_argument("--gcov-tool", default=None, help="Path to cross-compiler gcov")
    parser.add_argument("--output", default="coverage.json", help="Output JSON summary")
    parser.add_argument("--tracefile", default="coverage.info", help="Output lcov tracefile")
    parser.add_argument("--uncovered-report", default=None, help="Path for markdown uncovered-functions report")
    parser.add_argument("--core-root", default=None, help="Root path for making source paths relative in the report")
    args = parser.parse_args()

    # Collect log text from all sources
    log_text = ""
    for src in args.log:
        p = Path(src)
        if p.is_file():
            log_text += p.read_text(errors="replace") + "\n"
        elif p.is_dir():
            for lf in sorted(p.rglob("*.log")):
                log_text += lf.read_text(errors="replace") + "\n"
        else:
            print(f"WARNING: {src} not found, skipping", file=sys.stderr)

    # Extract gcov data
    gcov_files = extract_gcov_files(log_text)
    if not gcov_files:
        print("No gcov data found in logs.")
        summary = {"lines_total": 0, "lines_covered": 0, "coverage_pct": 0.0}
        Path(args.output).write_text(json.dumps(summary, indent=2))
        return

    print(f"Extracted {len(gcov_files)} .gcda files from logs.")

    # Place .gcda next to .gcno
    written = place_gcda_files(gcov_files, args.build_dir)
    print(f"Placed {len(written)} .gcda files in build tree.")

    if not written:
        print("ERROR: Could not place any .gcda files (no matching .gcno found).", file=sys.stderr)
        summary = {"lines_total": 0, "lines_covered": 0, "coverage_pct": 0.0}
        Path(args.output).write_text(json.dumps(summary, indent=2))
        sys.exit(1)

    # Run lcov
    if run_lcov(args.build_dir, args.gcov_tool, args.tracefile):
        covered, total = parse_lcov_summary(args.tracefile)
        pct = (covered / total * 100.0) if total > 0 else 0.0
        print(f"Coverage: {covered}/{total} lines ({pct:.1f}%)")
    else:
        print("WARNING: lcov failed; reporting 0% coverage.", file=sys.stderr)
        covered, total, pct = 0, 0, 0.0

    summary = {"lines_total": total, "lines_covered": covered, "coverage_pct": round(pct, 2)}
    Path(args.output).write_text(json.dumps(summary, indent=2))
    print(f"Summary written to {args.output}")

    if args.uncovered_report and os.path.exists(args.tracefile):
        generate_uncovered_report(
            args.tracefile, args.gcov_tool,
            args.uncovered_report, args.core_root,
        )


if __name__ == "__main__":
    main()
