Deepalienist-3.50-27022026

Deepalienist is a UCI-compatible chess engine updated to SFNNv13, integrating:

NNUE neural network evaluation (SFNNv13 generation)

Dual Polyglot opening book support (Book1 / Book2)

Persistent experience file learning system

Advanced UCI configuration options for competitive and testing environments

This repository contains the full source tree of Deepalienist-3.50-27022026.

Version

Engine ID: Deepalienist-3.50-27022026
NNUE generation: SFNNv13
Protocol: UCI

Core Features
1. NNUE Evaluation (SFNNv13)

Deepalienist uses the SFNNv13 neural network architecture for position evaluation.
The evaluation is fully integrated in the search pipeline and compatible with modern deterministic testing environments.

Polyglot Opening Book Support (.bin)

Deepalienist exposes two configurable Polyglot slots, allowing either combined repertoire usage or fallback configurations.

Each slot provides identical controls.

UCI Options – Polyglot
Book Slots

Book1 (boolean) – enables slot 1

Book2 (boolean) – enables slot 2

Book Files

Book1 File – absolute path to .bin file

Book2 File – absolute path to .bin file

Absolute paths are recommended to avoid GUI working-directory issues.

Selection Controls

Book1 BestBookMove / Book2 BestBookMove
Restricts play to the highest-weighted move found in the book.

Book1 Depth / Book2 Depth (default: 255 plies)
Limits how deep into the game the engine extracts book moves.

Book1 Width / Book2 Width
Controls how many weighted alternatives are considered during selection.

Higher width values allow broader probabilistic exploration.

Practical GUI Notes (Fritz / Arena / CuteChess)

For stable Polyglot usage:

Place the .bin file in a simple directory without spaces, e.g.
C:\Engines\Deepalienist\

Use absolute paths in Book1 File.

Enable Book1 = true.

If the GUI also has its own book:

Test GUI book OFF + Engine book ON

Test GUI book ON + Engine book OFF

Check the engine log at startup to confirm:

setoption name Book1 value true
setoption name Book1 File value C:\...

If the GUI fails with long paths, shorten the folder name and avoid special characters.

Experience File Integration

Deepalienist integrates a persistent learning system compatible with the BrainLearn-style experience file format.

Learning data is stored in:

experience.exp
Experience File Structure

Each stored entry includes:

Best move

Board signature (hash key)

Best move depth

Best move score

Best move performance value

Performance is derived from score and depth and may be adjusted using compatible learning tools.

Load and Merge Behaviour

At startup, Deepalienist:

Loads experience.exp

Automatically merges files matching:

experience<qualityIndex>.exp

where:

qualityIndex is an integer

0 represents highest user-defined quality

Legacy .bin experience files may be reused by renaming them to .exp.

Learning Activation Rules

Learning is active:

From the beginning of each game

When eight or fewer pieces remain on the board

Positions are written back when:

A best score is found at depth ≥ 4 plies

The engine receives a quit or stop UCI command

Learning-Related UCI Options
Read only learning (boolean, default: false)

Prevents updates to the experience file.

Self Q-learning (boolean, default: false)

Activates Q-learning mode optimised for self-play environments.

Experience Book (boolean, default: false)

Allows Deepalienist to consult the experience file as an opening book.

Move selection priority:

Maximum win probability

Internal evaluation score

Search depth

Experience Book Max Moves

(integer, default 100, min 1, max 100)

Limits candidate moves read from experience.

Experience Book Min Depth

(integer, default 4, min 1, max 255)

Minimum stored depth required for move selection.

quickresetexp (UCI token)

Resets stored performance values to defaults.

Building

Standard build procedure applies depending on platform and compiler.
Refer to the src/ directory and build scripts provided in this repository.

Usage

Deepalienist is a UCI engine and can be used in:

CuteChess

Arena

Fritz-compatible GUIs

Custom tournament frameworks

Headless testing environments

Legal / Attribution

Deepalienist is distributed under the GNU General Public License (GPL).

This project is based on prior open-source chess engine research and NNUE developments.
All required license and attribution files are preserved in this repository.

See:

Copying.txt

AUTHORS

Repository Topics (Suggested)

uci
chess-engine
nnue
sfnnv13
polyglot
opening-book
experience-file
brainlearn
computer-chess
