"""Library compile costs: file-count prior, then measured times.

This is learning-augmented scheduling (predictions feed P||Cmax / LPT).
Seeds, ``# of TUs × sec/file``, and prebuilt ``.a`` size are the predictor. CI timings are
observations. A source fingerprint detects concept drift when a library
changes: stale measurements are blended back toward the prior instead of
trusted.

Units are seconds. A bin pays ``cold`` once per library that appears in it,
then ``warm`` for extra sketches that use that library, plus a small
per-sketch remainder.
"""

from __future__ import annotations

import fnmatch
import hashlib
import json
import os
from pathlib import Path

# First-run seeds (Linux CI samples) used only when the library tree is absent
# or has no countable sources.
SEED_COLD: dict[str, int] = {
    "Matter": 120,
    "Insights": 80,
    "RainMaker": 70,
    "BLE": 50,
    "Zigbee": 45,
    "OpenThread": 40,
    "ESP_SR": 40,
    "ESP_Video": 40,
    "HTTPClient": 25,
    "HTTPUpdate": 25,
    "HTTPUpdateServer": 25,
    "WebServer": 25,
    "ArduinoOTA": 25,
    "Update": 25,
    "SD": 20,
    "SD_MMC": 20,
    "FFat": 20,
    "LittleFS": 20,
    "SPIFFS": 20,
    "NetworkClientSecure": 20,
    "WiFi": 15,
    "Ethernet": 15,
    "PPP": 15,
    "WiFiProv": 15,
    "BluetoothSerial": 15,
    "ESP_NOW": 12,
    "Network": 10,
}

SEED_WARM: dict[str, int] = {
    "Matter": 20,
    "Insights": 15,
    "RainMaker": 12,
    "BLE": 8,
    "Zigbee": 8,
    "OpenThread": 8,
    "ESP_SR": 8,
    "ESP_Video": 8,
    "HTTPClient": 6,
    "HTTPUpdate": 6,
    "HTTPUpdateServer": 6,
    "WebServer": 6,
    "ArduinoOTA": 6,
    "Update": 6,
    "SD": 5,
    "SD_MMC": 5,
    "FFat": 5,
    "LittleFS": 5,
    "SPIFFS": 5,
    "NetworkClientSecure": 5,
    "WiFi": 4,
    "Ethernet": 4,
    "PPP": 4,
    "WiFiProv": 4,
    "BluetoothSerial": 4,
    "ESP_NOW": 3,
    "Network": 3,
}

# Backward-compatible names for the seed tables.
COLD = SEED_COLD
WARM = SEED_WARM

DEFAULT_COLD = 8
DEFAULT_WARM = 2
SKETCH_REMAINDER = 3
SEC_PER_SOURCE = 0.35
SEC_PER_ARCHIVE_MB = 4.0
EMA_ALPHA = 0.4
CACHED_COLD_FRACTION = 0.4
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".S", ".s"}
# Prebuilt archives under tools/esp32-arduino-libs/<target>/lib that dominate
# compile/link time. Wrapper .cpp counts miss these.
STATIC_ARCHIVE_PATTERNS: dict[str, tuple[str, ...]] = {
    "Matter": ("*matter*",),
    "BLE": ("libbt.a", "*nimble*"),
    "Zigbee": ("*zigbee*",),
    "RainMaker": ("*rainmaker*",),
    "OpenThread": ("*openthread*",),
    "Insights": ("*insights*",),
    "ESP_SR": ("*esp_sr*", "*esp-sr*"),
    "ESP_Video": ("*esp_video*", "*esp-video*"),
}


def _library_src_root(project_root: Path | None, library: str) -> Path | None:
    if project_root is None or not library:
        return None
    src = Path(project_root) / "libraries" / library / "src"
    return src if src.is_dir() else None


def source_count(library: str, project_root: Path | None) -> int:
    src = _library_src_root(project_root, library)
    if src is None:
        return 0
    return sum(1 for path in src.rglob("*") if path.suffix in SOURCE_SUFFIXES and path.is_file())


def _iter_static_archives(
    library: str, project_root: Path, target: str | None = None
):
    patterns = STATIC_ARCHIVE_PATTERNS.get(library)
    if not patterns:
        return
    libs_root = Path(project_root) / "tools" / "esp32-arduino-libs"
    if not libs_root.is_dir():
        return
    targets = [libs_root / target] if target else sorted(libs_root.iterdir())
    seen: set[str] = set()
    for target_dir in targets:
        libdir = target_dir / "lib"
        if not libdir.is_dir():
            continue
        for archive in sorted(libdir.glob("*.a")):
            name = archive.name
            if not any(fnmatch.fnmatch(name, pattern) for pattern in patterns):
                continue
            key = f"{target_dir.name}/{name}"
            if key in seen:
                continue
            seen.add(key)
            yield archive


def static_archive_bytes(
    library: str, project_root: Path | None, target: str = "esp32"
) -> int:
    if project_root is None or not library:
        return 0
    return sum(
        path.stat().st_size for path in _iter_static_archives(library, Path(project_root), target)
    )


def library_fingerprint(library: str, project_root: Path | None) -> str:
    """Hash of wrapper sources, ``library.properties``, and prebuilt archive sizes."""
    if project_root is None or not library:
        return ""
    lib_root = Path(project_root) / "libraries" / library
    hasher = hashlib.sha256()
    hashed = False
    files: list[Path] = []
    if lib_root.is_dir():
        prop = lib_root / "library.properties"
        if prop.is_file():
            files.append(prop)
        src = lib_root / "src"
        if src.is_dir():
            files.extend(
                path
                for path in src.rglob("*")
                if path.is_file() and path.suffix in SOURCE_SUFFIXES | {".h", ".hpp", ".hh"}
            )
    for path in sorted(files, key=lambda item: item.as_posix()):
        rel = path.relative_to(lib_root).as_posix().encode()
        hasher.update(rel)
        hasher.update(b"\0")
        try:
            hasher.update(path.read_bytes())
        except OSError:
            continue
        hasher.update(b"\0")
        hashed = True
    for archive in _iter_static_archives(library, Path(project_root)):
        hasher.update(archive.as_posix().encode())
        hasher.update(b"\0")
        try:
            hasher.update(str(archive.stat().st_size).encode())
        except OSError:
            continue
        hasher.update(b"\0")
        hashed = True
    if not hashed:
        return ""
    return hasher.hexdigest()[:16]


def prior_cold(library: str, project_root: Path | None = None) -> int:
    """max(seed, wrapper TUs, prebuilt archive size). Never undershoot a known-heavy lib."""
    seeded = SEED_COLD.get(library, DEFAULT_COLD)
    count = source_count(library, project_root)
    file_prior = (
        max(DEFAULT_COLD, int(round(SEC_PER_SOURCE * count))) if count else DEFAULT_COLD
    )
    size = static_archive_bytes(library, project_root)
    archive_prior = (
        max(DEFAULT_COLD, int(round(SEC_PER_ARCHIVE_MB * size / 1_000_000)))
        if size
        else DEFAULT_COLD
    )
    return max(seeded, file_prior, archive_prior)


def prior_warm(library: str, project_root: Path | None = None) -> int:
    cold = prior_cold(library, project_root)
    return max(SEED_WARM.get(library, DEFAULT_WARM), cold // 6, DEFAULT_WARM)


class CostTable:
    """Measured library costs keyed by source fingerprint."""

    def __init__(self, project_root: Path | None = None, entries: dict | None = None):
        self.project_root = Path(project_root) if project_root is not None else None
        self.entries: dict[str, dict] = dict(entries or {})
        self._fp_cache: dict[str, str] = {}

    def fingerprint(self, library: str) -> str:
        cached = self._fp_cache.get(library)
        if cached is None:
            cached = library_fingerprint(library, self.project_root)
            self._fp_cache[library] = cached
        return cached

    def cold(self, library: str) -> int:
        return int(self._lookup(library)[0])

    def warm(self, library: str) -> int:
        return int(self._lookup(library)[1])

    def _lookup(self, library: str) -> tuple[int, int]:
        prior_c = prior_cold(library, self.project_root)
        prior_w = prior_warm(library, self.project_root)
        entry = self.entries.get(library)
        if not entry:
            return prior_c, prior_w
        current_fp = self.fingerprint(library)
        stored_fp = str(entry.get("fingerprint") or "")
        stored_c = int(entry.get("cold", prior_c))
        stored_w = int(entry.get("warm", prior_w))
        if current_fp and stored_fp == current_fp:
            return stored_c, stored_w
        if stored_fp and current_fp and stored_fp != current_fp:
            # Concept drift: library sources changed. Pull toward the new prior.
            return (
                int(round(0.3 * stored_c + 0.7 * prior_c)),
                int(round(0.3 * stored_w + 0.7 * prior_w)),
            )
        return stored_c, stored_w

    def update_from_timings(self, records: list[dict]) -> None:
        from ci_chunking.affinity import CORE_BUCKET, OTHER_BUCKET, library_of_path

        samples: dict[str, list[int]] = {}
        for record in records:
            try:
                seconds = int(round(float(record.get("seconds") or 0)))
            except (TypeError, ValueError):
                continue
            if seconds <= 0:
                continue
            path = str(record.get("path") or "")
            lib = library_of_path(path)
            if lib in (CORE_BUCKET, OTHER_BUCKET, ""):
                continue
            samples.setdefault(lib, []).append(seconds)
        for lib, times in samples.items():
            measured_cold = max(times)
            measured_warm = (
                min(times) if len(times) > 1 else max(DEFAULT_WARM, measured_cold // 6)
            )
            fp = self.fingerprint(lib)
            old = self.entries.get(lib)
            prior_c = prior_cold(lib, self.project_root)
            cached = measured_cold < CACHED_COLD_FRACTION * max(prior_c, DEFAULT_COLD)
            if old and str(old.get("fingerprint") or "") == fp:
                if cached:
                    cold = float(old["cold"])
                else:
                    cold = (1.0 - EMA_ALPHA) * float(old["cold"]) + EMA_ALPHA * measured_cold
                warm = (1.0 - EMA_ALPHA) * float(old["warm"]) + EMA_ALPHA * measured_warm
                n_samples = int(old.get("samples") or 1) + 1
            elif cached:
                cold = float(prior_c)
                warm = float(measured_warm)
                n_samples = 1
            else:
                cold = float(measured_cold)
                warm = float(measured_warm)
                n_samples = 1
            self.entries[lib] = {
                "cold": max(DEFAULT_COLD, int(round(cold))),
                "warm": max(DEFAULT_WARM, int(round(warm))),
                "fingerprint": fp,
                "samples": n_samples,
            }

    def to_json(self) -> dict:
        return {"libraries": dict(sorted(self.entries.items()))}

    def save(self, path: Path) -> None:
        path = Path(path)
        path.write_text(json.dumps(self.to_json(), indent=2, sort_keys=True) + "\n", encoding="utf-8")

    @classmethod
    def load(cls, path: Path, project_root: Path | None = None) -> "CostTable":
        table = cls(project_root)
        payload = json.loads(Path(path).read_text(encoding="utf-8"))
        libraries = payload.get("libraries", payload)
        if isinstance(libraries, dict):
            table.entries = {
                str(name): dict(value)
                for name, value in libraries.items()
                if isinstance(value, dict)
            }
        return table


def load_cost_table(
    project_root: Path | None = None, path: Path | str | None = None
) -> CostTable:
    if path is None:
        path = os.environ.get("COMPILE_COSTS_FILE", "compile_costs.json")
    table_path = Path(path)
    if table_path.is_file():
        return CostTable.load(table_path, project_root)
    return CostTable(project_root)


def load_timing_records(paths: list[Path]) -> list[dict]:
    records: list[dict] = []
    for path in paths:
        path = Path(path)
        if not path.exists():
            raise FileNotFoundError(path)
        if path.is_dir():
            for child in sorted(path.rglob("*")):
                if child.suffix in {".jsonl", ".json"} and child.is_file():
                    records.extend(load_timing_records([child]))
            continue
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8").strip()
        if not text:
            continue
        if path.suffix == ".jsonl":
            for line in text.splitlines():
                line = line.strip()
                if line:
                    records.append(json.loads(line))
            continue
        payload = json.loads(text)
        if isinstance(payload, list):
            records.extend(item for item in payload if isinstance(item, dict))
        elif isinstance(payload, dict) and "sketches" in payload:
            records.extend(item for item in payload["sketches"] if isinstance(item, dict))
        elif isinstance(payload, dict):
            records.append(payload)
    return records


def cold_cost(library: str, table: CostTable | None = None) -> int:
    if table is not None:
        return table.cold(library)
    return SEED_COLD.get(library, DEFAULT_COLD)


def warm_cost(library: str, table: CostTable | None = None) -> int:
    if table is not None:
        return table.warm(library)
    return SEED_WARM.get(library, DEFAULT_WARM)


def bin_cost(
    sketches: list[str],
    libraries_of: dict[str, frozenset[str]],
    table: CostTable | None = None,
) -> int:
    """Submodular load of a bin: each library pays cold once, then warm."""
    if not sketches:
        return 0
    uses: dict[str, int] = {}
    for sketch in sketches:
        for lib in libraries_of.get(sketch, ()):
            uses[lib] = uses.get(lib, 0) + 1
    total = len(sketches) * SKETCH_REMAINDER
    for lib, count in uses.items():
        total += cold_cost(lib, table) + max(0, count - 1) * warm_cost(lib, table)
    return total
