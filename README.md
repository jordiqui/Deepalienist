# Deepalienist-3.50-27022026

UCI chess engine updated to SFNNv13, with Polyglot opening book support and Experience file support.

## Features

- **SFNNv13 NNUE** evaluation support.
- **Polyglot Book1/Book2** opening-book slots for `.bin` files.
- **Experience file** learning and book-style move reuse.

## UCI Options: Polyglot opening books (`.bin`)

Deepalienist supports two opening-book slots. In GUIs, these are typically configured through `setoption` commands.

### Core options

- **Book1 / Book2**: enable each slot by assigning a valid file path.
- **Book1 File / Book2 File**: path to the `.bin` opening book file.
  - Absolute paths are recommended to avoid GUI working-directory issues.
- **Book1 BestBookMove / Book2 BestBookMove**: if enabled, choose the highest-weight move from the book.
- **Book1 Depth / Book2 Depth**: maximum book usage depth in plies (default `255`).
- **Book1 Width / Book2 Width**: how many candidate moves are considered when selecting from the book.

### Practical GUI notes (Fritz / Arena / CuteChess)

- Prefer absolute file paths in `setoption name ... value ...`.
- Confirm option names exactly as exposed by your engine build.
- Verify in GUI logs that the option was accepted, for example:
  - `setoption name Book1 File value C:\\Books\\book1.bin`
  - `setoption name Book1 Depth value 255`
- If a GUI silently ignores an option, check for name mismatches and quoting/path issues.

## Experience file integration

Deepalienist uses `experience.exp` to persist learned move data.

### Entry structure

Each entry stores:

- Best move
- Position hash key
- Depth
- Score
- Performance

### Load and merge convention

At startup, Deepalienist loads `experience.exp` and then merges optional files matching:

- `experience[qualityIndex].exp` (for example `experience0.exp`, `experience1.exp`, ...)

Merged data is consolidated back into the main file.

### When writes happen

- Experience writes are finalized on `stop`/`quit` flows.
- Learning logic is active from game start.
- Experience usage is constrained to endgame-like states with **8 pieces or fewer**.

### Learning options

- **Read only learning** (default: `false`): load and use experience without writing updates.
- **Self Q-learning** (default: `false`): enables self-learning update mode.
- **Experience Book** (default: `false`): enables experience-driven move selection.
- **Experience Book Max Moves** (default: `100`, range `1..100`): cap on moves considered from experience.
- **Experience Book Min Depth** (default: `4`, range `1..255`): minimum depth before an entry is considered.
- **`quickresetexp` token**: rebuilds performance values from the current experience file.

Reference (BrainLearn): [amchess/BrainLearn](https://github.com/amchess/BrainLearn).

## Versioning / Build ID

This repository is branded as:

- **Deepalienist-3.50-27022026**

Use this identifier in release notes and packaging metadata. Engine runtime ID strings should remain code-driven unless you intentionally update engine source.

## Downloads / Usage

- Build from source in `src/` (GNU Make):

```bash
cd src
make -j profile-build
```

- Run via any UCI-compatible GUI or UCI command-line workflow.

## GitHub “About” suggestions

Suggested short description:

- `Deepalienist-3.50-27022026: UCI chess engine with SFNNv13 NNUE, Polyglot Book1/Book2, and Experience file learning.`

Suggested topics (6–10):

- `chess-engine`
- `uci`
- `nnue`
- `sfnnv13`
- `polyglot-book`
- `opening-book`
- `chess-ai`
- `cpp`
- `engine-dev`

## Legal / Attribution

- License: GNU General Public License v3.0 (see `Copying.txt`).
- Authors and notices are preserved in `AUTHORS` and source headers.
- Upstream historical attribution is retained in license-relevant files where required.
