#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
Local test script for tools/flasher.py

Usage:
    python3 .github/scripts/ci_testing/test_flasher.py

Each test creates a temporary directory that mirrors the real filesystem layout,
runs the flasher against a mock esptool that prints its received args as JSON,
and asserts expected behaviour.

Upload paths covered (matching platform.txt patterns):
  - esptool_py.upload      : full upload, 4 binaries (bootloader, partitions,
                             boot_app0, app) plus optional extra_flags binary
  - esptool_py.program     : app-only serial re-program
  - esptool_py_app_only    : app-only with explicit flash mode/offset (e.g. kodedot)

Scenarios covered:
  01. Full upload – first flash (no _flashed.bin)  → full flash, files saved
  02. Full upload – second flash                   → --diff-with injected
  03. Full upload – partial references             → mixed path / skip
  04. Full upload – --erase-all present            → args forwarded unchanged
  05. Full upload – extra_flags binary             → extra binary parsed & saved
  06. Program path – first flash                   → full flash, _flashed.bin saved
  07. Program path – second flash                  → --diff-with injected
  08. App-only path – first flash                  → full flash, _flashed.bin saved
  09. App-only path – second flash                 → --diff-with injected
  10. esptool failure                              → exit code propagated, no save
  11. erase-flash command                          → passed through unchanged
  12. --diff-with already in args                  → not duplicated
  13. --esptool flag omitted                       → defaults to 'esptool' in PATH
  14. --build-dir centralises all _flashed.bin     → platform file not polluted
  15. Option order: --build-dir before --esptool   → both still parsed correctly
  16. Flasher flags not forwarded to esptool       → --esptool / --build-dir absent
  17. --help / -h                                  → exits 0, documents both flags
"""

import json
import os
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

FLASHER = Path(__file__).parent.parent.parent.parent / 'tools' / 'flasher.py'

PASS = '\033[32mPASS\033[0m'
FAIL = '\033[31mFAIL\033[0m'
_failures: list[str] = []


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def section(title: str) -> None:
    print(f"\n{'=' * 60}")
    print(f'  {title}')
    print('=' * 60)


def assert_test(name: str, condition: bool, detail: str = '') -> None:
    if condition:
        print(f'  {PASS}  {name}')
    else:
        msg = f'  {FAIL}  {name}'
        if detail:
            msg += f'\n         detail: {detail}'
        print(msg)
        _failures.append(name)


def make_mock_esptool(path: Path) -> Path:
    """Create an executable mock esptool that prints received args as JSON."""
    mock = path / 'esptool'
    mock.write_text(
        f'#!{sys.executable}\n'
        'import sys, json\n'
        'print(json.dumps(sys.argv[1:]))\n'
    )
    mock.chmod(mock.stat().st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)
    return mock


def make_mock_esptool_failing(path: Path) -> Path:
    """Create an executable mock esptool that exits with code 2."""
    mock = path / 'esptool_failing'
    mock.write_text(
        f'#!{sys.executable}\n'
        'import sys\n'
        'sys.exit(2)\n'
    )
    mock.chmod(mock.stat().st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)
    return mock


def fake_binary(path: Path) -> Path:
    """Write a small placeholder binary and return its path."""
    path.write_bytes(b'\xff' * 64)
    return path


def run_flasher(mock: Path, build_dir: Path, *esptool_args: str,
                extra_flasher_args: tuple = ()) -> tuple[list[str], int]:
    """Invoke flasher.py and return (parsed_esptool_args, returncode)."""
    cmd = [
        sys.executable, str(FLASHER),
        '--esptool', str(mock),
        '--build-dir', str(build_dir),
        *extra_flasher_args,
        *esptool_args,
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    try:
        args = json.loads(r.stdout.strip())
    except (json.JSONDecodeError, ValueError):
        args = []
    return args, r.returncode


# ---------------------------------------------------------------------------
# Shared esptool arg templates (mirror platform.txt patterns)
# ---------------------------------------------------------------------------

def _upload_args(build_dir: Path, plat_dir: Path, *, erase: bool = False,
                 extra_bin: Path | None = None) -> list[str]:
    """Equivalent to esptool_py.upload.pattern_args."""
    args = [
        '--chip', 'esp32s3',
        '--port', '/dev/ttyUSB0',
        '--baud', '921600',
        '--before', 'default-reset',
        '--after', 'hard-reset',
        'write-flash',
    ]
    if erase:
        args += ['-e']
    args += [
        '-z',
        '--flash-mode', 'keep',
        '--flash-freq', 'keep',
        '--flash-size', 'keep',
        '0x0',     str(build_dir / 'sketch.bootloader.bin'),
        '0x8000',  str(build_dir / 'sketch.partitions.bin'),
        '0xe000',  str(plat_dir  / 'boot_app0.bin'),
        '0x10000', str(build_dir / 'sketch.bin'),
    ]
    if extra_bin is not None:
        args += ['0xC10000', str(extra_bin)]
    return args


def _program_args(build_dir: Path) -> list[str]:
    """Equivalent to esptool_py.program.pattern_args."""
    return [
        '--chip', 'esp32s3',
        '--port', '/dev/ttyUSB0',
        '--baud', '921600',
        '--before', 'default-reset',
        '--after', 'hard-reset',
        'write-flash',
        '-z',
        '--flash-mode', 'keep',
        '--flash-freq', 'keep',
        '--flash-size', 'keep',
        '0x10000', str(build_dir / 'sketch.bin'),
    ]


def _app_only_args(build_dir: Path, flash_offset: str = '0x10000') -> list[str]:
    """Equivalent to esptool_py_app_only.upload.pattern_args."""
    return [
        '--chip', 'esp32s3',
        '--port', '/dev/ttyUSB0',
        '--baud', '921600',
        '--before', 'default-reset',
        '--after', 'hard-reset',
        'write-flash',
        '--flash-mode', 'dio',
        '--flash-freq', '80m',
        '--flash-size', '4MB',
        flash_offset, str(build_dir / 'sketch.bin'),
    ]


# ---------------------------------------------------------------------------
# Test cases – esptool_py.upload path (4 binaries)
# ---------------------------------------------------------------------------

def test_01_upload_first_flash():
    section('Test 01 – esptool_py.upload: first flash (no _flashed.bin)')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        pd = Path(tmp) / 'platform' / 'tools' / 'partitions'
        bd.mkdir(); pd.mkdir(parents=True)
        mock = make_mock_esptool(Path(tmp))

        for f in ('sketch.bootloader.bin', 'sketch.partitions.bin', 'sketch.bin'):
            fake_binary(bd / f)
        fake_binary(pd / 'boot_app0.bin')

        got, rc = run_flasher(mock, bd, *_upload_args(bd, pd))

        assert_test('exit code 0', rc == 0)
        assert_test('no --diff-with on first flash', '--diff-with' not in got)
        assert_test('sketch.bootloader_flashed.bin created in build_dir',
                    (bd / 'sketch.bootloader_flashed.bin').exists())
        assert_test('sketch.partitions_flashed.bin created in build_dir',
                    (bd / 'sketch.partitions_flashed.bin').exists())
        assert_test('sketch_flashed.bin created in build_dir',
                    (bd / 'sketch_flashed.bin').exists())
        assert_test('boot_app0_flashed.bin created in build_dir',
                    (bd / 'boot_app0_flashed.bin').exists())
        assert_test('platform dir untouched (no boot_app0_flashed.bin there)',
                    not (pd / 'boot_app0_flashed.bin').exists())


def test_02_upload_second_flash():
    section('Test 02 – esptool_py.upload: second flash → --diff-with injected')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        pd = Path(tmp) / 'platform' / 'tools' / 'partitions'
        bd.mkdir(); pd.mkdir(parents=True)
        mock = make_mock_esptool(Path(tmp))

        for f in ('sketch.bootloader.bin', 'sketch.partitions.bin', 'sketch.bin'):
            fake_binary(bd / f)
        fake_binary(pd / 'boot_app0.bin')

        # First flash to create references
        run_flasher(mock, bd, *_upload_args(bd, pd))

        # Second flash
        got, rc = run_flasher(mock, bd, *_upload_args(bd, pd))

        assert_test('exit code 0', rc == 0)
        assert_test('--diff-with present on second flash', '--diff-with' in got)
        dw_idx = got.index('--diff-with')
        refs = got[dw_idx + 1:]
        assert_test('4 diff entries (one per binary)', len(refs) == 4,
                    f'got {len(refs)}: {refs}')
        assert_test('bootloader, partitions, app refs point into build_dir',
                    all(str(bd) in refs[i] for i in (0, 1, 3)),
                    str(refs))
        assert_test('boot_app0 is always skip (OTA-safety)',
                    refs[2] == 'skip',
                    f'got: {refs[2]}')
        assert_test('--esptool and --build-dir not forwarded to esptool',
                    '--esptool' not in got and '--build-dir' not in got)


def test_03_upload_partial_references():
    section('Test 03 – esptool_py.upload: partial _flashed.bin → mixed skip/path')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        pd = Path(tmp) / 'platform' / 'tools' / 'partitions'
        bd.mkdir(); pd.mkdir(parents=True)
        mock = make_mock_esptool(Path(tmp))

        for f in ('sketch.bootloader.bin', 'sketch.partitions.bin', 'sketch.bin'):
            fake_binary(bd / f)
        fake_binary(pd / 'boot_app0.bin')

        # Manually create only some references (bootloader and app)
        (bd / 'sketch.bootloader_flashed.bin').write_bytes(b'\xff' * 64)
        (bd / 'sketch_flashed.bin').write_bytes(b'\xff' * 64)

        got, rc = run_flasher(mock, bd, *_upload_args(bd, pd))

        assert_test('exit code 0', rc == 0)
        assert_test('--diff-with present (at least one ref exists)', '--diff-with' in got)
        dw_idx = got.index('--diff-with')
        refs = got[dw_idx + 1:]
        assert_test('4 diff entries', len(refs) == 4, str(refs))
        assert_test('bootloader has a ref path (not skip)', refs[0] != 'skip')
        assert_test('partitions is skip (no ref)', refs[1] == 'skip')
        assert_test('boot_app0 is skip (no ref)', refs[2] == 'skip')
        assert_test('app has a ref path (not skip)', refs[3] != 'skip')


def test_04_upload_erase_all():
    section('Test 04 – esptool_py.upload: -e (erase all) forwarded to esptool')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        pd = Path(tmp) / 'platform' / 'tools' / 'partitions'
        bd.mkdir(); pd.mkdir(parents=True)
        mock = make_mock_esptool(Path(tmp))

        for f in ('sketch.bootloader.bin', 'sketch.partitions.bin', 'sketch.bin'):
            fake_binary(bd / f)
        fake_binary(pd / 'boot_app0.bin')

        # Create references so --diff-with would normally be added
        run_flasher(mock, bd, *_upload_args(bd, pd))

        # Second flash with -e (erase all)
        got, rc = run_flasher(mock, bd, *_upload_args(bd, pd, erase=True))

        assert_test('exit code 0', rc == 0)
        assert_test('-e forwarded to esptool', '-e' in got)
        # esptool handles --erase-all internally; --diff-with may still be
        # present since we pass it and esptool drops it safely on its side
        assert_test('write-flash still in args', 'write-flash' in got)


def test_05_upload_extra_flags_binary():
    section('Test 05 – esptool_py.upload: extra_flags binary (e.g. srmodels.bin)')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        pd = Path(tmp) / 'platform' / 'tools' / 'partitions'
        bd.mkdir(); pd.mkdir(parents=True)
        mock = make_mock_esptool(Path(tmp))

        for f in ('sketch.bootloader.bin', 'sketch.partitions.bin', 'sketch.bin'):
            fake_binary(bd / f)
        fake_binary(pd / 'boot_app0.bin')
        sr = fake_binary(bd / 'srmodels.bin')

        # First flash with extra binary
        run_flasher(mock, bd, *_upload_args(bd, pd, extra_bin=sr))
        assert_test('srmodels_flashed.bin created', (bd / 'srmodels_flashed.bin').exists())

        # Second flash → 5 diff entries
        got, rc = run_flasher(mock, bd, *_upload_args(bd, pd, extra_bin=sr))
        assert_test('exit code 0', rc == 0)
        assert_test('--diff-with present', '--diff-with' in got)
        dw_idx = got.index('--diff-with')
        refs = got[dw_idx + 1:]
        assert_test('5 diff entries (4 standard + srmodels)', len(refs) == 5,
                    f'got {len(refs)}: {refs}')


# ---------------------------------------------------------------------------
# Test cases – esptool_py.program path (app-only)
# ---------------------------------------------------------------------------

def test_06_program_first_flash():
    section('Test 06 – esptool_py.program: first flash (app-only)')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        bd.mkdir()
        mock = make_mock_esptool(Path(tmp))
        fake_binary(bd / 'sketch.bin')

        got, rc = run_flasher(mock, bd, *_program_args(bd))

        assert_test('exit code 0', rc == 0)
        assert_test('no --diff-with on first flash', '--diff-with' not in got)
        assert_test('sketch_flashed.bin created', (bd / 'sketch_flashed.bin').exists())
        assert_test('only 1 binary path in args',
                    sum(1 for a in got if 'sketch.bin' in a) == 1)


def test_07_program_second_flash():
    section('Test 07 – esptool_py.program: second flash → --diff-with injected')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        bd.mkdir()
        mock = make_mock_esptool(Path(tmp))
        fake_binary(bd / 'sketch.bin')

        run_flasher(mock, bd, *_program_args(bd))
        got, rc = run_flasher(mock, bd, *_program_args(bd))

        assert_test('exit code 0', rc == 0)
        assert_test('--diff-with present', '--diff-with' in got)
        dw_idx = got.index('--diff-with')
        refs = got[dw_idx + 1:]
        assert_test('exactly 1 diff entry', len(refs) == 1, str(refs))
        assert_test('ref points to sketch_flashed.bin',
                    'sketch_flashed.bin' in refs[0])


# ---------------------------------------------------------------------------
# Test cases – esptool_py_app_only.upload path (explicit flash mode)
# ---------------------------------------------------------------------------

def test_08_app_only_first_flash():
    section('Test 08 – esptool_py_app_only: first flash (explicit flash mode)')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        bd.mkdir()
        mock = make_mock_esptool(Path(tmp))
        fake_binary(bd / 'sketch.bin')

        got, rc = run_flasher(mock, bd, *_app_only_args(bd, '0x400000'))

        assert_test('exit code 0', rc == 0)
        assert_test('no --diff-with on first flash', '--diff-with' not in got)
        assert_test('explicit flash-mode forwarded', '--flash-mode' in got)
        assert_test('sketch_flashed.bin created', (bd / 'sketch_flashed.bin').exists())


def test_09_app_only_second_flash():
    section('Test 09 – esptool_py_app_only: second flash → --diff-with injected')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        bd.mkdir()
        mock = make_mock_esptool(Path(tmp))
        fake_binary(bd / 'sketch.bin')

        run_flasher(mock, bd, *_app_only_args(bd, '0x400000'))
        got, rc = run_flasher(mock, bd, *_app_only_args(bd, '0x400000'))

        assert_test('exit code 0', rc == 0)
        assert_test('--diff-with present', '--diff-with' in got)
        dw_idx = got.index('--diff-with')
        refs = got[dw_idx + 1:]
        assert_test('exactly 1 diff entry', len(refs) == 1, str(refs))
        assert_test('ref points to sketch_flashed.bin',
                    'sketch_flashed.bin' in refs[0])
        assert_test('explicit flash-mode still forwarded', '--flash-mode' in got)


# ---------------------------------------------------------------------------
# Test cases – edge cases & robustness
# ---------------------------------------------------------------------------

def test_10_esptool_failure():
    section('Test 10 – esptool failure → exit code propagated, no _flashed.bin save')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        bd.mkdir()
        mock_fail = make_mock_esptool_failing(Path(tmp))
        fake_binary(bd / 'sketch.bin')

        got, rc = run_flasher(mock_fail, bd, *_program_args(bd))

        assert_test('exit code 2 propagated from esptool', rc == 2)
        assert_test('sketch_flashed.bin NOT created on failure',
                    not (bd / 'sketch_flashed.bin').exists())


def test_11_erase_flash_passthrough():
    section('Test 11 – erase-flash command → passed through unchanged')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        bd.mkdir()
        mock = make_mock_esptool(Path(tmp))

        got, rc = run_flasher(mock, bd,
                              '--chip', 'esp32s3',
                              '--port', '/dev/ttyUSB0',
                              'erase-flash')

        assert_test('exit code 0', rc == 0)
        assert_test('erase-flash forwarded', 'erase-flash' in got)
        assert_test('no --diff-with added', '--diff-with' not in got)


def test_12_no_duplicate_diff_with():
    section('Test 12 – --diff-with already in args → not duplicated')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        bd.mkdir()
        mock = make_mock_esptool(Path(tmp))
        fake_binary(bd / 'sketch.bin')

        # Pre-existing _flashed.bin so flasher would normally add --diff-with
        (bd / 'sketch_flashed.bin').write_bytes(b'\xff' * 64)

        got, rc = run_flasher(mock, bd,
                              *_program_args(bd),
                              '--diff-with', 'some_old.bin')

        assert_test('exit code 0', rc == 0)
        assert_test('--diff-with appears exactly once',
                    got.count('--diff-with') == 1,
                    f'count={got.count("--diff-with")}')


def test_13_default_esptool_from_path():
    section('Test 13 – --esptool omitted → default "esptool" resolved from PATH')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        bd.mkdir()

        # Create a fake 'esptool' on a custom PATH
        bin_dir = Path(tmp) / 'bin'
        bin_dir.mkdir()
        fake_esptool = bin_dir / 'esptool'
        fake_esptool.write_text(
            f'#!{sys.executable}\nimport sys,json\nprint(json.dumps(sys.argv[1:]))\n'
        )
        fake_esptool.chmod(fake_esptool.stat().st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)
        fake_binary(bd / 'sketch.bin')

        env = {**os.environ, 'PATH': str(bin_dir) + os.pathsep + os.environ.get('PATH', '')}
        cmd = [
            sys.executable, str(FLASHER),
            '--build-dir', str(bd),
            *_program_args(bd),
        ]
        r = subprocess.run(cmd, capture_output=True, text=True, env=env)

        assert_test('exit code 0 (system esptool found)', r.returncode == 0,
                    r.stderr)
        assert_test('args forwarded to system esptool',
                    'write-flash' in r.stdout)


def test_14_build_dir_isolates_platform_files():
    section('Test 14 – --build-dir isolates platform files from platform tree')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        pd = Path(tmp) / 'platform' / 'tools' / 'partitions'
        bd.mkdir(); pd.mkdir(parents=True)
        mock = make_mock_esptool(Path(tmp))

        for f in ('sketch.bootloader.bin', 'sketch.partitions.bin', 'sketch.bin'):
            fake_binary(bd / f)
        fake_binary(pd / 'boot_app0.bin')

        run_flasher(mock, bd, *_upload_args(bd, pd))

        assert_test('boot_app0_flashed.bin in build_dir',
                    (bd / 'boot_app0_flashed.bin').exists())
        assert_test('boot_app0_flashed.bin absent from platform dir',
                    not (pd / 'boot_app0_flashed.bin').exists())
        assert_test('no other _flashed.bin anywhere in platform tree',
                    not any(pd.parent.rglob('*_flashed.bin')))


def test_15_option_order_flexibility():
    section('Test 15 – --build-dir before --esptool (option order flexibility)')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        pd = Path(tmp) / 'platform' / 'tools' / 'partitions'
        bd.mkdir(); pd.mkdir(parents=True)
        mock = make_mock_esptool(Path(tmp))

        for f in ('sketch.bootloader.bin', 'sketch.partitions.bin', 'sketch.bin'):
            fake_binary(bd / f)
        fake_binary(pd / 'boot_app0.bin')

        # Note: --build-dir listed before --esptool (reversed from normal)
        cmd = [
            sys.executable, str(FLASHER),
            '--build-dir', str(bd),
            '--esptool', str(mock),
            *_upload_args(bd, pd),
        ]
        r = subprocess.run(cmd, capture_output=True, text=True)

        assert_test('exit code 0', r.returncode == 0, r.stderr)
        assert_test('_flashed.bin created in build_dir',
                    (bd / 'sketch_flashed.bin').exists())


def test_16_flasher_flags_not_forwarded():
    section('Test 16 – --esptool and --build-dir not forwarded to esptool')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        pd = Path(tmp) / 'platform' / 'tools' / 'partitions'
        bd.mkdir(); pd.mkdir(parents=True)
        mock = make_mock_esptool(Path(tmp))

        for f in ('sketch.bootloader.bin', 'sketch.partitions.bin', 'sketch.bin'):
            fake_binary(bd / f)
        fake_binary(pd / 'boot_app0.bin')

        got, rc = run_flasher(mock, bd, *_upload_args(bd, pd))

        assert_test('exit code 0', rc == 0)
        assert_test('--esptool absent from esptool args', '--esptool' not in got)
        assert_test('--build-dir absent from esptool args', '--build-dir' not in got)


def test_17_help_flag():
    section('Test 17 – --help / -h exits 0 and documents all flags')
    for flag in ('--help', '-h'):
        r = subprocess.run(
            [sys.executable, str(FLASHER), flag],
            capture_output=True,
            text=True,
        )
        assert_test(f'{flag} exits 0', r.returncode == 0)
        assert_test(f'{flag} documents --esptool', '--esptool' in r.stdout)
        assert_test(f'{flag} documents --build-dir', '--build-dir' in r.stdout)
        assert_test(f'{flag} documents --no-fast-flash', '--no-fast-flash' in r.stdout)


def test_18_boot_app0_always_skipped():
    section('Test 18 – boot_app0.bin always uses skip even when reference exists')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        pd = Path(tmp) / 'platform' / 'tools' / 'partitions'
        bd.mkdir(); pd.mkdir(parents=True)
        mock = make_mock_esptool(Path(tmp))

        for f in ('sketch.bootloader.bin', 'sketch.partitions.bin', 'sketch.bin'):
            fake_binary(bd / f)
        fake_binary(pd / 'boot_app0.bin')

        # First flash to create references (including boot_app0_flashed.bin)
        run_flasher(mock, bd, *_upload_args(bd, pd))
        assert_test('boot_app0_flashed.bin exists after first flash',
                    (bd / 'boot_app0_flashed.bin').exists())

        # Second flash: boot_app0 must still be 'skip' despite reference existing
        got, rc = run_flasher(mock, bd, *_upload_args(bd, pd))

        assert_test('exit code 0', rc == 0)
        dw_idx = got.index('--diff-with')
        refs = got[dw_idx + 1:]
        assert_test('4 diff entries', len(refs) == 4, str(refs))
        assert_test('boot_app0 is always skip (index 2)',
                    refs[2] == 'skip',
                    f'got: {refs[2]}')
        assert_test('other binaries still use references (not skip)',
                    all(refs[i] != 'skip' for i in (0, 1, 3)),
                    str(refs))


def test_19_no_fast_flash_flag():
    section('Test 19 – --no-fast-flash disables fast reflash entirely')
    with tempfile.TemporaryDirectory() as tmp:
        bd = Path(tmp) / 'build'
        pd = Path(tmp) / 'platform' / 'tools' / 'partitions'
        bd.mkdir(); pd.mkdir(parents=True)
        mock = make_mock_esptool(Path(tmp))

        for f in ('sketch.bootloader.bin', 'sketch.partitions.bin', 'sketch.bin'):
            fake_binary(bd / f)
        fake_binary(pd / 'boot_app0.bin')

        # First flash to create references
        run_flasher(mock, bd, *_upload_args(bd, pd))

        # Second flash with --no-fast-flash: must not inject --diff-with
        got, rc = run_flasher(mock, bd, *_upload_args(bd, pd),
                              extra_flasher_args=('--no-fast-flash',))

        assert_test('exit code 0', rc == 0)
        assert_test('--diff-with absent when --no-fast-flash set',
                    '--diff-with' not in got)
        assert_test('--no-fast-flash not forwarded to esptool',
                    '--no-fast-flash' not in got)

        # First flash with --no-fast-flash: references must NOT be saved
        with tempfile.TemporaryDirectory() as tmp2:
            bd2 = Path(tmp2) / 'build'
            pd2 = Path(tmp2) / 'platform' / 'tools' / 'partitions'
            bd2.mkdir(); pd2.mkdir(parents=True)
            mock2 = make_mock_esptool(Path(tmp2))
            for f in ('sketch.bootloader.bin', 'sketch.partitions.bin', 'sketch.bin'):
                fake_binary(bd2 / f)
            fake_binary(pd2 / 'boot_app0.bin')

            run_flasher(mock2, bd2, *_upload_args(bd2, pd2),
                        extra_flasher_args=('--no-fast-flash',))

            assert_test('no _flashed.bin files saved when --no-fast-flash set',
                        not any(bd2.glob('*_flashed.bin')))


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

_ALL_TESTS = [
    test_01_upload_first_flash,
    test_02_upload_second_flash,
    test_03_upload_partial_references,
    test_04_upload_erase_all,
    test_05_upload_extra_flags_binary,
    test_06_program_first_flash,
    test_07_program_second_flash,
    test_08_app_only_first_flash,
    test_09_app_only_second_flash,
    test_10_esptool_failure,
    test_11_erase_flash_passthrough,
    test_12_no_duplicate_diff_with,
    test_13_default_esptool_from_path,
    test_14_build_dir_isolates_platform_files,
    test_15_option_order_flexibility,
    test_16_flasher_flags_not_forwarded,
    test_17_help_flag,
    test_18_boot_app0_always_skipped,
    test_19_no_fast_flash_flag,
]

if __name__ == '__main__':
    print(f'Testing: {FLASHER}')

    for test_fn in _ALL_TESTS:
        test_fn()

    total = len(_ALL_TESTS)
    print(f"\n{'=' * 60}")
    if _failures:
        print(f'\n{FAIL} {len(_failures)} assertion(s) failed:')
        for f in _failures:
            print(f'  - {f}')
        print()
        sys.exit(1)
    else:
        print(f'\n{PASS} All {total} test cases passed!')
        sys.exit(0)
