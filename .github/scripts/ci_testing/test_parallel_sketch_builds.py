import json
import os
import signal
import shutil
import subprocess
import time
from pathlib import Path

import pytest


SCRIPT_DIR = Path(__file__).resolve().parents[1]
SKETCH_UTILS = SCRIPT_DIR / "sketch_utils.sh"
TESTS_BUILD = SCRIPT_DIR / "tests_build.sh"


FAKE_COMPILER = r"""
import fcntl
import json
import os
import sys
import time
from pathlib import Path


def update_state(delta, event=None):
    state_path = Path(os.environ["BUILD_TEST_STATE"])
    lock_path = state_path.with_suffix(".lock")
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    with lock_path.open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if state_path.exists():
            state = json.loads(state_path.read_text())
        else:
            state = {"active": 0, "max_active": 0, "events": []}
        state["active"] += delta
        state["max_active"] = max(state["max_active"], state["active"])
        if event is not None:
            state["events"].append(event)
        state_path.write_text(json.dumps(state))


args = sys.argv[1:]
build_path = Path(args[args.index("--build-path") + 1])
sketch = Path(args[args.index("--sketch") + 1])
event = {
    "sketch": str(sketch),
    "build_path": str(build_path),
    "cmake_jobs": os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL"),
    "args": args,
}
update_state(1, event)
try:
    build_path.mkdir(parents=True, exist_ok=True)
    (build_path / "compile_commands.json").write_text("[]")
    (build_path / "sdkconfig").write_text('CONFIG_IDF_TARGET="esp32"\n')
    print(f"compiler output for {sketch.name}", flush=True)
    print("Sketch uses 123 bytes (1%) of program storage space.", flush=True)
    print("Global variables use 45 bytes (2%) of dynamic memory.", flush=True)
    time.sleep(float(os.environ.get("BUILD_TEST_DELAY", "0.12")))
    failures = set(filter(None, os.environ.get("BUILD_TEST_FAILURES", "").split(",")))
    if sketch.name in failures:
        print(f"FAIL_MARKER:{sketch.name}", flush=True)
        sys.exit(7)
finally:
    update_state(-1)
"""


FAKE_CLI = r"""#!/usr/bin/env python3
import fcntl
import json
import os
import subprocess
import sys
import time
from pathlib import Path

state_path = Path(os.environ["INSTALL_TEST_STATE"])
lock_path = state_path.with_suffix(".lock")
lock_path.parent.mkdir(parents=True, exist_ok=True)


def update(delta, item=None):
    with lock_path.open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if state_path.exists():
            state = json.loads(state_path.read_text())
        else:
            state = {"active": 0, "max_active": 0, "items": []}
        state["active"] += delta
        state["max_active"] = max(state["max_active"], state["active"])
        if item is not None:
            state["items"].append(item)
        state_path.write_text(json.dumps(state))


args = sys.argv[1:]
if args[:2] == ["lib", "install"]:
    update(1, args[-1])
    try:
        time.sleep(0.05)
    finally:
        update(-1)
    sys.exit(0)
if args[:2] == ["config", "get"]:
    print("false")
    sys.exit(0)
if args[:2] == ["config", "set"]:
    sys.exit(0)
if args[:1] == ["compile"]:
    compiler = Path(os.environ["GITHUB_WORKSPACE"]) / "tools" / "arduino_cmake.py"
    build_path = args[args.index("--build-path") + 1]
    sketch = args[-1]
    completed = subprocess.run(
        [sys.executable, str(compiler), "compile", "--build-path", build_path, "--sketch", sketch],
        check=False,
    )
    sys.exit(completed.returncode)
print(f"unexpected fake arduino-cli invocation: {args}", file=sys.stderr)
sys.exit(90)
"""


def _run(
    command: list[str],
    cwd: Path,
    env: dict[str, str],
    timeout: float = 15,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        env=env,
        text=True,
        capture_output=True,
        check=False,
        timeout=timeout,
    )


def _fake_logical_cpus(tmp_path: Path, env: dict[str, str], count: int) -> None:
    fake_bin = tmp_path / "bin"
    fake_bin.mkdir(exist_ok=True)
    getconf = fake_bin / "getconf"
    getconf.write_text(f"#!/bin/sh\necho {count}\n", encoding="utf-8")
    getconf.chmod(0o755)
    env["PATH"] = f"{fake_bin}:{env['PATH']}"


def _make_sketch(root: Path, name: str, ci_yml: str | None = None) -> Path:
    sketch = root / name
    sketch.mkdir(parents=True)
    (sketch / f"{name}.ino").write_text(
        "void setup() {}\nvoid loop() {}\n", encoding="utf-8"
    )
    if ci_yml is not None:
        (sketch / "ci.yml").write_text(ci_yml, encoding="utf-8")
    return sketch


def _make_repo(tmp_path: Path) -> tuple[Path, dict[str, str]]:
    repo = tmp_path / "repo"
    scripts = repo / ".github" / "scripts"
    scripts.mkdir(parents=True)
    for name in (
        "sketch_utils.sh",
        "env.sh",
        "socs_config.sh",
        "tests_build.sh",
        "tests_run.sh",
        "on-push.sh",
    ):
        shutil.copy2(SCRIPT_DIR / name, scripts / name)

    (scripts / "install-arduino-core-esp32.sh").write_text(":\n", encoding="utf-8")
    (scripts / "install-arduino-cli.sh").write_text(":\n", encoding="utf-8")
    (scripts / "install-arduino-ide.sh").write_text(":\n", encoding="utf-8")
    (scripts / "tests_utils.sh").write_text(
        'detect_test_type_and_folder() { test_folder="$PWD/tests/validation"; return 0; }\n',
        encoding="utf-8",
    )

    libs = repo / "tools" / "esp32-arduino-libs" / "esp32"
    libs.mkdir(parents=True)
    (libs / "sdkconfig").write_text("CONFIG_AVAILABLE=y\n", encoding="utf-8")
    compiler = repo / "tools" / "arduino_cmake.py"
    compiler.write_text(FAKE_COMPILER, encoding="utf-8")

    cli_dir = repo / "fake-cli"
    cli_dir.mkdir()
    cli = cli_dir / "arduino-cli"
    cli.write_text(FAKE_CLI, encoding="utf-8")
    cli.chmod(0o755)

    fake_bin = repo / "fake-bin"
    fake_bin.mkdir()
    pytest_script = fake_bin / "pytest"
    pytest_script.write_text(
        '#!/bin/sh\nprintf \'%s\\n\' "$@" > "$PYTEST_ARGS_LOG"\n',
        encoding="utf-8",
    )
    pytest_script.chmod(0o755)

    home = tmp_path / "home"
    home.mkdir()
    env = os.environ.copy()
    env.update(
        {
            "BUILD_TEST_STATE": str(tmp_path / "build-state.json"),
            "INSTALL_TEST_STATE": str(tmp_path / "install-state.json"),
            "GITHUB_WORKSPACE": str(repo),
            "HOME": str(home),
            "PYTEST_ARGS_LOG": str(tmp_path / "pytest-args.txt"),
            "PATH": f"{fake_bin}:{env['PATH']}",
        }
    )
    env.pop("CMAKE_BUILD_PARALLEL_LEVEL", None)
    return repo, env


def _write_list(path: Path, sketches: list[Path]) -> None:
    path.write_text("".join(f"{sketch}\n" for sketch in sketches), encoding="utf-8")


def _configure_push_repo(repo: Path, os_name: str) -> None:
    scripts = repo / ".github" / "scripts"
    flags = {
        "linux": (1, 0, 0),
        "macos": (0, 1, 0),
        "windows": (0, 0, 1),
    }[os_name]
    (scripts / "env.sh").write_text(
        f"""\
OS_IS_LINUX={flags[0]}
OS_IS_MACOS={flags[1]}
OS_IS_WINDOWS={flags[2]}
ARDUINO_USR_PATH="$GITHUB_WORKSPACE/user"
ARDUINO_ESP32_PATH="$GITHUB_WORKSPACE"
ARDUINO_IDE_PATH="$GITHUB_WORKSPACE/fake-ide"
REPO_ROOT="$GITHUB_WORKSPACE"
SDKCONFIG_DIR="$GITHUB_WORKSPACE/tools/esp32-arduino-libs"
""",
        encoding="utf-8",
    )
    (scripts / "socs_config.sh").write_text("CORE_SOCS=(esp32)\n", encoding="utf-8")


def _prepare_push_repo(repo: Path) -> tuple[Path, list[Path]]:
    _configure_push_repo(repo, "linux")
    sketches = [
        _make_sketch(repo / "libraries" / library / "examples", "Shared")
        for library in ("LibraryA", "LibraryB", "LibraryC", "LibraryD")
    ]
    selected = repo / "affected_sketches.txt"
    _write_list(selected, [sketch / "Shared.ino" for sketch in sketches])
    return selected, sketches


def _prepare_non_linux_push_repo(repo: Path, os_name: str) -> list[Path]:
    _configure_push_repo(repo, os_name)
    sketches = [
        _make_sketch(
            repo / "libraries" / "NetworkClientSecure" / "examples",
            "WiFiClientSecure",
        ),
        _make_sketch(repo / "libraries" / "BLE" / "examples", "Server"),
        _make_sketch(
            repo / "libraries" / "ESP32" / "examples" / "Camera",
            "CameraWebServer",
        ),
        _make_sketch(
            repo / "libraries" / "Insights" / "examples",
            "MinimalDiagnostics",
        ),
    ]
    if os_name == "windows":
        ide_tools = repo / "fake-ide" / "tools-builder"
        (ide_tools / "ctags" / "1.2.3").mkdir(parents=True)
        (ide_tools / "arduino-preprocessor" / "4.5.6").mkdir(parents=True)
    return sketches


def _state(env: dict[str, str], name: str = "BUILD_TEST_STATE") -> dict:
    return json.loads(Path(env[name]).read_text(encoding="utf-8"))


def _build_list_command(
    repo: Path,
    sketch_list: Path,
    jobs: int | None = None,
    *,
    isolated_build_dirs: bool = False,
) -> list[str]:
    command = [
        str(repo / ".github" / "scripts" / "sketch_utils.sh"),
        "build_list",
        "-F",
        str(sketch_list),
    ]
    if jobs is not None:
        command.extend(["-j", str(jobs)])
    if isolated_build_dirs:
        command.append("--isolated-build-dirs")
    command.extend(
        [
            "-ai",
            str(repo / "fake-cli"),
            "-au",
            str(repo / "user"),
            "-t",
            "esp32",
            "-i",
            "0",
        ]
    )
    return command


def test_build_list_defaults_to_serial_output(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [_make_sketch(repo / "tests", name) for name in ("one", "two")]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)

    result = _run(_build_list_command(repo, sketch_list), repo, env)

    assert result.returncode == 0, result.stderr
    assert _state(env)["max_active"] == 1
    assert "Parallel build summary" not in result.stdout
    assert [event["cmake_jobs"] for event in _state(env)["events"]] == [None, None]


def test_parallel_builds_are_bounded_and_have_per_build_outputs(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [_make_sketch(repo / "tests", f"sketch-{index}") for index in range(5)]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)
    compile_commands = tmp_path / "compile-commands"
    env["COMPILE_COMMANDS_DIR"] = str(compile_commands)

    result = _run(_build_list_command(repo, sketch_list, 3), repo, env)

    assert result.returncode == 0, result.stderr
    state = _state(env)
    assert 1 < state["max_active"] <= 3
    assert "Parallel sketch builds: 5 sketches, 3 jobs" in result.stdout
    assert result.stdout.count("[parallel] start ") == 5
    assert result.stdout.count("[parallel] done  ") == 5
    assert "Parallel build summary: 5 succeeded, 0 failed" in result.stdout
    assert f"(max active {state['max_active']}/3)" in result.stdout
    for sketch in sketches:
        output = (
            tmp_path
            / "home"
            / ".arduino"
            / "tests"
            / "esp32"
            / sketch.name
            / "build.tmp"
        )
        assert (
            (output / "cli_compile_output.txt")
            .read_text()
            .startswith("compiler output")
        )
        assert (output / "compile_commands.json").exists()
    assert len(list(compile_commands.glob("*.json"))) == 5


def test_parallel_failures_aggregate_with_deterministic_logs(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [
        _make_sketch(repo / "tests", name) for name in ("ok", "fail-one", "fail-two")
    ]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)
    env["BUILD_TEST_FAILURES"] = "fail-one,fail-two"

    result = _run(_build_list_command(repo, sketch_list, 3), repo, env)

    combined = result.stdout + result.stderr
    assert result.returncode == 7
    assert "Parallel build summary: 1 succeeded, 2 failed" in combined
    assert "FAIL_MARKER:fail-one" in combined
    assert "FAIL_MARKER:fail-two" in combined
    assert combined.index("Build log: " + str(sketches[0])) < combined.index(
        "Build log: " + str(sketches[1])
    )
    assert combined.index("Build log: " + str(sketches[1])) < combined.index(
        "Build log: " + str(sketches[2])
    )
    summary_at = combined.index("Parallel build summary: 1 succeeded, 2 failed")
    assert combined.index("FAILED: " + str(sketches[1])) > summary_at
    assert combined.index("FAILED: " + str(sketches[2])) > summary_at
    assert combined.rindex("FAILED:") > combined.rindex(
        "Build log: " + str(sketches[2])
    )


def test_parallel_failure_logs_follow_success_logs(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [
        _make_sketch(repo / "tests", name) for name in ("fail-one", "ok", "fail-two")
    ]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)
    env["BUILD_TEST_FAILURES"] = "fail-one,fail-two"

    result = _run(_build_list_command(repo, sketch_list, 3), repo, env)
    stdout = result.stdout

    assert result.returncode == 7
    ok_log = stdout.index("Build log: " + str(sketches[1]))
    fail_one_log = stdout.index("Build log: " + str(sketches[0]))
    fail_two_log = stdout.index("Build log: " + str(sketches[2]))
    summary = stdout.index("Parallel build summary: 1 succeeded, 2 failed")
    assert ok_log < fail_one_log < fail_two_log < summary
    assert stdout.index("FAILED: " + str(sketches[0])) > summary
    assert stdout.index("FAILED: " + str(sketches[2])) > summary


def test_worker_exit_without_status_is_detected_and_synthesized(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [
        _make_sketch(repo / "tests", name) for name in ("killed", "survivor", "third")
    ]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)
    env["BUILD_TEST_DELAY"] = "1"
    worker_tmp = tmp_path / "worker-tmp"
    worker_tmp.mkdir()
    env["TMPDIR"] = str(worker_tmp)
    process = subprocess.Popen(
        _build_list_command(repo, sketch_list, 2),
        cwd=repo,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    try:
        deadline = time.monotonic() + 5
        worker_pid = None
        while time.monotonic() < deadline:
            state_path = Path(env["BUILD_TEST_STATE"])
            if state_path.exists() and _state(env)["active"] >= 2:
                pid_files = list(worker_tmp.glob("arduino-build-workers.*/pid.1"))
                if pid_files:
                    worker_pid = int(pid_files[0].read_text(encoding="utf-8"))
                    break
            time.sleep(0.05)
        assert worker_pid is not None
        os.kill(worker_pid, signal.SIGKILL)
        stdout, stderr = process.communicate(timeout=5)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()

    combined = stdout + stderr
    assert process.returncode != 0
    assert "Worker exited without status" in combined
    assert "Parallel build summary: 2 succeeded, 1 failed" in combined
    assert list(worker_tmp.glob("arduino-build-workers.*")) == []


@pytest.mark.parametrize(
    ("sent_signal", "expected_code"), [(signal.SIGINT, 130), (signal.SIGTERM, 143)]
)
def test_signal_cleanup_removes_workers_without_killing_unrelated_jobs(
    tmp_path: Path,
    sent_signal: signal.Signals,
    expected_code: int,
) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [_make_sketch(repo / "tests", name) for name in ("one", "two")]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)
    worker_tmp = tmp_path / "worker-tmp"
    worker_tmp.mkdir()
    env["TMPDIR"] = str(worker_tmp)
    env["BUILD_TEST_DELAY"] = "5"
    unrelated = subprocess.Popen(["sleep", "10"])
    process = subprocess.Popen(
        _build_list_command(repo, sketch_list, 2),
        cwd=repo,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    try:
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            state_path = Path(env["BUILD_TEST_STATE"])
            if state_path.exists() and _state(env)["active"] >= 2:
                break
            time.sleep(0.05)
        else:
            pytest.fail("parallel workers did not start")

        process.send_signal(sent_signal)
        process.communicate(timeout=5)

        assert process.returncode == expected_code
        assert unrelated.poll() is None
        assert list(worker_tmp.glob("arduino-build-workers.*")) == []
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        unrelated.terminate()
        unrelated.wait()


def test_parallel_duplicate_sketch_basenames_fail_before_compiling(
    tmp_path: Path,
) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [
        _make_sketch(repo / "first", "duplicate"),
        _make_sketch(repo / "second", "duplicate"),
    ]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)

    result = _run(_build_list_command(repo, sketch_list, 2), repo, env)

    assert result.returncode == 2
    assert "Duplicate sketch basename 'duplicate'" in result.stderr
    assert str(sketches[0]) in result.stderr
    assert str(sketches[1]) in result.stderr
    assert not Path(env["BUILD_TEST_STATE"]).exists()


def test_serial_duplicate_sketch_basenames_keep_legacy_behavior(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [
        _make_sketch(repo / "first", "duplicate"),
        _make_sketch(repo / "second", "duplicate"),
    ]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)

    result = _run(_build_list_command(repo, sketch_list, 1), repo, env)

    assert result.returncode == 0, result.stderr
    assert len(_state(env)["events"]) == 2


@pytest.mark.parametrize("jobs", [1, 2])
def test_arduino_build_dir_allows_serial_and_rejects_parallel(
    tmp_path: Path, jobs: int
) -> None:
    repo, env = _make_repo(tmp_path)
    validation = repo / "tests" / "validation"
    _make_sketch(validation, "sample")
    override = tmp_path / "shared-build"
    env["ARDUINO_BUILD_DIR"] = str(override)

    result = _run(
        [
            str(repo / ".github" / "scripts" / "tests_build.sh"),
            "-s",
            "sample",
            "-type",
            "validation",
            "-t",
            "esp32",
            "-j",
            str(jobs),
        ],
        repo,
        env,
    )

    if jobs == 1:
        assert result.returncode == 0, result.stderr
        assert (override / "cli_compile_output.txt").exists()
    else:
        assert result.returncode == 2
        assert "cannot be used with ARDUINO_BUILD_DIR" in result.stdout
        assert not Path(env["BUILD_TEST_STATE"]).exists()


def test_isolated_parallel_build_dirs_allow_shared_override_and_duplicate_names(
    tmp_path: Path,
) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [
        _make_sketch(repo / parent, "duplicate")
        for parent in ("first", "second", "third")
    ]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)
    shared_override = tmp_path / "shared-build"
    env["ARDUINO_BUILD_DIR"] = str(shared_override)

    result = _run(
        _build_list_command(repo, sketch_list, 3, isolated_build_dirs=True),
        repo,
        env,
    )

    assert result.returncode == 0, result.stderr
    state = _state(env)
    assert state["max_active"] == 3
    assert len({event["build_path"] for event in state["events"]}) == 3
    assert all(event["build_path"] != str(shared_override) for event in state["events"])
    assert not shared_override.exists()
    assert all(not Path(event["build_path"]).exists() for event in state["events"])
    assert all("build.slot." in event["build_path"] for event in state["events"])


def test_isolated_build_dirs_are_reused_per_worker_slot(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [_make_sketch(repo / "tests", f"sketch-{index}") for index in range(5)]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)

    result = _run(
        _build_list_command(repo, sketch_list, 2, isolated_build_dirs=True),
        repo,
        env,
    )

    assert result.returncode == 0, result.stderr
    state = _state(env)
    paths = {event["build_path"] for event in state["events"]}
    assert state["max_active"] == 2
    assert len(state["events"]) == 5
    assert len(paths) == 2
    assert all("build.slot." in path for path in paths)


def test_parallel_build_handles_paths_with_spaces(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [
        _make_sketch(repo / "tests with spaces", name)
        for name in ("first sketch", "second sketch")
    ]
    sketch_list = tmp_path / "sketch paths.list"
    _write_list(sketch_list, sketches)

    result = _run(_build_list_command(repo, sketch_list, 2), repo, env)

    assert result.returncode == 0, result.stderr
    assert {event["sketch"] for event in _state(env)["events"]} == {
        str(sketch) for sketch in sketches
    }


def test_existing_cmake_parallel_level_is_preserved(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [_make_sketch(repo / "tests", name) for name in ("one", "two")]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)
    env["CMAKE_BUILD_PARALLEL_LEVEL"] = "9"

    result = _run(_build_list_command(repo, sketch_list, 2), repo, env)

    assert result.returncode == 0, result.stderr
    assert [event["cmake_jobs"] for event in _state(env)["events"]] == ["9", "9"]


def test_automatic_inner_budget_keeps_full_cpu_count(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [_make_sketch(repo / "tests", name) for name in ("one", "two", "three")]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)
    _fake_logical_cpus(tmp_path, env, 8)

    result = _run(_build_list_command(repo, sketch_list, 3), repo, env)

    assert result.returncode == 0, result.stderr
    assert [event["cmake_jobs"] for event in _state(env)["events"]] == ["8", "8", "8"]
    assert "CMAKE_BUILD_PARALLEL_LEVEL" not in env


def test_logical_cpu_count_uses_windows_environment_fallback(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    fake_bin = tmp_path / "bin"
    fake_bin.mkdir()
    for command in ("getconf", "sysctl", "nproc"):
        executable = fake_bin / command
        executable.write_text("#!/bin/sh\nexit 1\n", encoding="utf-8")
        executable.chmod(0o755)
    env["PATH"] = f"{fake_bin}:{env['PATH']}"
    env["NUMBER_OF_PROCESSORS"] = "6"

    result = _run(
        [str(repo / ".github" / "scripts" / "sketch_utils.sh"), "logical_cpu_count"],
        repo,
        env,
    )

    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == "6"


@pytest.mark.parametrize("cpus,expected", [(1, 1), (2, 1), (3, 1), (4, 2), (8, 4)])
def test_auto_sketch_jobs_use_half_the_logical_cpus(
    tmp_path: Path, cpus: int, expected: int
) -> None:
    repo, env = _make_repo(tmp_path)
    _fake_logical_cpus(tmp_path, env, cpus)

    result = _run(
        [str(repo / ".github" / "scripts" / "sketch_utils.sh"), "auto_sketch_jobs"],
        repo,
        env,
    )

    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == str(expected)


def test_libraries_are_preinstalled_serially_once_before_workers(
    tmp_path: Path,
) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [
        _make_sketch(repo / "tests", "one", "libs:\n  - LibraryOne\n"),
        _make_sketch(repo / "tests", "two", "libs:\n  - LibraryTwo\n"),
    ]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)

    result = _run(_build_list_command(repo, sketch_list, 2), repo, env)

    assert result.returncode == 0, result.stderr
    install_state = _state(env, "INSTALL_TEST_STATE")
    assert install_state["max_active"] == 1
    assert install_state["items"] == ["LibraryOne", "LibraryTwo"]


def test_parallel_build_preserves_arduino_cli_backend(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = [_make_sketch(repo / "tests", name) for name in ("one", "two")]
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, sketches)
    command = _build_list_command(repo, sketch_list, 2)
    command.append("--arduino-cli")

    result = _run(command, repo, env)

    assert result.returncode == 0, result.stderr
    assert 1 < _state(env)["max_active"] <= 2
    assert "with arduino-cli" in result.stdout


def test_parallel_build_preserves_multi_fqbn_and_size_fragments(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    sketch = _make_sketch(
        repo / "tests",
        "multi-fqbn",
        "fqbn:\n"
        "  esp32:\n"
        "    - espressif:esp32:esp32:FlashFreq=40\n"
        "    - espressif:esp32:esp32:FlashFreq=80\n",
    )
    sketch_list = tmp_path / "sketches.list"
    _write_list(sketch_list, [sketch])
    command = _build_list_command(repo, sketch_list, 2)
    command.extend(["-l", "1"])

    result = _run(command, repo, env)

    assert result.returncode == 0, result.stderr
    build_root = tmp_path / "home" / ".arduino" / "tests" / "esp32" / "multi-fqbn"
    assert (build_root / "build0.tmp" / "cli_compile_output.txt").exists()
    assert (build_root / "build1.tmp" / "cli_compile_output.txt").exists()
    assert len(_state(env)["events"]) == 2
    sizes = (repo / "cli_compile_0.json").read_text(encoding="utf-8")
    assert sizes.count('"name":') == 2


def test_chunk_size_json_finalization_is_bsd_portable(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    validation = repo / "tests" / "validation"
    for name in ("one", "two"):
        _make_sketch(validation, name)

    result = _run(
        [
            str(repo / ".github" / "scripts" / "tests_build.sh"),
            "-c",
            "-type",
            "validation",
            "-t",
            "esp32",
            "-j",
            "2",
            "-l",
            "1",
        ],
        repo,
        env,
    )

    assert result.returncode == 0, result.stderr
    sizes = (repo / "cli_compile_0.json").read_text(encoding="utf-8")
    assert sizes.count('"name":') == 2
    assert "},\n]" not in sizes
    assert sizes.rstrip().endswith("},")


@pytest.mark.parametrize("mode", ["explicit", "chunk"])
def test_tests_build_parallelizes_explicit_and_chunk_modes(
    tmp_path: Path, mode: str
) -> None:
    repo, env = _make_repo(tmp_path)
    validation = repo / "tests" / "validation"
    for name in ("one", "two", "three"):
        _make_sketch(validation, name)

    command = [str(repo / ".github" / "scripts" / "tests_build.sh")]
    if mode == "explicit":
        command.extend(["-s", "one,two,three"])
    else:
        command.append("-c")
    command.extend(["-type", "validation", "-t", "esp32", "-j", "2"])

    result = _run(command, repo, env)

    assert result.returncode == 0, result.stderr
    state = _state(env)
    assert 1 < state["max_active"] <= 2
    assert len(state["events"]) == 3


def test_platform_filtering_happens_before_parallel_scheduling(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    validation = repo / "tests" / "validation"
    _make_sketch(validation, "blocked", "platforms:\n  hardware: false\n")
    allowed = _make_sketch(validation, "allowed", "platforms:\n  hardware: true\n")

    result = _run(
        [
            str(repo / ".github" / "scripts" / "tests_build.sh"),
            "-s",
            "blocked,allowed",
            "-type",
            "validation",
            "-t",
            "esp32",
            "-H",
            "-j",
            "2",
        ],
        repo,
        env,
    )

    assert result.returncode == 0, result.stderr
    assert [event["sketch"] for event in _state(env)["events"]] == [str(allowed)]


def test_tests_build_parses_known_options_after_passthrough_and_separator(
    tmp_path: Path,
) -> None:
    repo, env = _make_repo(tmp_path)
    validation = repo / "tests" / "validation"
    for name in ("one", "two"):
        _make_sketch(validation, name)

    result = _run(
        [
            str(repo / ".github" / "scripts" / "tests_build.sh"),
            "--compiler-flag",
            "compiler-value",
            "-d",
            "debug",
            "-s",
            "one,two",
            "-type",
            "validation",
            "-j",
            "2",
            "-t",
            "esp32",
            "--",
            "--ambiguous",
            "-t",
            "compiler-target",
        ],
        repo,
        env,
    )

    assert result.returncode == 0, result.stderr
    events = _state(env)["events"]
    assert len(events) == 2
    for event in events:
        args = event["args"]
        assert "DebugLevel=debug" in args[args.index("--fqbn") + 1]
        assert args[args.index("--compiler-flag") + 1] == "compiler-value"
        ambiguous_index = args.index("--ambiguous")
        assert args[ambiguous_index + 1 : ambiguous_index + 3] == [
            "-t",
            "compiler-target",
        ]


def test_on_push_parallelizes_duplicate_names_with_isolated_build_dirs(
    tmp_path: Path,
) -> None:
    repo, env = _make_repo(tmp_path)
    selected, sketches = _prepare_push_repo(repo)
    env["SKETCH_BUILD_JOBS"] = "3"
    compile_commands = tmp_path / "compile-commands"
    env["COMPILE_COMMANDS_DIR"] = str(compile_commands)

    result = _run(
        [
            str(repo / ".github" / "scripts" / "on-push.sh"),
            "0",
            "1",
            "1",
            "none",
            str(selected),
        ],
        repo,
        env,
        timeout=30,
    )

    assert result.returncode == 0, result.stderr
    state = _state(env)
    assert 1 < state["max_active"] <= 3
    assert {event["sketch"] for event in state["events"]} == {
        str(sketch) for sketch in sketches
    }
    assert len({event["build_path"] for event in state["events"]}) == 3
    assert all(
        event["build_path"] != str(Path(env["HOME"]) / ".arduino" / "build.tmp")
        for event in state["events"]
    )
    assert all(not Path(event["build_path"]).exists() for event in state["events"])
    assert len(list(compile_commands.glob("*.json"))) == len(sketches)

    sizes = json.loads((repo / "cli_compile_0.json").read_text(encoding="utf-8"))
    assert len(sizes["boards"]) == 1
    assert len(sizes["boards"][0]["sketches"]) == len(sketches)


@pytest.mark.parametrize("os_name", ["macos", "windows"])
def test_on_push_parallelizes_non_linux_presets(tmp_path: Path, os_name: str) -> None:
    repo, env = _make_repo(tmp_path)
    sketches = _prepare_non_linux_push_repo(repo, os_name)
    env["SKETCH_BUILD_JOBS"] = "2"

    result = _run(
        [str(repo / ".github" / "scripts" / "on-push.sh")],
        repo,
        env,
        timeout=30,
    )

    assert result.returncode == 0, result.stderr
    state = _state(env)
    assert state["max_active"] == 2
    assert {event["sketch"] for event in state["events"]} == {
        str(sketch) for sketch in sketches
    }
    assert len({event["build_path"] for event in state["events"]}) == 2
    assert all(not Path(event["build_path"]).exists() for event in state["events"])

    pref_args = [
        arg
        for event in state["events"]
        for arg in event["args"]
        if arg.startswith("-prefs=")
    ]
    if os_name == "windows":
        assert len(pref_args) == 2 * len(sketches)
        assert any("ctags/1.2.3" in arg for arg in pref_args)
        assert any("arduino-preprocessor/4.5.6" in arg for arg in pref_args)
    else:
        assert pref_args == []


@pytest.mark.parametrize("os_name", ["macos", "windows"])
def test_on_push_non_linux_jobs_one_preserves_shared_build_dir(
    tmp_path: Path, os_name: str
) -> None:
    repo, env = _make_repo(tmp_path)
    _prepare_non_linux_push_repo(repo, os_name)
    env["SKETCH_BUILD_JOBS"] = "1"

    result = _run(
        [str(repo / ".github" / "scripts" / "on-push.sh")],
        repo,
        env,
        timeout=30,
    )

    assert result.returncode == 0, result.stderr
    state = _state(env)
    assert state["max_active"] == 1
    assert {event["build_path"] for event in state["events"]} == {
        str(Path(env["HOME"]) / ".arduino" / "build.tmp")
    }


def test_on_push_auto_jobs_use_half_the_logical_cpus(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    selected, sketches = _prepare_push_repo(repo)
    env["SKETCH_BUILD_JOBS"] = "auto"
    _fake_logical_cpus(tmp_path, env, 4)

    result = _run(
        [
            str(repo / ".github" / "scripts" / "on-push.sh"),
            "0",
            "1",
            "0",
            "none",
            str(selected),
        ],
        repo,
        env,
        timeout=30,
    )

    assert result.returncode == 0, result.stderr
    assert _state(env)["max_active"] == 2
    assert "inner ninja 4" in result.stdout
    assert len(sketches) == 4


def test_on_push_jobs_one_preserves_shared_build_dir(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    selected, sketches = _prepare_push_repo(repo)
    env["SKETCH_BUILD_JOBS"] = "1"

    result = _run(
        [
            str(repo / ".github" / "scripts" / "on-push.sh"),
            "0",
            "1",
            "0",
            "none",
            str(selected),
        ],
        repo,
        env,
        timeout=30,
    )

    assert result.returncode == 0, result.stderr
    state = _state(env)
    assert state["max_active"] == 1
    assert len(state["events"]) == len(sketches)
    assert {event["build_path"] for event in state["events"]} == {
        str(Path(env["HOME"]) / ".arduino" / "build.tmp")
    }


def test_on_push_propagates_parallel_worker_failures(tmp_path: Path) -> None:
    repo, env = _make_repo(tmp_path)
    selected, sketches = _prepare_push_repo(repo)
    env.update({"SKETCH_BUILD_JOBS": "4", "BUILD_TEST_FAILURES": "Shared"})

    result = _run(
        [
            str(repo / ".github" / "scripts" / "on-push.sh"),
            "0",
            "1",
            "0",
            "none",
            str(selected),
        ],
        repo,
        env,
        timeout=30,
    )

    assert result.returncode == 7
    assert (
        f"Parallel build summary: 0 succeeded, {len(sketches)} failed" in result.stdout
    )
    assert result.stdout.count("FAILED:") == len(sketches)
    assert result.stdout.rindex("FAILED:") > result.stdout.rindex(
        "Parallel build summary:"
    )


@pytest.mark.parametrize("jobs", ["0", "-1", "invalid"])
def test_on_push_rejects_invalid_parallelism_before_building(
    tmp_path: Path, jobs: str
) -> None:
    repo, env = _make_repo(tmp_path)
    selected, _ = _prepare_push_repo(repo)
    env["SKETCH_BUILD_JOBS"] = jobs

    result = _run(
        [
            str(repo / ".github" / "scripts" / "on-push.sh"),
            "0",
            "1",
            "0",
            "none",
            str(selected),
        ],
        repo,
        env,
    )

    assert result.returncode == 2
    assert "SKETCH_BUILD_JOBS must be 'auto' or a positive integer" in result.stdout
    assert not Path(env["BUILD_TEST_STATE"]).exists()
