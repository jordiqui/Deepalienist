#!/usr/bin/env python3

import argparse
import csv
import json
import statistics
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


DEFAULT_FENS = [
    "8/8/4k3/8/8/1r1K1B2/1b6/5R2 w - - 0 86",
    "8/8/6B1/7R/8/4b1k1/r7/4K3 w - - 84 128",
]


@dataclass
class Measurement:
    fen: str
    wtime_ms: int
    iteration: int
    bestmove: str
    latency_ms: float


class UCIProcess:
    def __init__(self, engine_path: Path):
        self.process = subprocess.Popen(
            [str(engine_path)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )

    def send(self, command: str) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def read_until(self, prefix: str) -> str:
        assert self.process.stdout is not None
        while True:
            line = self.process.stdout.readline()
            if not line:
                raise RuntimeError(f"Engine terminated before emitting '{prefix}'")

            line = line.strip()
            if line.startswith(prefix):
                return line

    def initialize(self, syzygy_path: str, threads: int, hash_mb: int) -> None:
        self.send("uci")
        self.read_until("uciok")

        self.send(f"setoption name Threads value {threads}")
        self.send(f"setoption name Hash value {hash_mb}")
        self.send("setoption name MultiPV value 1")
        self.send("setoption name SyzygyProbeLimit value 6")
        self.send("setoption name Syzygy50MoveRule value true")
        if syzygy_path:
            self.send(f"setoption name SyzygyPath value {syzygy_path}")

        self.send("isready")
        self.read_until("readyok")

    def close(self) -> None:
        if self.process.poll() is None:
            self.send("quit")
            self.process.wait(timeout=10)


def percentile(samples: list[float], pct: float) -> float:
    if not samples:
        return 0.0

    ordered = sorted(samples)
    index = min(len(ordered) - 1, round((len(ordered) - 1) * pct))
    return ordered[index]


def run_case(engine: UCIProcess, fen: str, wtime_ms: int, iterations: int, warmup: int) -> list[Measurement]:
    results: list[Measurement] = []

    for iteration in range(1, iterations + warmup + 1):
        engine.send(f"position fen {fen}")

        t_go_sent = time.perf_counter_ns()
        engine.send(f"go wtime {wtime_ms} btime {wtime_ms} winc 0 binc 0")
        bestmove_line = engine.read_until("bestmove ")
        t_bestmove_printed = time.perf_counter_ns()

        measurement = Measurement(
            fen=fen,
            wtime_ms=wtime_ms,
            iteration=max(1, iteration - warmup),
            bestmove=bestmove_line.split()[1],
            latency_ms=(t_bestmove_printed - t_go_sent) / 1_000_000,
        )

        if iteration <= warmup:
            continue

        results.append(measurement)

    return results


def summarize(results: Iterable[Measurement]) -> list[dict]:
    grouped: dict[tuple[str, int], list[Measurement]] = {}
    for result in results:
        grouped.setdefault((result.fen, result.wtime_ms), []).append(result)

    summary = []
    for (fen, wtime_ms), group in grouped.items():
        latencies = [g.latency_ms for g in group]
        summary.append(
            {
                "fen": fen,
                "wtime_ms": wtime_ms,
                "iterations": len(group),
                "min_ms": min(latencies),
                "mean_ms": statistics.fmean(latencies),
                "p95_ms": percentile(latencies, 0.95),
                "p99_ms": percentile(latencies, 0.99),
                "max_ms": max(latencies),
            }
        )

    return summary


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Measure UCI bestmove latency from go-send to bestmove receipt. "
            "The parent process timestamp is a close proxy for stdout flush time on Windows."
        )
    )
    parser.add_argument("--engine", required=True, type=Path, help="Path to the engine binary.")
    parser.add_argument("--syzygy-path", default="", help="Path to Syzygy tablebases.")
    parser.add_argument("--iterations", type=int, default=1000, help="Iterations per FEN/time bucket.")
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--hash-mb", type=int, default=16)
    parser.add_argument("--warmup", type=int, default=5, help="Unreported warmup iterations per FEN/time bucket.")
    parser.add_argument("--wtime", nargs="+", type=int, default=[200, 500, 1000])
    parser.add_argument("--fen", dest="fens", nargs="+", default=DEFAULT_FENS)
    parser.add_argument("--assert-max-ms", type=float, default=50.0)
    parser.add_argument("--csv", type=Path, help="Optional CSV output path.")
    parser.add_argument("--json", type=Path, help="Optional JSON summary output path.")
    args = parser.parse_args()

    engine = UCIProcess(args.engine)
    try:
        engine.initialize(args.syzygy_path, args.threads, args.hash_mb)

        all_results: list[Measurement] = []
        for fen in args.fens:
            for wtime_ms in args.wtime:
                all_results.extend(run_case(engine, fen, wtime_ms, args.iterations, args.warmup))

        if args.csv:
            args.csv.parent.mkdir(parents=True, exist_ok=True)
            with args.csv.open("w", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=list(asdict(all_results[0]).keys()))
                writer.writeheader()
                for result in all_results:
                    writer.writerow(asdict(result))

        summary = summarize(all_results)
        if args.json:
            args.json.parent.mkdir(parents=True, exist_ok=True)
            args.json.write_text(json.dumps(summary, indent=2) + "\n")

        print(json.dumps(summary, indent=2))

        failures = [row for row in summary if row["max_ms"] > args.assert_max_ms]
        if failures:
            for failure in failures:
                print(
                    f"Latency regression: wtime={failure['wtime_ms']} max={failure['max_ms']:.3f}ms "
                    f"for FEN {failure['fen']}",
                    file=sys.stderr,
                )
            return 1

        return 0
    finally:
        engine.close()


if __name__ == "__main__":
    raise SystemExit(main())
