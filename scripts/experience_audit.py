#!/usr/bin/env python3
"""Measurement-only audit/compaction tool for Revolution textual experience format v1."""
from __future__ import annotations

import argparse
from collections import Counter, OrderedDict, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

HEADER = "# Revolution experience format v1"
UINT16_MAX = 65535
ABS_RANGES = [(0, 9), (10, 19), (20, 29), (30, 49), (50, 99), (100, 199), (200, 399), (400, None)]
COUNT_RANGES = [(0, 1), (2, 2), (3, 4), (5, 7), (8, 11), (12, 15), (16, 31), (32, None)]


@dataclass(frozen=True)
class Entry:
    key: int
    move: int
    score: int
    eval: int
    depth: int
    visits: int


def clamp_u16(value: int) -> int:
    return max(0, min(UINT16_MAX, value))


def merge(existing: Entry, incoming: Entry) -> Entry:
    if incoming.depth > existing.depth:
        merged = incoming
    elif incoming.depth == existing.depth:
        merged = Entry(existing.key, existing.move, incoming.score, incoming.eval, existing.depth, existing.visits)
    else:
        merged = existing
    return Entry(merged.key, merged.move, merged.score, merged.eval, merged.depth, max(existing.visits, incoming.visits))


def parse_entries(path: Path) -> tuple[list[Entry], int]:
    entries: list[Entry] = []
    skipped = 0
    with path.open('r', encoding='utf-8', newline='') as fh:
        for line in fh:
            stripped = line.strip()
            if not stripped or stripped.startswith('#'):
                continue
            parts = stripped.split()
            if len(parts) != 6:
                skipped += 1
                continue
            try:
                key, move, score, eval_, depth, visits = map(int, parts)
            except ValueError:
                skipped += 1
                continue
            entries.append(Entry(key, move, score, eval_, depth, clamp_u16(visits)))
    return entries, skipped


def bucket_counts(values: Iterable[int], ranges: list[tuple[int, int | None]], *, absolute: bool) -> OrderedDict[str, int]:
    counts = OrderedDict((f"{lo}-{hi if hi is not None else '+'}", 0) for lo, hi in ranges)
    for value in values:
        sample = abs(value) if absolute else value
        for lo, hi in ranges:
            key = f"{lo}-{hi if hi is not None else '+'}"
            if hi is None:
                if sample >= lo:
                    counts[key] += 1
                    break
            elif lo <= sample <= hi:
                counts[key] += 1
                break
    return counts


def survival_percent(entries: list[Entry], predicate) -> float:
    if not entries:
        return 0.0
    survivors = sum(1 for entry in entries if predicate(entry))
    return 100.0 * survivors / len(entries)


def summarize(entries: list[Entry]) -> dict:
    exact_duplicates = Counter(entries)
    position_moves: dict[int, set[int]] = defaultdict(set)
    for entry in entries:
        position_moves[entry.key].add(entry.move)

    return {
        'total_entries': len(entries),
        'unique_positions': len(position_moves),
        'exact_duplicate_entries': sum(count - 1 for count in exact_duplicates.values() if count > 1),
        'positions_with_multiple_candidate_moves': sum(1 for moves in position_moves.values() if len(moves) > 1),
        'visits_histogram': bucket_counts((e.visits for e in entries), COUNT_RANGES, absolute=False),
        'depth_histogram': bucket_counts((e.depth for e in entries), COUNT_RANGES, absolute=False),
        'score_histogram': bucket_counts((e.score for e in entries), ABS_RANGES, absolute=True),
        'survival': OrderedDict([
            ('abs(score)>=10', survival_percent(entries, lambda e: abs(e.score) >= 10)),
            ('abs(score)>=20', survival_percent(entries, lambda e: abs(e.score) >= 20)),
            ('abs(score)>=30', survival_percent(entries, lambda e: abs(e.score) >= 30)),
            ('visits>=2', survival_percent(entries, lambda e: e.visits >= 2)),
            ('visits>=3', survival_percent(entries, lambda e: e.visits >= 3)),
            ('visits>=5', survival_percent(entries, lambda e: e.visits >= 5)),
            ('depth>=8', survival_percent(entries, lambda e: e.depth >= 8)),
            ('depth>=12', survival_percent(entries, lambda e: e.depth >= 12)),
            ('depth>=16', survival_percent(entries, lambda e: e.depth >= 16)),
        ]),
    }


def compact(entries: list[Entry]) -> list[Entry]:
    merged: OrderedDict[tuple[int, int], Entry] = OrderedDict()
    for entry in entries:
        key = (entry.key, entry.move)
        merged[key] = merge(merged[key], entry) if key in merged else entry
    return list(merged.values())


def write_entries(path: Path, entries: list[Entry]) -> None:
    with path.open('w', encoding='utf-8', newline='\n') as fh:
        fh.write(HEADER + '\n')
        for entry in entries:
            fh.write(f"{entry.key} {entry.move} {entry.score} {entry.eval} {entry.depth} {entry.visits}\n")


def emit_summary(label: str, summary: dict, *, skipped: int | None = None) -> None:
    print(f"[{label}]")
    print(f"total_entries={summary['total_entries']}")
    print(f"unique_positions={summary['unique_positions']}")
    print(f"exact_duplicate_entries={summary['exact_duplicate_entries']}")
    print(f"positions_with_multiple_candidate_moves={summary['positions_with_multiple_candidate_moves']}")
    if skipped is not None:
        print(f"skipped_lines={skipped}")
    for field in ('visits_histogram', 'depth_histogram', 'score_histogram'):
        values = ' '.join(f"{bucket}:{count}" for bucket, count in summary[field].items())
        print(f"{field} {values}")
    thresholds = ' '.join(f"{name}:{value:.1f}%" for name, value in summary['survival'].items())
    print(f"survival_percent {thresholds}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, help='Path to experience.exp file')
    parser.add_argument('--compact-out', type=Path, help='Write compacted textual v1 file')
    args = parser.parse_args()

    entries, skipped = parse_entries(args.input)
    compacted = compact(entries)

    print(f"file={args.input}")
    emit_summary('raw', summarize(entries), skipped=skipped)
    emit_summary('compacted', summarize(compacted))

    if args.compact_out:
        write_entries(args.compact_out, compacted)
        print(f"wrote_compacted={args.compact_out}")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
