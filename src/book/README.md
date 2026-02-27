# Book Management (Polyglot BIN / ChessBase CTG)

This directory contains the integrated book management code (thanks to Khalid Omar).

## Selection order

When a root move is requested, BrainLearnJR checks the configured sources in this order:

1. **All configured BIN books** (Book 1 + Book 2 merged)
2. **All configured CTG books** (Book 1 + Book 2 merged)
3. **Live book** (only if the livebook subsystem is compiled in)

If any BIN move exists, CTG and live book are skipped. If BIN has no move and CTG has a move,
live book is skipped. If none provide a move, the normal search runs.

## UCI options

Book 1:
- `CTG/BIN Book 1 File` (string, default empty)
- `Book 1 Width` (spin, default 1, min 1, max 50)
- `Book 1 Depth` (spin, default 255, min 0, max 255)
- `(CTG) Book 1 Only Green` (check, default false)

Book 2:
- `CTG/BIN Book 2 File` (string, default empty)
- `Book 2 Width` (spin, default 1, min 1, max 50)
- `Book 2 Depth` (spin, default 255, min 0, max 255)
- `(CTG) Book 2 Only Green` (check, default false)

### File paths

- An empty file path disables the book.
- Relative paths are resolved relative to the engine **executable directory**.
- Absolute paths are used as-is.

### Width and randomness

- `Width = 1` selects the best book move deterministically.
- `Width > 1` selects uniformly at random among the top-N moves.

### Depth

Depth limits book usage **by ply** starting from the current root position:

- The internal book ply counter resets to `0` on each new `position` command.
- Depth is checked as `ply <= depth`; if `ply > depth`, that slot is ignored.

## `book` UCI command

The custom `book` command prints a short report for the current position, including:

- ASCII board
- FEN
- Polyglot hash key
- Configured book files and their available moves

If no book moves are available, the command prints `no book moves` and exits cleanly.

## Known limitations (CTG)

ChessBase CTG parsing has the following limitations:

- **Underpromotion** moves are not supported.
- Positions with **two queens of the same color** are not supported.

These limitations are inherent to the CTG parsing logic and are not specific to BrainLearnJR.
