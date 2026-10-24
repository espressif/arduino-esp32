#!/usr/bin/env python3
"""Assign affected sketches to compile chunks by library affinity.

Examples:
  python3 .github/scripts/chunk_assign.py --list affected_sketches.txt --max 15 --out-dir chunks/
  python3 .github/scripts/chunk_assign.py --from-tree libraries --max 15 --print-summary
  python3 .github/scripts/chunk_assign.py --update-costs compile_timings/ --costs compile_costs.json
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from ci_chunking.affinity import find_sketches_under, posix_path  # noqa: E402
from ci_chunking.costs import load_cost_table, load_timing_records  # noqa: E402
from ci_chunking.pack import assign, print_summary, write_assignment  # noqa: E402


def _read_list(path: Path) -> list[str]:
    sketches: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        sketch = posix_path(line)
        if sketch and not sketch.startswith("#"):
            sketches.append(sketch)
    return sketches


def _load_dependencies(path: Path | None) -> dict[str, list[str]] | None:
    if path is None:
        return None
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise SystemExit(f"dependencies JSON must be an object: {path}")
    result: dict[str, list[str]] = {}
    for key, value in data.items():
        if isinstance(value, list):
            result[str(key)] = [str(item) for item in value]
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Group sketches by library affinity and pack compile chunks."
    )
    parser.add_argument(
        "--list",
        type=Path,
        help="File with one sketch path per line (get_affected.py stdout).",
    )
    parser.add_argument(
        "--from-tree",
        type=Path,
        help="Discover Arduino sketches under this directory (usually libraries/).",
    )
    parser.add_argument(
        "--max",
        type=int,
        default=15,
        help="Maximum number of chunks (default: 15).",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("chunks"),
        help="Directory for chunk_N.txt and chunks.json (default: chunks/).",
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=None,
        help="Project root for library.properties depends= (default: parent of --from-tree or cwd).",
    )
    parser.add_argument(
        "--dependencies",
        type=Path,
        default=None,
        help="Optional forward-include graph JSON from get_affected.py.",
    )
    parser.add_argument(
        "--print-summary",
        action="store_true",
        help="Print Cmax, extra colds, and per-bin library sets to stderr.",
    )
    parser.add_argument(
        "--costs",
        type=Path,
        default=None,
        help="compile_costs.json to read (packing) or write (--update-costs).",
    )
    parser.add_argument(
        "--update-costs",
        nargs="+",
        metavar="TIMING",
        default=None,
        help="JSON/JSONL timing files or directories; merge into --costs and exit.",
    )
    args = parser.parse_args(argv)

    if args.update_costs is not None:
        if args.root is None:
            args.root = Path.cwd()
        costs_path = args.costs or Path("compile_costs.json")
        table = load_cost_table(args.root, costs_path)
        try:
            records = load_timing_records([Path(item) for item in args.update_costs])
        except FileNotFoundError as exc:
            missing = exc.filename or exc
            raise SystemExit(f"timing path not found: {missing}") from exc
        table.update_from_timings(records)
        table.save(costs_path)
        print(f"Updated {costs_path} from {len(records)} timing records", file=sys.stderr)
        print(len(table.entries))
        return 0

    if args.list is None and args.from_tree is None:
        parser.error("one of --list or --from-tree is required")
    if args.max < 1:
        parser.error("--max must be >= 1")

    sketches: list[str] = []
    if args.list is not None:
        sketches.extend(_read_list(args.list))

    tree_root: Path | None = None
    if args.from_tree is not None:
        tree_root = args.from_tree if args.from_tree.is_absolute() else Path.cwd() / args.from_tree
        tree_root = tree_root.resolve()

    project_root = args.root
    if project_root is None and tree_root is not None:
        project_root = tree_root.parent if tree_root.name == "libraries" else tree_root
    elif project_root is None:
        project_root = Path.cwd()
    else:
        project_root = Path(project_root)

    if tree_root is not None:
        relative_to = Path(project_root) if tree_root.name == "libraries" else tree_root
        sketches.extend(find_sketches_under(tree_root, relative_to=relative_to))

    assignment = assign(
        sketches,
        args.max,
        dependencies=_load_dependencies(args.dependencies),
        project_root=project_root,
        cost_table=load_cost_table(project_root, args.costs),
    )
    write_assignment(assignment, args.out_dir)
    print_summary(assignment, sys.stderr)
    print(assignment.chunk_count)
    return 0


if __name__ == "__main__":
    sys.exit(main())
