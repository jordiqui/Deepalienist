# Experience Book Min Visits: merge-ready note

Status: candidate for merge, pending local binary/runtime validation.

## Scope lock

This change remains the first opt-in experiment only:

- Option: `Experience Book Min Visits`
- Default: `1`
- Behavior: eligibility gate only
- No ranking changes
- No reorder-policy changes
- No file-format changes
- `src/learn/learn.cpp` remains untouched

## Strict pre-merge verification

### Exact files changed by the feature

Feature commit `9b501576f29a0f320bc995a9b1f2f9cc0c020d0b` changes exactly these files:

- `scripts/experience_audit.py`
- `src/engine.cpp`
- `src/experience.cpp`
- `src/experience.h`

### Exact option wiring and default

- UCI option name: `Experience Book Min Visits`
- Declared default: `1`
- Declared range: `1..65535`
- Settings fallback default: `1`

### Exact code path where the gate is applied

The gate is applied only in `Experience::on_new_position()` in `src/experience.cpp` while building experience-book candidates for root move handling:

1. Read `settings.bookMinVisits`
2. Iterate stored experience entries for the current position
3. Reject entries with `entry.visits < settings.bookMinVisits`
4. Continue with the existing score/depth/quality pipeline for the remaining entries

This means the new option affects only experience-book eligibility before candidate selection. It does not alter parsing, persistence, saved entry layout, score computation, ranking formula, or reorder policy.

### Exact audit fields added

The audit report exposes `min_visits_rejected` in both counter groups:

- lifetime counters: `min_visits_rejected`
- since-newgame counters: `min_visits_rejected`

The report continues to emit the existing counters and distributions; this change adds rejection visibility for the new eligibility gate.

### Defaults and unchanged behavior

- Defaults remain unchanged for normal search behavior.
- With the default value `Experience Book Min Visits = 1`, every stored entry with at least one visit remains eligible exactly as before.
- No default search behavior is intentionally changed.
- No save/load path is intentionally changed.
- No parsing behavior is intentionally changed.
- No ranking or reorder logic is intentionally changed.

### Runtime-validation status

This change is **not yet runtime-validated** in the user's real Deepalienist environment with the user's actual `experience.exp` file.

## Merge-ready summary

- Commit hash: `9b501576f29a0f320bc995a9b1f2f9cc0c020d0b`
- Concise changelog:
  - adds opt-in UCI option `Experience Book Min Visits`
  - stores the configured threshold in experience settings
  - applies the threshold only as an experience-book eligibility gate
  - adds `min_visits_rejected` audit visibility
- Risk assessment:
  - low risk at defaults because default `1` preserves prior eligibility
  - medium observational risk when opted in because fewer experience-book candidates may qualify
  - low persistence risk because no file-format or parsing changes are introduced
- Rollback note:
  - immediate operational rollback is `setoption name Experience Book Min Visits value 1`
  - code rollback is reverting commit `9b501576f29a0f320bc995a9b1f2f9cc0c020d0b`

## Exact post-merge local test procedure

Run these commands against a fully compiled local binary in the real Deepalienist environment using the real `experience.exp` file.

### A) Baseline

```text
uci
setoption name Experience Audit Enabled value true
setoption name Experience File value experience.exp
setoption name Experience Book value true
isready
ucinewgame
position startpos
go depth 8
setoption name Experience Audit Report
quit
```

### B) Experiment

```text
uci
setoption name Experience Audit Enabled value true
setoption name Experience File value experience.exp
setoption name Experience Book value true
setoption name Experience Book Min Visits value 2
isready
ucinewgame
position startpos
go depth 8
setoption name Experience Audit Report
quit
```

### Expected comparison focus

- Confirm the engine starts and accepts both option sets.
- Confirm the baseline remains behaviorally equivalent to current default usage.
- Confirm the experiment shows non-zero `min_visits_rejected` only when the real data contains one-visit entries that would otherwise qualify.
- Confirm any move-selection differences arise only from eligibility filtering, not from a ranking or reorder-policy change.

## Ultra-strict final verification note before merge

Before merge, verify all of the following remain true:

- no duplicate option wiring
- no unintended change to default search behavior
- no unintended change to save/load path
- no unintended change to parsing
- no unintended change to ranking or reorder logic
- no hidden dependency on synthetic test files

Final status statement: candidate for merge, pending local binary/runtime validation.
