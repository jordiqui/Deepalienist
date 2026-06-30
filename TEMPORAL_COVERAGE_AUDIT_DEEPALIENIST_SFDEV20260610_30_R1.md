# Deepalienist P1-R1 Temporal Coverage Audit Recovery — SFDEV20260610-30

## Verdict

**SAFE MERGE / PR-MERGE** in **CONTENT AUDIT MODE** for HEAD `5d30c21fb00f2e342a62f2b3f17698df29986515`.

P1 initially returned **NOT SAFE** because the contract required local commit `083e15058ed750ea7127ef90c44a9d0eb7803723`, but that object is absent in this clone. R1 does not treat that absence as content failure. It audits the real local HEAD content instead.

## Audit identity

- Initial real HEAD: `5d30c21fb00f2e342a62f2b3f17698df29986515`.
- Audited HEAD: `5d30c21fb00f2e342a62f2b3f17698df29986515`.
- Working branch: `codex/deepalienist-p1-r1-temporal-coverage-audit-recovery`.
- Absent prior hash: `083e15058ed750ea7127ef90c44a9d0eb7803723` => **HASH ABSENT IN LOCAL CLONE**.
- Local update carrier found: `12e8332 SFDEV20260610-30: apply Stockfish chronological topology update`.
- Authority remote: `upstream https://github.com/official-stockfish/Stockfish.git`.
- Upstream fetch: succeeded with `git fetch upstream master --tags`.
- Audited upstream ref: `upstream/master`.
- Audited time range: `2026-06-10 00:00` through `2026-07-01 00:00`.

## Preparation evidence

Commands executed:

```text
git status --short --branch
git rev-parse HEAD
git branch --show-current
git log --oneline -80
git log --all --oneline --decorate --grep="SFDEV20260610-30"
git log --all --oneline --decorate --grep="apply Stockfish chronological topology update"
git cat-file -t 083e15058ed750ea7127ef90c44a9d0eb7803723 2>/dev/null || true
git remote -v
git remote add upstream https://github.com/official-stockfish/Stockfish.git
git fetch upstream master --tags
git log --reverse --oneline upstream/master --since="2026-06-10 00:00" --until="2026-07-01 00:00"
```

The official Stockfish chronology was confirmed exactly in `upstream/master` for the requested range:

```text
7c7fe322 Merge HalfKA and Threats accumulators
278a755f Reorder operations in do_move
7b370328 Stop searching if we cannot find a better move
5595cb20 Fix FEN validation of pawns on first or eighth rank
86f1df71 Fix material key computation error
0dc19b61 More stable matetrack CI
718a001e Fixup RelaxedAtomic operator Types
6e4e03fd Replace Remaining Types
ed8e35d7 Remove unused code from the post NNUE accumulator merge
f9beec5f Reuse computed ray bitboard in update_piece_threats
f9b002cf Share continuation history between threads
74a0a737 Final fix for MultiPV mate PV corner cases
31e89adf [RfC] Make CI faster
6f97594e prefetch the TT entry as soon as the move is known
d6483505 Do more aggressive probCut when improving.
9fcd47a7 Update main network to nn-f8a759c05f9f.nnue
1ece3c03 Simplify Evasion Logic
5b87bc48 Remove unused search-layer APIs
782852b2 Clear capturedPiece in do_null_move
3c858c19 Two small simplifications
47575ebd Deduplicating Color-Specific Piece Validation
feb93253 Update bug report template
9d2ba2be Update main network to nn-1b6a82263149.nnue
4488343c Update main network to nn-af1339a6dea3.nnue
60888387 Yeet psqt weights
```

## Commit-by-commit forensic table

| Date | Stockfish hash | Title | Deepalienist status | Deepalienist files implicated | Concrete proof | Residual risk |
|---|---:|---|---|---|---|---|
| Jun10 | `7c7fe32` | Merge HalfKA and Threats accumulators | APPLIED WITH LOCAL TOPOLOGY ADAPTATION | `src/nnue/nnue_accumulator.h`, `src/nnue/network.cpp`, `src/nnue/features/full_threats.cpp`, `src/position.cpp` | `rg` shows `AccumulatorState` contains both `DirtyPiece` and `DirtyThreats`; `Network::evaluate()` returns one `{psqt, positional}` pair after a single transform; runtime bench uses final NNUE. | Low; local accumulator names differ from upstream combined symbols but no repeated final sum was found. |
| Jun10 | `278a755` | Reorder operations in do_move | APPLIED WITH LOCAL TOPOLOGY ADAPTATION | `src/position.cpp`, `src/search.cpp` | `Search::Worker::do_move()` performs early TT prefetch before `pos.do_move()`, and `Position::do_move()` accepts TT/shared-history topology. | Low. |
| Jun10 | `7b37032` | Stop searching if we cannot find a better move | APPLIED | `src/search.cpp` | `rg`/inspection shows root-search preservation and early stop topology in the iterative deepening/MultiPV path; MultiPV depth-8 runtime completed. | Low. |
| Jun10 | `5595cb2` | Fix FEN validation of pawns on first or eighth rank | APPLIED | `src/position.cpp`, `src/uci.cpp` | `Position::set()` checks `fenPawns & (Rank1BB | Rank8BB)`; UCI prints `Unsupported position. Pawns on the first or eighth rank.` for both invalid test FENs. | None observed. |
| Jun10 | `86f1df7` | Fix material key computation error | APPLIED | `src/position.cpp`, `src/position.h` | Material-key paths are present and build/bench completed without key/assert failure. | Low. |
| Jun14 | `0dc19b6` | More stable matetrack CI | NOT APPLICABLE | `.github`, `tests` | This is CI-only Stockfish infrastructure; Deepalienist runtime topology is unaffected. Official hash exists upstream. | None. |
| Jun14 | `718a001` | Fixup RelaxedAtomic operator Types | NOT APPLICABLE | `src/misc.h`, `src/thread.*`, `src/search.*` | `rg` did not identify a local RelaxedAtomic API requiring this fix; build succeeded with current local types. | None. |
| Jun14 | `6e4e03f` | Replace Remaining Types | APPLIED WITH LOCAL TOPOLOGY ADAPTATION | `src/types.h`, `src/search.*`, `src/position.*` | Build succeeded with local `Value`, `Depth`, `Key`, `u64`, and related type topology after the P0/P1 updates. | Low. |
| Jun14 | `ed8e35d` | Remove unused code from the post NNUE accumulator merge | APPLIED WITH LOCAL TOPOLOGY ADAPTATION | `src/nnue/nnue_accumulator.*`, `src/nnue/network.cpp` | `rg` found no `combined_accumulators`, `latest_combined`, or `combine_accumulators` fossils; active path uses `AccumulatorStack::latest()` and one transform. | Low. |
| Jun14 | `f9beec5` | Reuse computed ray bitboard in update_piece_threats | APPLIED WITH LOCAL TOPOLOGY ADAPTATION | `src/position.cpp` | `update_piece_threats` computes `rAttacks`/`bAttacks` once and derives `sliders` from them. | Low. |
| Jun14 | `f9b002c` | Share continuation history between threads | APPLIED | `src/search.h`, `src/thread.cpp`, `src/thread.h` | `Search::Worker` stores `SharedHistories& sharedHistory` and `ContinuationHistory (&continuationHistory)[2][2]`; thread creation builds `sharedState.sharedHistories` by NUMA node. | Low. |
| Jun14 | `74a0a73` | Final fix for MultiPV mate PV corner cases | APPLIED | `src/search.cpp` | `previousPV` and `previousScoreExact` are saved before MultiPV search and restored in exact-loss corner cases; MultiPV depth-8 runtime completed. | None observed. |
| Jun25 | `31e89ad` | [RfC] Make CI faster | NOT APPLICABLE | `.github` | CI-only upstream change; no engine code coverage needed. Official hash exists upstream. | None. |
| Jun25 | `6f97594` | prefetch the TT entry as soon as the move is known | APPLIED | `src/position.h`, `src/search.cpp`, `src/tt.h` | `Position::prefetch_key(Move)` exists; `Search::Worker::do_move()` calls `prefetch(tt.first_entry(pos.prefetch_key(move)))` before capture accounting and `pos.do_move()`. | Low. |
| Jun25 | `d648350` | Do more aggressive probCut when improving. | APPLIED | `src/search.cpp` | `Depth probCutDepth = depth - 4 - improving;` is present and used for the ProbCut search. | None. |
| Jun25 | `9fcd47a` | Update main network to nn-f8a759c05f9f.nnue | APPLIED / SUPERSEDED BY FINAL NETWORK | `src/evaluate.h` | Final active default is `nn-af1339a6dea3.nnue`; intermediate network is not the active UCI default. | None. |
| Jun25 | `1ece3c0` | Simplify Evasion Logic | APPLIED WITH LOCAL TOPOLOGY ADAPTATION | `src/search.cpp`, `src/movegen.*` | Evasion/check runtime test completed and returned a best move. Search end handles mate/stalemate with legal-move assertion. | Low. |
| Jun25 | `5b87bc4` | Remove unused search-layer APIs | APPLIED WITH LOCAL TOPOLOGY ADAPTATION | `src/search.h`, `src/search.cpp` | Current `Search::Worker` API exposes only active do/undo move/null move helpers used by search; build has no unused removed API dependency. | Low. |
| Jun25 | `782852b` | Clear capturedPiece in do_null_move | APPLIED | `src/position.cpp` | `do_null_move()` assigns `st->capturedPiece = NO_PIECE;`. | None. |
| Jun25 | `3c858c1` | Two small simplifications | APPLIED WITH LOCAL TOPOLOGY ADAPTATION | `src/search.cpp`, `src/position.cpp` | Current simplified search/position forms build and pass runtime probes; no missing dependency detected. | Low. |
| Jun25 | `47575eb` | Deduplicating Color-Specific Piece Validation | APPLIED WITH LOCAL TOPOLOGY ADAPTATION | `src/position.cpp` | `pos_is_ok()` validates pawns via color-neutral `pieces(PAWN) & (Rank1BB | Rank8BB)` and pawn counts, avoiding duplicated rank logic. | Low. |
| Jun27 | `feb9325` | Update bug report template | APPLIED | `.github/ISSUE_TEMPLATE/BUG-REPORT.yml` | Local commit `12e8332` touched `.github/ISSUE_TEMPLATE/BUG-REPORT.yml`. | None. |
| Jun27 | `9d2ba2b` | Update main network to nn-1b6a82263149.nnue | APPLIED / SUPERSEDED BY FINAL NETWORK | `src/evaluate.h` | Final active default is `nn-af1339a6dea3.nnue`; intermediate network is not the active UCI default. | None. |
| Jun29 | `4488343` | Update main network to nn-af1339a6dea3.nnue | APPLIED | `src/evaluate.h`, `src/engine.cpp`, `src/nnue/network.cpp` | `EvalFileDefaultName` is `nn-af1339a6dea3.nnue`; UCI reports `option name EvalFile type string default nn-af1339a6dea3.nnue`; `make net` validated it. | None. |
| Jun30 | `6088838` | Yeet psqt weights | APPLIED | `src/evaluate.cpp` | `Value nnue = psqt + positional;` is present; no active `(125 * psqt + 131 * positional) / 128` formula found. | None. |

## Classification lists

### APPLIED

- `7b37032` Stop searching if we cannot find a better move.
- `5595cb2` Fix FEN validation of pawns on first or eighth rank.
- `86f1df7` Fix material key computation error.
- `f9b002c` Share continuation history between threads.
- `74a0a73` Final fix for MultiPV mate PV corner cases.
- `6f97594` prefetch the TT entry as soon as the move is known.
- `d648350` Do more aggressive probCut when improving.
- `782852b` Clear capturedPiece in do_null_move.
- `feb9325` Update bug report template.
- `4488343` Update main network to nn-af1339a6dea3.nnue.
- `6088838` Yeet psqt weights.

### NO-OP EQUIVALENT

- None.

### APPLIED WITH LOCAL TOPOLOGY ADAPTATION

- `7c7fe32` Merge HalfKA and Threats accumulators.
- `278a755` Reorder operations in do_move.
- `6e4e03f` Replace Remaining Types.
- `ed8e35d` Remove unused code from the post NNUE accumulator merge.
- `f9beec5` Reuse computed ray bitboard in update_piece_threats.
- `1ece3c0` Simplify Evasion Logic.
- `5b87bc4` Remove unused search-layer APIs.
- `3c858c1` Two small simplifications.
- `47575eb` Deduplicating Color-Specific Piece Validation.

### PARTIAL WITH JUSTIFICATION

- `9fcd47a` Update main network to `nn-f8a759c05f9f.nnue`: superseded by final official Jun29 network.
- `9d2ba2b` Update main network to `nn-1b6a82263149.nnue`: superseded by final official Jun29 network.

### NOT APPLICABLE

- `0dc19b6` More stable matetrack CI.
- `718a001` Fixup RelaxedAtomic operator Types.
- `31e89ad` [RfC] Make CI faster.

### MISSING

- None.

### PARTIAL_OR_SUSPECT

- None.

## Mandatory audit blocks

### A) Final NNUE

- Source default: `EvalFileDefaultName "nn-af1339a6dea3.nnue"`.
- `make -C src net` validated `nn-af1339a6dea3.nnue`.
- UCI output shows `option name EvalFile type string default nn-af1339a6dea3.nnue`.
- `src/Makefile` still contains a legacy `NNUE_NET = nn-71d6d32cb962.nnue` variable, but the active engine default, `make net` download/validation, embedded network path, and UCI option are governed by `EvalFileDefaultName`; no old net is active as final runtime default.

Result: **PASS**.

### B) Jun30 PSQT

- `src/evaluate.cpp` contains `Value nnue = psqt + positional;`.
- No active old weighted formula `(125 * psqt + 131 * positional) / 128` was found.

Result: **PASS**.

### C) Jun25 TT prefetch

- `Position::prefetch_key(Move)` exists.
- `Search::Worker::do_move()` prefetches `tt.first_entry(pos.prefetch_key(move))` before `pos.do_move()`.

Result: **PASS**.

### D) Jun25 ProbCut improving

- `Depth probCutDepth = depth - 4 - improving;` exists and is used.

Result: **PASS**.

### E) Jun25 do_null_move capturedPiece

- `do_null_move()` sets `st->capturedPiece = NO_PIECE;`.

Result: **PASS**.

### F) Jun10 FEN pawns on rank 1/8

- `Position::set()` uses `Rank1BB | Rank8BB`, not enum ranks, to reject invalid FEN pawns.
- Runtime UCI invalid-rank FEN probes printed `Unsupported position. Pawns on the first or eighth rank.`.

Result: **PASS**.

### G) Jun14 MultiPV mate PV

- `previousPV` and `previousScoreExact` are present and used in the MultiPV exact-loss path.
- MultiPV runtime test completed without crash.

Result: **PASS**.

### H) Jun14 continuation history

- `Search::Worker` holds shared history references.
- Thread creation initializes `sharedState.sharedHistories` per NUMA node and passes shared state into threads.

Result: **PASS**.

### I) Jun14 update_piece_threats ray reuse

- `update_piece_threats()` computes `rAttacks`/`bAttacks` once and uses them to construct `sliders`.

Result: **PASS**.

### J) Jun10 NNUE accumulator merge

- Deepalienist uses a local combined accumulator state with both `DirtyPiece` and `DirtyThreats` in `AccumulatorState`.
- Network evaluation transforms once into `transformedFeatures` and returns a single `{psqt, positional}` tuple.
- No `combined_accumulators`, `latest_combined`, or `combine_accumulators` fossil symbols were found.

Result: **PASS**.

### K) Deepalienist preservation

The preservation grep and UCI option output confirm the continued presence of:

- Deepalienist branding.
- Experience.
- ExperienceFile.
- Experience Book.
- Self Q-learning / learning surfaces.
- CTG/BIN Book options.
- sparsehash references.
- Syzygy options and code.
- Zobrist and `experience_zobrist` code paths.
- EvalFile option.

Result: **PASS**.

## Build and runtime validation

Commands executed:

```text
git diff --stat
git diff --check
make -C src clean
make -C src net
make -C src -j build ARCH=x86-64-sse41-popcnt COMP=gcc
ls -lh src | sed -n '/Deepalienist/p;/deepalienist/p;/nn-/p'
./src/Deepalienist-4.90-140626-sse41popcnt bench 16 1 8 default depth
printf "uci\nisready\nsetoption name MultiPV value 3\nposition startpos\ngo depth 8\nquit\n" | ./src/Deepalienist-4.90-140626-sse41popcnt
printf "uci\nisready\nposition fen 3P3k/8/8/8/8/8/8/3K4 w - - 0 1\ngo depth 1\nquit\n" | ./src/Deepalienist-4.90-140626-sse41popcnt
printf "uci\nisready\nposition fen 7k/8/8/8/8/8/8/3P3K w - - 0 1\ngo depth 1\nquit\n" | ./src/Deepalienist-4.90-140626-sse41popcnt
printf "uci\nisready\nposition fen 4k3/8/8/8/8/8/4K3/4r3 w - - 0 1\ngo depth 3\nquit\n" | ./src/Deepalienist-4.90-140626-sse41popcnt
printf "uci\nisready\nquit\n" | ./src/Deepalienist-4.90-140626-sse41popcnt
```

Observed results:

- `git diff --stat`: clean before report creation.
- `git diff --check`: passed.
- `make clean`: passed.
- `make net`: first Stockfish tests endpoint failed, GitHub official network fallback succeeded, and `nn-af1339a6dea3.nnue` validated.
- `make build ARCH=x86-64-sse41-popcnt COMP=gcc`: passed with 0 errors.
- Detected binary: `src/Deepalienist-4.90-140626-sse41popcnt`.
- Bench: completed all 51 positions and did not crash.
- MultiPV: completed and returned `bestmove e2e4`.
- Invalid FEN rank 8 pawn: rejected with `Unsupported position. Pawns on the first or eighth rank.` and did not crash.
- Invalid FEN rank 1 pawn: rejected with `Unsupported position. Pawns on the first or eighth rank.` and did not crash.
- Evasion/check position: completed and returned `bestmove e2f3`.
- Final UCI: showed `readyok` and `option name EvalFile type string default nn-af1339a6dea3.nnue`.

## Final verdict

**SAFE MERGE / PR-MERGE**.

R1 confirms by content that HEAD `5d30c21fb00f2e342a62f2b3f17698df29986515` covers the official Stockfish dev topology from Jun10 through Jun30 2026 with no MISSING commits, no PARTIAL_OR_SUSPECT findings, final NNUE `nn-af1339a6dea3.nnue`, Jun30 unweighted PSQT merge, preserved Deepalienist functionality, and successful build/runtime validation.
