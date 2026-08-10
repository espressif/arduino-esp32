#!/usr/bin/env python3

import sys
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

from generate_missing_junits import (  # noqa: E402
    _platform_allowed,
    _test_enabled_for_target,
    expected_from_artifacts,
)


def test_platform_allowed_per_target_disable():
    ci = {
        "platforms": {
            "qemu": False,
            "hardware": {"esp32p4": False},
            "wokwi": {"esp32p4": False},
        }
    }
    assert _platform_allowed(ci, "qemu", "esp32p4") is False
    assert _platform_allowed(ci, "hardware", "esp32p4") is False
    assert _platform_allowed(ci, "wokwi", "esp32p4") is False
    assert _platform_allowed(ci, "wokwi", "esp32") is True


def test_test_enabled_for_target():
    ci = {"targets": {"esp32p4": False, "esp32": True}}
    assert _test_enabled_for_target(ci, "esp32p4") is False
    assert _test_enabled_for_target(ci, "esp32") is True
    assert _test_enabled_for_target({}, "esp32p4") is True


def test_expected_from_artifacts_honors_per_target_platform_disable():
    ci_yml = """\
platforms:
  qemu: false
  hardware:
    esp32p4: false
  wokwi:
    esp32p4: false
requires_any:
  - CONFIG_ESP_HOSTED_ENABLED=y
"""
    sdkconfig = 'CONFIG_ESP_HOSTED_ENABLED=y\nCONFIG_IDF_TARGET="esp32p4"\n'

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        build_tmp = root / "test-bin-esp32p4-validation" / "wifi" / "build.tmp"
        build_tmp.mkdir(parents=True)
        (build_tmp / "ci.yml").write_text(ci_yml, encoding="utf-8")
        (build_tmp / "sdkconfig").write_text(sdkconfig, encoding="utf-8")

        expected, built_tests = expected_from_artifacts(root)

    assert ("wokwi", "esp32p4", "validation", "wifi") not in expected
    assert ("hardware", "esp32p4", "validation", "wifi") not in expected
    assert expected == {}
    assert ("esp32p4", "validation", "wifi") in built_tests


def test_built_tests_blocks_cache_fallback():
    """Cache fallback must not re-add entries for tests that were built
    but excluded from a platform via per-target ci.yml disable."""
    ci_yml = """\
platforms:
  qemu: false
  hardware:
    esp32p4: false
  wokwi:
    esp32p4: false
requires_any:
  - CONFIG_ESP_HOSTED_ENABLED=y
"""
    sdkconfig = 'CONFIG_ESP_HOSTED_ENABLED=y\nCONFIG_IDF_TARGET="esp32p4"\n'

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        build_tmp = root / "test-bin-esp32p4-validation" / "wifi" / "build.tmp"
        build_tmp.mkdir(parents=True)
        (build_tmp / "ci.yml").write_text(ci_yml, encoding="utf-8")
        (build_tmp / "sdkconfig").write_text(sdkconfig, encoding="utf-8")

        expected_from_build, built_tests = expected_from_artifacts(root)

    cache_key = ("wokwi", "esp32p4", "validation", "wifi")
    assert cache_key not in expected_from_build
    _, target, test_type, sketch = cache_key
    assert (target, test_type, sketch) in built_tests, \
        "built_tests should prevent cache from re-adding this entry"


if __name__ == "__main__":
    test_platform_allowed_per_target_disable()
    test_test_enabled_for_target()
    test_expected_from_artifacts_honors_per_target_platform_disable()
    test_built_tests_blocks_cache_fallback()
    print("OK")
