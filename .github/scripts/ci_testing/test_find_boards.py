#!/usr/bin/env python3
"""Local validation for .github/scripts/find_boards.sh (not run by GHA).

Usage:
  python3 .github/scripts/ci_testing/test_find_boards.py
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SCRIPTS = ROOT / ".github" / "scripts"
FIND_BOARDS = SCRIPTS / "find_boards.sh"
SOCS_CONFIG = SCRIPTS / "socs_config.sh"

_ARRAY_LINE_RE = re.compile(r'^"([^"]+)"\s*,?\s*$')


def load_bash_array(config_text: str, array_name: str) -> list[str]:
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


def core_socs() -> list[str]:
    text = SOCS_CONFIG.read_text(encoding="utf-8")
    all_socs = load_bash_array(text, "ALL_SOCS")
    skip = set(load_bash_array(text, "SKIP_LIB_BUILD_SOCS"))
    return [soc for soc in all_socs if soc not in skip]


def skip_lib_build_socs() -> list[str]:
    return load_bash_array(SOCS_CONFIG.read_text(encoding="utf-8"), "SKIP_LIB_BUILD_SOCS")


def board_block(board_id: str, *, tarch: str = "xtensa") -> str:
    return "\n".join(
        [
            f"{board_id}.name={board_id} Board",
            f"{board_id}.build.mcu={board_id}",
            f"{board_id}.build.tarch={tarch}",
            f"{board_id}.upload.tool=esptool_py",
            "",
        ]
    )


def sample_boards_txt(*, include_skip_soc: bool = True, community: bool = True) -> str:
    parts = [
        "menu.UploadSpeed=Upload Speed",
        "esp32_family.name=ESP32 Arduino",
        "",
        board_block("esp32"),
        board_block("esp32c3", tarch="riscv32"),
        board_block("esp32s3"),
    ]
    if include_skip_soc:
        parts.append(board_block("esp32c2", tarch="riscv32"))
    if community:
        parts.append(board_block("some_community_board"))
    return "\n".join(parts)


def run_find_boards(
    args: list[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
    github_env: Path | None = None,
) -> subprocess.CompletedProcess[str]:
    full_env = os.environ.copy()
    if env:
        full_env.update(env)
    if github_env is not None:
        full_env["GITHUB_ENV"] = str(github_env)
    else:
        full_env.pop("GITHUB_ENV", None)
    return subprocess.run(
        ["bash", str(FIND_BOARDS), *args],
        cwd=cwd,
        env=full_env,
        text=True,
        capture_output=True,
        check=False,
    )


def extract_json_payload(stdout: str) -> object | None:
    """Return the last JSON array/object line from script stdout, if any."""
    for line in reversed(stdout.splitlines()):
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("[") or stripped.startswith("{"):
            return json.loads(stripped)
        if stripped == "FQBNS=":
            return None
    return None


def assert_exit(proc: subprocess.CompletedProcess[str], code: int, label: str) -> None:
    if proc.returncode != code:
        raise AssertionError(
            f"{label}: expected exit {code}, got {proc.returncode}\n"
            f"stdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
        )


def test_requires_output_format() -> None:
    proc = run_find_boards(["all"], cwd=ROOT)
    assert_exit(proc, 1, "missing --output-format")
    assert "--output-format" in proc.stderr


def test_rejects_unknown_subcommand_and_format() -> None:
    proc = run_find_boards(["nope", "--output-format=array"], cwd=ROOT)
    assert_exit(proc, 1, "unknown subcommand")

    proc = run_find_boards(["all", "--output-format=yaml"], cwd=ROOT)
    assert_exit(proc, 1, "bad format")


def test_help_exits_zero() -> None:
    proc = run_find_boards(["--help"], cwd=ROOT)
    assert_exit(proc, 0, "help")
    assert "Usage:" in proc.stdout


def test_all_array_and_matrix_and_format_alias() -> None:
    array_proc = run_find_boards(["all", "--output-format=array"], cwd=ROOT)
    assert_exit(array_proc, 0, "all array")
    array_payload = extract_json_payload(array_proc.stdout)
    assert isinstance(array_payload, list)
    assert array_payload
    assert all(isinstance(x, str) and x.startswith("espressif:esp32:") for x in array_payload)

    matrix_proc = run_find_boards(["all", "--format", "matrix"], cwd=ROOT)
    assert_exit(matrix_proc, 0, "all matrix alias")
    matrix_payload = extract_json_payload(matrix_proc.stdout)
    assert isinstance(matrix_payload, dict)
    assert "fqbn" in matrix_payload
    assert matrix_payload["fqbn"] == array_payload


def test_all_skips_lib_build_socs() -> None:
    proc = run_find_boards(["all", "--output-format=array"], cwd=ROOT)
    assert_exit(proc, 0, "all skip libs")
    payload = extract_json_payload(proc.stdout)
    assert isinstance(payload, list)
    ids = {fqbn.rsplit(":", 1)[-1] for fqbn in payload}
    for skip_soc in skip_lib_build_socs():
        assert skip_soc not in ids, f"{skip_soc} should be skipped by all"
    for soc in core_socs():
        assert soc in ids, f"CORE_SOCS board {soc} should be present"


def test_all_writes_github_env() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        github_env = Path(tmp) / "github.env"
        github_env.write_text("", encoding="utf-8")
        proc = run_find_boards(["all", "--output-format=array"], cwd=ROOT, github_env=github_env)
        assert_exit(proc, 0, "all github env")
        env_text = github_env.read_text(encoding="utf-8")
        assert re.search(r"^BOARD-COUNT=\d+$", env_text, re.M)
        assert re.search(r"^FQBNS=\[", env_text, re.M)


def test_new_no_changes_empty() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        boards = sample_boards_txt()
        (work / "boards.txt").write_text(boards, encoding="utf-8")
        base = work / "base_boards.txt"
        base.write_text(boards, encoding="utf-8")
        proc = run_find_boards(
            ["new", "owner/repo", "master", "--output-format=matrix"],
            cwd=work,
            env={"FIND_BOARDS_BASE_FILE": str(base)},
        )
        assert_exit(proc, 0, "new no changes")
        assert "No changes in boards.txt file" in proc.stdout
        assert extract_json_payload(proc.stdout) is None
        assert "FQBNS=" in proc.stdout


def test_new_detects_community_and_official_changes() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        base_text = sample_boards_txt()
        base = work / "base_boards.txt"
        base.write_text(base_text, encoding="utf-8")

        # Change community board + official esp32c3 property
        changed = base_text.replace(
            "some_community_board.name=some_community_board Board",
            "some_community_board.name=Renamed Community",
        ).replace(
            "esp32c3.upload.tool=esptool_py",
            "esp32c3.upload.tool=esptool_py\nesp32c3.upload.speed=115200",
        )
        (work / "boards.txt").write_text(changed, encoding="utf-8")

        proc = run_find_boards(
            ["new", "owner/repo", "master", "--output-format=array"],
            cwd=work,
            env={"FIND_BOARDS_BASE_FILE": str(base)},
        )
        assert_exit(proc, 0, "new with changes")
        payload = extract_json_payload(proc.stdout)
        assert isinstance(payload, list)
        ids = {fqbn.rsplit(":", 1)[-1] for fqbn in payload}
        assert "some_community_board" in ids
        assert "esp32c3" in ids


def test_official_filters_to_core_socs() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        base_text = sample_boards_txt()
        base = work / "base_boards.txt"
        base.write_text(base_text, encoding="utf-8")

        # Community-only change
        community_only = base_text.replace(
            "some_community_board.name=some_community_board Board",
            "some_community_board.name=Community Only Change",
        )
        (work / "boards.txt").write_text(community_only, encoding="utf-8")
        proc = run_find_boards(
            ["official", "owner/repo", "master", "--output-format=matrix"],
            cwd=work,
            env={"FIND_BOARDS_BASE_FILE": str(base)},
        )
        assert_exit(proc, 0, "official community-only")
        assert "Skipping non-official board" in proc.stdout
        assert extract_json_payload(proc.stdout) is None

        # Official + community change
        both = community_only.replace(
            "esp32.name=esp32 Board",
            "esp32.name=ESP32 Official Renamed",
        )
        (work / "boards.txt").write_text(both, encoding="utf-8")
        proc = run_find_boards(
            ["official", "owner/repo", "master", "--output-format=matrix"],
            cwd=work,
            env={"FIND_BOARDS_BASE_FILE": str(base)},
        )
        assert_exit(proc, 0, "official with core change")
        payload = extract_json_payload(proc.stdout)
        assert isinstance(payload, dict)
        ids = {fqbn.rsplit(":", 1)[-1] for fqbn in payload["fqbn"]}
        assert ids == {"esp32"}
        assert "some_community_board" not in ids


def test_excludes_menu_and_family_and_dedupes() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        base = work / "base_boards.txt"
        base.write_text(sample_boards_txt(community=False), encoding="utf-8")
        # Diff touches menu + family + two lines of the same board
        changed = (
            sample_boards_txt(community=False)
            .replace("menu.UploadSpeed=Upload Speed", "menu.UploadSpeed=Upload Baud")
            .replace("esp32_family.name=ESP32 Arduino", "esp32_family.name=ESP32 Family")
            .replace("esp32s3.name=esp32s3 Board", "esp32s3.name=S3 Renamed")
            .replace("esp32s3.build.mcu=esp32s3", "esp32s3.build.mcu=esp32s3\nesp32s3.build.variant=esp32s3")
        )
        (work / "boards.txt").write_text(changed, encoding="utf-8")
        proc = run_find_boards(
            ["new", "owner/repo", "master", "--output-format=array"],
            cwd=work,
            env={"FIND_BOARDS_BASE_FILE": str(base)},
        )
        assert_exit(proc, 0, "exclude + dedupe")
        payload = extract_json_payload(proc.stdout)
        assert isinstance(payload, list)
        ids = [fqbn.rsplit(":", 1)[-1] for fqbn in payload]
        assert "menu" not in ids
        assert "esp32_family" not in ids
        assert ids.count("esp32s3") == 1


def test_missing_args_for_new_and_official() -> None:
    proc = run_find_boards(["new"], cwd=ROOT)
    assert_exit(proc, 1, "new missing refs")
    assert "requires <owner/repo> <base_ref>" in proc.stderr
    proc = run_find_boards(["official", "owner/repo"], cwd=ROOT)
    assert_exit(proc, 1, "official missing base_ref")
    assert "requires <owner/repo> <base_ref>" in proc.stderr


def test_missing_base_file_env_errors() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        (work / "boards.txt").write_text(sample_boards_txt(), encoding="utf-8")
        proc = run_find_boards(
            ["new", "owner/repo", "master", "--output-format=array"],
            cwd=work,
            env={"FIND_BOARDS_BASE_FILE": str(work / "does-not-exist.txt")},
        )
        assert_exit(proc, 1, "missing FIND_BOARDS_BASE_FILE")
        assert "FIND_BOARDS_BASE_FILE not found" in proc.stderr


def main() -> int:
    tests = [
        test_requires_output_format,
        test_rejects_unknown_subcommand_and_format,
        test_help_exits_zero,
        test_all_array_and_matrix_and_format_alias,
        test_all_skips_lib_build_socs,
        test_all_writes_github_env,
        test_new_no_changes_empty,
        test_new_detects_community_and_official_changes,
        test_official_filters_to_core_socs,
        test_excludes_menu_and_family_and_dedupes,
        test_missing_args_for_new_and_official,
        test_missing_base_file_env_errors,
    ]
    failed = 0
    for test in tests:
        name = test.__name__
        try:
            test()
            print(f"PASS  {name}")
        except Exception as exc:  # noqa: BLE001 - report each failure and continue
            failed += 1
            print(f"FAIL  {name}: {exc}", file=sys.stderr)
    if failed:
        print(f"\n{failed}/{len(tests)} failed", file=sys.stderr)
        return 1
    print(f"\nAll {len(tests)} tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
