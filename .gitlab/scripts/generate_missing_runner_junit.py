#!/usr/bin/env python3

import os
import sys
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from typing import Optional


def read_env_list(name: str) -> list[str]:
    raw = os.environ.get(name, "")
    return [item.strip() for item in raw.splitlines() if item.strip()]


def write_single_suite(out_path: Path, suite_name: str, testcase_name: str, error_message: str) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    suite = ET.Element(
        "testsuite",
        attrib={
            "name": suite_name,
            "tests": "1",
            "errors": "1",
            "failures": "0",
        },
    )
    tc = ET.SubElement(
        suite,
        "testcase",
        attrib={"classname": "hardware.missing_runner", "name": testcase_name},
    )
    err = ET.SubElement(
        tc,
        "error",
        attrib={"message": error_message},
    )
    err.text = (
        "The hardware test could not be scheduled because no runner with the "
        "required tag combination is online/available."
    )
    ET.ElementTree(suite).write(out_path, encoding="utf-8", xml_declaration=True)


def _leading_spaces_count(s: str) -> int:
    return len(s) - len(s.lstrip(" "))


def _manual_parse_fqbn_length(ci_text: str, chip: str) -> Optional[int]:
    lines = ci_text.splitlines()
    fqbn_idx = None
    fqbn_indent = None
    for idx, line in enumerate(lines):
        if line.strip().startswith("fqbn:"):
            fqbn_idx = idx
            fqbn_indent = _leading_spaces_count(line)
            break
    if fqbn_idx is None:
        return None
    chip_idx = None
    chip_indent = None
    for j in range(fqbn_idx + 1, len(lines)):
        line = lines[j]
        if not line.strip():
            continue
        indent = _leading_spaces_count(line)
        if indent <= fqbn_indent:
            break
        stripped = line.strip()
        # Match '<chip>:' at this indentation level
        if stripped.startswith(f"{chip}:"):
            chip_idx = j
            chip_indent = indent
            break
    if chip_idx is None:
        return None
    count = 0
    for k in range(chip_idx + 1, len(lines)):
        line = lines[k]
        if not line.strip():
            continue
        indent = _leading_spaces_count(line)
        if indent <= chip_indent:
            break
        if line.strip().startswith("-"):
            count += 1
    return count if count > 0 else 1


def detect_fqbn_count(test_dir: Path, chip: str) -> int:
    """Return number of FQBN configs for this test and chip. Defaults to 1.

    Tries PyYAML if available; otherwise uses a simple indentation-based parser.
    """
    ci_path = test_dir / "ci.yml"
    if not ci_path.exists():
        return 1
    try:
        import yaml  # type: ignore

        data = yaml.safe_load(ci_path.read_text(encoding="utf-8")) or {}
        fqbn = data.get("fqbn", {})
        if isinstance(fqbn, dict):
            v = fqbn.get(chip)
            if isinstance(v, list):
                return len(v) if len(v) > 0 else 1
        return 1
    except Exception:
        # Fallback to manual parsing
        try:
            text = ci_path.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            return 1
        length = _manual_parse_fqbn_length(text, chip)
        return length if length is not None else 1


def main() -> int:
    groups_json = os.environ.get("MISSING_GROUPS_JSON")
    if groups_json:
        try:
            groups = json.loads(groups_json)
            if not isinstance(groups, list):
                groups = []
        except Exception:
            groups = []
        for g in groups:
            if not isinstance(g, dict):
                continue
            chip = str(g.get("chip", "unknown"))
            test_type = str(g.get("test_type", "unknown"))
            required_tags = " ".join(g.get("required_tags", []) or [])
            test_dirs = g.get("test_dirs", []) or []
            for test_dir in test_dirs:
                sketchdir = Path(test_dir)
                sketchname = sketchdir.name
                count = detect_fqbn_count(sketchdir, chip)
                if count <= 1:
                    out_path = sketchdir / chip / f"{sketchname}.xml"
                    suite_name = f"{test_type}_hardware_{chip}_{sketchname}"
                    msg = f"No available runner matches required tags: {required_tags} (chip={chip})"
                    write_single_suite(out_path, suite_name, sketchname, msg)
                    print(f"Wrote JUnit error report to {out_path}")
                else:
                    for i in range(count):
                        out_path = sketchdir / chip / f"{sketchname}{i}.xml"
                        suite_name = f"{test_type}_hardware_{chip}_{sketchname}{i}"
                        msg = f"No available runner matches required tags: {required_tags} (chip={chip})"
                        write_single_suite(out_path, suite_name, f"{sketchname}{i}", msg)
                        print(f"Wrote JUnit error report to {out_path}")
        return 0

    # Legacy single-group envs
    tests = read_env_list("TEST_LIST")
    chip = os.environ.get("TEST_CHIP", "unknown")
    test_type = os.environ.get("TEST_TYPE", "unknown")
    required_tags = os.environ.get("REQUIRED_TAGS", "").strip()

    if tests:
        for test_dir in tests:
            sketchdir = Path(test_dir)
            sketchname = sketchdir.name
            # Determine number of configs (FQBN list entries) for this chip
            count = detect_fqbn_count(sketchdir, chip)
            if count <= 1:
                out_path = sketchdir / chip / f"{sketchname}.xml"
                suite_name = f"{test_type}_hardware_{chip}_{sketchname}"
                msg = f"No available runner matches required tags: {required_tags} (chip={chip})"
                write_single_suite(out_path, suite_name, sketchname, msg)
                print(f"Wrote JUnit error report to {out_path}")
            else:
                for i in range(count):
                    out_path = sketchdir / chip / f"{sketchname}{i}.xml"
                    suite_name = f"{test_type}_hardware_{chip}_{sketchname}{i}"
                    msg = f"No available runner matches required tags: {required_tags} (chip={chip})"
                    write_single_suite(out_path, suite_name, f"{sketchname}{i}", msg)
                    print(f"Wrote JUnit error report to {out_path}")
    else:
        # Fallback: produce a generic suite so the pipeline reports an error
        out_dir = Path("tests") / test_type / chip
        out_path = out_dir / "missing_runner.xml"
        suite_name = f"{test_type}_hardware_{chip}_missing"
        msg = f"No available runner matches required tags: {required_tags} (chip={chip})"
        write_single_suite(out_path, suite_name, "missing_runner", msg)
        print(f"Wrote JUnit error report to {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())


