# Deepalienist 4.90-140626 Final Closure Audit

Audit date: 2026-06-15

## Repository state

- Branch at audit start: `work`
- Audited HEAD: `8c409661ef709699d8b7bc6f0bc65e7da7aa3da4`
- Accepted rebrand hash `4ec665aad8b8d0dd7d425999b57c3ed5ed7860cf` was not present in the local object database.
- Local same-contract rebrand commit: `e2450066ccb4a285ab8078bee30651b110114551`
- Patch-content verification confirmed that the local rebrand changes only:
  - the executable identity in `src/Makefile` from `4.80-0500626` to `4.90-140626`; and
  - the UCI identity in `src/misc.cpp` from `4.80-0500626` to `4.90-140626`.
- No source files were changed during this audit.

## Identity and invariants

- Executable naming contract: `Deepalienist-4.90-140626-<architecture>`.
- Runtime UCI identity: `Deepalienist 4.90-140626-<architecture>`.
- Active and default NNUE: `nn-71d6d32cb962.nnue`.
- No old `Deepalienist-4.80-0500626` or `Deepalienist 4.80-0500626` identity remained in `src` or `README.md`.
- No forbidden single-net fossils matched:
  `EvalFileSmall`, `NetworkSmall`, `EvalFileDefaultNameSmall`, `SmallNNUE`,
  `BigNNUE`, `nn-83a0d6daf7e5`, `MCTS`, or `MonteCarlo`.
- `src/attacks.h` and `src/attacks.cpp` remained absent.
- Expected experience, book, CTG/Polyglot, Syzygy, and Zobrist surfaces remained present.
- `git diff --check` passed.
- No Git conflict markers were found. Separator-only matches in the Makefile,
  UCI diagnostic output, and test shell output were not conflict markers.

## Linux build matrix

All builds used Clang, two parallel jobs, external NNUE, and LLD:

`make -C src -j2 build ARCH=<target> COMP=clang NNUE_EMBEDDING_OFF=yes EXTRALDFLAGS="-fuse-ld=lld"`

| Target | Result | Warnings | Errors/linker blockers | Produced binary |
| --- | --- | ---: | ---: | --- |
| `x86-64` | PASS | 0 | 0 | `Deepalienist-4.90-140626-x86-64` |
| `x86-64-sse3-popcnt` | PASS | 0 | 0 | `Deepalienist-4.90-140626-sse3-popcnt` |
| `x86-64-ssse3` | PASS | 0 | 0 | `Deepalienist-4.90-140626-ssse3` |
| `x86-64-sse41-popcnt` | PASS | 0 | 0 | `Deepalienist-4.90-140626-sse41popcnt` |
| `x86-64-avx2` | PASS | 0 | 0 | `Deepalienist-4.90-140626-avx2` |
| `x86-64-bmi2` | PASS | 0 | 0 | `Deepalienist-4.90-140626-bmi2` |
| `x86-64-fma3` | PASS | 0 | 0 | `Deepalienist-4.90-140626-fma3` |

Every build log passed the required scan for compiler warnings, compiler errors,
undefined references/symbols, linker errors, and Make failures.

## Runtime smoke tests

The runtime checks used `Deepalienist-4.90-140626-sse41popcnt`.

### UCI

- Reported `id name Deepalienist 4.90-140626-sse41popcnt`.
- Reported `EvalFile` default `nn-71d6d32cb962.nnue`.
- Returned `uciok`, `readyok`, and `bestmove`.
- Reported NNUE evaluation using the active 106 MiB network.

### MultiPV with experience/book disabled

- `Experience Book` was advertised and accepted.
- The prescribed command's additional `Use NNUE` option was not advertised and
  produced the exact response `No such option: Use NNUE`; this is consistent
  with the audited single-net UCI surface.
- A confirmation run disabled both `Experience` and `Experience Book`, set
  `MultiPV` to 3, waited for search completion, and returned PVs 1, 2, and 3
  through depth 3 followed by `bestmove`.

### NNUE

- Both supplied positions initialized and used `nn-71d6d32cb962.nnue`.
- Both searches completed and returned `bestmove`.

### FEN and promotion regression

- All supplied FEN commands were accepted without assertion, abort, crash, or
  invalid-FEN diagnostic.
- All three readiness checks returned `readyok`.
- The three searches returned `bestmove`, including promotion output
  `bestmove a2a1q`.

## Cleanup and verdict

- Build objects, generated binaries, build logs, downloaded NNUE data, and
  generated experience data were removed.
- The tracked source tree had no diff after cleanup.
- Final verdict: **FINAL AUDIT PASS**.
