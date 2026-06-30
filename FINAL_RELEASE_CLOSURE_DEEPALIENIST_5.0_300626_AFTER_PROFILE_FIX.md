# Deepalienist 5.0-300626 — P3 Final Release Closure Audit After Profile Fix

## Scope

This authorized report records the final release closure audit for Deepalienist 5.0-300626 after the P2-R1 profile-build fix. No source code, Makefile, evaluation, search, NNUE, Book, Experience, learning, Zobrist, Syzygy, or sparsehash files were changed by this audit.

## Forensic baseline

- Initial HEAD observed: `e88aa903d8ece5fff3569db304cc22f657bb2632`.
- Audited HEAD: `e88aa903d8ece5fff3569db304cc22f657bb2632`.
- Required P2-R1 validated base: `305bfc03982d9b6e58cf8b7e429a45d61d075b35`.
- P2-R1 commit present in audited history: `37e5f33 P2-R1-PROFILE-SSE41POPCNT: fix profile-build benchmark failure`.
- Current HEAD is a descendant of the P2-R1 fix through merge commit `e88aa90 Merge pull request #73 from jordiqui/codex/corregir-fallo-de-benchmark-profile-build`.
- P2 validated commit present: `073ae50 P2-REBRAND-5.0-300626: update Deepalienist identity`.
- Branch observed by `git status --short --branch`: `## work`.
- Remote state was inspected with `git remote -v`.

## Closed P2-R1 root cause

The closed P2-R1 root cause was that `src/Makefile` previously referenced obsolete `NNUE_NET = nn-71d6d32cb962.nnue`, causing `profile-build` to abort during Step 2/4 preflight before the PGO benchmark. The audited tree keeps `NNUE_NET = nn-af1339a6dea3.nnue`, and the SSE41/POPCNT profile-build completed all four steps with `STATUS=0`.

## Identity audit

- Final active identity: `Deepalienist 5.0-300626`.
- Binary naming scheme observed in Makefile/readme audit: `Deepalienist-5.0-300626-<arch>`.
- Profile-build binary generated and validated: `src/Deepalienist-5.0-300626-sse41popcnt`.
- UCI `id name` observed: `Deepalienist 5.0-300626+sse41popcnt`.
- Observed architecture: `sse41popcnt`.
- `Deepalienist 4.90-140626` was not observed as active identity in the audited source paths; old 4.x strings appear only in historical artifacts such as prior benchmark text files.

## NNUE, Makefile, and PSQT audit

- Final NNUE: `nn-af1339a6dea3.nnue`.
- `EvalFileDefaultName`: `nn-af1339a6dea3.nnue`.
- Makefile `NNUE_NET`: `nn-af1339a6dea3.nnue`.
- UCI EvalFile option observed: `option name EvalFile type string default nn-af1339a6dea3.nnue`.
- PSQT Jun30 expression preserved: `Value nnue = psqt + positional;`.
- Obsolete active `NNUE_NET = nn-71d6d32cb962.nnue`: not present.
- Old weighted formula (`125 * psqt`, `131 * positional`) did not reappear in the audited active evaluation expression.

## Deepalienist feature preservation audit

The preservation search confirmed the following features remain present:

- Experience: preserved.
- ExperienceFile: preserved.
- Experience Book: preserved.
- Self Q-learning: preserved.
- Book / BIN / CTG / live book: preserved.
- Polyglot/bookManager: preserved.
- sparsehash: preserved.
- Syzygy: preserved.
- Zobrist and `experience_zobrist`: preserved.
- EvalFile option: preserved.

## Profile-build validation

Commands executed:

```sh
make -C src clean
make -C src -j profile-build ARCH=x86-64-sse41-popcnt COMP=clang EXTRALDFLAGS="-fuse-ld=lld" 2>&1 | tee build-profile-sse41popcnt-final.log
echo "STATUS=${PIPESTATUS[0]}"
rg -n "warning:|error:|undefined reference|undefined symbol|ld: error|clang: error|make: \*\*\*" build-profile-sse41popcnt-final.log && exit 1 || true
```

Observed result:

- `STATUS=0`.
- Step 1/4 built the instrumented executable.
- Step 2/4 ran the PGO benchmark successfully.
- Step 3/4 built the optimized executable.
- Step 4/4 deleted profile data.
- No `warning:`, `error:`, undefined reference/symbol, linker blocker, clang error, or make fatal marker was found in the build log.
- The build used and validated `nn-af1339a6dea3.nnue`.
- The build did not search for `nn-71d6d32cb962.nnue`.

## Runtime and UCI validation

Detected binary:

```sh
src/Deepalienist-5.0-300626-sse41popcnt
```

Runtime commands executed:

```sh
printf "uci\nisready\nquit\n" | "$BIN" | tee uci-profile-sse41popcnt-final.log
printf "uci\nsetoption name EvalFile value nn-af1339a6dea3.nnue\nisready\nposition startpos\ngo depth 1\nquit\n" | "$BIN" | tee runtime-profile-sse41popcnt-final.log
```

Observed results:

- UCI `id name`: `Deepalienist 5.0-300626+sse41popcnt`.
- UCI `EvalFile`: `nn-af1339a6dea3.nnue`.
- UCI returned `uciok`.
- Readiness returned `readyok`.
- Runtime search returned `bestmove e2e4`.

## Additional smoke tests

Commands executed:

```sh
printf "uci\nisready\nsetoption name MultiPV value 3\nposition startpos\ngo depth 8\nquit\n" | "$BIN" | tee multipv-profile-sse41popcnt-final.log
printf "uci\nisready\nposition fen 3P3k/8/8/8/8/8/8/3K4 w - - 0 1\ngo depth 1\nquit\n" | "$BIN" | tee invalid-rank8-profile-sse41popcnt-final.log
printf "uci\nisready\nposition fen 7k/8/8/8/8/8/8/3P3K w - - 0 1\ngo depth 1\nquit\n" | "$BIN" | tee invalid-rank1-profile-sse41popcnt-final.log
printf "uci\nisready\nposition fen 4k3/8/8/8/8/8/4K3/4r3 w - - 0 1\ngo depth 3\nquit\n" | "$BIN" | tee evasion-profile-sse41popcnt-final.log
```

Observed results:

- MultiPV depth-8 search did not crash and returned `bestmove e2e4`.
- Rank-8 pawn FEN reported `Unsupported position. Pawns on the first or eighth rank.` and did not crash.
- Rank-1 pawn FEN reported `Unsupported position. Pawns on the first or eighth rank.` and did not crash.
- Evasion/check smoke test did not crash and returned `bestmove e2f3`.

## MISSING

none

## PARTIAL_OR_SUSPECT

none

## Final verdict

SAFE MERGE / PR-MERGE
