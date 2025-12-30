#!/usr/bin/env python3

import argparse
import json
import yaml
import os
import sys
import copy
import traceback
from pathlib import Path
from typing import Iterable
from urllib.parse import urlencode
import urllib.request
import urllib.error

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


def read_yaml(p: Path):
    try:
        with p.open("r", encoding="utf-8") as f:
            return yaml.safe_load(f) or {}
    except Exception as e:
        sys.stderr.write(f"[WARN] Failed to parse YAML file '{p}': {e}\n")
        sys.stderr.write(traceback.format_exc() + "\n")
        return {}


def find_tests() -> list[Path]:
    tests = []
    if not TESTS_ROOT.exists():
        return tests
    for ci in TESTS_ROOT.rglob("ci.yml"):
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
    v = ci_json.get("tags")
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


def _gitlab_auth_header() -> tuple[str, str]:
    """Return header key and value for GitLab API auth, preferring PRIVATE-TOKEN, then JOB-TOKEN.

    Falls back to empty auth if neither is available.
    """
    private = os.environ.get("GITLAB_API_TOKEN") or os.environ.get("PRIVATE_TOKEN")
    if private:
        return ("PRIVATE-TOKEN", private)
    job = os.environ.get("CI_JOB_TOKEN")
    if job:
        return ("JOB-TOKEN", job)
    return ("", "")


def _gitlab_api_get(path: str) -> tuple[int, dict | list | None]:
    """Perform a GET to GitLab API v4 and return (status_code, json_obj_or_None).

    Uses project-level API base from CI env. Returns (0, None) if base env is missing.
    """
    base = os.environ.get("CI_API_V4_URL")
    if not base:
        return 0, None
    url = base.rstrip("/") + "/" + path.lstrip("/")
    key, value = _gitlab_auth_header()
    req = urllib.request.Request(url)
    if key:
        req.add_header(key, value)
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            status = resp.getcode()
            data = resp.read()
            try:
                obj = json.loads(data.decode("utf-8")) if data else None
            except Exception:
                obj = None
            return status, obj
    except urllib.error.HTTPError as e:
        try:
            body = e.read().decode("utf-8")
        except Exception:
            body = str(e)
        sys.stderr.write(f"[WARN] GitLab API GET {url} failed: {e} body={body}\n")
        return e.code, None
    except Exception as e:
        sys.stderr.write(f"[WARN] GitLab API GET {url} error: {e}\n")
        sys.stderr.write(traceback.format_exc() + "\n")
        return -1, None


def list_project_runners() -> list[dict]:
    """List runners available to this project via GitLab API.

    Requires CI vars CI_API_V4_URL and CI_PROJECT_ID and either GITLAB_API_TOKEN or CI_JOB_TOKEN.
    Returns an empty list if not accessible.
    """
    project_id = os.environ.get("CI_PROJECT_ID")
    if not project_id:
        return []

    key, value = _gitlab_auth_header()
    sys.stderr.write(f"[DEBUG] Attempting to list runners for project {project_id}\n")
    sys.stderr.write(f"[DEBUG] Auth method: {'Authenticated' if key else 'None'}\n")

    runners: list[dict] = []
    page = 1
    per_page = 100
    while True:
        q = urlencode({"per_page": per_page, "page": page})
        status, obj = _gitlab_api_get(f"projects/{project_id}/runners?{q}")
        if status != 200 or not isinstance(obj, list):
            # Project-scoped listing might be restricted for JOB-TOKEN in some instances.
            # Return what we have (likely nothing) and let caller decide.
            if status == 403:
                sys.stderr.write("\n[ERROR] 403 Forbidden when listing project runners\n")
                sys.stderr.write(f"  Project ID: {project_id}\n")
                sys.stderr.write(f"  Authentication: {'Present' if key else 'None'}\n")
                sys.stderr.write("  Endpoint: projects/{project_id}/runners\n\n")

                sys.stderr.write("Required permissions:\n")
                sys.stderr.write("  - Token scope: 'api' (you likely have this)\n")
                sys.stderr.write("  - Project role: Maintainer or Owner (you may be missing this)\n\n")

                sys.stderr.write("Solutions:\n")
                sys.stderr.write("  1. Ensure the token owner has Maintainer/Owner role on project {project_id}\n")
                sys.stderr.write("  2. Use a Group Access Token if available (has higher privileges)\n")
                sys.stderr.write("  3. Set environment variable: ASSUME_TAGGED_GROUPS_MISSING=0\n")
                sys.stderr.write("     (This will skip runner enumeration and schedule all groups)\n\n")
            break
        runners.extend(x for x in obj if isinstance(x, dict))
        if len(obj) < per_page:
            break
        page += 1

    # The /projects/:id/runners endpoint returns simplified objects without tag_list.
    # Fetch full details for each runner to get tags.
    sys.stderr.write(f"[DEBUG] Fetching full details for {len(runners)} runners to get tags...\n")
    full_runners = []
    for runner in runners:
        runner_id = runner.get("id")
        if not runner_id:
            continue
        status, details = _gitlab_api_get(f"runners/{runner_id}")
        if status == 200 and isinstance(details, dict):
            full_runners.append(details)
        else:
            # If we can't get details, keep the basic info (no tags)
            full_runners.append(runner)

    return full_runners


def runner_supports_tags(runner: dict, required_tags: Iterable[str]) -> bool:
    tag_list = runner.get("tag_list") or []
    if not isinstance(tag_list, list):
        return False
    tags = {str(t).strip() for t in tag_list if isinstance(t, str) and t.strip()}
    if not tags:
        return False
    # Skip paused/inactive runners
    if runner.get("paused") is True:
        return False
    if runner.get("active") is False:
        return False
    return all(t in tags for t in required_tags)


def any_runner_matches(required_tags: Iterable[str], runners: list[dict]) -> bool:
    req = [t for t in required_tags if t]
    for r in runners:
        try:
            if runner_supports_tags(r, req):
                return True
        except Exception as e:
            # Be robust to unexpected runner payloads
            runner_id = r.get("id", "unknown")
            sys.stderr.write(f"[WARN] Error checking runner #{runner_id} against required tags: {e}\n")
            sys.stderr.write(traceback.format_exc() + "\n")
            continue
    return False


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
    print(f"Discovered {len(all_ci)} ci.yml files under tests/")

    matched_count = 0
    for test_type, test_path in find_sketch_test_dirs(types):
        ci_path = test_path / "ci.yml"
        ci = read_yaml(ci_path) if ci_path.exists() else {}
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

    # Discover available runners
    available_runners = list_project_runners()
    if not available_runners:
        print("\n[ERROR] Could not enumerate project runners!")
        print("This is required to match test groups to runners by tags.")
        print("\nPossible causes:")
        print("  - No runners are registered to the project")
        print("  - API token lacks required permissions (needs 'api' scope + Maintainer/Owner role)")
        print("  - Network/API connectivity issues")
        sys.exit(1)

    print(f"\n=== Available Runners ({len(available_runners)}) ===")
    for runner in available_runners:
        runner_id = runner.get("id", "?")
        runner_desc = runner.get("description", "")
        runner_tags = runner.get("tag_list", [])
        runner_active = runner.get("active", False)
        runner_paused = runner.get("paused", False)
        status = "ACTIVE" if (runner_active and not runner_paused) else "INACTIVE/PAUSED"
        print(f"  Runner #{runner_id} ({status}): {runner_desc}")
        print(f"    Tags: {', '.join(runner_tags) if runner_tags else '(none)'}")
    print("=" * 60 + "\n")

    # Track skipped groups for reporting
    skipped_groups: list[dict] = []

    print("\n=== Test Group Scheduling ===")
    for (chip, tagset, test_type), test_dirs in group_map.items():
        tag_list = sorted(tagset)
        # Build name suffix excluding the SOC itself to avoid duplication
        non_soc_tags = [t for t in tag_list if t != chip]
        tag_suffix = "-".join(non_soc_tags) if non_soc_tags else "generic"

        # Determine if any runner can serve this job
        can_schedule = any_runner_matches(tag_list, available_runners)
        print(f"  Group: {chip}-{test_type}-{tag_suffix}")
        print(f"    Required tags: {', '.join(tag_list)}")
        print(f"    Tests: {len(test_dirs)}")
        if can_schedule:
            print("    ✓ Runner found - scheduling")
        else:
            print("    ✗ NO RUNNER FOUND - skipping")

        if can_schedule:
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
        else:
            # Track skipped groups for reporting
            skipped_groups.append(
                {
                    "chip": chip,
                    "test_type": test_type,
                    "required_tags": tag_list,
                    "test_count": len(test_dirs),
                }
            )

    # Print summary
    print("\n=== Summary ===")
    print(f"  Scheduled groups: {len(jobs_entries)}")
    print(f"  Skipped groups (no runner): {len(skipped_groups)}")
    if skipped_groups:
        print("\n  Skipped group details:")
        for sg in skipped_groups:
            chip = sg.get("chip")
            test_type = sg.get("test_type")
            tags = sg.get("required_tags", [])
            test_count = sg.get("test_count", 0)
            print(f"    - {chip}-{test_type}: requires tags {tags}, {test_count} tests")

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
