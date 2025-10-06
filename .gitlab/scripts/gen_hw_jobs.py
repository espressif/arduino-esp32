#!/usr/bin/env python3

import argparse
import json
import yaml
import os
import sys
import copy
import traceback
from pathlib import Path

# Resolve repository root from this script location
SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent
TESTS_ROOT = REPO_ROOT / "tests"

# Ensure we run from repo root so relative paths work consistently
try:
    os.chdir(REPO_ROOT)
except Exception as e:
    sys.stderr.write(f"[WARN] Failed to chdir to repo root '{REPO_ROOT}': {e}\n")
    sys.stderr.write(traceback.format_exc() + "\n")


class PrettyDumper(yaml.SafeDumper):
    def increase_indent(self, flow=False, indentless=False):
        return super().increase_indent(flow, False)


def str_representer(dumper, data):
    style = "|" if "\n" in data else None
    return dumper.represent_scalar("tag:yaml.org,2002:str", data, style=style)


def read_json(p: Path):
    try:
        with p.open("r", encoding="utf-8") as f:
            return json.load(f)
    except Exception as e:
        sys.stderr.write(f"[WARN] Failed to parse JSON file '{p}': {e}\n")
        sys.stderr.write(traceback.format_exc() + "\n")
        return {}


def find_tests() -> list[Path]:
    tests = []
    if not TESTS_ROOT.exists():
        return tests
    for ci in TESTS_ROOT.rglob("ci.json"):
        if ci.is_file():
            tests.append(ci)
    return tests


def find_sketch_test_dirs(types_filter: list[str]) -> list[tuple[str, Path]]:
    """
    Return list of (test_type, test_dir) where test_dir contains a sketch named <dir>/<dir>.ino
    If types_filter provided, only include those types.
    """
    results: list[tuple[str, Path]] = []
    if not TESTS_ROOT.exists():
        return results
    for type_dir in TESTS_ROOT.iterdir():
        if not type_dir.is_dir():
            continue
        test_type = type_dir.name
        if types_filter and test_type not in types_filter:
            continue
        for candidate in type_dir.iterdir():
            if not candidate.is_dir():
                continue
            sketch = candidate.name
            ino = candidate / f"{sketch}.ino"
            if ino.exists():
                results.append((test_type, candidate))
    return results


def load_tags_for_test(ci_json: dict, chip: str) -> set[str]:
    tags = set()
    # Global tags
    for key in "tags":
        v = ci_json.get(key)
        if isinstance(v, list):
            for e in v:
                if isinstance(e, str) and e.strip():
                    tags.add(e.strip())
    # Per-SoC tags
    soc_tags = ci_json.get("soc_tags")
    if isinstance(soc_tags, dict):
        v = soc_tags.get(chip)
        if isinstance(v, list):
            for e in v:
                if isinstance(e, str) and e.strip():
                    tags.add(e.strip())
    return tags


def test_enabled_for_target(ci_json: dict, chip: str) -> bool:
    targets = ci_json.get("targets")
    if isinstance(targets, dict):
        v = targets.get(chip)
        if v is False:
            return False
    return True


def platform_allowed(ci_json: dict, platform: str = "hardware") -> bool:
    platforms = ci_json.get("platforms")
    if isinstance(platforms, dict):
        v = platforms.get(platform)
        if v is False:
            return False
    return True


def sketch_name_from_ci(ci_path: Path) -> str:
    # The sketch directory holds .ino named as the directory
    sketch_dir = ci_path.parent
    return sketch_dir.name


def sdkconfig_path_for(chip: str, sketch: str, ci_json: dict) -> Path:
    # Match logic from tests_run.sh: if multiple FQBN entries -> build0.tmp
    fqbn = ci_json.get("fqbn", {}) if isinstance(ci_json, dict) else {}
    length = 0
    if isinstance(fqbn, dict):
        v = fqbn.get(chip)
        if isinstance(v, list):
            length = len(v)
    if length <= 1:
        return Path.home() / f".arduino/tests/{chip}/{sketch}/build.tmp/sdkconfig"
    return Path.home() / f".arduino/tests/{chip}/{sketch}/build0.tmp/sdkconfig"


def sdk_meets_requirements(sdkconfig: Path, ci_json: dict) -> bool:
    # Mirror check_requirements in sketch_utils.sh
    if not sdkconfig.exists():
        # Build might have been skipped or failed; allow parent to skip scheduling
        return False
    try:
        requires = ci_json.get("requires") or []
        requires_any = ci_json.get("requires_any") or []
        content = sdkconfig.read_text(encoding="utf-8", errors="ignore")
        # AND requirements
        for req in requires:
            if not isinstance(req, str):
                continue
            if not any(line.startswith(req) for line in content.splitlines()):
                return False
        # OR requirements
        if requires_any:
            ok = any(
                any(line.startswith(req) for line in content.splitlines())
                for req in requires_any
                if isinstance(req, str)
            )
            if not ok:
                return False
        return True
    except Exception as e:
        sys.stderr.write(f"[WARN] Failed to evaluate requirements against '{sdkconfig}': {e}\n")
        sys.stderr.write(traceback.format_exc() + "\n")
        return False


def parse_list_arg(s: str) -> list[str]:
    if not s:
        return []
    txt = s.strip()
    if txt.startswith("[") and txt.endswith("]"):
        try:
            return [str(x).strip() for x in json.loads(txt)]
        except Exception as e:
            sys.stderr.write(f"[WARN] Failed to parse JSON list '{txt}': {e}. Retrying with quote normalization.\n")
            try:
                fixed = txt.replace("'", '"')
                return [str(x).strip() for x in json.loads(fixed)]
            except Exception as e2:
                sys.stderr.write(
                    f"[WARN] Failed to parse JSON list after normalization: {e2}. Falling back to CSV parsing.\n"
                )
    # Fallback: comma-separated
    return [part.strip() for part in txt.split(",") if part.strip()]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--chips", required=True, help="Comma-separated or JSON array list of SoCs")
    ap.add_argument(
        "--types",
        required=False,
        default="validation",
        help="Comma-separated or JSON array of test type directories under tests/",
    )
    ap.add_argument("--out", required=True, help="Output YAML path for child pipeline")
    ap.add_argument(
        "--dry-run", action="store_true", help="Print planned groups/jobs and skip sdkconfig requirement checks"
    )
    args = ap.parse_args()

    chips = parse_list_arg(args.chips)
    types = parse_list_arg(args.types)

    print(f"Inputs: chips={chips or '[]'}, types={types or '[]'}")
    print(f"Repo root: {REPO_ROOT}")
    print(f"Tests root: {TESTS_ROOT}")

    # Aggregate mapping: (chip, frozenset(tags or generic), test_type) -> list of test paths
    group_map: dict[tuple[str, frozenset[str], str], list[str]] = {}
    all_ci = find_tests()
    print(f"Discovered {len(all_ci)} ci.json files under tests/")

    matched_count = 0
    for test_type, test_path in find_sketch_test_dirs(types):
        ci_path = test_path / "ci.json"
        ci = read_json(ci_path) if ci_path.exists() else {}
        test_dir = str(test_path)
        sketch = test_path.name
        for chip in chips:
            tags = load_tags_for_test(ci, chip)
            if not test_enabled_for_target(ci, chip):
                continue
            # Skip tests that explicitly disable the hardware platform
            if not platform_allowed(ci, "hardware"):
                continue
            sdk = sdkconfig_path_for(chip, sketch, ci)
            if not args.dry_run and not sdk_meets_requirements(sdk, ci):
                continue
            key_tags = tags.copy()
            # SOC must always be one runner tag
            key_tags.add(chip)
            if len(key_tags) == 1:
                # Only SOC present, add generic
                key_tags.add("generic")
            key = (chip, frozenset(sorted(key_tags)), test_type)
            group_map.setdefault(key, []).append(test_dir)
            matched_count += 1

    print(f"Matched {matched_count} test entries into {len(group_map)} groups")

    # Load template job
    template_path = REPO_ROOT / ".gitlab/workflows/hw_test_template.yml"
    template = yaml.safe_load(template_path.read_text(encoding="utf-8"))
    if not isinstance(template, dict) or "hw-test-template" not in template:
        print("ERROR: hw_test_template.yml missing hw-test-template")
        sys.exit(2)
    base_job = template["hw-test-template"]

    # Build child pipeline YAML in deterministic order
    jobs_entries = []  # list of (sort_key, job_name, job_dict)
    for (chip, tagset, test_type), test_dirs in group_map.items():
        tag_list = sorted(tagset)
        # Build name suffix excluding the SOC itself to avoid duplication
        non_soc_tags = [t for t in tag_list if t != chip]
        tag_suffix = "-".join(non_soc_tags) if non_soc_tags else "generic"
        job_name = f"hw-{chip}-{test_type}-{tag_suffix}"[:255]

        # Clone base job and adjust (preserve key order using deepcopy)
        job = copy.deepcopy(base_job)
        # Ensure tags include SOC+extras
        job["tags"] = tag_list
        vars_block = job.get("variables", {})
        vars_block["TEST_CHIP"] = chip
        vars_block["TEST_TYPE"] = test_type
        # Provide list of test directories for this job
        vars_block["TEST_LIST"] = "\n".join(sorted(test_dirs))
        job["variables"] = vars_block

        sort_key = (chip, test_type, tag_suffix)
        jobs_entries.append((sort_key, job_name, job))

    # Order jobs by (chip, type, tag_suffix)
    jobs = {}
    for _, name, job in sorted(jobs_entries, key=lambda x: x[0]):
        jobs[name] = job

    if args.dry_run:
        print("Planned hardware test jobs:")
        for name, job in jobs.items():
            tags = job.get("tags", [])
            soc = job.get("variables", {}).get("TEST_CHIP")
            ttype = job.get("variables", {}).get("TEST_TYPE")
            tlist = job.get("variables", {}).get("TEST_LIST", "")
            tests = [p for p in tlist.split("\n") if p]
            print(f"- {name} tags={tags} soc={soc} type={ttype} tests={len(tests)}")
            for t in tests:
                print(f"    * {t}")

    # If no jobs matched, create a no-op job to avoid failing trigger
    if not jobs:
        jobs["no-op"] = {
            "stage": "test",
            "script": ["echo No matching hardware tests to run"],
            "rules": [{"when": "on_success"}],
        }

    # Ensure child pipeline defines stages
    child = {"stages": ["test"]}

    for name, job in jobs.items():
        child[name] = job

    if args.dry_run:
        print("\n--- Generated child pipeline YAML (dry run) ---")
        PrettyDumper.add_representer(str, str_representer)
        sys.stdout.write(yaml.dump(child, Dumper=PrettyDumper, sort_keys=False, width=4096))
        return 0

    out = Path(args.out)

    PrettyDumper.add_representer(str, str_representer)
    out.write_text(yaml.dump(child, Dumper=PrettyDumper, sort_keys=False, width=4096), encoding="utf-8")
    print(f"Wrote child pipeline with {len(jobs)} job(s) to {out}")


if __name__ == "__main__":
    sys.exit(main())
