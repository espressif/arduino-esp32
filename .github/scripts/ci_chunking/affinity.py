"""Map a sketch to the libraries it will compile."""

from __future__ import annotations

from collections import deque
from pathlib import Path

from ci_chunking.costs import cold_cost

CORE_BUCKET = "core"
OTHER_BUCKET = "other"


def posix_path(path: str) -> str:
    return str(path).replace("\\", "/").strip()


def library_of_path(path: str) -> str:
    """Parent ``libraries/<name>`` folder, else a stable bucket.

    Also accepts paths relative to ``libraries/`` itself (``Matter/examples/...``),
    which ``--from-tree libraries`` used to emit.
    """
    parts = [p for p in posix_path(path).split("/") if p]
    if len(parts) >= 2 and parts[0] == "libraries":
        return parts[1]
    if parts and parts[0] == "cores":
        return CORE_BUCKET
    if "examples" in parts:
        idx = parts.index("examples")
        if idx >= 1:
            return parts[idx - 1]
    return OTHER_BUCKET


def _is_real_library(name: str) -> bool:
    return name not in (CORE_BUCKET, OTHER_BUCKET, "")


def load_library_depends(project_root: Path | None) -> dict[str, set[str]]:
    """Folder name -> dependent library folder names from ``library.properties``."""
    if project_root is None:
        return {}
    lib_root = Path(project_root) / "libraries"
    if not lib_root.is_dir():
        return {}
    name_to_folder: dict[str, str] = {}
    raw_depends: dict[str, set[str]] = {}
    for child in sorted(lib_root.iterdir(), key=lambda p: p.name.lower()):
        if not child.is_dir():
            continue
        folder = child.name
        names = {folder}
        depends: set[str] = set()
        prop = child / "library.properties"
        if prop.is_file():
            try:
                text = prop.read_text(encoding="utf-8", errors="replace")
            except OSError:
                text = ""
            for line in text.splitlines():
                if line.startswith("name="):
                    value = line.split("=", 1)[1].strip()
                    if value:
                        names.add(value)
                elif line.startswith("depends="):
                    for part in line.split("=", 1)[1].split(","):
                        dep = part.strip()
                        if dep:
                            depends.add(dep)
        for name in names:
            name_to_folder[name.lower()] = folder
        raw_depends[folder] = depends
    resolved: dict[str, set[str]] = {}
    for folder, deps in raw_depends.items():
        resolved[folder] = {
            name_to_folder[dep.lower()]
            for dep in deps
            if dep.lower() in name_to_folder
        }
    return resolved


def _depends_closure(libraries: set[str], depends: dict[str, set[str]]) -> set[str]:
    seen = set(libraries)
    queue = deque(libraries)
    while queue:
        current = queue.popleft()
        for dep in depends.get(current, ()):
            if dep not in seen:
                seen.add(dep)
                queue.append(dep)
    return seen


def libraries_from_graph(
    sketch: str,
    dependencies: dict[str, list[str]] | None,
    *,
    project_root: Path | None = None,
    depends: dict[str, set[str]] | None = None,
) -> frozenset[str]:
    """Libraries this sketch will compile.

    Walk the forward include graph when provided, ignore ``cores/``, and union
    ``library.properties`` ``depends=``. Fall back to the parent-folder library.
    """
    sketch = posix_path(sketch)
    path_lib = library_of_path(sketch)
    path_set = frozenset({path_lib}) if _is_real_library(path_lib) else frozenset()
    if dependencies is None:
        found = set(path_set)
    else:
        found: set[str] = set()
        seen: set[str] = set()
        queue = deque([sketch])
        if sketch not in dependencies:
            # Scanner keys are project-relative; accept suffix matches.
            for key in dependencies:
                if posix_path(key) == sketch or posix_path(key).endswith("/" + sketch):
                    queue.append(posix_path(key))
        while queue:
            current = queue.popleft()
            current = posix_path(current)
            if current in seen:
                continue
            seen.add(current)
            lib = library_of_path(current)
            if _is_real_library(lib):
                found.add(lib)
            for dep in dependencies.get(current, ()):
                queue.append(posix_path(dep))
        if not found:
            found = set(path_set)
    if depends is None:
        depends = load_library_depends(project_root)
    if depends:
        found = _depends_closure(found, depends)
    return frozenset(found)


def affinity_key(sketch: str, libraries: frozenset[str], table=None) -> str:
    """Highest-cost library in the compile set, else the parent folder."""
    if libraries:
        return max(libraries, key=lambda lib: (cold_cost(lib, table), lib))
    return library_of_path(sketch)


def find_sketches_under(root: Path, relative_to: Path | None = None) -> list[str]:
    """Sketch ``.ino`` paths (posix) matching Arduino naming.

    Paths are relative to *relative_to* when given, otherwise *root*. Pass the
    project root as *relative_to* when scanning ``libraries/`` so results look
    like ``libraries/Matter/examples/...``.
    """
    sketches: list[str] = []
    if not root.is_dir():
        return sketches
    root = root.resolve()
    base = relative_to.resolve() if relative_to is not None else root
    for ino in sorted(root.rglob("*.ino")):
        if ino.parent.name + ".ino" != ino.name:
            continue
        sketches.append(ino.resolve().relative_to(base).as_posix())
    return sketches
