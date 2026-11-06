#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
arduino-esp32 integration test: flasher.py + socket:// + platform upload recipe.

Protocol and esptool validation live in the esp32-mock-bootloader package:
https://github.com/espressif/esp32-mock-bootloader

Usage:
    pip install esp32-mock-bootloader
    python3 .github/scripts/ci_testing/test_mock_bootloader.py
"""

from __future__ import annotations

import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
FLASHER_SCRIPT = REPO_ROOT / 'tools' / 'flasher.py'

PASS = '\033[32mPASS\033[0m'
FAIL = '\033[31mFAIL\033[0m'


def _esptool_available() -> bool:
    if shutil.which('esptool'):
        return True
    try:
        subprocess.run(
            [sys.executable, '-m', 'esptool', 'version'],
            capture_output=True,
            timeout=5,
            check=False,
        )
        return True
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False


def _create_fake_binary(path: Path, size: int = 4096) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b'\x00' * size)
    return path


def _start_mock(port: int) -> None:
    subprocess.run(
        [
            'esp32-mock-bootloader', 'start',
            '--port', str(port),
            '--chip', 'esp32',
            '--startup-timeout', '60',
        ],
        check=True,
    )


def _stop_mock(port: int) -> None:
    subprocess.run(
        ['esp32-mock-bootloader', 'stop', '--port', str(port)],
        check=False,
    )


def test_flasher_socket_platform_recipe() -> bool:
    print('=' * 60)
    print('  Test – flasher.py + socket:// + platform upload recipe (twice)')
    print('=' * 60)

    if not FLASHER_SCRIPT.is_file():
        print('  SKIPPED (flasher.py not found)')
        return True
    if not _esptool_available():
        print('  SKIPPED (esptool not installed)')
        return True

    esptool = shutil.which('esptool')
    if not esptool:
        print('  SKIPPED (esptool binary not on PATH)')
        return True

    if not shutil.which('esp32-mock-bootloader'):
        print('  SKIPPED (esp32-mock-bootloader not installed)')
        return True

    port = 19824
    _start_mock(port)
    try:
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            bin_file = _create_fake_binary(build_dir / 'sketch.bin', 4096)
            port_url = f'socket://127.0.0.1:{port}'
            flasher_cmd = [
                sys.executable, str(FLASHER_SCRIPT),
                '--esptool', esptool,
                '--build-dir', str(build_dir),
                '--chip', 'esp32',
                '--port', port_url,
                '--baud', '115200',
                '--before', 'default-reset',
                '--after', 'hard-reset',
                'write-flash', '-z',
                '--flash-mode', 'keep', '--flash-freq', 'keep', '--flash-size', 'keep',
                '0x10000', str(bin_file),
            ]
            first = subprocess.run(
                flasher_cmd, capture_output=True, text=True, timeout=60,
            )
            ok = first.returncode == 0
            if not ok:
                print(f'  {FAIL}  first flasher upload')
                print(f'         rc={first.returncode}\nstdout: {first.stdout[-500:]}\n'
                      f'stderr: {first.stderr[-300:]}')
                return False
            print(f'  {PASS}  first flasher upload')

            time.sleep(0.5)
            second = subprocess.run(
                flasher_cmd, capture_output=True, text=True, timeout=60,
            )
            ok = second.returncode == 0
            if not ok:
                print(f'  {FAIL}  second flasher upload (diff-with path)')
                print(f'         rc={second.returncode}\nstdout: {second.stdout[-500:]}\n'
                      f'stderr: {second.stderr[-300:]}')
                return False
            print(f'  {PASS}  second flasher upload (diff-with path)')
            return True
    finally:
        _stop_mock(port)


if __name__ == '__main__':
    ok = test_flasher_socket_platform_recipe()
    print()
    if ok:
        print(f'{PASS} flasher integration test passed')
        sys.exit(0)
    print(f'{FAIL} flasher integration test failed')
    sys.exit(1)
