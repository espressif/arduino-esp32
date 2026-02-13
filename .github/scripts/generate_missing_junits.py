#!/usr/bin/env python3

import json
import logging
import os
import re
import sys
from pathlib import Path
from xml.etree.ElementTree import Element, SubElement, ElementTree
import yaml

# Configure logging
logging.basicConfig(level=logging.DEBUG, format='[%(levelname)s] %(message)s', stream=sys.stderr)


def parse_array(value) -> list[str]:
    if isinstance(value, list):
        return [str(x) for x in value]
    if not isinstance(value, str):
        return []
    txt = value.strip()
    if not txt:
        return []
    # Try JSON
    try:
        return [str(x) for x in json.loads(txt)]
    except Exception as e:
        logging.debug(f"Failed to parse value as JSON: {e}")
    # Normalize single quotes then JSON
    try:
        fixed = txt.replace("'", '"')
        return [str(x) for x in json.loads(fixed)]
    except Exception as e:
        logging.debug(f"Failed to parse value as JSON with quote normalization: {e}")
    # Fallback: CSV
    logging.debug(f"Falling back to CSV parsing for value: {txt}")
    return [p.strip() for p in txt.strip("[]").split(",") if p.strip()]


def _parse_ci_yml(content: str) -> dict:
    if not content:
        return {}
    try:
        data = yaml.safe_load(content) or {}
        if not isinstance(data, dict):
            logging.warning("YAML content is not a dictionary, returning empty dict")
            return {}
        return data
    except Exception as e:
        logging.error(f"Failed to parse ci.yml content: {e}")
        return {}


def _fqbn_counts_from_yaml(ci: dict) -> dict[str, int]:
    counts: dict[str, int] = {}
    if not isinstance(ci, dict):
        return counts
    fqbn = ci.get("fqbn")
    if not isinstance(fqbn, dict):
        return counts
    for target, entries in fqbn.items():
        if isinstance(entries, list):
            counts[str(target)] = len(entries)
        elif entries is not None:
            # Single value provided as string
            counts[str(target)] = 1
    return counts


def _sdkconfig_meets(ci_cfg: dict, sdk_text: str) -> bool:
    if not sdk_text:
        return True
    for req in ci_cfg.get("requires", []):
        if not req or not isinstance(req, str):
            continue
        if not any(line.startswith(req) for line in sdk_text.splitlines()):
            return False
    req_any = ci_cfg.get("requires_any", [])
    if req_any:
        if not any(any(line.startswith(r.strip()) for line in sdk_text.splitlines()) for r in req_any if isinstance(r, str)):
            return False
    return True


def expected_from_artifacts(build_root: Path) -> dict[tuple[str, str, str, str], int]:
    """Compute expected runs using ci.yml and sdkconfig found in build artifacts.
    Returns mapping (platform, target, type, sketch) -> expected_count
    """
    expected: dict[tuple[str, str, str, str], int] = {}
    if not build_root.exists():
        return expected
    print(f"[DEBUG] Scanning build artifacts in: {build_root}", file=sys.stderr)
    for artifact_dir in build_root.iterdir():
        if not artifact_dir.is_dir():
            continue
        m = re.match(r"test-bin-([A-Za-z0-9_\-]+)-([A-Za-z0-9_\-]+)", artifact_dir.name)
        if not m:
            continue
        target = m.group(1)
        test_type = m.group(2)
        print(f"[DEBUG] Artifact group target={target} type={test_type} dir={artifact_dir}", file=sys.stderr)

        # Find all build*.tmp directories and determine test names.
        # Directory layout:
        #   Single-device: test-bin-<target>-<type>/<test>/build*.tmp/
        #   Multi-device:  test-bin-<target>-<type>/<test>/<device>/build*.tmp/
        # The test name is always the first path component after the artifact group.
        tests_processed = set()

        for build_tmp in artifact_dir.rglob("build*.tmp"):
            if not build_tmp.is_dir():
                continue
            if not re.search(r"build\d*\.tmp$", build_tmp.name):
                continue

            # Test name is always the first directory component after the artifact group
            rel = build_tmp.relative_to(artifact_dir)
            effective_sketch = rel.parts[0]

            # Skip if we already processed this test
            if effective_sketch in tests_processed:
                continue
            tests_processed.add(effective_sketch)

            print(f"[DEBUG]  Processing test={effective_sketch} from artifact {artifact_dir.name}", file=sys.stderr)

            ci_path = build_tmp / "ci.yml"
            sdk_path = build_tmp / "sdkconfig"

            # Read ci.yml if it exists, otherwise use empty (defaults)
            ci_text = ""
            if ci_path.exists():
                try:
                    ci_text = ci_path.read_text(encoding="utf-8")
                except Exception as e:
                    logging.warning(f"Failed to read ci.yml from {ci_path}: {e}")
            else:
                logging.debug(f"No ci.yml found at {ci_path}, using defaults")

            try:
                sdk_text = sdk_path.read_text(encoding="utf-8", errors="ignore") if sdk_path.exists() else ""
            except Exception as e:
                logging.warning(f"Failed to read sdkconfig from {sdk_path}: {e}")
                sdk_text = ""

            ci = _parse_ci_yml(ci_text)
            fqbn_counts = _fqbn_counts_from_yaml(ci)

            # Determine allowed platforms for this test
            # Performance tests are only run on hardware
            if test_type == "performance":
                allowed_platforms = ["hardware"]
            else:
                allowed_platforms = []
                platforms_cfg = ci.get("platforms") if isinstance(ci, dict) else None
                for plat in ("hardware", "wokwi", "qemu"):
                    dis = None
                    if isinstance(platforms_cfg, dict):
                        dis = platforms_cfg.get(plat)
                    if dis is False:
                        continue
                    allowed_platforms.append(plat)

            # Requirements check
            minimal = {
                "requires": ci.get("requires") or [],
                "requires_any": ci.get("requires_any") or [],
            }
            if not _sdkconfig_meets(minimal, sdk_text):
                print(f"[DEBUG]   Skip (requirements not met): target={target} type={test_type} sketch={effective_sketch}", file=sys.stderr)
                continue

            # Expected runs = number from fqbn_counts in ci.yml (how many FQBNs for this target)
            exp_runs = fqbn_counts.get(target, 0) or 1
            print(f"[DEBUG]   ci.yml specifies {exp_runs} FQBN(s) for target={target}", file=sys.stderr)

            for plat in allowed_platforms:
                expected[(plat, target, test_type, effective_sketch)] = exp_runs
                print(f"[DEBUG]   Expected: plat={plat} target={target} type={test_type} sketch={effective_sketch} runs={exp_runs}", file=sys.stderr)

        if len(tests_processed) == 0:
            print(f"[DEBUG]  No sketches found in this artifact group", file=sys.stderr)
    return expected


def scan_executed_xml(xml_root: Path, valid_types: set[str]) -> dict[tuple[str, str, str, str], int]:
    """Return executed counts per (platform, target, type, sketch).
    Type/sketch/target are inferred from .../<type>/<sketch>/<target>/<file>.xml
    """
    counts: dict[tuple[str, str, str, str], int] = {}
    if not xml_root.exists():
        print(f"[DEBUG] Results root not found: {xml_root}", file=sys.stderr)
        return counts
    print(f"[DEBUG] Scanning executed XMLs in: {xml_root}", file=sys.stderr)
    for xml_path in xml_root.rglob("*.xml"):
        if not xml_path.is_file():
            continue
        rel = str(xml_path)
        platform = "hardware"
        if "test-results-wokwi-" in rel:
            platform = "wokwi"
        elif "test-results-qemu-" in rel:
            platform = "qemu"
        # Expect .../<type>/<sketch>/<target>/*.xml
        parts = xml_path.parts
        t_idx = -1
        for i, p in enumerate(parts):
            if p in valid_types:
                t_idx = i
        if t_idx == -1 or t_idx + 3 >= len(parts):
            continue
        test_type = parts[t_idx]
        sketch = parts[t_idx + 1]
        target = parts[t_idx + 2]
        key = (platform, target, test_type, sketch)
        old_count = counts.get(key, 0)
        counts[key] = old_count + 1
        print(f"[DEBUG]  Executed XML #{old_count + 1}: plat={platform} target={target} type={test_type} sketch={sketch} file={xml_path.name}", file=sys.stderr)
    print(f"[DEBUG] Executed entries discovered: {len(counts)}", file=sys.stderr)
    return counts


def write_missing_xml(out_root: Path, platform: str, target: str, test_type: str, sketch: str, missing_count: int):
    out_tests_dir = out_root / f"test-results-{platform}" / "tests" / test_type / sketch / target
    out_tests_dir.mkdir(parents=True, exist_ok=True)
    # Create one XML per missing index
    for idx in range(missing_count):
        suite_name = f"{test_type}_{platform}_{target}_{sketch}"
        root = Element("testsuite", name=suite_name, tests="1", failures="0", errors="1")
        case = SubElement(root, "testcase", classname=f"{test_type}.{sketch}", name="missing-run")
        error = SubElement(case, "error", message="Expected test run missing")
        error.text = "This placeholder indicates an expected test run did not execute."
        tree = ElementTree(root)
        out_file = out_tests_dir / f"{sketch}_missing_{idx}.xml"
        tree.write(out_file, encoding="utf-8", xml_declaration=True)


def main():
    # Args: <build_artifacts_dir> <test_results_dir> <output_junit_dir>
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <build_artifacts_dir> <test_results_dir> <output_junit_dir>", file=sys.stderr)
        return 2

    build_root = Path(sys.argv[1]).resolve()
    results_root = Path(sys.argv[2]).resolve()
    out_root = Path(sys.argv[3]).resolve()

    # Validate inputs
    if not build_root.is_dir():
        print(f"ERROR: Build artifacts directory not found: {build_root}", file=sys.stderr)
        return 2
    if not results_root.is_dir():
        print(f"ERROR: Test results directory not found: {results_root}", file=sys.stderr)
        return 2
    # Ensure output directory exists
    try:
        out_root.mkdir(parents=True, exist_ok=True)
    except Exception as e:
        print(f"ERROR: Failed to create output directory {out_root}: {e}", file=sys.stderr)
        return 2

    # Read matrices from environment variables injected by workflow
    hw_enabled = (os.environ.get("HW_TESTS_ENABLED", "false").lower() == "true")
    wokwi_enabled = (os.environ.get("WOKWI_TESTS_ENABLED", "false").lower() == "true")
    qemu_enabled = (os.environ.get("QEMU_TESTS_ENABLED", "false").lower() == "true")

    hw_targets = parse_array(os.environ.get("HW_TARGETS", "[]"))
    wokwi_targets = parse_array(os.environ.get("WOKWI_TARGETS", "[]"))
    qemu_targets = parse_array(os.environ.get("QEMU_TARGETS", "[]"))

    hw_types = parse_array(os.environ.get("HW_TYPES", "[]"))
    wokwi_types = parse_array(os.environ.get("WOKWI_TYPES", "[]"))
    qemu_types = parse_array(os.environ.get("QEMU_TYPES", "[]"))

    expected = expected_from_artifacts(build_root)  # (platform, target, type, sketch) -> expected_count
    executed_types = set(hw_types + wokwi_types + qemu_types)
    executed = scan_executed_xml(results_root, executed_types)      # (platform, target, type, sketch) -> count
    print(f"[DEBUG] Expected entries computed: {len(expected)}", file=sys.stderr)

    # Filter expected by enabled platforms and target/type matrices
    enabled_plats = set()
    if hw_enabled:
        enabled_plats.add("hardware")
    if wokwi_enabled:
        enabled_plats.add("wokwi")
    if qemu_enabled:
        enabled_plats.add("qemu")

    # Build platform-specific target and type sets
    plat_targets = {
        "hardware": set(hw_targets),
        "wokwi": set(wokwi_targets),
        "qemu": set(qemu_targets),
    }
    plat_types = {
        "hardware": set(hw_types),
        "wokwi": set(wokwi_types),
        "qemu": set(qemu_types),
    }

    missing_total = 0
    extra_total = 0
    for (plat, target, test_type, sketch), exp_count in expected.items():
        if plat not in enabled_plats:
            continue
        # Check if target and type are valid for this specific platform
        if target not in plat_targets.get(plat, set()):
            continue
        if test_type not in plat_types.get(plat, set()):
            continue
        got = executed.get((plat, target, test_type, sketch), 0)
        if got < exp_count:
            print(f"[DEBUG] Missing: plat={plat} target={target} type={test_type} sketch={sketch} expected={exp_count} got={got}", file=sys.stderr)
            write_missing_xml(out_root, plat, target, test_type, sketch, exp_count - got)
            missing_total += (exp_count - got)
        elif got > exp_count:
            print(f"[DEBUG] Extra runs: plat={plat} target={target} type={test_type} sketch={sketch} expected={exp_count} got={got}", file=sys.stderr)
            extra_total += (got - exp_count)

    # Check for executed tests that were not expected at all
    for (plat, target, test_type, sketch), got in executed.items():
        if (plat, target, test_type, sketch) not in expected:
            print(f"[DEBUG] Unexpected test: plat={plat} target={target} type={test_type} sketch={sketch} got={got} (not in expected)", file=sys.stderr)

    print(f"Generated {missing_total} placeholder JUnit files for missing runs.", file=sys.stderr)
    if extra_total > 0:
        print(f"WARNING: {extra_total} extra test runs detected (more than expected).", file=sys.stderr)


if __name__ == "__main__":
    sys.exit(main())


