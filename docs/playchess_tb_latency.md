# Playchess / Syzygy latency harness

This repository now includes a minimal UCI latency harness for reproducing `go -> bestmove` latency under low remaining time, with Syzygy enabled.

## Windows-oriented usage

```powershell
python tests/uci_latency_harness.py ^
  --engine .\src\Deepalienist-3.70-100326-modern.exe ^
  --syzygy-path D:\syzygy ^
  --iterations 1000 ^
  --warmup 5 ^
  --wtime 200 500 1000 ^
  --fen "8/8/4k3/8/8/1r1K1B2/1b6/5R2 w - - 0 86" "8/8/6B1/7R/8/4b1k1/r7/4K3 w - - 84 128" ^
  --assert-max-ms 50 ^
  --csv artifacts\playchess_syzygy_latency.csv ^
  --json artifacts\playchess_syzygy_latency.json
```

The harness records:
- `t_go_sent`: parent-side timestamp immediately before flushing `go ...`.
- `t_bestmove_printed`: parent-side timestamp immediately after `bestmove ...` is observed on the engine pipe.

Because the harness timestamps the read side of the pipe, it is a close proxy for when the engine actually wrote and flushed `bestmove` to stdout.

## Included smoke data

The CI/container environment only had the lightweight 4-man test Syzygy bundle already used by the existing test suite, not the 6-man `KRBvKRB` tables required for the Playchess reproduction FENs. The included logs therefore demonstrate the harness and the root-TB fast path on a known 4-man Syzygy position (`4k3/PP6/8/8/8/8/8/4K3 w - - 0 1`).

### Before change
- See `artifacts/uci_latency_4man_before.json`.
- Worst-case observed latency in the smoke run: `21.527 ms` at `wtime=500`.

### After change
- See `artifacts/uci_latency_4man_after.json`.
- Worst-case observed latency in the smoke run: `2.155 ms` at `wtime=1000`.
- The same smoke setup dropped from `16.894 ms` max at `wtime=200` before the change to `1.413 ms` after the change.

## Notes

- The engine now short-circuits root Syzygy positions before normal search startup and before NNUE verification/search-thread work.
- Low-time UCI `info` output is throttled/suppressed so `bestmove` keeps highest priority on the stdout channel.
- For the exact Playchess 6-man regression, supply a local Syzygy directory containing the relevant 6-man `KRBvKRB` WDL/DTZ files and run the command above on Windows.
