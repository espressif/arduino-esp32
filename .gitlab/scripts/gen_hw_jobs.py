#!/usr/bin/env python3
import argparse
import json
import os
import sys
from pathlib import Path

import yaml  # PyYAML is available in python image


# Resolve repository root from this script location: .gitlab/scripts -> esp32 root
SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent

# Ensure we run from repo root so relative paths work consistently
try:
    os.chdir(REPO_ROOT)
except Exception:
    pass

TESTS_ROOT = REPO_ROOT / "tests"


def read_json(p: Path):
    try:
        with p.open("r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def find_tests() -> list[Path]:
    tests = []
    if not TESTS_ROOT.exists():
        return tests
    for ci in TESTS_ROOT.rglob("ci.json"):
        if ci.is_file():
            tests.append(ci)
    return tests


def load_tags_for_test(ci_json: dict, chip: str) -> set[str]:
    tags = set()
    # Global tags
    for key in ("tags"):
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
                for req in requires_any if isinstance(req, str)
            )
            if not ok:
                return False
        return True
    except Exception:
        return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--chips", required=True, help="Comma-separated list of SoCs")
    ap.add_argument("--types", required=False, default="validation",
                    help="Comma-separated test type directories under tests/")
    ap.add_argument("--out", required=True, help="Output YAML path for child pipeline")
    ap.add_argument("--dry-run", action="store_true", help="Print planned groups/jobs and skip sdkconfig requirement checks")
    args = ap.parse_args()

    chips = [c.strip() for c in args.chips.split(",") if c.strip()]
    types = [t.strip() for t in args.types.split(",") if t.strip()]

    # Aggregate mapping: (chip, frozenset(tags or generic)) -> list of test paths
    group_map: dict[tuple[str, frozenset[str]], list[str]] = {}

    for ci_path in find_tests():
        # Filter by test type if provided
        try:
            rel = ci_path.relative_to(TESTS_ROOT)
            parts = rel.parts
            if not parts:
                continue
            test_type = parts[0]
        except Exception:
            continue
        if types and test_type not in types:
            continue

        ci = read_json(ci_path)
        test_dir = str(ci_path.parent)
        sketch = sketch_name_from_ci(ci_path)
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
            key = (chip, frozenset(sorted(key_tags)))
            group_map.setdefault(key, []).append(test_dir)

    # Load template job
    template_path = REPO_ROOT / '.gitlab/workflows/hw_test_template.yml'
    template = yaml.safe_load(template_path.read_text(encoding='utf-8'))
    if not isinstance(template, dict) or 'hw-test-template' not in template:
        print('ERROR: hw_test_template.yml missing hw-test-template')
        sys.exit(2)
    base_job = template['hw-test-template']

    # Build child pipeline YAML
    jobs = {}
    for (chip, tagset), test_dirs in group_map.items():
        tag_list = sorted(list(tagset))
        tag_str = "-".join(tag_list)
        job_name = f"hw-{chip}-{tag_str}"[:255]

        # Clone base job and adjust
        job = yaml.safe_load(yaml.safe_dump(base_job))
        # Ensure tags include SOC+extras
        job['tags'] = tag_list
        # Force type to 'all' so tests_run.sh discovers correct directories per -s
        vars_block = job.get('variables', {})
        vars_block['TEST_TYPE'] = 'all'
        vars_block['TEST_CHIP'] = chip
        vars_block['TEST_TYPES'] = args.types
        # Provide list of test directories for this job
        vars_block['TEST_LIST'] = "\n".join(sorted(test_dirs))
        job['variables'] = vars_block

        # Override script to run only selected tests
        job['script'] = [
            'echo Using binaries for $TEST_CHIP',
            'ls -laR ~/.arduino/tests || true',
            'set -e',
            'rc=0',
            'while IFS= read -r d; do \n  [ -z "$d" ] && continue; \n  sketch=$(basename "$d"); \n  echo Running $sketch in $d; \n  bash .github/scripts/tests_run.sh -t $TEST_CHIP -s $sketch -e || rc=$?; \n done <<< "$TEST_LIST"; exit $rc',
        ]

        # Override before_script to fetch only this SOC's artifacts for all TEST_TYPES
        job['before_script'] = [
            'echo Running hardware tests for chip:$TEST_CHIP',
            'pip install -U pip',
            'apt-get update',
            'apt-get install -y jq unzip curl',
            'rm -rf ~/.arduino/tests',
            'mkdir -p ~/.arduino/tests/$TEST_CHIP',
            'IFS=',' read -r -a types <<< "$TEST_TYPES"; for t in ${types[@]}; do export TEST_TYPE="$t"; echo Fetching binaries for $TEST_CHIP $t; bash .gitlab/scripts/get_artifacts.sh; done',
            'pip install -r tests/requirements.txt --extra-index-url https://dl.espressif.com/pypi',
        ]

        jobs[job_name] = job

    if args.dry_run:
        print("Planned hardware test jobs:")
        for name, job in jobs.items():
            tags = job.get('tags', [])
            soc = job.get('variables', {}).get('TEST_CHIP')
            tlist = job.get('variables', {}).get('TEST_LIST', '')
            tests = [p for p in tlist.split('\n') if p]
            print(f"- {name} tags={tags} soc={soc} tests={len(tests)}")
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
    child.update(jobs)

    if args.dry_run:
        print("\n--- Generated child pipeline YAML (dry run) ---")
        sys.stdout.write(yaml.safe_dump(child, sort_keys=False))
        return 0

    out = Path(args.out)
    out.write_text(yaml.safe_dump(child, sort_keys=False), encoding="utf-8")
    print(f"Wrote child pipeline with {len(jobs)} job(s) to {out}")


if __name__ == "__main__":
    sys.exit(main())


