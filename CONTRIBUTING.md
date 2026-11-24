# Contributing Guidelines

Thank you for helping improve this project. Follow every rule below before opening a pull request.

## Legal and Copyright Rules

1. Never commit ROM files (`*.sfc`, `*.smc`, `*.fig`, `*.swc`, `*.rom`).
2. Never commit lifted C output inside `generated/`.
3. Never commit extracted sprites, tilemaps, audio samples, or original disassembly dumps.
4. Keep every pull request limited to original C runtime, lifter, UI, test, or documentation code.
5. Respect the project license in [`LICENSE`](LICENSE) (PolyForm Noncommercial License 1.0.0).

## Build and Test Steps

1. Clone the repository with submodules:
   `git clone --recurse-submodules https://github.com/Vedovate/TopGear2-snes-Recomp.git`
2. Build and run unit regression tests without a ROM:
   `make NO_LIFT=1 all test_regressions`
   `./test_regressions`
3. Lift your own legally obtained ROM locally:
   `make lift ROM="path/to/Top Gear 2 (USA).sfc"`
4. Build and run all integration tests with your ROM:
   `make all`
   `make test ROM="path/to/Top Gear 2 (USA).sfc"`
5. Clean generated artifacts before running `git status`:
   `make distclean`

## Code Quality Rules

1. Compile all C files cleanly with `-Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation`.
2. Avoid hardcoding machine paths, user names, or fixed ROM filenames.
3. Add a unit or regression test in `tests/test_regressions.c` for every bug fix.
