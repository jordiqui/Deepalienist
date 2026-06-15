# Deepalienist 4.90-140626 temporal coverage audit after P0I

## 1. Deepalienist HEAD audited

- Repository: current Deepalienist repository at `/workspace/Deepalienist`.
- Branch at audit start: `work`.
- Audited HEAD: `921ab748` (`921ab74`, merge of PR #68).
- The audit started from a clean tracked/untracked state (`git status --short --branch` printed only `## work`).
- This phase changes no source: this report is the only intended repository addition.

## 2. P0I commit/equivalence verification

The requested P0I hash `3c5be082ae9a7021648799cb9daa7770a706721e` is not an object in this clone. The local same-contract commit is:

- `a1108ddbbc069d07fae8a114509bfac6d0b44127` — `P0I-SFDEV20260614: complete Stockfish topology through Jun 14`.
- It is an ancestor of audited HEAD through merge `921ab74`.
- Patch-content verification, rather than subject-only verification, shows the contract-defining topology: adds `src/attacks.h` and `src/attacks.cpp`; removes attack implementation from `bitboard.cpp/h`; adds `attacks.o`; calls `Attacks::init()`; exposes `pext`/`pdep`; moves `RESTRICT` to `misc.h`; and repairs `numa.h` type visibility/order.

Therefore `a1108dd` is accepted as the local patch-equivalent P0I baseline.

## 3. Official Stockfish authority commit

Authority was freshly cloned only from `https://github.com/official-stockfish/Stockfish` and checked out at:

- Full commit: `74a0a73715322608332038f7c0151ddf0609a59a`.
- Tag: `stockfish-dev-20260614-74a0a737`.
- Title: `Final fix for MultiPV mate PV corner cases`.

Every listed commit was resolved in that official clone and its patch, changed paths, and temporal position were inspected directly. Revolution and local archives were not used.

## 4. Identity and NNUE audit

PASS.

- Runtime identity is `Deepalienist 4.90-140626-<arch>`.
- Executable identity is `Deepalienist-4.90-140626-<arch>`.
- The sole default network is `nn-71d6d32cb962.nnue` in both evaluator and Makefile surfaces.
- An AVX2 build downloaded/validated that network and bench reported the same network and identity.

## 5. Forbidden fossils audit

PASS. The required search returned no occurrence in `src`, `scripts`, `tests`, or `README.md` of:

- `EvalFileSmall`
- `NetworkSmall`
- `EvalFileDefaultNameSmall`
- `SmallNNUE`
- `BigNNUE`
- `nn-83a0d6daf7e5`
- `MCTS`
- `MonteCarlo`

## 6. Custom surfaces preservation audit

PASS. Deepalienist-owned surfaces remain present and compile together with the imported topology:

- Experience persistence/options and experience Zobrist ownership.
- Book manager, CTG, Polyglot, and book utility surfaces.
- Syzygy probing and Deepalienist root authority integration.
- Strict single-network NNUE ownership.
- Deepalienist release identity.

The successful complete AVX2 link included `book/book_manager.o`, `book/book_utils.o`, `experience.o`, Syzygy, NNUE, attacks, and engine objects; the bench initialized NNUE and searched all 51 positions.

## 7. Attacks topology audit

PASS.

| Surface | Finding |
|---|---|
| `attacks.h` | Present; owns attack templates, `Magic`, `DualMagic`, lookup access, `ray_pass_bb`, and attack declarations. |
| `attacks.cpp` | Present; owns `LineBB`, `BetweenBB`, `DualMagics`, `RankAttacks`, magic initialization, `Attacks::init`, and ray implementation. |
| `Attacks::init` | Present and called by `main.cpp` before `Bitboards::init()`. |
| `attacks.o` | Present in Makefile object topology and was visibly compiled and linked by the AVX2 build. |
| `pdep` | Exposed in `types.h` beside `pext` under `USE_PEXT`. |
| `pext` | Exposed in `types.h`; consumed by attacks indexing. |
| `RESTRICT` | Exposed centrally in `misc.h` for GNU/MSVC/fallback compilers and consumed by attacks. |
| AVX2/BMI2 readiness | AVX2 non-PEXT path compiled and benched. BMI2/PEXT selection exists in Makefile and attack code; Excavator is excluded by native detection. |

The Jun 6 attack commits are represented in the final P0I topology even though their original official files did not exist in Deepalienist before P0I.

## 8. Windows/MSYS `numa.h` audit

PASS by declaration/include ordering.

- `numa.h` includes `misc.h` before entering its `Stockfish` aliases.
- `misc.h` declares `using usize = std::size_t;` at namespace scope.
- Only after that include does `numa.h` declare `CpuIndex`, `NumaIndex`, and `static constexpr usize WIN_PROCESSOR_GROUP_SIZE = 64`.
- Thus the former Windows/MSYS failure mode—using `usize` for `WIN_PROCESSOR_GROUP_SIZE` before `usize` is visible—cannot remain in the audited source order.
- The P0I patch specifically moved the aliases/constants out of the premature Windows pre-namespace region and into the correctly ordered namespace region.

A native Windows/MSYS compiler was not available in this Linux environment; this conclusion is a source-order proof, not a Windows binary execution claim.

## 9. Block Jun 6

| Commit | Title | Status | Deepalienist evidence | Notes |
|---|---|---|---|---|
| `e4a6354` | Simplify margin formula | APPLIED_EXACT | Current SEE margin is the simplified un-clamped expression in `search.cpp`. | Search semantic matches official patch. |
| `92fe6b6` | Simplify RankAttacks initialization | APPLIED_EXACT | `attacks.cpp` defines `RankAttacks` with the final compact constexpr lambda topology. | Arrived through P0I attacks extraction/completion. |
| `7382635` | Simplify quiet history update expression | APPLIED_EXACT | Current quiet-TT bonus uses `std::min(114 * depth, 724)`. | AUTHORS metadata is non-engine topology. |
| `db11ca4` | Guard tt depth edits from racy underflows | APPLIED_EXACT | TT depth decrement and penalty clamp through `std::max(..., 0)`. | Prevents unsigned/racy underflow. |
| `645b636` | Tweak best move history updates. | APPLIED_EXACT | `update_all_stats` receives `PvNode`; non-PV bonus scales with searched move counts using a 64-bit multiplication. | Semantic and overflow guard match. |
| `9eb836b` | Compute simplified HQ r/rr at runtime | APPLIED_EXACT | Final `DualMagic` topology computes the simplified rank-reversal operands at runtime; obsolete stored r/rr topology is absent. | Present in P0I attack files. |
| `db6cba4` | More loongarch optimizations | APPLIED_EXACT | Current NNUE transformer/SIMD files contain the official LoongArch vectorized final state inherited before the temporal block ports. | No Deepalienist-specific divergence found in affected logic. |
| `8416cca` | fix Makefile universal build selection | NOT_APPLICABLE | Deepalienist has no `.github/workflows/universal_compilation.yml`, no `src/universal/` dispatch tree, and no `x86-64-universal` target. | Official commit repairs a universal-binary surface intentionally not owned by this repository. |
| `0c1001c` | Disable PEXT on AMD Excavator CPUs | APPLIED_ADAPTED | `scripts/get_native_properties.sh` has explicit `check_slow_bmi2`, family/model detection, and suppresses the BMI2 tier; `src/universal/entry_x86.cpp` is absent. | Applicable native-detection half is preserved; universal-dispatch half is not applicable. |
| `91045b2` | [CI] Update SDE action | APPLIED_ADAPTED | Deepalienist's equivalent compilation CI pins `petarpetrovt/setup-sde` at the official updated action revision and SDE 9.33.0. | Workflow name/topology is local rather than official universal workflow. |

## 10. Block Jun 8

| Commit | Title | Status | Deepalienist evidence | Notes |
|---|---|---|---|---|
| `dd3e1c4` | Consistent Integer Types | APPLIED_ADAPTED | Fixed-width aliases plus `usize/isize` are in `misc.h`; affected engine, history, NNUE, NUMA, search, TT, UCI, Syzygy, and shared-memory surfaces use the new aliases. | Adapted around Deepalienist book/experience, strict single-net ownership, and the later P0I attacks split. |

## 11. Block Jun 9

| Commit | Title | Status | Deepalienist evidence | Notes |
|---|---|---|---|---|
| `0111d11` | Add ability to disable performance-sensitive relaxed atomic operations at compile time | APPLIED_ADAPTED | `misc.h` provides `RelaxedAtomic`, `USE_SLOPPY_ATOMICS`, lock-free/size selection, typed operators; TT/search/history/thread use it. | Adapted to local ownership and subsequently includes Jun 14 operator-type fix. |
| `415ff79` | Fix test harness timeout enforcement | APPLIED_EXACT | `tests/testing.py` uses a monotonic deadline, queue timeout, and dedicated timeout exception/output handling. | Final harness behavior matches official fix. |
| `1eff8b0` | Guarantee complete mate PVs in multiPV analysis | APPLIED_EXACT | Root moves retain previous PV/score state, incomplete loss lines get bound handling, and previous mate PV recovery exists. | Later `74a0a73` correctly supersedes/refines this topology. |
| `8e711c2` | Add wasm32 and wasm32-relaxed-simd targets, and light optimizations | NOT_APPLICABLE | No `wasm32`/`wasm32-relaxed-simd` Makefile target or wasm compilation workflow exists; Deepalienist's native release matrix does not claim WebAssembly support. | The commit's conditional light optimizations are tied to the unsupported wasm build surface; importing only fragments would create an unowned/unvalidated target. |

## 12. Block Jun 10

| Commit | Title | Status | Deepalienist evidence | Notes |
|---|---|---|---|---|
| `7c7fe32` | Merge HalfKA and Threats accumulators | APPLIED_EXACT | NNUE has the merged accumulator topology; old independent post-merge accumulator machinery is absent. | Complete large topology port compiled and benched. |
| `278a755` | Reorder operations in do_move | APPLIED_ADAPTED | Key/prefetch finalization occurs before physical piece movement; movement block is relocated accordingly. | Adapted to local history prefetch and Deepalienist position hooks. |
| `7b37032` | Stop searching if we cannot find a better move | APPLIED_EXACT | Iterative deepening stops for `multiPV` mate-in-3 floor or forced mated-in-2 best line in addition to time. | Exact search condition present. |
| `5595cb2` | Fix FEN validation of pawns on first or eighth rank | APPLIED_ADAPTED | FEN parsing rejects `fenPawns & (Rank1BB | Rank8BB)`. | Deepalienist parser has a local `fenPawns` staging variable, so expression is semantically adapted. |
| `86f1df7` | Fix material key computation error | APPLIED_EXACT | Promotion material-key indices use incremented promoted count and decremented pawn count in the corrected direction. | Full-position material check topology also retained. |

## 13. Block Jun 14

| Commit | Title | Status | Deepalienist evidence | Notes |
|---|---|---|---|---|
| `0dc19b6` | More stable matetrack CI | APPLIED_ADAPTED | Local `matetrack.yml` contains the stabilization/retry changes atop Deepalienist workflow naming and ownership. | CI adaptation only. |
| `718a001` | Fixup RelaxedAtomic operator Types | APPLIED_EXACT | `RelaxedAtomic::operator+=` and `operator-=` take `T`, not `int`. | Exact type correction present. |
| `6e4e03f` | Replace Remaining Types | APPLIED_ADAPTED | Remaining affected benchmark/memory/movegen/NNUE/search/thread/tune surfaces use fixed-width and size aliases. | Adapted around local APIs and files while preserving type intent. |
| `ed8e35d` | Remove unused code from the post NNUE accumulator merge | APPLIED_EXACT | Obsolete full-threat/half-KA helper code and stale accumulator/type fields are removed. | Current code is the post-cleanup topology and compiles. |
| `f9beec5` | Reuse computed ray bitboard in update_piece_threats | APPLIED_EXACT | `update_piece_threats` reuses local `ray` in the `noRaysContaining` condition. | Exact one-line optimization present. |
| `f9b002c` | Share continuation history between threads | APPLIED_EXACT | `PieceToHistory` is atomic, `SharedHistories` owns continuation history, and workers bind to the shared table. | Complete shared ownership is present. |
| `74a0a73` | Final fix for MultiPV mate PV corner cases | APPLIED_EXACT | Current root state has `lastBestMovePV`, `lastBestMoveScore`, `previousScoreExact`, bound repair, partial-line marking, and full PV restoration. | Final authority behavior is present and supersedes intermediate Jun 9 form. |

## 14. Missing commits summary

No commits are classified `MISSING` or `PARTIAL_OR_SUSPECT`.

## 15. Not-applicable summary

- **Jun 6 — `8416cca`:** official universal-build workflow/target is absent and intentionally unsupported in Deepalienist.
- **Jun 9 — `8e711c2`:** wasm32 and wasm32-relaxed-simd targets/workflow are absent and intentionally unsupported in Deepalienist.

No Jun 8, Jun 10, or Jun 14 item is `NOT_APPLICABLE`.

## 16. Required follow-up phases

No temporal topology recovery phase is required. The next action may be release packaging/closure, subject to the project's normal release checks. Unsupported universal-binary and WebAssembly targets should not be introduced as a “recovery” unless Deepalienist separately chooses to own and validate those platforms.

## 17. Final verdict

# TEMPORAL COVERAGE PASS

All applicable official Stockfish development topology from Jun 6 through Jun 14, 2026, ending at `74a0a73715322608332038f7c0151ddf0609a59a`, is present in temporal effect. There are no `MISSING` or `PARTIAL_OR_SUSPECT` commits. The two `NOT_APPLICABLE` classifications are backed by absent, intentionally unsupported build surfaces, not by missing engine topology.

## Validation record

- `make -C src -j2 build ARCH=x86-64-avx2 COMP=gcc`: PASS; complete compile/link, including `attacks.o`, custom book/experience, Syzygy, NNUE, and engine surfaces.
- `./src/Deepalienist-4.90-140626-avx2 bench 16 1 5 default depth`: PASS; 51/51 positions, 44,137 nodes, correct identity/network.
- The initially attempted binary spelling `Deepalienist-4.90-140626-x86-64-avx2` did not exist because the Makefile's actual `BUILD_ARCH_TAG` is `avx2`; this was a command-path correction, not a build or engine failure.
