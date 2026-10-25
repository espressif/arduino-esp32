"""Family coarsen then LPT onto k bins with submodular library-union load."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

from ci_chunking.affinity import (
    affinity_key,
    libraries_from_graph,
    load_library_depends,
    posix_path,
)
from ci_chunking.costs import CostTable, DEFAULT_COLD, bin_cost, cold_cost, load_cost_table


@dataclass
class Assignment:
    bins: list[list[str]]
    costs: list[int]
    libraries_of: dict[str, list[str]]
    groups: dict[str, list[str]]
    splits: list[dict[str, object]] = field(default_factory=list)
    chunk_count: int = 0
    cmax: int = 0
    connectivity: dict[str, int] = field(default_factory=dict)

    cost_table: CostTable | None = None

    def extra_cold(self) -> int:
        extra = 0
        for lib, copies in self.connectivity.items():
            extra += max(0, copies - 1) * cold_cost(lib, self.cost_table)
        return extra


def _libraries_map(
    sketches: list[str],
    dependencies: dict[str, list[str]] | None,
    project_root: Path | None,
) -> dict[str, frozenset[str]]:
    depends = load_library_depends(project_root)
    result: dict[str, frozenset[str]] = {}
    for sketch in sketches:
        result[sketch] = libraries_from_graph(
            sketch,
            dependencies,
            project_root=project_root,
            depends=depends,
        )
    return result


def _split_group(
    sketches: list[str],
    libraries_of: dict[str, frozenset[str]],
    fair: int,
    max_chunks: int,
    *,
    n_groups: int,
    table: CostTable | None = None,
) -> tuple[list[list[str]], int]:
    """Split a family only when it exceeds a fair bin; keep cold-heavy libs together."""
    ordered = sorted(sketches)
    if len(ordered) <= 1:
        return [ordered], 1
    cost = bin_cost(ordered, libraries_of, table)
    union: set[str] = set()
    for sketch in ordered:
        union |= set(libraries_of.get(sketch, ()))
    cold = sum(cold_cost(lib, table) for lib in union)
    warm_tail = max(0, cost - cold)
    if n_groups <= 1:
        # The whole job is one family. A second cold is worth it only when the
        # warm tail is larger than that cold and the library is expensive.
        n_parts = (
            2
            if warm_tail > cold and cold >= DEFAULT_COLD * 4 and max_chunks > 1
            else 1
        )
    else:
        if fair <= 0 or cost <= fair:
            return [ordered], 1
        if cold >= fair:
            n_parts = 2 if warm_tail > fair else 1
        else:
            n_parts = (cost + fair - 1) // fair
    n_parts = min(n_parts, max_chunks, len(ordered))
    n_parts = max(1, n_parts)
    if n_parts <= 1:
        return [ordered], 1
    # Each extra part pays `cold` again. Skip the split unless the warm tail
    # that packing would cut is larger than that extra cold.
    if warm_tail <= cold:
        return [ordered], 1
    size, extra = divmod(len(ordered), n_parts)
    parts: list[list[str]] = []
    index = 0
    for part_i in range(n_parts):
        take = size + (1 if part_i < extra else 0)
        if take:
            parts.append(ordered[index : index + take])
            index += take
    return parts, len(parts)


def assign(
    sketches: list[str],
    max_chunks: int,
    dependencies: dict[str, list[str]] | None = None,
    project_root: Path | None = None,
    cost_table: CostTable | None = None,
) -> Assignment:
    """Pack sketches into at most *max_chunks* bins.

    Coarsen by highest-cost library, split a family only if it exceeds
    ``total_cost / max_chunks``, then LPT onto ``min(max_chunks, n_pieces)``
    machines with submodular load ``f(S)``.
    """
    unique: list[str] = []
    seen: set[str] = set()
    for raw in sketches:
        sketch = posix_path(raw)
        if not sketch or sketch in seen:
            continue
        seen.add(sketch)
        unique.append(sketch)

    if max_chunks < 1:
        raise ValueError("max_chunks must be >= 1")

    if not unique:
        return Assignment(
            bins=[],
            costs=[],
            libraries_of={},
            groups={},
            chunk_count=0,
            cmax=0,
        )

    if cost_table is None:
        cost_table = load_cost_table(project_root)

    libraries_of = _libraries_map(unique, dependencies, project_root)
    groups: dict[str, list[str]] = {}
    for sketch in unique:
        key = affinity_key(sketch, libraries_of[sketch], cost_table)
        groups.setdefault(key, []).append(sketch)
    for key in groups:
        groups[key] = sorted(groups[key])

    total_cost = bin_cost(unique, libraries_of, cost_table)
    n_groups = max(1, len(groups))
    fair = max(1, total_cost // min(max_chunks, n_groups))

    pieces: list[tuple[int, list[str], str]] = []
    splits: list[dict[str, object]] = []
    for key in sorted(
        groups, key=lambda name: (-bin_cost(groups[name], libraries_of, cost_table), name)
    ):
        parts, n_parts = _split_group(
            groups[key],
            libraries_of,
            fair,
            max_chunks,
            n_groups=n_groups,
            table=cost_table,
        )
        if n_parts > 1:
            splits.append({"library": key, "parts": n_parts, "sketches": len(groups[key])})
        for part in parts:
            pieces.append((bin_cost(part, libraries_of, cost_table), part, key))
    pieces.sort(key=lambda item: (-item[0], item[2], item[1][0]))

    machine_count = min(max_chunks, max(1, len(pieces)))
    bins: list[list[str]] = [[] for _ in range(machine_count)]
    for _cost, part, _key in pieces:
        best = min(
            range(machine_count),
            key=lambda index: (
                bin_cost(bins[index] + part, libraries_of, cost_table),
                index,
            ),
        )
        bins[best].extend(part)

    bins = [sorted(bin_sketches) for bin_sketches in bins if bin_sketches]
    costs = [bin_cost(bin_sketches, libraries_of, cost_table) for bin_sketches in bins]
    connectivity: dict[str, int] = {}
    for bin_sketches in bins:
        present: set[str] = set()
        for sketch in bin_sketches:
            present |= set(libraries_of[sketch])
        for lib in present:
            connectivity[lib] = connectivity.get(lib, 0) + 1

    return Assignment(
        bins=bins,
        costs=costs,
        libraries_of={sketch: sorted(libs) for sketch, libs in libraries_of.items()},
        groups=groups,
        splits=splits,
        chunk_count=len(bins),
        cmax=max(costs) if costs else 0,
        connectivity=dict(sorted(connectivity.items())),
        cost_table=cost_table,
    )


def assign_equal_count(
    sketches: list[str],
    max_chunks: int,
    dependencies: dict[str, list[str]] | None = None,
    project_root: Path | None = None,
    cost_table: CostTable | None = None,
) -> Assignment:
    """Contiguous equal-count slices. Used as SoC-outer sketch shards.

    Sharing-oblivious on purpose: with one SoC per GHA job, library cold is
    paid once per (shard, SoC). Replay on CI JSONL showed this beats affinity
    LPT for Linux wall when the matrix is ``CORE_SOCS x shards``.
    """
    unique: list[str] = []
    seen: set[str] = set()
    for raw in sketches:
        sketch = posix_path(raw)
        if not sketch or sketch in seen:
            continue
        seen.add(sketch)
        unique.append(sketch)

    if max_chunks < 1:
        raise ValueError("max_chunks must be >= 1")

    if not unique:
        return Assignment(
            bins=[],
            costs=[],
            libraries_of={},
            groups={},
            chunk_count=0,
            cmax=0,
            cost_table=cost_table,
        )

    if cost_table is None:
        cost_table = load_cost_table(project_root)

    libraries_of = _libraries_map(unique, dependencies, project_root)
    ordered = sorted(unique)
    machine_count = min(max_chunks, len(ordered))
    size, extra = divmod(len(ordered), machine_count)
    bins: list[list[str]] = []
    index = 0
    for part_i in range(machine_count):
        take = size + (1 if part_i < extra else 0)
        bins.append(ordered[index : index + take])
        index += take

    groups: dict[str, list[str]] = {"equal": list(ordered)}
    costs = [bin_cost(bin_sketches, libraries_of, cost_table) for bin_sketches in bins]
    connectivity: dict[str, int] = {}
    for bin_sketches in bins:
        present: set[str] = set()
        for sketch in bin_sketches:
            present |= set(libraries_of[sketch])
        for lib in present:
            connectivity[lib] = connectivity.get(lib, 0) + 1

    return Assignment(
        bins=bins,
        costs=costs,
        libraries_of={sketch: sorted(libs) for sketch, libs in libraries_of.items()},
        groups=groups,
        splits=[],
        chunk_count=len(bins),
        cmax=max(costs) if costs else 0,
        connectivity=dict(sorted(connectivity.items())),
        cost_table=cost_table,
    )


def write_assignment(assignment: Assignment, out_dir: Path) -> None:
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for existing in out_dir.glob("chunk_*.txt"):
        existing.unlink()
    for index, sketches in enumerate(assignment.bins):
        (out_dir / f"chunk_{index}.txt").write_text(
            "".join(f"{sketch}\n" for sketch in sketches),
            encoding="utf-8",
        )
    payload = {
        "chunk_count": assignment.chunk_count,
        "cmax": assignment.cmax,
        "extra_cold": assignment.extra_cold(),
        "connectivity": assignment.connectivity,
        "splits": assignment.splits,
        "groups": assignment.groups,
        "bins": [
            {
                "index": index,
                "cost": assignment.costs[index],
                "libraries": sorted(
                    {
                        lib
                        for sketch in sketches
                        for lib in assignment.libraries_of.get(sketch, ())
                    }
                ),
                "sketches": sketches,
            }
            for index, sketches in enumerate(assignment.bins)
        ],
    }
    (out_dir / "chunks.json").write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def format_chunks_json(chunk_count: int) -> str:
    if chunk_count <= 0:
        return "[]"
    return "[" + ",".join(f'"{index}"' for index in range(chunk_count)) + "]"


def print_summary(assignment: Assignment, stream) -> None:
    print(
        f"Packed {sum(len(b) for b in assignment.bins)} sketches into "
        f"{assignment.chunk_count} chunks, Cmax={assignment.cmax}, "
        f"extra library colds={assignment.extra_cold()}.",
        file=stream,
    )
    if assignment.splits:
        print(f"Splits: {assignment.splits}", file=stream)
    for index, sketches in enumerate(assignment.bins):
        libs = sorted(
            {
                lib
                for sketch in sketches
                for lib in assignment.libraries_of.get(sketch, ())
            }
        )
        print(
            f"  chunk {index}: cost={assignment.costs[index]} "
            f"sketches={len(sketches)} libs={libs}",
            file=stream,
        )
