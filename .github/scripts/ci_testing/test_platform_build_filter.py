import os
import shutil
import subprocess
from pathlib import Path

import pytest


SCRIPT_DIR = Path(__file__).resolve().parents[1]
SKETCH_UTILS = SCRIPT_DIR / "sketch_utils.sh"
TESTS_BUILD = SCRIPT_DIR / "tests_build.sh"
SOCS_CONFIG = SCRIPT_DIR / "socs_config.sh"


def _run(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        env=env,
        text=True,
        capture_output=True,
        check=False,
    )


def _write_sketch(root: Path, name: str, ci_yml: str | None = None) -> Path:
    sketch_dir = root / name
    sketch_dir.mkdir(parents=True)
    (sketch_dir / f"{name}.ino").write_text("void setup() {}\nvoid loop() {}\n", encoding="utf-8")
    if ci_yml is not None:
        (sketch_dir / "ci.yml").write_text(ci_yml, encoding="utf-8")
    return sketch_dir


@pytest.mark.parametrize(
    ("ci_yml", "platform", "target", "allowed"),
    [
        (None, "hardware", "esp32", True),
        ("", "hardware", "esp32", True),
        ("{broken", "hardware", "esp32", True),
        ("platforms:\n  hardware: true\n", "hardware", "esp32", True),
        ("platforms:\n  hardware: false\n", "hardware", "esp32", False),
        ("platforms:\n  hardware:\n    esp32: true\n", "hardware", "esp32", True),
        ("platforms:\n  hardware:\n    esp32: false\n", "hardware", "esp32", False),
        ("platforms:\n  hardware:\n    esp32: false\n", "hardware", "esp32c3", True),
        ("platforms:\n  wokwi: false\n", "hardware", "esp32", True),
        ("platforms: invalid\n", "hardware", "esp32", True),
        ("platforms:\n  - hardware\n", "hardware", "esp32", True),
    ],
)
def test_platform_build_allowed_matches_run_and_report_semantics(
    tmp_path: Path,
    ci_yml: str | None,
    platform: str,
    target: str,
    allowed: bool,
) -> None:
    sketch_dir = _write_sketch(tmp_path, "sample", ci_yml)

    result = _run(
        [str(SKETCH_UTILS), "platform_build_allowed", str(sketch_dir), platform, target],
        tmp_path,
    )

    assert (result.returncode == 0) is allowed, result.stderr


def test_count_filters_platform_target_and_requirements_together(tmp_path: Path) -> None:
    sketches = tmp_path / "sketches"
    _write_sketch(sketches, "allowed", "platforms:\n  hardware: true\n")
    _write_sketch(sketches, "platform_blocked", "platforms:\n  hardware: false\n")
    _write_sketch(sketches, "pair_blocked", "platforms:\n  hardware:\n    esp32: false\n")
    _write_sketch(sketches, "other_pair_blocked", "platforms:\n  hardware:\n    esp32c3: false\n")
    _write_sketch(sketches, "target_blocked", "targets:\n  esp32: false\n")
    _write_sketch(sketches, "requirements_met", "requires:\n  - CONFIG_SOC_WIFI_SUPPORTED=y\n")
    _write_sketch(sketches, "requirements_missing", "requires:\n  - CONFIG_TEST_OPTION_THAT_DOES_NOT_EXIST=y\n")
    selected_file = tmp_path / "selected.txt"

    result = _run(
        [str(SKETCH_UTILS), "count", str(sketches), "esp32", "0", "", "hardware", str(selected_file)],
        tmp_path,
    )

    assert result.returncode == 0, result.stderr
    assert result.stdout.strip().splitlines()[-1] == "3"
    selected = {Path(line).parent.name for line in selected_file.read_text().splitlines()}
    assert selected == {"allowed", "other_pair_blocked", "requirements_met"}


def test_count_without_platform_remains_unfiltered(tmp_path: Path) -> None:
    sketches = tmp_path / "sketches"
    _write_sketch(sketches, "hardware_disabled", "platforms:\n  hardware: false\n")
    _write_sketch(sketches, "wokwi_disabled", "platforms:\n  wokwi:\n    esp32: false\n")
    selected_file = tmp_path / "selected.txt"

    result = _run(
        [str(SKETCH_UTILS), "count", str(sketches), "esp32", "0", "", "", str(selected_file)],
        tmp_path,
    )

    assert result.returncode == 0, result.stderr
    assert result.stdout.strip().splitlines()[-1] == "2"
    assert not (tmp_path / "sketches.txt").exists()


def test_count_legacy_interface_writes_cwd_sketches_txt(tmp_path: Path) -> None:
    sketches = tmp_path / "sketches"
    first = _write_sketch(sketches, "first")
    second = _write_sketch(sketches, "second")

    result = _run([str(SKETCH_UTILS), "count", str(sketches), "esp32"], tmp_path)

    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == "2"
    assert (tmp_path / "sketches.txt").read_text(encoding="utf-8").splitlines() == [
        str(first / "first.ino"),
        str(second / "second.ino"),
    ]


def _build_harness(tmp_path: Path) -> tuple[Path, Path, dict[str, str]]:
    scripts = tmp_path / ".github" / "scripts"
    scripts.mkdir(parents=True)
    shutil.copy2(TESTS_BUILD, scripts / "tests_build.sh")
    shutil.copy2(SOCS_CONFIG, scripts / "socs_config.sh")

    (scripts / "env.sh").write_text(
        'export ARDUINO_USR_PATH="$PWD/.arduino-user"\n',
        encoding="utf-8",
    )
    (scripts / "install-arduino-core-esp32.sh").write_text(":\n", encoding="utf-8")
    (scripts / "install-arduino-cli.sh").write_text(
        'export ARDUINO_IDE_PATH="$PWD/.arduino-cli"\n',
        encoding="utf-8",
    )
    (scripts / "tests_utils.sh").write_text(
        'detect_test_type_and_folder() { test_folder="$PWD/tests/validation"; return 0; }\n',
        encoding="utf-8",
    )
    wrapper = scripts / "sketch_utils.sh"
    wrapper.write_text(
        """#!/bin/bash
case "$1" in
    platform_build_allowed|check_requirements|fqbn_append)
        exec "$REAL_SKETCH_UTILS" "$@"
        ;;
    build|chunk_build)
        printf '%s\\n' "$*" >> "$BUILD_LOG"
        exit 0
        ;;
    build_list)
        original_args="$*"
        shift
        list_file=""
        while [ -n "$1" ]; do
            if [ "$1" = "-F" ]; then
                shift
                list_file="$1"
                break
            fi
            shift
        done
        while IFS= read -r sketch || [ -n "$sketch" ]; do
            printf '%s -s %s\\n' "$original_args" "$sketch" >> "$BUILD_LOG"
        done < "$list_file"
        exit 0
        ;;
    *)
        echo "unexpected sketch_utils command: $1" >&2
        exit 99
        ;;
esac
""",
        encoding="utf-8",
    )
    wrapper.chmod(0o755)

    log = tmp_path / "build.log"
    env = os.environ.copy()
    env.update({"BUILD_LOG": str(log), "REAL_SKETCH_UTILS": str(SKETCH_UTILS)})
    return scripts / "tests_build.sh", log, env


def _build_log(log: Path) -> list[str]:
    if not log.exists():
        return []
    return log.read_text(encoding="utf-8").splitlines()


def test_explicit_comma_separated_sketches_filter_only_when_requested(tmp_path: Path) -> None:
    script, log, env = _build_harness(tmp_path)
    validation = tmp_path / "tests" / "validation"
    _write_sketch(validation, "blocked", "platforms:\n  hardware: false\n")
    _write_sketch(validation, "allowed", "platforms:\n  hardware: true\n")

    filtered = _run(
        [str(script), "-s", "blocked,allowed", "-type", "validation", "-t", "esp32", "--platform", "hardware"],
        tmp_path,
        env,
    )

    assert filtered.returncode == 0, filtered.stderr
    assert len(_build_log(log)) == 1
    assert "-s " + str(validation / "allowed") in _build_log(log)[0]

    log.unlink()
    unfiltered = _run(
        [str(script), "-s", "blocked,allowed", "-type", "validation", "-t", "esp32"],
        tmp_path,
        env,
    )

    assert unfiltered.returncode == 0, unfiltered.stderr
    assert len(_build_log(log)) == 2


def test_explicit_target_combinations_apply_nested_platform_map(tmp_path: Path) -> None:
    script, log, env = _build_harness(tmp_path)
    validation = tmp_path / "tests" / "validation"
    _write_sketch(
        validation,
        "sample",
        "platforms:\n  hardware:\n    esp32: false\n    esp32c3: true\n",
    )

    result = _run(
        [str(script), "-s", "sample", "-type", "validation", "-t", "esp32,esp32c3", "-H"],
        tmp_path,
        env,
    )

    assert result.returncode == 0, result.stderr
    lines = _build_log(log)
    assert len(lines) == 1
    assert "-t esp32c3" in lines[0]


@pytest.mark.parametrize(
    ("option", "expected_targets"),
    [
        ("-H", ["esp32", "esp32c3", "esp32c5", "esp32c6", "esp32h2", "esp32p4", "esp32s2", "esp32s3"]),
        ("-W", ["esp32", "esp32c3", "esp32c6", "esp32h2", "esp32p4", "esp32s2", "esp32s3"]),
        ("-Q", ["esp32", "esp32c3"]),
    ],
)
def test_platform_aliases_select_socs_config_defaults(
    tmp_path: Path,
    option: str,
    expected_targets: list[str],
) -> None:
    script, log, env = _build_harness(tmp_path)
    validation = tmp_path / "tests" / "validation"
    _write_sketch(validation, "sample")

    result = _run([str(script), "-s", "sample", "-type", "validation", option], tmp_path, env)

    assert result.returncode == 0, result.stderr
    lines = _build_log(log)
    assert [line.split("-t ", 1)[1].split()[0] for line in lines] == expected_targets


def test_explicit_target_wins_over_platform_defaults(tmp_path: Path) -> None:
    script, log, env = _build_harness(tmp_path)
    validation = tmp_path / "tests" / "validation"
    _write_sketch(validation, "sample")

    result = _run(
        [str(script), "-s", "sample", "-type", "validation", "-t", "esp32s3", "--platform", "qemu"],
        tmp_path,
        env,
    )

    assert result.returncode == 0, result.stderr
    assert len(_build_log(log)) == 1
    assert "-t esp32s3" in _build_log(log)[0]


def test_chunk_build_receives_platform_filter_and_default_targets(tmp_path: Path) -> None:
    script, log, env = _build_harness(tmp_path)
    (tmp_path / "tests" / "validation").mkdir(parents=True)

    result = _run([str(script), "-c", "-type", "validation", "-i", "2", "-m", "5", "-W"], tmp_path, env)

    assert result.returncode == 0, result.stderr
    lines = _build_log(log)
    assert len(lines) == 7
    assert all(line.startswith("chunk_build ") for line in lines)
    assert all("--platform wokwi" in line for line in lines)
    assert all("-i 2 -m 5" in line for line in lines)


def _write_multi_device(validation: Path, ci_yml_suffix: str = "") -> None:
    test_dir = validation / "multi"
    _write_sketch(test_dir, "sender")
    _write_sketch(test_dir, "receiver")
    (test_dir / "ci.yml").write_text(
        "multi_device:\n"
        "  first: sender\n"
        "  second: receiver\n"
        f"{ci_yml_suffix}",
        encoding="utf-8",
    )


@pytest.mark.parametrize("option", ["-W", "-Q"])
def test_simulators_always_skip_parent_configured_multi_device(tmp_path: Path, option: str) -> None:
    script, log, env = _build_harness(tmp_path)
    validation = tmp_path / "tests" / "validation"
    _write_multi_device(validation, "platforms:\n  wokwi: true\n  qemu: true\n")

    result = _run(
        [str(script), "-s", "multi", "-type", "validation", "-t", "esp32", option],
        tmp_path,
        env,
    )

    assert result.returncode == 0, result.stderr
    assert _build_log(log) == []
    assert "Skipping multi-device test multi for platform" in result.stdout


def test_hardware_multi_device_uses_parent_platform_target_and_requirements(tmp_path: Path) -> None:
    script, log, env = _build_harness(tmp_path)
    validation = tmp_path / "tests" / "validation"
    sdkconfig = tmp_path / "tools" / "esp32-arduino-libs" / "esp32" / "sdkconfig"
    sdkconfig.parent.mkdir(parents=True)
    sdkconfig.write_text("CONFIG_AVAILABLE=y\n", encoding="utf-8")

    _write_multi_device(
        validation,
        "platforms:\n  hardware:\n    esp32: true\nrequires:\n  - CONFIG_AVAILABLE=y\n",
    )
    allowed = _run(
        [str(script), "-s", "multi", "-type", "validation", "-t", "esp32", "-H"],
        tmp_path,
        env,
    )

    assert allowed.returncode == 0, allowed.stderr
    assert len(_build_log(log)) == 2

    log.unlink()
    (validation / "multi" / "ci.yml").write_text(
        "multi_device:\n  first: sender\n  second: receiver\nplatforms:\n  hardware:\n    esp32: false\n",
        encoding="utf-8",
    )
    blocked = _run(
        [str(script), "-s", "multi", "-type", "validation", "-t", "esp32", "-H"],
        tmp_path,
        env,
    )
    assert blocked.returncode == 0, blocked.stderr
    assert _build_log(log) == []

    (validation / "multi" / "ci.yml").write_text(
        "multi_device:\n"
        "  first: sender\n"
        "  second: receiver\n"
        "platforms:\n"
        "  hardware: true\n"
        "requires:\n"
        "  - CONFIG_UNAVAILABLE=y\n",
        encoding="utf-8",
    )
    requirements_blocked = _run(
        [str(script), "-s", "multi", "-type", "validation", "-t", "esp32", "-H"],
        tmp_path,
        env,
    )
    assert requirements_blocked.returncode == 0, requirements_blocked.stderr
    assert _build_log(log) == []


def test_invalid_platform_is_rejected_before_setup(tmp_path: Path) -> None:
    script, _, env = _build_harness(tmp_path)

    result = _run([str(script), "--platform", "simulator"], tmp_path, env)

    assert result.returncode == 2
    assert "--platform must be one of" in result.stdout
